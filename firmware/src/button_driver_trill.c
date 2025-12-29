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
#include "SEGGER_RTT.h"

// Enable RTT debug output for Trill sensors (for trill_visualizer.py)
#define TRILL_DEBUG_RTT 1

// simple_delay_ms is defined in nchorder_trill.c (included before this file)

// ============================================================================
// CONFIGURATION
// ============================================================================

// Polling interval (ms)
#define TRILL_POLL_INTERVAL_MS      15

// Debounce time (ms) - longer than GPIO for capacitive sensors
#define TRILL_DEBOUNCE_MS           30

// Minimum touch size to register as button press (increased to reduce noise/voltage transients)
#define TRILL_MIN_TOUCH_SIZE        500

// Release threshold (lower than press threshold for hysteresis)
#define TRILL_RELEASE_SIZE          250

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

    // Use hardcoded threshold for now (CONFIG.INI support to be added later)
    uint16_t min_touch_size = TRILL_MIN_TOUCH_SIZE;

    // === Thumb buttons from Trill Square (channel 0) ===
    trill_sensor_t *square = &m_sensors[MUX_CH_THUMB];
    if (square->initialized && square->num_touches > 0) {
        // Process all touches on Square
        for (int t = 0; t < square->num_touches; t++) {
            uint16_t x = square->touches_2d[t].x;
            uint16_t y = square->touches_2d[t].y;
            uint16_t size = square->touches_2d[t].size;

            if (size >= min_touch_size) {
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

            if (size >= min_touch_size) {
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

            if (size >= min_touch_size) {
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

            if (size >= min_touch_size) {
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
            any_touch = true;
        }
    }

#if TRILL_DEBUG_RTT
    // Output sensor data for visualization: TRILL:ch,type,init,touches,data...
    static uint32_t rtt_counter = 0;
    if ((rtt_counter++ % 10) == 0) {  // Every 10th poll (~150ms)
        for (int ch = 0; ch < MUX_NUM_CHANNELS; ch++) {
            trill_sensor_t *s = &m_sensors[ch];
            if (s->is_2d) {
                // 2D sensor (Square): TRILL:ch,2D,init,n,x0,y0,s0,x1,y1,s1,...
                SEGGER_RTT_printf(0, "TRILL:%d,2D,%d,%d", ch, s->initialized, s->num_touches);
                for (int t = 0; t < s->num_touches && t < 5; t++) {
                    SEGGER_RTT_printf(0, ",%d,%d,%d",
                        s->touches_2d[t].x, s->touches_2d[t].y, s->touches_2d[t].size);
                }
                SEGGER_RTT_printf(0, "\n");
            } else {
                // 1D sensor (Bar): TRILL:ch,1D,init,n,p0,s0,p1,s1,...
                SEGGER_RTT_printf(0, "TRILL:%d,1D,%d,%d", ch, s->initialized, s->num_touches);
                for (int t = 0; t < s->num_touches && t < 5; t++) {
                    SEGGER_RTT_printf(0, ",%d,%d",
                        s->touches[t].position, s->touches[t].size);
                }
                SEGGER_RTT_printf(0, "\n");
            }
        }
    }
#endif

    (void)any_touch;  // Suppress unused warning

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

    // Schedule polling work to run in main context (not interrupt)
    app_sched_event_put(NULL, 0, poll_scheduled_handler);
}

// ============================================================================
// PUBLIC API
// ============================================================================

uint32_t buttons_init(void)
{
    ret_code_t err_code;
    uint8_t dummy;

    NRF_LOG_INFO("Trill buttons: Initializing");

    // Initialize I2C bus
    err_code = nchorder_i2c_init();
    if (err_code != NRF_SUCCESS) {
        NRF_LOG_ERROR("Trill buttons: I2C init failed: 0x%08X", err_code);
        return err_code;
    }

    // Reset mux
    nchorder_i2c_mux_reset();

    // Probe MUX directly at address 0x70 (before any channel selection)
    NRF_LOG_INFO("Trill buttons: Probing MUX at 0x%02X...", I2C_ADDR_MUX);
    err_code = nchorder_i2c_read(I2C_ADDR_MUX, &dummy, 1);
    if (err_code != NRF_SUCCESS) {
        NRF_LOG_ERROR("Trill buttons: MUX not responding! Error 0x%08X", err_code);
        // Continue anyway to see what happens
    } else {
        NRF_LOG_INFO("Trill buttons: MUX responded (read 0x%02X)", dummy);
    }

    // Initialize each Trill sensor
    for (int ch = 0; ch < MUX_NUM_CHANNELS; ch++) {
        NRF_LOG_INFO("Trill buttons: Initializing sensor on channel %d", ch);

        err_code = nchorder_i2c_mux_select(ch);
        if (err_code != NRF_SUCCESS) {
            NRF_LOG_ERROR("Trill buttons: Mux select ch%d failed: 0x%08X", ch, err_code);
            continue;
        }

        // Trill Square (ch0) uses default address 0x28, Bars (ch1-3) use 0x20
        uint8_t addr = (ch == MUX_CH_THUMB) ? TRILL_ADDR_SQUARE : I2C_ADDR_TRILL;

        // Initialize sensor inline to avoid function call overhead
        trill_sensor_t *sensor = &m_sensors[ch];
        static uint8_t identify_buf[4] = {0};

        // Clear sensor state first
        memset(sensor, 0, sizeof(trill_sensor_t));
        sensor->i2c_addr = addr;

        // First set read pointer to offset 0 (identify area)
        uint8_t zero_offset = 0;
        err_code = nchorder_i2c_write(addr, &zero_offset, 1);
        if (err_code != NRF_SUCCESS) {
            NRF_LOG_WARNING("Ch%d: Set read ptr failed: 0x%04X", ch, err_code);
            continue;
        }

        simple_delay_ms(2);  // Small delay after write

        // Now read 4 bytes from offset 0
        err_code = nchorder_i2c_read(addr, identify_buf, 4);
        if (err_code != NRF_SUCCESS) {
            NRF_LOG_WARNING("Ch%d: Identify read failed: 0x%04X", ch, err_code);
            continue;
        }

        NRF_LOG_INFO("Ch%d: Raw %02X %02X %02X %02X", ch,
                     identify_buf[0], identify_buf[1], identify_buf[2], identify_buf[3]);

        // Check for FE header - if not FE, try assuming Bar sensor anyway
        if (identify_buf[0] == 0xFE) {
            sensor->device_type = identify_buf[1];
            sensor->firmware_version = identify_buf[2];
        } else {
            // Unknown header - assume it's a capacitive touch sensor
            // For ch0 (Trill Square), assume 2D
            // For ch1-3 (Trill Bar address 0x20), assume 1D
            NRF_LOG_WARNING("Ch%d: Unknown header 0x%02X, assuming %s",
                            ch, identify_buf[0], (ch == 0) ? "Square" : "Bar");
            sensor->device_type = (ch == 0) ? TRILL_TYPE_SQUARE : TRILL_TYPE_BAR;
            sensor->firmware_version = 0;
        }
        sensor->is_2d = (sensor->device_type == TRILL_TYPE_SQUARE ||
                         sensor->device_type == TRILL_TYPE_HEX);

        // Force channel 0 (Square) to be 2D regardless of identification
        // The Square may return wrong device_type in identification response
        if (ch == MUX_CH_THUMB) {
            sensor->device_type = TRILL_TYPE_SQUARE;
            sensor->is_2d = true;
        }

        NRF_LOG_INFO("Ch%d: Trill %s (fw=%d)", ch,
                     trill_type_name(sensor->device_type), sensor->firmware_version);

        // Step 3: Set mode to CENTROID
        uint8_t mode_cmd[3] = {TRILL_OFFSET_COMMAND, TRILL_CMD_MODE, TRILL_MODE_CENTROID};
        err_code = nchorder_i2c_write(addr, mode_cmd, 3);
        if (err_code != NRF_SUCCESS) {
            NRF_LOG_WARNING("Ch%d: Set mode failed: 0x%04X", ch, err_code);
        }
        simple_delay_ms(5);

        // Step 4: Configure scan settings (speed=0 ultra fast, resolution=12 bits)
        uint8_t scan_cmd[4] = {TRILL_OFFSET_COMMAND, TRILL_CMD_SCAN_SETTINGS, 0, 12};
        err_code = nchorder_i2c_write(addr, scan_cmd, 4);
        if (err_code != NRF_SUCCESS) {
            NRF_LOG_WARNING("Ch%d: Scan settings failed: 0x%04X", ch, err_code);
        }
        simple_delay_ms(5);

        // Step 5: Enable auto-scan
        uint8_t autoscan_cmd[3] = {TRILL_OFFSET_COMMAND, TRILL_CMD_AUTO_SCAN, 1};
        err_code = nchorder_i2c_write(addr, autoscan_cmd, 3);
        if (err_code != NRF_SUCCESS) {
            NRF_LOG_WARNING("Ch%d: Auto-scan failed: 0x%04X", ch, err_code);
        }
        simple_delay_ms(5);

        // Step 6: Update baseline
        uint8_t baseline_cmd[2] = {TRILL_OFFSET_COMMAND, TRILL_CMD_BASELINE_UPDATE};
        err_code = nchorder_i2c_write(addr, baseline_cmd, 2);
        if (err_code != NRF_SUCCESS) {
            NRF_LOG_WARNING("Ch%d: Baseline update failed: 0x%04X", ch, err_code);
        }
        simple_delay_ms(10);

        sensor->initialized = true;
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
