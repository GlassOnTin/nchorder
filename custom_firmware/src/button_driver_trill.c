/**
 * Northern Chorder - Trill Capacitive Button Driver
 *
 * Implements button input using Trill capacitive sensors instead of GPIO.
 * Polls 4 sensors via I2C mux and converts touch positions to button bitmask.
 *
 * Column-oriented sensor mapping:
 *   - Channel 0: Trill Square (thumb buttons T1-T4 via quadrants)
 *   - Channel 1: Trill Bar 1 (Left column:   F1L, F2L, F3L, F4L)
 *   - Channel 2: Trill Bar 2 (Middle column: F1M, F2M, F3M, F4M)
 *   - Channel 3: Trill Bar 3 (Right column:  F1R, F2R, F3R, F4R)
 *
 * Each bar has 4 zones mapping to finger rows (index, middle, ring, pinky).
 */

#include "nchorder_buttons.h"
#include "nchorder_i2c.h"
#include "nchorder_trill.h"
#include "nchorder_config.h"
#include "app_timer.h"
#include "app_scheduler.h"
#include "nrf_log.h"
#include "nrf_gpio.h"

// ============================================================================
// CONFIGURATION
// ============================================================================

// Polling interval (ms)
#define TRILL_POLL_INTERVAL_MS      15

// Debounce time (ms) - same as GPIO driver
#define TRILL_DEBOUNCE_MS           CHORD_DEBOUNCE_MS

// Minimum touch size to register as button press
#define TRILL_MIN_TOUCH_SIZE        100

// ============================================================================
// STATE
// ============================================================================

// Polling timer
APP_TIMER_DEF(m_poll_timer);

// Debounce timer
APP_TIMER_DEF(m_debounce_timer);

// Sensor state for each channel
static trill_sensor_t m_sensors[MUX_NUM_CHANNELS];

// Current debounced button state
static uint16_t m_button_state = 0;

// Raw button state (before debounce)
static uint16_t m_raw_state = 0;

// Callback for button state changes
static buttons_callback_t m_callback = NULL;

// Debounce in progress flag
static volatile bool m_debounce_pending = false;

// Initialization complete flag
static bool m_initialized = false;

// Button names for debug output
static const char* const m_button_names[NCHORDER_TOTAL_BUTTONS] = {
    "T1", "F1L", "F1M", "F1R",
    "T2", "F2L", "F2M", "F2R",
    "T3", "F3L", "F3M", "F3R",
    "T4", "F4L", "F4M", "F4R"
};

// ============================================================================
// POSITION TO BUTTON MAPPING
// ============================================================================

/**
 * Convert Trill Bar position to zone (0-3)
 * Each zone corresponds to one finger row (index, middle, ring, pinky)
 */
static uint8_t bar_position_to_zone(uint16_t position)
{
    if (position < TRILL_ZONE_0_END) return 0;  // Index finger
    if (position < TRILL_ZONE_1_END) return 1;  // Middle finger
    if (position < TRILL_ZONE_2_END) return 2;  // Ring finger
    return 3;  // Pinky finger
}

/**
 * Convert Trill Square position to quadrant (0-3)
 * Quadrants map to thumb buttons T1-T4
 *
 *   T1 (0) | T2 (1)
 *   -------+-------
 *   T3 (2) | T4 (3)
 */
static uint8_t square_position_to_quadrant(uint16_t x, uint16_t y)
{
    uint8_t quadrant = 0;

    if (x >= TRILL_SQUARE_CENTER) {
        quadrant |= 1;  // Right half
    }
    if (y >= TRILL_SQUARE_CENTER) {
        quadrant |= 2;  // Bottom half
    }

    return quadrant;
}

/**
 * Build button bitmask from all sensor readings
 *
 * Column-oriented mapping (3 bars = 3 columns, 4 zones = 4 finger rows):
 *   Bar 1 (ch1) zones 0-3 → F1L, F2L, F3L, F4L (Left column)
 *   Bar 2 (ch2) zones 0-3 → F1M, F2M, F3M, F4M (Middle column)
 *   Bar 3 (ch3) zones 0-3 → F1R, F2R, F3R, F4R (Right column)
 */
