/**
 * Twiddler 4 Custom Firmware - Chord Detection Implementation
 *
 * Implements the chord input state machine
 */

#include "nchorder_chords.h"
#include "nchorder_hid.h"
#include "app_timer.h"
#include "nrf_error.h"
#include "nrf_log.h"
#include <string.h>

// Chord mapping storage
#define MAX_CHORD_MAPPINGS 256
#define MAX_MOUSE_MAPPINGS 32
#define MAX_CONSUMER_MAPPINGS 32
#define MAX_MULTICHAR_MAPPINGS 64
#define MAX_MULTICHAR_KEYS 512  // Total keys across all macros

static chord_mapping_t m_key_mappings[MAX_CHORD_MAPPINGS];
static uint16_t m_key_mapping_count = 0;

static chord_mouse_t m_mouse_mappings[MAX_MOUSE_MAPPINGS];
static uint16_t m_mouse_mapping_count = 0;

// Consumer control (media keys) storage
typedef struct {
    chord_t chord;          // Button combination
    uint16_t usage_code;    // Consumer control usage code
} chord_consumer_t;

static chord_consumer_t m_consumer_mappings[MAX_CONSUMER_MAPPINGS];
static uint16_t m_consumer_mapping_count = 0;

// Multi-character macro storage
typedef struct {
    chord_t chord;              // Button combination
    uint16_t keys_offset;       // Offset into m_multichar_keys
    uint16_t keys_count;        // Number of keys in sequence
} multichar_mapping_t;

static multichar_mapping_t m_multichar_mappings[MAX_MULTICHAR_MAPPINGS];
static uint16_t m_multichar_mapping_count = 0;

static multichar_key_t m_multichar_keys[MAX_MULTICHAR_KEYS];
static uint16_t m_multichar_keys_used = 0;

// Pointer to config data for string table access
static const uint8_t *m_config_data = NULL;
static size_t m_config_size = 0;

// Counters for skipped/unimplemented chord types
static uint16_t m_system_chords_skipped = 0;
static uint16_t m_multichar_chords_skipped = 0;
static uint16_t m_unknown_chords_skipped = 0;

// Config file format constants (from RE)
#define CFG_HEADER_SIZE       128
#define CFG_CHORD_SIZE        8
#define CFG_CHORD_COUNT_OFF   0x08
#define CFG_STRING_OFF_OFF    0x0A
#define CFG_INDEX_TABLE_OFF   0x60
#define CFG_CHORDS_START      0x80

// Event types (low byte of modifier field)
#define CFG_EVENT_MOUSE       0x01
#define CFG_EVENT_KEYBOARD    0x02
#define CFG_EVENT_CONSUMER    0x03  // Consumer control (media keys)
#define CFG_EVENT_SYSTEM      0x07
#define CFG_EVENT_MULTICHAR   0xFF

// Mouse function codes (high byte when event type = 0x01)
#define CFG_MOUSE_TOGGLE      0x01
#define CFG_MOUSE_LEFT_CLICK  0x02
#define CFG_MOUSE_SCROLL_TOG  0x04
#define CFG_MOUSE_SPEED_DEC   0x05
#define CFG_MOUSE_SPEED_CYC   0x06
#define CFG_MOUSE_MIDDLE      0x0A
#define CFG_MOUSE_SPEED_INC   0x0B
#define CFG_MOUSE_RIGHT_CLICK 0x0C

