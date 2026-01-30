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
#include "nchorder_mouse.h"
#include "nchorder_cdc.h"
#include "app_timer.h"
#include "app_scheduler.h"
#include "nrf_log.h"
#include "nrf_gpio.h"
#include "nrf_drv_wdt.h"
#include "SEGGER_RTT.h"

// Simple abs for int16_t (avoid stdlib dependency)
static inline int16_t abs16(int16_t x) { return x < 0 ? -x : x; }

// Enable RTT debug output for Trill sensors (for trill_visualizer.py)
#define TRILL_DEBUG_RTT 1

// Enable noise statistics collection
#define TRILL_NOISE_STATS 0

// simple_delay_ms is defined in nchorder_trill.c (included before this file)

// ============================================================================
// CONFIGURATION
// ============================================================================

// Polling interval (ms)
#define TRILL_POLL_INTERVAL_MS      15

// Debounce time (ms) - longer than GPIO for capacitive sensors
#define TRILL_DEBOUNCE_MS           30

// Minimum touch size to register as button press
// Square (as Flex): noise floor observed up to 600, require higher to filter
// Bars: noise floor is ~100-260, need margin above peaks
#define TRILL_MIN_TOUCH_SIZE_SQUARE 800
#define TRILL_MIN_TOUCH_SIZE_BAR    350

// Release threshold (lower than press threshold for hysteresis)
#define TRILL_RELEASE_SIZE          250

// Gesture detection thresholds (tune via experimentation)
#define GESTURE_SLIDE_THRESHOLD     300   // Movement units to trigger mouse mode (lower = more sensitive slide)
#define GESTURE_TAP_MIN_FRAMES      5     // Minimum frames for valid tap (~75ms, filter noise spikes)
#define GESTURE_TAP_MAX_FRAMES      20    // Max frames for valid tap (~300ms at 15ms/frame)
#define GESTURE_MIN_MOVE_FRAMES     3     // Minimum frames before mouse mode can activate
#define GESTURE_MOUSE_SCALE         6     // Divisor: 3200 range → reasonable mouse delta (lower = faster)

// ============================================================================
// STATE
// ============================================================================

// Gesture tracking for square sensor (slide vs tap detection)
typedef struct {
    bool     active;           // Touch currently active
    uint16_t start_x;          // Position when touch began
    uint16_t start_y;
    uint16_t prev_x;           // Position in previous frame
    uint16_t prev_y;
    uint16_t frame_count;      // Frames since touch start (15ms/frame)
    uint16_t cumulative_dist;  // Total movement since touch start
    bool     is_mouse_mode;    // True once slide threshold exceeded
} gesture_state_t;

static gesture_state_t m_gesture = {0};

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

// Scan results for diagnostics (which addresses found on each channel)
static uint8_t m_scan_results[MUX_NUM_CHANNELS];  // Stores first found address per channel, 0 if none

// Settling period - ignore touches for first N polls after init (suppress boot noise)
#define SETTLING_POLL_COUNT     40   // ~600ms at 15ms/poll
static uint16_t m_settling_polls = 0;

#if TRILL_NOISE_STATS
// Noise statistics for each sensor
typedef struct {
    uint32_t sample_count;
    uint16_t size_min;
    uint16_t size_max;
    uint32_t size_sum;
    uint16_t pos_min;
    uint16_t pos_max;
    uint16_t spurious_count;  // Touches below threshold
} noise_stats_t;

static noise_stats_t m_noise_stats[MUX_NUM_CHANNELS] = {0};
static uint32_t m_stats_interval = 0;
#define NOISE_STATS_INTERVAL 200  // Output stats every N polls (~3 seconds)
#endif

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
 * Convert position to zone (0-3) - direct mapping (no inversion)
 * Used for thumb sensor in 1D mode
 */