static uint16_t build_button_mask(void)
{
    uint16_t mask = 0;

    // === Thumb buttons from Trill Square (channel 0) ===
    trill_sensor_t *square = &m_sensors[MUX_CH_THUMB];
    if (square->initialized && square->num_touches > 0) {
        // Process all touches on Square
        for (int t = 0; t < square->num_touches; t++) {
            uint16_t x = square->touches_2d[t].x;
            uint16_t y = square->touches_2d[t].y;
            uint16_t size = square->touches_2d[t].size;

            if (size >= TRILL_MIN_TOUCH_SIZE) {
                uint8_t quadrant = square_position_to_quadrant(x, y);

                // Map quadrant to thumb button bit
                // Q0 (top-left) → T1 (bit 0)
                // Q1 (top-right) → T2 (bit 4)
                // Q2 (bottom-left) → T3 (bit 8)
                // Q3 (bottom-right) → T4 (bit 12)
                switch (quadrant) {
                    case 0: mask |= (1 << BTN_T1); break;
                    case 1: mask |= (1 << BTN_T2); break;
                    case 2: mask |= (1 << BTN_T3); break;
                    case 3: mask |= (1 << BTN_T4); break;
                }
            }
        }
    }

    // === Left column from Trill Bar 1 (channel 1) ===
    // Zone 0-3 → F1L, F2L, F3L, F4L (bits 1, 5, 9, 13)
    trill_sensor_t *bar_l = &m_sensors[MUX_CH_COL_L];
    if (bar_l->initialized && bar_l->num_touches > 0) {
        for (int t = 0; t < bar_l->num_touches; t++) {
            uint16_t pos = bar_l->touches[t].position;
            uint16_t size = bar_l->touches[t].size;

            if (size >= TRILL_MIN_TOUCH_SIZE) {
                uint8_t zone = bar_position_to_zone(pos);

                switch (zone) {
                    case 0: mask |= (1 << BTN_F1L); break;  // Index
                    case 1: mask |= (1 << BTN_F2L); break;  // Middle
                    case 2: mask |= (1 << BTN_F3L); break;  // Ring
                    case 3: mask |= (1 << BTN_F4L); break;  // Pinky
                }
            }
        }
    }

    // === Middle column from Trill Bar 2 (channel 2) ===
    // Zone 0-3 → F1M, F2M, F3M, F4M (bits 2, 6, 10, 14)
    trill_sensor_t *bar_m = &m_sensors[MUX_CH_COL_M];
    if (bar_m->initialized && bar_m->num_touches > 0) {
        for (int t = 0; t < bar_m->num_touches; t++) {
            uint16_t pos = bar_m->touches[t].position;
            uint16_t size = bar_m->touches[t].size;

            if (size >= TRILL_MIN_TOUCH_SIZE) {
                uint8_t zone = bar_position_to_zone(pos);

                switch (zone) {
                    case 0: mask |= (1 << BTN_F1M); break;  // Index
                    case 1: mask |= (1 << BTN_F2M); break;  // Middle
                    case 2: mask |= (1 << BTN_F3M); break;  // Ring
                    case 3: mask |= (1 << BTN_F4M); break;  // Pinky
                }
            }
        }
    }

    // === Right column from Trill Bar 3 (channel 3) ===
    // Zone 0-3 → F1R, F2R, F3R, F4R (bits 3, 7, 11, 15)
    trill_sensor_t *bar_r = &m_sensors[MUX_CH_COL_R];
    if (bar_r->initialized && bar_r->num_touches > 0) {
        for (int t = 0; t < bar_r->num_touches; t++) {
            uint16_t pos = bar_r->touches[t].position;
            uint16_t size = bar_r->touches[t].size;

            if (size >= TRILL_MIN_TOUCH_SIZE) {
                uint8_t zone = bar_position_to_zone(pos);

                switch (zone) {
                    case 0: mask |= (1 << BTN_F1R); break;  // Index
                    case 1: mask |= (1 << BTN_F2R); break;  // Middle
                    case 2: mask |= (1 << BTN_F3R); break;  // Ring
                    case 3: mask |= (1 << BTN_F4R); break;  // Pinky
                }
            }
        }
    }

    return mask;
}

// ============================================================================
// TIMER HANDLERS
// ============================================================================