// Default basic chord mappings (can be overridden by config)
// Using standard Twiddler TabSpace layout for common letters
static const chord_mapping_t default_mappings[] = {
    // Single finger buttons - common letters (row 1 = index, row 2 = middle, etc.)
    {CHORD_F1M, 0, HID_KEY_E, 0},  // Most common letter
    {CHORD_F2M, 0, HID_KEY_T, 0},
    {CHORD_F1L, 0, HID_KEY_A, 0},
    {CHORD_F1R, 0, HID_KEY_O, 0},
    {CHORD_F2L, 0, HID_KEY_I, 0},
    {CHORD_F2R, 0, HID_KEY_N, 0},
    {CHORD_F3L, 0, HID_KEY_S, 0},
    {CHORD_F3M, 0, HID_KEY_R, 0},
    {CHORD_F3R, 0, HID_KEY_H, 0},
    {CHORD_F4L, 0, HID_KEY_L, 0},  // Row 4 (pinky)
    {CHORD_F4M, 0, HID_KEY_D, 0},
    {CHORD_F4R, 0, HID_KEY_C, 0},

    // Thumb + finger combinations
    {CHORD_T1 | CHORD_F1M, HID_MOD_LSHIFT, HID_KEY_E, 0},  // Shift+E

    // Space and common controls
    {CHORD_F2L | CHORD_F2M, 0, HID_KEY_SPACE, 0},
    {CHORD_F3L | CHORD_F3M | CHORD_F3R, 0, HID_KEY_ENTER, 0},
    {CHORD_F4L | CHORD_F4M | CHORD_F4R, 0, HID_KEY_BACKSPACE, 0},
};

void chord_init(chord_context_t *ctx)
{
    memset(ctx, 0, sizeof(chord_context_t));
    ctx->state = CHORD_STATE_IDLE;

    // Load default mappings
    m_key_mapping_count = sizeof(default_mappings) / sizeof(default_mappings[0]);
    memcpy(m_key_mappings, default_mappings, sizeof(default_mappings));
}

bool chord_update(chord_context_t *ctx, chord_t buttons)
{
    bool chord_completed = false;

    switch (ctx->state) {
        case CHORD_STATE_IDLE:
            if (buttons != 0) {
                // First button pressed, start building chord
                ctx->state = CHORD_STATE_BUILDING;
                ctx->current_chord = buttons;
                ctx->max_chord = buttons;
                ctx->chord_fired = false;
                ctx->press_time = app_timer_cnt_get();
            }
            break;

        case CHORD_STATE_BUILDING:
            if (buttons == 0) {
                // All buttons released - fire the chord
                ctx->state = CHORD_STATE_IDLE;
                chord_completed = true;
            } else if (buttons != ctx->current_chord) {
                // Buttons changed
                ctx->current_chord = buttons;
                // Track maximum chord (all buttons ever pressed together)
                ctx->max_chord |= buttons;
            }
            // If buttons are stable, could transition to HELD state
            // but for simplicity we just track max_chord
            break;

        case CHORD_STATE_HELD:
            if (buttons == 0) {
                // Released
                ctx->state = CHORD_STATE_IDLE;
                chord_completed = true;
            } else if (buttons != ctx->current_chord) {
                // Some buttons released but not all
                ctx->current_chord = buttons;
            }
            break;

        case CHORD_STATE_RELEASING:
            if (buttons == 0) {
                ctx->state = CHORD_STATE_IDLE;
                chord_completed = true;
            }
            break;
    }

    return chord_completed;
}

chord_t chord_get_completed(chord_context_t *ctx)
{
    return ctx->max_chord;
}

const chord_mapping_t* chord_lookup_key(chord_t chord)
{
    for (uint16_t i = 0; i < m_key_mapping_count; i++) {
        if (m_key_mappings[i].chord == chord) {
            return &m_key_mappings[i];
        }
    }
    return NULL;
}

const chord_mouse_t* chord_lookup_mouse(chord_t chord)
{
    for (uint16_t i = 0; i < m_mouse_mapping_count; i++) {
        if (m_mouse_mappings[i].chord == chord) {
            return &m_mouse_mappings[i];
        }
    }
    return NULL;
}