static uint8_t position_to_zone_direct(uint16_t position)
{
    if (position < TRILL_ZONE_0_END) return 0;  // 0-800 → T1
    if (position < TRILL_ZONE_1_END) return 1;  // 800-1600 → T2
    if (position < TRILL_ZONE_2_END) return 2;  // 1600-2400 → T3
    return 3;  // 2400-3200 → T4
}

/**
 * Convert Trill Bar position to zone (0-3)
 * Each zone corresponds to one finger row (index, middle, ring, pinky)
 * Bar is mounted with high position values at the top (index finger),
 * so we invert the position before mapping.
 */
static uint8_t bar_position_to_zone(uint16_t position)
{
    // Invert: physical top (index) = high position, but we want zone 0
    uint16_t inverted = (position >= 3200) ? 0 : (3200 - position);

    if (inverted < TRILL_ZONE_0_END) return 0;  // Index finger (top)
    if (inverted < TRILL_ZONE_1_END) return 1;  // Middle finger
    if (inverted < TRILL_ZONE_2_END) return 2;  // Ring finger
    return 3;  // Pinky finger (bottom)
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
 * Process thumb sensor for gesture detection (slide vs tap)
 *
 * Works for both 2D (Square) and 1D (Flex) modes:
 * - 2D: Sliding → mouse X/Y movement
 * - 1D: Sliding → mouse X movement (Y fixed at 0)
 *
 * Quick tap → button press (handled by build_button_mask checking m_gesture.is_mouse_mode)
 */
static void process_square_gesture(trill_sensor_t *sensor)
{
    // Check if there's a valid touch (works for both 1D and 2D)
    bool has_touch = false;
    uint16_t x = 0, y = 0;

    if (sensor->is_2d) {
        // 2D mode: use touches_2d
        has_touch = (sensor->num_touches > 0 &&
                     sensor->touches_2d[0].size >= TRILL_MIN_TOUCH_SIZE_SQUARE &&
                     sensor->touches_2d[0].x != 0xFFFF &&
                     sensor->touches_2d[0].y != 0xFFFF);
        if (has_touch) {
            x = sensor->touches_2d[0].x;
            y = sensor->touches_2d[0].y;
        }
    } else {
        // 1D mode: use touches array, position maps to X, no Y
        has_touch = (sensor->num_touches > 0 &&
                     sensor->touches[0].size >= TRILL_MIN_TOUCH_SIZE_SQUARE &&
                     sensor->touches[0].position != 0xFFFF);
        if (has_touch) {
            x = sensor->touches[0].position;
            y = 1600;  // Fixed Y at center for 1D
        }
    }

    if (has_touch) {

        if (!m_gesture.active) {
            // New touch starting
            m_gesture.active = true;
            m_gesture.start_x = x;
            m_gesture.start_y = y;
            m_gesture.prev_x = x;
            m_gesture.prev_y = y;
            m_gesture.frame_count = 0;
            m_gesture.cumulative_dist = 0;
            m_gesture.is_mouse_mode = false;
            NRF_LOG_DEBUG("Gesture: Touch start at (%d,%d)", x, y);
        } else {
            // Continuing touch - calculate delta
            int16_t dx = (int16_t)x - (int16_t)m_gesture.prev_x;
            int16_t dy = (int16_t)y - (int16_t)m_gesture.prev_y;

            // Increment frame counter
            m_gesture.frame_count++;

            // Accumulate total movement
            m_gesture.cumulative_dist += abs16(dx) + abs16(dy);

            // Check if we should enter mouse mode (need minimum frames AND distance)
            if (!m_gesture.is_mouse_mode &&
                m_gesture.frame_count >= GESTURE_MIN_MOVE_FRAMES &&
                m_gesture.cumulative_dist >= GESTURE_SLIDE_THRESHOLD) {
                m_gesture.is_mouse_mode = true;
                NRF_LOG_INFO("Gesture: Mouse mode (dist=%d, frames=%d)",
                             m_gesture.cumulative_dist, m_gesture.frame_count);
            }

            // If in mouse mode, send mouse delta
            if (m_gesture.is_mouse_mode) {
                int8_t mouse_dx = dx / GESTURE_MOUSE_SCALE;
                int8_t mouse_dy = dy / GESTURE_MOUSE_SCALE;
                if (mouse_dx != 0 || mouse_dy != 0) {
                    nchorder_mouse_move(mouse_dx, mouse_dy);
                }
            }

            m_gesture.prev_x = x;
            m_gesture.prev_y = y;
        }
    } else if (m_gesture.active) {
        // Touch released - check if it was a valid tap
        if (!m_gesture.is_mouse_mode &&
            m_gesture.frame_count >= GESTURE_TAP_MIN_FRAMES &&
            m_gesture.frame_count < GESTURE_TAP_MAX_FRAMES) {
            // It was a valid tap! Log it (button handled by build_button_mask)
            if (sensor->is_2d) {
                uint8_t quadrant = square_position_to_quadrant(m_gesture.start_x, m_gesture.start_y);
                NRF_LOG_INFO("Gesture: Tap Q%d at (%d,%d) frames=%d",
                             quadrant, m_gesture.start_x, m_gesture.start_y, m_gesture.frame_count);
            } else {
                uint8_t zone = position_to_zone_direct(m_gesture.start_x);
                NRF_LOG_INFO("Gesture: Tap Z%d at pos=%d frames=%d",
                             zone, m_gesture.start_x, m_gesture.frame_count);
            }
        } else if (m_gesture.frame_count < GESTURE_TAP_MIN_FRAMES) {
            // Too short - filter out as noise
            NRF_LOG_DEBUG("Gesture: Ignored noise (frames=%d < %d)",
                          m_gesture.frame_count, GESTURE_TAP_MIN_FRAMES);
        } else if (m_gesture.is_mouse_mode) {
            NRF_LOG_DEBUG("Gesture: Mouse ended (dist=%d)", m_gesture.cumulative_dist);
        }

        m_gesture.active = false;
        m_gesture.is_mouse_mode = false;
    }
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

    // === Thumb buttons from Trill sensor on channel 0 ===
    // Note: Sensor may be 2D (Square) or 1D (Flex) depending on firmware
    trill_sensor_t *thumb = &m_sensors[MUX_CH_THUMB];
    if (thumb->initialized && thumb->num_touches > 0) {
        if (thumb->is_2d) {
            // 2D mode: map quadrants to T1-T4
            bool valid_2d_touch = (!m_gesture.is_mouse_mode &&
                                   m_gesture.active &&
                                   m_gesture.frame_count >= GESTURE_TAP_MIN_FRAMES);
            if (valid_2d_touch) {
                for (int t = 0; t < thumb->num_touches; t++) {
                    uint16_t x = thumb->touches_2d[t].x;
                    uint16_t y = thumb->touches_2d[t].y;
                    uint16_t size = thumb->touches_2d[t].size;

                    if (size >= TRILL_MIN_TOUCH_SIZE_SQUARE) {
                        uint8_t quadrant = square_position_to_quadrant(x, y);
                        switch (quadrant) {
                            case 0: mask |= (1 << BTN_T1); break;
                            case 1: mask |= (1 << BTN_T2); break;
                            case 2: mask |= (1 << BTN_T3); break;
                            case 3: mask |= (1 << BTN_T4); break;
                        }
                    }
                }
            }
        } else {
            // 1D mode (Flex): map position zones to T1-T4
            // Position 0-800 → T1, 800-1600 → T2, 1600-2400 → T3, 2400-3200 → T4
            // Only register button if NOT in mouse mode and valid tap gesture
            bool valid_1d_touch = (!m_gesture.is_mouse_mode &&
                                   m_gesture.active &&
                                   m_gesture.frame_count >= GESTURE_TAP_MIN_FRAMES);
            if (valid_1d_touch) {
                for (int t = 0; t < thumb->num_touches; t++) {
                    uint16_t pos = thumb->touches[t].position;
                    uint16_t size = thumb->touches[t].size;

                    if (size >= TRILL_MIN_TOUCH_SIZE_SQUARE) {
                        uint8_t zone = position_to_zone_direct(pos);  // Direct mapping for thumb
                        switch (zone) {
                            case 0: mask |= (1 << BTN_T1); break;
                            case 1: mask |= (1 << BTN_T2); break;
                            case 2: mask |= (1 << BTN_T3); break;
                            case 3: mask |= (1 << BTN_T4); break;
                        }
                    }
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

            if (size >= TRILL_MIN_TOUCH_SIZE_BAR) {
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

            if (size >= TRILL_MIN_TOUCH_SIZE_BAR) {
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

            if (size >= TRILL_MIN_TOUCH_SIZE_BAR) {
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

#if TRILL_NOISE_STATS
        // Collect noise statistics for each touch
        trill_sensor_t *s = &m_sensors[ch];
        if (s->num_touches > 0) {
            noise_stats_t *ns = &m_noise_stats[ch];
            uint16_t size, pos;

            if (s->is_2d) {
                size = s->touches_2d[0].size;
                pos = s->touches_2d[0].y;  // Use Y position for Square
            } else {
                size = s->touches[0].size;
                pos = s->touches[0].position;
            }

            // Initialize min on first sample
            if (ns->sample_count == 0) {
                ns->size_min = size;
                ns->size_max = size;
                ns->pos_min = pos;
                ns->pos_max = pos;
            } else {
                if (size < ns->size_min) ns->size_min = size;
                if (size > ns->size_max) ns->size_max = size;
                if (pos < ns->pos_min) ns->pos_min = pos;
                if (pos > ns->pos_max) ns->pos_max = pos;
            }
            ns->size_sum += size;
            ns->sample_count++;

            // Count spurious touches (below threshold)
            uint16_t thresh = (ch == MUX_CH_THUMB) ? TRILL_MIN_TOUCH_SIZE_SQUARE : TRILL_MIN_TOUCH_SIZE_BAR;
            if (size < thresh) {
                ns->spurious_count++;
            }
        }
#endif
    }

#if TRILL_NOISE_STATS
    // Output noise statistics periodically
    if (++m_stats_interval >= NOISE_STATS_INTERVAL) {
        m_stats_interval = 0;
        SEGGER_RTT_printf(0, "NOISE_STATS:\n");
        for (int ch = 0; ch < MUX_NUM_CHANNELS; ch++) {
            noise_stats_t *ns = &m_noise_stats[ch];
            if (ns->sample_count > 0) {
                uint32_t size_avg = ns->size_sum / ns->sample_count;
                SEGGER_RTT_printf(0, "  Ch%d: n=%lu size=[%u,%u,avg%lu] pos=[%u,%u] spurious=%u\n",
                    ch, ns->sample_count, ns->size_min, ns->size_max, size_avg,
                    ns->pos_min, ns->pos_max, ns->spurious_count);
            }
        }
        // Reset stats for next interval
        memset(m_noise_stats, 0, sizeof(m_noise_stats));
    }
#endif

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

    // Send CDC touch stream if streaming is enabled
    if (nchorder_cdc_is_streaming()) {
        cdc_touch_frame_t frame = {0};
        frame.sync = CDC_STREAM_SYNC;

        // Thumb (Square sensor - 2D)
        trill_sensor_t *thumb = &m_sensors[MUX_CH_THUMB];
        if (thumb->initialized && thumb->num_touches > 0) {
            if (thumb->is_2d) {
                frame.thumb_x = thumb->touches_2d[0].x;
                frame.thumb_y = thumb->touches_2d[0].y;
                frame.thumb_size = thumb->touches_2d[0].size;
            } else {
                // Fallback if running as 1D (Flex mode)
                frame.thumb_x = thumb->touches[0].position;
                frame.thumb_y = 0;
                frame.thumb_size = thumb->touches[0].size;
            }
        }

        // Bar sensors (1D) - fill all touches
        trill_sensor_t *bar_l = &m_sensors[MUX_CH_COL_L];
        for (int i = 0; i < CDC_MAX_BAR_TOUCHES; i++) {
            if (bar_l->initialized && i < bar_l->num_touches) {
                frame.bar0[i].pos = bar_l->touches[i].position;
                frame.bar0[i].size = bar_l->touches[i].size;
            } else {
                frame.bar0[i].pos = 0xFFFF;  // No touch marker
                frame.bar0[i].size = 0;
            }
        }

        trill_sensor_t *bar_m = &m_sensors[MUX_CH_COL_M];
        for (int i = 0; i < CDC_MAX_BAR_TOUCHES; i++) {
            if (bar_m->initialized && i < bar_m->num_touches) {
                frame.bar1[i].pos = bar_m->touches[i].position;
                frame.bar1[i].size = bar_m->touches[i].size;
            } else {
                frame.bar1[i].pos = 0xFFFF;
                frame.bar1[i].size = 0;
            }
        }

        trill_sensor_t *bar_r = &m_sensors[MUX_CH_COL_R];
        for (int i = 0; i < CDC_MAX_BAR_TOUCHES; i++) {
            if (bar_r->initialized && i < bar_r->num_touches) {
                frame.bar2[i].pos = bar_r->touches[i].position;
                frame.bar2[i].size = bar_r->touches[i].size;
            } else {
                frame.bar2[i].pos = 0xFFFF;
                frame.bar2[i].size = 0;
            }
        }

        // Button state from processed sensor readings
        frame.buttons = m_button_state;

        nchorder_cdc_send_touch_frame(&frame);
    }

    (void)any_touch;  // Suppress unused warning

    // Settling period - ignore all touches while sensors stabilize after init
    if (m_settling_polls > 0) {
        m_settling_polls--;
        if (m_settling_polls == 0) {
            NRF_LOG_INFO("Trill buttons: Settling complete, accepting input");
        }
        return;  // Skip button detection during settling
    }

    // Process gesture on thumb sensor (slide vs tap detection)
    // Works for both 2D (Square) and 1D (Flex) modes
    // Must be called BEFORE build_button_mask so m_gesture.is_mouse_mode is set
    if (m_sensors[MUX_CH_THUMB].initialized) {
        process_square_gesture(&m_sensors[MUX_CH_THUMB]);
    }

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

    SEGGER_RTT_printf(0, "INIT:Trill buttons starting\n");
    NRF_LOG_INFO("Trill buttons: Initializing");

    // Initialize I2C bus
    err_code = nchorder_i2c_init();
    if (err_code != NRF_SUCCESS) {
        NRF_LOG_ERROR("Trill buttons: I2C init failed: 0x%08X", err_code);
        return err_code;
    }

    // Reset mux
    nchorder_i2c_mux_reset();

    // Hardware reset all Trill sensors via dedicated RESET pin
    NRF_LOG_INFO("Trill buttons: Hardware reset via P0.07");
    nrf_gpio_cfg_output(PIN_TRILL_RESET);
    nrf_gpio_pin_set(PIN_TRILL_RESET);    // Start high (inactive)
    simple_delay_ms(10);
    nrf_gpio_pin_clear(PIN_TRILL_RESET);  // Pulse low (active)
    simple_delay_ms(10);
    nrf_gpio_pin_set(PIN_TRILL_RESET);    // Back to high
    simple_delay_ms(500);                  // Wait for sensors to boot
    nrf_drv_wdt_feed();                    // Keep watchdog happy during long init

    // Probe MUX directly at address 0x70 (before any channel selection)
    NRF_LOG_INFO("Trill buttons: Probing MUX at 0x%02X...", I2C_ADDR_MUX);
    err_code = nchorder_i2c_read(I2C_ADDR_MUX, &dummy, 1);
    if (err_code != NRF_SUCCESS) {
        NRF_LOG_ERROR("Trill buttons: MUX not responding! Error 0x%08X", err_code);
        // Continue anyway to see what happens
    } else {
        NRF_LOG_INFO("Trill buttons: MUX responded (read 0x%02X)", dummy);
    }

    // Scan ALL mux channels for I2C devices
    SEGGER_RTT_printf(0, "SCAN:Starting full I2C scan on all mux channels\n");
    memset(m_scan_results, 0, sizeof(m_scan_results));
    for (int scan_ch = 0; scan_ch < MUX_NUM_CHANNELS; scan_ch++) {
        err_code = nchorder_i2c_mux_select(scan_ch);
        if (err_code != NRF_SUCCESS) {
            SEGGER_RTT_printf(0, "SCAN:Ch%d mux select FAILED\n", scan_ch);
            continue;
        }

        SEGGER_RTT_printf(0, "SCAN:Ch%d ", scan_ch);
        for (uint8_t addr = 0x20; addr <= 0x50; addr++) {
            err_code = nchorder_i2c_read(addr, &dummy, 1);
            if (err_code == NRF_SUCCESS) {
                SEGGER_RTT_printf(0, "0x%02X ", addr);
                if (m_scan_results[scan_ch] == 0) {
                    m_scan_results[scan_ch] = addr;  // Store first found address
                }
            }
        }
        if (m_scan_results[scan_ch] == 0) {
            SEGGER_RTT_printf(0, "NO_DEVICES");
        }
        SEGGER_RTT_printf(0, "\n");
        nrf_drv_wdt_feed();  // Feed watchdog after each channel scan
    }
    SEGGER_RTT_printf(0, "SCAN:Complete ch0=0x%02X ch1=0x%02X ch2=0x%02X ch3=0x%02X\n",
        m_scan_results[0], m_scan_results[1], m_scan_results[2], m_scan_results[3]);

    // Initialize each Trill sensor using addresses found during scan
    for (int ch = 0; ch < MUX_NUM_CHANNELS; ch++) {
        SEGGER_RTT_printf(0, "INIT:Ch%d starting\n", ch);
        NRF_LOG_INFO("Trill buttons: Initializing sensor on channel %d", ch);

        // Skip if no device was found during scan
        if (m_scan_results[ch] == 0) {
            SEGGER_RTT_printf(0, "INIT:Ch%d SKIP - no device found in scan\n", ch);
            continue;
        }

        err_code = nchorder_i2c_mux_select(ch);
        if (err_code != NRF_SUCCESS) {
            SEGGER_RTT_printf(0, "INIT:Ch%d mux_select FAIL 0x%04X\n", ch, err_code);
            NRF_LOG_ERROR("Trill buttons: Mux select ch%d failed: 0x%08X", ch, err_code);
            continue;
        }

        // Use the address found during scan
        uint8_t addr = m_scan_results[ch];
        SEGGER_RTT_printf(0, "INIT:Ch%d using scanned addr 0x%02X\n", ch, addr);

        // Initialize sensor inline to avoid function call overhead
        trill_sensor_t *sensor = &m_sensors[ch];
        static uint8_t identify_buf[4] = {0};

        // Clear sensor state first
        memset(sensor, 0, sizeof(trill_sensor_t));
        sensor->i2c_addr = addr;

        // Step 1: Send IDENTIFY command (required before reading identification)
        uint8_t identify_cmd[2] = {TRILL_OFFSET_COMMAND, TRILL_CMD_IDENTIFY};
        err_code = nchorder_i2c_write(addr, identify_cmd, 2);
        if (err_code != NRF_SUCCESS) {
            NRF_LOG_WARNING("Ch%d: IDENTIFY cmd failed: 0x%04X", ch, err_code);
            continue;
        }

        simple_delay_ms(50);  // Wait for sensor to populate identification data

        // Step 2: Set read pointer to offset 0
        uint8_t zero_offset = 0;
        err_code = nchorder_i2c_write(addr, &zero_offset, 1);
        if (err_code != NRF_SUCCESS) {
            NRF_LOG_WARNING("Ch%d: Set read ptr failed: 0x%04X", ch, err_code);
            continue;
        }

        simple_delay_ms(5);

        // Step 3: Read identification bytes
        err_code = nchorder_i2c_read(addr, identify_buf, 4);
        if (err_code != NRF_SUCCESS) {
            NRF_LOG_WARNING("Ch%d: Identify read failed: 0x%04X", ch, err_code);
            continue;
        }

        NRF_LOG_INFO("Ch%d: IDENTIFY response %02X %02X %02X %02X", ch,
                     identify_buf[0], identify_buf[1], identify_buf[2], identify_buf[3]);

        // Step 4: Reset sensor AFTER identification
        NRF_LOG_INFO("Ch%d: Sending reset command", ch);
        uint8_t reset_cmd[2] = {TRILL_OFFSET_COMMAND, TRILL_CMD_RESET};
        err_code = nchorder_i2c_write(addr, reset_cmd, 2);
        if (err_code != NRF_SUCCESS) {
            NRF_LOG_WARNING("Ch%d: Reset failed: 0x%04X", ch, err_code);
        }
        simple_delay_ms(500);  // Wait for sensor to recover
        nrf_drv_wdt_feed();    // Keep watchdog happy during long sensor init

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

        // Log actual detected type before any forcing
        NRF_LOG_INFO("Ch%d: Detected as %s (type=%d, fw=%d)", ch,
                     trill_type_name(sensor->device_type), sensor->device_type, sensor->firmware_version);

        sensor->is_2d = (sensor->device_type == TRILL_TYPE_SQUARE ||
                         sensor->device_type == TRILL_TYPE_HEX);

        // Channel 0 identifies as Flex (1D) - try treating it as 1D
        // to see if the data parses correctly. Physical Square might be
        // misconfigured or running different firmware.
        if (ch == MUX_CH_THUMB) {
            if (sensor->device_type == TRILL_TYPE_FLEX) {
                NRF_LOG_WARNING("Ch%d: Keeping as 1D Flex (not forcing 2D)", ch);
                sensor->is_2d = false;  // Use 1D parsing
            }
        }

        NRF_LOG_INFO("Ch%d: Using Trill %s mode", ch,
                     trill_type_name(sensor->device_type));

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

        // Step 4b: Set prescaler (1-8, higher = more sensitive but noisier)
        // Square sensor: use 3 (moderate sensitivity)
        // Bar sensors: use 3 (balanced sensitivity/noise)
        uint8_t prescaler = 3;  // Same for all sensors
        uint8_t prescaler_cmd[3] = {TRILL_OFFSET_COMMAND, TRILL_CMD_PRESCALER, prescaler};
        err_code = nchorder_i2c_write(addr, prescaler_cmd, 3);
        if (err_code != NRF_SUCCESS) {
            NRF_LOG_WARNING("Ch%d: Prescaler failed: 0x%04X", ch, err_code);
        }
        simple_delay_ms(5);

        // Step 4c: Set noise threshold (0-255, higher = less sensitive)
        // Trill expects single byte. Default is ~40, max 255.
        // Use same value for all sensors for consistent behavior
        uint8_t noise_thresh = 100;  // Moderate filtering, same for all
        uint8_t noise_cmd[3] = {TRILL_OFFSET_COMMAND, TRILL_CMD_NOISE_THRESHOLD, noise_thresh};
        err_code = nchorder_i2c_write(addr, noise_cmd, 3);
        if (err_code != NRF_SUCCESS) {
            NRF_LOG_WARNING("Ch%d: Noise threshold failed: 0x%04X", ch, err_code);
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
    m_settling_polls = SETTLING_POLL_COUNT;  // Start settling period
    NRF_LOG_INFO("Trill buttons: Init complete, polling every %d ms (settling for %d ms)",
                 TRILL_POLL_INTERVAL_MS, SETTLING_POLL_COUNT * TRILL_POLL_INTERVAL_MS);

    return NRF_SUCCESS;
}

uint32_t buttons_scan(void)
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

const char* buttons_to_string(uint32_t bitmask)
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
