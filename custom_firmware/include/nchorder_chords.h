/**
 * Twiddler 4 Custom Firmware - Chord Detection and Mapping
 *
 * Chord input handling for the Twiddler keyboard
 */

#ifndef NCHORDER_CHORDS_H
#define NCHORDER_CHORDS_H

#include <stdint.h>
#include <stdbool.h>
#include "nchorder_config.h"

// Chord state representation
// Uses a 16-bit bitmask where each bit represents one button
typedef uint16_t chord_t;

// Button bit positions in chord_t (matches nchorder_config.h)
// Naming: T1-T4 = Thumb buttons, F1-F4 = Finger rows, L/M/R = columns
#define CHORD_T1    (1 << BTN_T1)    // Bit 0  - Thumb N (Num)
#define CHORD_F1L   (1 << BTN_F1L)   // Bit 1  - Finger row 1 Left
#define CHORD_F1M   (1 << BTN_F1M)   // Bit 2  - Finger row 1 Middle
#define CHORD_F1R   (1 << BTN_F1R)   // Bit 3  - Finger row 1 Right
#define CHORD_T2    (1 << BTN_T2)    // Bit 4  - Thumb A (Alt)
#define CHORD_F2L   (1 << BTN_F2L)   // Bit 5  - Finger row 2 Left
#define CHORD_F2M   (1 << BTN_F2M)   // Bit 6  - Finger row 2 Middle
#define CHORD_F2R   (1 << BTN_F2R)   // Bit 7  - Finger row 2 Right
#define CHORD_T3    (1 << BTN_T3)    // Bit 8  - Thumb E (Ctrl/Enter)
#define CHORD_F3L   (1 << BTN_F3L)   // Bit 9  - Finger row 3 Left
#define CHORD_F3M   (1 << BTN_F3M)   // Bit 10 - Finger row 3 Middle
#define CHORD_F3R   (1 << BTN_F3R)   // Bit 11 - Finger row 3 Right
#define CHORD_T4    (1 << BTN_T4)    // Bit 12 - Thumb SP (Shift/Space)
#define CHORD_F4L   (1 << BTN_F4L)   // Bit 13 - Finger row 4 Left
#define CHORD_F4M   (1 << BTN_F4M)   // Bit 14 - Finger row 4 Middle
#define CHORD_F4R   (1 << BTN_F4R)   // Bit 15 - Finger row 4 Right

// Thumb button masks for modifier detection
#define CHORD_ANY_THUMB   (CHORD_T1 | CHORD_T2 | CHORD_T3 | CHORD_T4)
#define CHORD_ANY_FINGER  (~CHORD_ANY_THUMB & 0xFFFF)  // All finger buttons

// Chord mapping entry
typedef struct {
    chord_t chord;           // Button combination
    uint8_t modifiers;       // HID modifier bits (Ctrl, Shift, Alt, GUI)
    uint8_t keycode;         // HID keycode (0 for none)
    uint16_t consumer_code;  // Consumer control code (0 for none)
} chord_mapping_t;

// Mouse action for chord
typedef struct {
    chord_t chord;
    int8_t dx;               // Mouse X movement
    int8_t dy;               // Mouse Y movement
    uint8_t buttons;         // Mouse button state
    int8_t wheel;            // Scroll wheel
} chord_mouse_t;

// Chord state machine
typedef enum {
    CHORD_STATE_IDLE,        // No buttons pressed
    CHORD_STATE_BUILDING,    // Buttons being pressed
    CHORD_STATE_HELD,        // Chord held, waiting for release
    CHORD_STATE_RELEASING    // Buttons being released
} chord_state_t;

// Chord detection context
typedef struct {
    chord_state_t state;
    chord_t current_chord;   // Currently pressed buttons
    chord_t max_chord;       // Maximum chord seen (for release detection)
    uint32_t press_time;     // Timestamp of first button press
    uint32_t release_time;   // Timestamp of last release
    bool chord_fired;        // Has this chord already fired?
} chord_context_t;

// Initialize chord detection
void chord_init(chord_context_t *ctx);

// Update chord state with new button readings
// Returns true if a chord was just completed (released)
bool chord_update(chord_context_t *ctx, chord_t buttons);

// Get the chord that was just completed
chord_t chord_get_completed(chord_context_t *ctx);

// Look up keyboard mapping for a chord
const chord_mapping_t* chord_lookup_key(chord_t chord);

// Look up mouse mapping for a chord
const chord_mouse_t* chord_lookup_mouse(chord_t chord);

// Load chord mappings from configuration
void chord_load_config(const uint8_t *config_data, size_t config_size);

// Get number of loaded chord mappings
uint16_t chord_get_mapping_count(void);

// Get number of loaded mouse mappings
uint16_t chord_get_mouse_mapping_count(void);

// Get count of skipped chords (NOT IMPLEMENTED features)
// Non-zero return indicates config uses features we don't support yet
uint16_t chord_get_skipped_count(void);

// Get details of what chord types were skipped
void chord_get_skipped_details(uint16_t *system, uint16_t *multichar, uint16_t *unknown);

// Multi-character macro entry
typedef struct {
    chord_t chord;           // Button combination
    uint16_t string_index;   // Index into string table
} chord_multichar_t;

// Multi-character sequence element
typedef struct {
    uint8_t modifiers;       // HID modifier bits
    uint8_t keycode;         // HID keycode
} multichar_key_t;

/**
 * @brief Look up multi-char macro for a chord.
 *
 * @param[in]  chord       The chord to look up.
 * @param[out] p_keys      Pointer to receive key sequence array.
 * @param[out] p_count     Pointer to receive number of keys in sequence.
 *
 * @return true if chord has a multi-char macro, false otherwise.
 */
bool chord_lookup_multichar(chord_t chord, const multichar_key_t **p_keys, uint16_t *p_count);

/**
 * @brief Get count of loaded multi-char macros.
 *
 * @return Number of multi-char chord mappings.
 */
uint16_t chord_get_multichar_count(void);

/**
 * @brief Look up consumer control code for a chord.
 *
 * @param[in]  chord          The chord to look up.
 * @param[out] p_usage_code   Pointer to receive 16-bit consumer usage code.
 *
 * @return true if chord has a consumer control mapping, false otherwise.
 */
bool chord_lookup_consumer(chord_t chord, uint16_t *p_usage_code);

/**
 * @brief Get count of loaded consumer control mappings.
 *
 * @return Number of consumer chord mappings.
 */
uint16_t chord_get_consumer_count(void);

#endif // NCHORDER_CHORDS_H