/**
 * Debounce timer handler
 */
static void debounce_timer_handler(void *p_context)
{
    (void)p_context;

    // Check if state is stable
    if (m_raw_state != m_button_state) {
        // State changed, update and notify
        uint16_t old_state = m_button_state;
        m_button_state = m_raw_state;

        NRF_LOG_DEBUG("Trill buttons: 0x%04X -> 0x%04X (%s)",
                      old_state, m_button_state,
                      buttons_to_string(m_button_state));

        if (m_callback != NULL) {
            m_callback(m_button_state);
        }
    }

    m_debounce_pending = false;
}

// External function to send test keypress (defined in main.c)
extern void debug_send_test_key(void);

/**
 * Poll timer handler - scheduled to run in main context
 */
static void poll_scheduled_handler(void *p_event_data, uint16_t event_size)
{
    (void)p_event_data;
    (void)event_size;

    ret_code_t err;
    static uint32_t poll_count = 0;
    static bool touch_was_active = false;

    poll_count++;

    // Log every 100 polls (~1.5 seconds) to confirm polling is running
    if (poll_count % 100 == 0) {
        NRF_LOG_INFO("Trill poll #%u running", poll_count);
    }

    // Read all sensors
    bool any_touch = false;
    for (int ch = 0; ch < MUX_NUM_CHANNELS; ch++) {
        if (!m_sensors[ch].initialized) {
            continue;
        }

        // Select mux channel
        err = nchorder_i2c_mux_select(ch);
        if (err != NRF_SUCCESS) {
            NRF_LOG_WARNING("Trill mux ch%d select failed: 0x%08X", ch, err);
            continue;
        }

        // Read sensor data
        err = trill_read(&m_sensors[ch]);
        if (err != NRF_SUCCESS) {
            NRF_LOG_WARNING("Trill ch%d read failed: 0x%08X", ch, err);
        } else if (m_sensors[ch].num_touches > 0) {
            // Log touches at INFO level
            NRF_LOG_INFO("Trill ch%d: %d touches", ch, m_sensors[ch].num_touches);
            any_touch = true;
        }
    }

    // DEBUG: Send test key on new touch (edge-triggered)
    if (any_touch && !touch_was_active) {
        NRF_LOG_INFO("DEBUG: Touch detected! Sending test key 'A'");
        debug_send_test_key();
    }
    touch_was_active = any_touch;

    // Build button mask from sensor readings
    uint16_t new_raw_state = build_button_mask();

    // Always log if any buttons detected
    if (new_raw_state != 0) {
        NRF_LOG_INFO("Trill raw buttons: 0x%04X (%s)", new_raw_state, buttons_to_string(new_raw_state));
    }

    // Check for state change
    if (new_raw_state != m_raw_state) {
        m_raw_state = new_raw_state;

        // Start/restart debounce timer
        if (!m_debounce_pending) {
            m_debounce_pending = true;
            app_timer_start(m_debounce_timer, APP_TIMER_TICKS(TRILL_DEBOUNCE_MS), NULL);
        } else {
            app_timer_stop(m_debounce_timer);
            app_timer_start(m_debounce_timer, APP_TIMER_TICKS(TRILL_DEBOUNCE_MS), NULL);
        }
    }
}

/**
 * Poll timer callback - schedules work to main context
 */
static void poll_timer_handler(void *p_context)
{
    (void)p_context;

    // DEBUG: Disabled - conflicts with debug LED on P0.06
    // Toggle LED in interrupt context to verify timer is firing
    // static uint8_t timer_count = 0;
    // timer_count++;
    // if (timer_count % 50 == 0) {
    //     nrf_gpio_pin_toggle(PIN_LED_STATUS);
    // }

    // Schedule polling work to run in main context (not interrupt)
    app_sched_event_put(NULL, 0, poll_scheduled_handler);
}

// ============================================================================
// PUBLIC API
// ============================================================================