// Helper to read little-endian u16
static uint16_t read_u16_le(const uint8_t *data) {
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

// Helper to read little-endian u32
static uint32_t read_u32_le(const uint8_t *data) {
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

// Convert config modifier byte to HID modifier bits
static uint8_t config_mod_to_hid(uint8_t cfg_mod) {
    uint8_t hid_mod = 0;
    if (cfg_mod & 0x01) hid_mod |= 0x02;  // Shift -> Left Shift
    if (cfg_mod & 0x02) hid_mod |= 0x01;  // Ctrl -> Left Ctrl
    if (cfg_mod & 0x04) hid_mod |= 0x04;  // Alt -> Left Alt
    if (cfg_mod & 0x20) hid_mod |= 0x08;  // GUI -> Left GUI
    return hid_mod;
}

void chord_load_config(const uint8_t *config_data, size_t config_size)
{
    // Validate minimum size
    if (config_data == NULL || config_size < CFG_HEADER_SIZE) {
        return;  // Keep default mappings
    }

    // Read header fields
    uint16_t chord_count = read_u16_le(&config_data[CFG_CHORD_COUNT_OFF]);
    uint16_t string_table_offset = read_u16_le(&config_data[CFG_STRING_OFF_OFF]);

    // Validate chord count
    if (chord_count == 0 || chord_count > MAX_CHORD_MAPPINGS) {
        return;  // Invalid config
    }

    // Validate file size
    size_t required_size = CFG_CHORDS_START + (chord_count * CFG_CHORD_SIZE);
    if (config_size < required_size) {
        return;  // File too small
    }

    // Validate string table offset (must not point into chord data)
    if (string_table_offset < required_size && string_table_offset != 0) {
        // String table overlaps chord data - this would crash firmware
        return;
    }

    // Reset mappings and counters
    m_key_mapping_count = 0;
    m_mouse_mapping_count = 0;
    m_consumer_mapping_count = 0;
    m_multichar_mapping_count = 0;
    m_multichar_keys_used = 0;
    m_system_chords_skipped = 0;
    m_multichar_chords_skipped = 0;
    m_unknown_chords_skipped = 0;

    // Store config data pointer for string table access
    m_config_data = config_data;
    m_config_size = config_size;

    // Parse chord entries
    const uint8_t *chord_ptr = &config_data[CFG_CHORDS_START];

    for (uint16_t i = 0; i < chord_count; i++) {
        uint32_t bitmask = read_u32_le(&chord_ptr[0]);
        uint16_t modifier = read_u16_le(&chord_ptr[4]);
        uint16_t keycode = read_u16_le(&chord_ptr[6]);

        // Extract button chord (low 16 bits of bitmask)
        chord_t chord = (chord_t)(bitmask & 0xFFFF);

        // Extract event type (low byte of modifier)
        uint8_t event_type = modifier & 0xFF;
        uint8_t mod_flags = (modifier >> 8) & 0xFF;

        switch (event_type) {
            case CFG_EVENT_KEYBOARD:
                // Standard keyboard event
                if (m_key_mapping_count < MAX_CHORD_MAPPINGS) {
                    m_key_mappings[m_key_mapping_count].chord = chord;
                    m_key_mappings[m_key_mapping_count].modifiers = config_mod_to_hid(mod_flags);
                    m_key_mappings[m_key_mapping_count].keycode = (uint8_t)(keycode & 0xFF);
                    m_key_mappings[m_key_mapping_count].consumer_code = 0;
                    m_key_mapping_count++;
                }
                break;

            case CFG_EVENT_MOUSE:
                // Mouse action
                if (m_mouse_mapping_count < MAX_MOUSE_MAPPINGS) {
                    m_mouse_mappings[m_mouse_mapping_count].chord = chord;
                    m_mouse_mappings[m_mouse_mapping_count].dx = 0;
                    m_mouse_mappings[m_mouse_mapping_count].dy = 0;
                    m_mouse_mappings[m_mouse_mapping_count].wheel = 0;

                    // Map mouse function to button state
                    switch (mod_flags) {
                        case CFG_MOUSE_LEFT_CLICK:
                            m_mouse_mappings[m_mouse_mapping_count].buttons = 0x01;
                            break;
                        case CFG_MOUSE_RIGHT_CLICK:
                            m_mouse_mappings[m_mouse_mapping_count].buttons = 0x02;
                            break;
                        case CFG_MOUSE_MIDDLE:
                            m_mouse_mappings[m_mouse_mapping_count].buttons = 0x04;
                            break;
                        default:
                            m_mouse_mappings[m_mouse_mapping_count].buttons = 0;
                            break;
                    }
                    m_mouse_mapping_count++;
                }
                break;

            case CFG_EVENT_CONSUMER:
                // Consumer control (media keys)
                // keycode field contains the 16-bit consumer usage code
                if (m_consumer_mapping_count < MAX_CONSUMER_MAPPINGS) {
                    m_consumer_mappings[m_consumer_mapping_count].chord = chord;
                    m_consumer_mappings[m_consumer_mapping_count].usage_code = (uint16_t)(keycode & 0xFFFF);
                    m_consumer_mapping_count++;
                    NRF_LOG_DEBUG("Consumer chord: 0x%04X -> usage 0x%04X",
                                  chord, keycode & 0xFFFF);
                }
                break;

            case CFG_EVENT_SYSTEM:
                // System function - NOT IMPLEMENTED
                // (config switching, toggles, sleep/wake)
                m_system_chords_skipped++;
                break;

            case CFG_EVENT_MULTICHAR:
                // Multi-character string - store for later string table parsing
                if (m_multichar_mapping_count < MAX_MULTICHAR_MAPPINGS) {
                    m_multichar_mappings[m_multichar_mapping_count].chord = chord;
                    m_multichar_mappings[m_multichar_mapping_count].keys_offset = 0;
                    m_multichar_mappings[m_multichar_mapping_count].keys_count = 0;
                    // keycode field is index into string table
                    // Store temporarily in keys_offset, will resolve after parsing all chords
                    m_multichar_mappings[m_multichar_mapping_count].keys_offset = (uint16_t)(keycode & 0xFFFF);
                    m_multichar_mapping_count++;
                } else {
                    m_multichar_chords_skipped++;
                }
                break;

            default:
                // Unknown event type - NOT IMPLEMENTED
                m_unknown_chords_skipped++;
                break;
        }

        chord_ptr += CFG_CHORD_SIZE;
    }

    // Parse string table for multi-char macros with defensive validation
    // Note: Original Twiddler firmware has crash bugs in this area, so we're extra careful
    if (m_multichar_mapping_count > 0 && string_table_offset > 0 &&
        string_table_offset < config_size)
    {
        // Find maximum string index to know how many location entries to read
        uint16_t max_string_index = 0;
        for (uint16_t i = 0; i < m_multichar_mapping_count; i++) {
            uint16_t idx = m_multichar_mappings[i].keys_offset;
            // Sanity check: string index should be reasonable (< 256 strings)
            if (idx < 256 && idx > max_string_index) {
                max_string_index = idx;
            }
        }

        // String location table starts at string_table_offset
        // Each entry is 4 bytes (uint32_t offset into config)
        size_t loc_table_size = (size_t)(max_string_index + 1) * 4;

        // Validate location table fits in file
        if (string_table_offset + loc_table_size > config_size) {
            NRF_LOG_WARNING("Storage: String location table truncated");
            // Clear mappings we can't resolve
            m_multichar_mapping_count = 0;
        } else {
            // Read location table
            const uint8_t *loc_table = &config_data[string_table_offset];

            // Process each multi-char mapping
            for (uint16_t i = 0; i < m_multichar_mapping_count; i++) {
                uint16_t str_index = m_multichar_mappings[i].keys_offset;

                // Bounds check on string index
                if (str_index > max_string_index) {
                    NRF_LOG_WARNING("Storage: Invalid string index %d", str_index);
                    m_multichar_mappings[i].keys_count = 0;
                    continue;
                }

                // Read string offset with bounds check
                size_t loc_off = (size_t)str_index * 4;
                if (loc_off + 4 > loc_table_size) {
                    m_multichar_mappings[i].keys_count = 0;
                    continue;
                }

                uint32_t str_offset = read_u32_le(&loc_table[loc_off]);

                // Validate string offset is within config
                if (str_offset < 2 || str_offset + 2 > config_size) {
                    NRF_LOG_WARNING("Storage: Invalid string offset 0x%08X", str_offset);
                    m_multichar_mappings[i].keys_count = 0;
                    continue;
                }

                // String format: 2-byte length (in bytes), then (mod, key) pairs
                uint16_t str_len = read_u16_le(&config_data[str_offset]);

                // Sanity checks on string length
                // - Must be even (2 bytes per key pair + 2 for length)
                // - Must have at least one key pair (str_len >= 4)
                // - Must be reasonable (< 512 bytes = 255 keys max)
                if ((str_len & 1) != 0 || str_len < 4 || str_len > 512) {
                    NRF_LOG_WARNING("Storage: Invalid string length %d", str_len);
                    m_multichar_mappings[i].keys_count = 0;
                    continue;
                }

                uint16_t num_keys = (str_len / 2) - 1;  // Subtract 1 for length field

                // Final bounds check: ensure all key data fits in config
                if (str_offset + 2 + (size_t)num_keys * 2 > config_size) {
                    NRF_LOG_WARNING("Storage: String data truncated");
                    m_multichar_mappings[i].keys_count = 0;
                    continue;
                }

                // Store keys in our buffer (with capacity check)
                uint16_t start_offset = m_multichar_keys_used;
                uint16_t keys_stored = 0;

                for (uint16_t k = 0; k < num_keys; k++) {
                    if (m_multichar_keys_used >= MAX_MULTICHAR_KEYS) {
                        NRF_LOG_WARNING("Storage: Multichar key buffer full");
                        break;
                    }

                    size_t key_off = str_offset + 2 + (size_t)k * 2;
                    uint8_t mod = config_data[key_off];
                    uint8_t key = config_data[key_off + 1];

                    // Skip null/invalid keys (keycode 0 with no modifiers)
                    if (key == 0 && mod == 0) {
                        continue;
                    }

                    // Convert config modifier to HID modifier
                    m_multichar_keys[m_multichar_keys_used].modifiers = config_mod_to_hid(mod);
                    m_multichar_keys[m_multichar_keys_used].keycode = key;
                    m_multichar_keys_used++;
                    keys_stored++;
                }

                // Update mapping with actual key buffer location
                m_multichar_mappings[i].keys_offset = start_offset;
                m_multichar_mappings[i].keys_count = keys_stored;
            }
        }
    }

    NRF_LOG_DEBUG("Storage: Loaded %d multichar macros", m_multichar_mapping_count);
}

uint16_t chord_get_mapping_count(void)
{
    return m_key_mapping_count;
}

uint16_t chord_get_mouse_mapping_count(void)
{
    return m_mouse_mapping_count;
}

// Returns count of skipped chords that need implementation
// Use this to detect when config has features we don't support
uint16_t chord_get_skipped_count(void)
{
    return m_system_chords_skipped + m_multichar_chords_skipped + m_unknown_chords_skipped;
}

// Get details of what was skipped (for debugging)
void chord_get_skipped_details(uint16_t *system, uint16_t *multichar, uint16_t *unknown)
{
    if (system) *system = m_system_chords_skipped;
    if (multichar) *multichar = m_multichar_chords_skipped;
    if (unknown) *unknown = m_unknown_chords_skipped;
}

bool chord_lookup_multichar(chord_t chord, const multichar_key_t **p_keys, uint16_t *p_count)
{
    if (p_keys == NULL || p_count == NULL) {
        return false;
    }

    for (uint16_t i = 0; i < m_multichar_mapping_count; i++) {
        if (m_multichar_mappings[i].chord == chord) {
            uint16_t offset = m_multichar_mappings[i].keys_offset;
            uint16_t count = m_multichar_mappings[i].keys_count;

            if (count > 0 && offset + count <= m_multichar_keys_used) {
                *p_keys = &m_multichar_keys[offset];
                *p_count = count;
                return true;
            }
            break;
        }
    }

    *p_keys = NULL;
    *p_count = 0;
    return false;
}

uint16_t chord_get_multichar_count(void)
{
    return m_multichar_mapping_count;
}

bool chord_lookup_consumer(chord_t chord, uint16_t *p_usage_code)
{
    if (p_usage_code == NULL) {
        return false;
    }

    for (uint16_t i = 0; i < m_consumer_mapping_count; i++) {
        if (m_consumer_mappings[i].chord == chord) {
            *p_usage_code = m_consumer_mappings[i].usage_code;
            return true;
        }
    }

    *p_usage_code = 0;
    return false;
}

uint16_t chord_get_consumer_count(void)
{
    return m_consumer_mapping_count;
}