uint32_t buttons_init(void)
{
    ret_code_t err_code;

    NRF_LOG_INFO("Trill buttons: Initializing");

    // Initialize I2C bus
    err_code = nchorder_i2c_init();
    if (err_code != NRF_SUCCESS) {
        NRF_LOG_ERROR("Trill buttons: I2C init failed: 0x%08X", err_code);
        return err_code;
    }

    // Reset mux
    nchorder_i2c_mux_reset();

    // Scan I2C bus (debug)
    nchorder_i2c_scan();

    // Initialize each Trill sensor
    for (int ch = 0; ch < MUX_NUM_CHANNELS; ch++) {
        NRF_LOG_INFO("Trill buttons: Initializing sensor on channel %d", ch);

        err_code = nchorder_i2c_mux_select(ch);
        if (err_code != NRF_SUCCESS) {
            NRF_LOG_ERROR("Trill buttons: Mux select ch%d failed: 0x%08X", ch, err_code);
            continue;
        }

        // Debug: scan this mux channel to see what devices are present
        NRF_LOG_INFO("Trill buttons: Scanning mux channel %d...", ch);
        nchorder_i2c_scan();

        // Channel 0 (Square) is at 0x28, channels 1-3 (Bars) are at 0x20
        uint8_t addr = (ch == 0) ? 0x28 : I2C_ADDR_TRILL;
        err_code = trill_init(&m_sensors[ch], addr);
        if (err_code != NRF_SUCCESS) {
            // Try alternate address
            addr = (ch == 0) ? I2C_ADDR_TRILL : 0x28;
            NRF_LOG_WARNING("Trill buttons: Trying alternate addr 0x%02X...", addr);
            err_code = trill_init(&m_sensors[ch], addr);
            if (err_code != NRF_SUCCESS) {
                NRF_LOG_WARNING("Trill buttons: Sensor ch%d init failed at both addresses", ch);
            }
        }
    }

    // Count initialized sensors
    int num_sensors = 0;
    for (int ch = 0; ch < MUX_NUM_CHANNELS; ch++) {
        if (m_sensors[ch].initialized) {
            num_sensors++;
        }
    }

    if (num_sensors == 0) {
        NRF_LOG_ERROR("Trill buttons: No sensors initialized!");
        return NRF_ERROR_NOT_FOUND;
    }

    NRF_LOG_INFO("Trill buttons: %d/%d sensors initialized", num_sensors, MUX_NUM_CHANNELS);

    // Create debounce timer
    err_code = app_timer_create(&m_debounce_timer,
                                APP_TIMER_MODE_SINGLE_SHOT,
                                debounce_timer_handler);
    if (err_code != NRF_SUCCESS) {
        NRF_LOG_ERROR("Trill buttons: Debounce timer create failed: 0x%08X", err_code);
        return err_code;
    }

    // Create polling timer
    err_code = app_timer_create(&m_poll_timer,
                                APP_TIMER_MODE_REPEATED,
                                poll_timer_handler);
    if (err_code != NRF_SUCCESS) {
        NRF_LOG_ERROR("Trill buttons: Poll timer create failed: 0x%08X", err_code);
        return err_code;
    }

    // Start polling timer
    err_code = app_timer_start(m_poll_timer, APP_TIMER_TICKS(TRILL_POLL_INTERVAL_MS), NULL);
    if (err_code != NRF_SUCCESS) {
        NRF_LOG_ERROR("Trill buttons: Poll timer start failed: 0x%08X", err_code);
        return err_code;
    }

    m_initialized = true;
    NRF_LOG_INFO("Trill buttons: Init complete, polling every %d ms", TRILL_POLL_INTERVAL_MS);

    return NRF_SUCCESS;
}

uint16_t buttons_scan(void)
{
    return m_button_state;
}

void buttons_set_callback(buttons_callback_t callback)
{
    m_callback = callback;
}

bool buttons_any_pressed(void)
{
    return m_button_state != 0;
}

const char* buttons_to_string(uint16_t bitmask)
{
    static char buffer[64];
    char *ptr = buffer;
    bool first = true;

    if (bitmask == 0) {
        return "(none)";
    }

    for (int i = 0; i < NCHORDER_TOTAL_BUTTONS; i++) {
        if (bitmask & (1 << i)) {
            if (!first) {
                *ptr++ = '+';
            }
            const char *name = m_button_names[i];
            while (*name) {
                *ptr++ = *name++;
            }
            first = false;
        }
    }

    *ptr = '\0';
    return buffer;
}
