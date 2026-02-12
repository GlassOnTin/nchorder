/**
 * Northern Chorder - Button Scanning Driver
 *
 * This file provides button input handling. The actual implementation
 * is selected at compile time based on the board configuration:
 *
 * - GPIO driver (default): For Twiddler 4 and DK boards
 * - Trill driver: For XIAO nRF52840 with capacitive touch sensors
 *
 * GPIO Reading: Uses direct register polling (NRF_P0->IN), not GPIOTE interrupts.
 * This was found to be more reliable for scanning multiple buttons.
 */

#include "nchorder_buttons.h"
#include "nchorder_config.h"

// ============================================================================
// DRIVER SELECTION
// ============================================================================
// If BUTTON_DRIVER_TRILL is defined (in board header), use Trill driver.
// Otherwise, use the GPIO driver below.

#if defined(BUTTON_DRIVER_TRILL)

// Include Trill sensor driver and button driver implementation
// Both included here to ensure they're in the same compilation unit
#include "nchorder_trill.c"
#include "button_driver_trill.c"

#else // GPIO Driver

// ============================================================================
// GPIO BUTTON DRIVER IMPLEMENTATION
// ============================================================================

#include "nrf_gpio.h"
#include "nrfx_gpiote.h"
#include "app_timer.h"
#include "nrf_log.h"
#include "nrf_sdh.h"
#include "nrf_nvic.h"
#include "nrf.h"
#include "nchorder_cdc.h"

// Debounce handled by counter in poll timer (no separate timer needed)

// Button pin array (indexed by bitmask position)
static const uint8_t m_button_pins[NCHORDER_TOTAL_BUTTONS] = BUTTON_PINS;

// Button names for debug output
#if defined(BOARD_TWIDDLER4)
static const char* const m_button_names[NCHORDER_TOTAL_BUTTONS] = {
    "T1", "F1L", "F1M", "F1R",
    "T2", "F2L", "F2M", "F2R",
    "T3", "F3L", "F3M", "F3R",
    "T4", "F4L", "F4M", "F4R",
    "F0L", "F0M", "F0R", "T0",
    "EXT1", "EXT2"
};
#else
static const char* const m_button_names[NCHORDER_TOTAL_BUTTONS] = {
    "T1", "F1L", "F1M", "F1R",
    "T2", "F2L", "F2M", "F2R",
    "T3", "F3L", "F3M", "F3R",
    "T4", "F4L", "F4M", "F4R"
};
#endif

// Current debounced button state
static uint32_t m_button_state = 0;

// Raw button state (before debounce)
static uint32_t m_raw_state = 0;

// Debug: count button press events
static uint32_t m_button_press_count = 0;

// Debug: count callback invocations
static uint32_t m_callback_count = 0;

// Callback for button state changes
static buttons_callback_t m_callback = NULL;

// Debounce counter (counts stable polls)
static volatile uint8_t m_debounce_count = 0;

/**
 * Read raw GPIO state and convert to button bitmask
 * GPIO is active-low, returns active-high bitmask
 *
 * Note: All Twiddler 4 buttons are on P0. P1 is not used for buttons
 * (the E73 module routes pins 38/40/42 to P0.15/P0.20/P0.17, not P1).
 */
static uint32_t read_gpio_state_raw(void)
{
    uint32_t port0_state = NRF_P0->IN;
    uint32_t port1_state = NRF_P1->IN;
    uint32_t bitmask = 0;

    for (int i = 0; i < NCHORDER_TOTAL_BUTTONS; i++) {
        uint8_t pin = m_button_pins[i];
        uint32_t port_state;
        uint8_t port_pin;

        // Determine which port this pin is on
        if (pin >= 32) {
            port_state = port1_state;
            port_pin = pin - 32;
        } else {
            port_state = port0_state;
            port_pin = pin;
        }

        // Active-low: pin LOW means button pressed
        if (!(port_state & (1UL << port_pin))) {
            bitmask |= (1UL << i);
        }
    }

    return bitmask;
}

static uint32_t read_gpio_state(void)
{
    return read_gpio_state_raw();
}

// Debounce is now handled by counter in poll_timer_handler

/**
 * GPIO Polling timer handler
 * Called periodically to scan button state
 */
APP_TIMER_DEF(m_poll_timer);
#define POLL_INTERVAL_MS  5  // Poll every 5ms

// Runtime debounce: reads m_config.debounce_ms via CDC config getter
static inline uint8_t debounce_polls_required(void) {
    const cdc_config_t *cfg = nchorder_cdc_get_config();
    uint16_t ms = cfg ? cfg->debounce_ms : CHORD_DEBOUNCE_MS;
    return (uint8_t)(ms / POLL_INTERVAL_MS + 1);
}

// Stream rate divider (send every N polls to match desired rate)
static uint8_t m_stream_divider = 0;
#define STREAM_POLLS_PER_FRAME  3  // 5ms * 3 = 15ms = ~66Hz

static void poll_timer_handler(void *p_context)
{
    (void)p_context;

    uint32_t current_state = read_gpio_state_raw();

    // Check if state changed from last raw reading
    if (current_state != m_raw_state) {
        // State changed - reset debounce counter
        m_raw_state = current_state;
        m_debounce_count = 0;
    } else if (current_state != m_button_state) {
        // State stable but different from debounced state - count up
        m_debounce_count++;
        if (m_debounce_count >= debounce_polls_required()) {
            // Debounce complete - update state and notify
            uint32_t old_state = m_button_state;
            m_button_state = current_state;
            m_debounce_count = 0;

            NRF_LOG_INFO("Button state: 0x%05X -> 0x%05X", old_state, current_state);

            // Debug: count button events
            if (current_state != 0) {
                m_button_press_count++;
            }

            if (m_callback != NULL) {
                m_callback_count++;
                m_callback(m_button_state);
            }
        }
    }

    // CDC streaming: send button state at reduced rate
    if (nchorder_cdc_is_streaming()) {
        m_stream_divider++;
        if (m_stream_divider >= STREAM_POLLS_PER_FRAME) {
            m_stream_divider = 0;

            // Read raw GPIO state for diagnostics
            uint32_t raw_p0 = NRF_P0->IN;
            uint32_t raw_p1 = NRF_P1->IN;
            uint32_t raw_buttons = read_gpio_state_raw();

            // Create frame with button data (no touch sensors)
            cdc_touch_frame_t frame = {0};
            frame.sync = CDC_STREAM_SYNC;
            frame.buttons = m_button_state;

            // Debug markers in thumb fields
            frame.thumb_x = 0x1234;  // Marker to confirm GPIO driver
            frame.thumb_y = m_callback_count & 0xFFFF;  // Callback invocation count
            frame.thumb_size = raw_buttons & 0xFFFF;  // Raw button state (low 16 bits)

            // Use bar positions to send raw GPIO state for debugging
            // Bar0: P0->IN (split into 3 x 16-bit values)
            frame.bar0[0].pos = (raw_p0 >> 0) & 0xFFFF;   // P0 bits 0-15
            frame.bar0[0].size = (raw_p0 >> 16) & 0xFFFF; // P0 bits 16-31
            frame.bar0[1].pos = (raw_buttons >> 16) & 0xFFFF; // Raw buttons high bits
            frame.bar0[1].size = m_debounce_count;  // Debounce counter

            // Bar1: P1->IN and state tracking
            frame.bar1[0].pos = (raw_p1 >> 0) & 0xFFFF;   // P1 bits 0-15
            frame.bar1[0].size = (raw_p1 >> 16) & 0xFFFF; // P1 bits 16-31
            frame.bar1[1].pos = m_raw_state & 0xFFFF;     // Previous raw state (low)
            frame.bar1[1].size = (m_raw_state >> 16) & 0xFFFF; // Previous raw state (high)

            // Mark unused bar slots
            for (int i = 2; i < CDC_MAX_BAR_TOUCHES; i++) {
                frame.bar0[i].pos = 0xFFFF;
                frame.bar1[i].pos = 0xFFFF;
            }
            for (int i = 0; i < CDC_MAX_BAR_TOUCHES; i++) {
                frame.bar2[i].pos = 0xFFFF;
            }

            nchorder_cdc_send_touch_frame(&frame);
        }
    }
}

uint32_t buttons_init(void)
{
    ret_code_t err_code;

    NRF_LOG_INFO("Buttons: Initializing %d buttons (polling mode, default debounce=%dms)",
                 NCHORDER_TOTAL_BUTTONS, CHORD_DEBOUNCE_MS);

    // Create polling timer (debounce handled by counter)
    err_code = app_timer_create(&m_poll_timer,
                                APP_TIMER_MODE_REPEATED,
                                poll_timer_handler);
    if (err_code != NRF_SUCCESS) {
        NRF_LOG_ERROR("Buttons: Poll timer create failed: %d", err_code);
        return err_code;
    }

    // Configure each button pin as input with pull-up
    for (int i = 0; i < NCHORDER_TOTAL_BUTTONS; i++) {
        uint8_t pin = m_button_pins[i];
        nrf_gpio_cfg_input(pin, NRF_GPIO_PIN_PULLUP);
        NRF_LOG_DEBUG("Buttons: Pin %d (%s) configured", pin, m_button_names[i]);
    }

    // GPIO scan: enable pull-ups on all unassigned P0 pins so we can
    // observe which ones the thumb buttons actually connect to.
    // Skip: P0.30/P0.31 (I2C), and pins already configured above.
    {
        // Build bitmask of pins already configured as button inputs
        uint32_t p0_configured = 0;
        for (int i = 0; i < NCHORDER_TOTAL_BUTTONS; i++) {
            uint8_t pin = m_button_pins[i];
            if (pin < 32) p0_configured |= (1UL << pin);
        }
        // Exclude only pins already configured as button inputs
        // (P0.30/P0.31 included in scan - optical TWI is disabled)
        uint32_t p0_exclude = p0_configured;
        for (uint8_t p = 0; p < 32; p++) {
            if (!(p0_exclude & (1UL << p))) {
                nrf_gpio_cfg_input(p, NRF_GPIO_PIN_PULLUP);
            }
        }
        NRF_LOG_INFO("Buttons: GPIO scan pull-ups enabled on P0 (exclude mask=0x%08X)", p0_exclude);

        // Also pull up all P1 pins except LED power (P1.10) and LED data (P1.13)
        uint32_t p1_exclude = (1UL << 10) | (1UL << 13);
        // Include P1.09 (EXT2) which is already configured above
        for (uint8_t p = 0; p < 16; p++) {
            if (!(p1_exclude & (1UL << p))) {
                nrf_gpio_cfg_input(NRF_GPIO_PIN_MAP(1, p), NRF_GPIO_PIN_PULLUP);
            }
        }
        NRF_LOG_INFO("Buttons: GPIO scan pull-ups enabled on P1 (exclude mask=0x%04X)", p1_exclude);
    }

    // Brief delay for pins to settle
    for (volatile int j = 0; j < 10000; j++);

    // Log initial GPIO state
    NRF_LOG_INFO("Buttons: P0.IN=0x%08X", NRF_P0->IN);
    NRF_LOG_INFO("Buttons: P1.IN=0x%08X", NRF_P1->IN);
#if defined(BOARD_TWIDDLER4)
    // Debug EXT1 (P0.28) and EXT2 (P1.09) specifically
    NRF_LOG_INFO("Buttons: EXT1(P0.28)=%d EXT2(P1.09)=%d",
                 (NRF_P0->IN >> 28) & 1, (NRF_P1->IN >> 9) & 1);
    // Debug: show pin numbers at positions 20 and 21
    NRF_LOG_INFO("Buttons: pin[20]=%d pin[21]=%d", m_button_pins[20], m_button_pins[21]);
#endif

    // Read initial state
    m_button_state = read_gpio_state();
    m_raw_state = m_button_state;

    // Start polling timer
    err_code = app_timer_start(m_poll_timer, APP_TIMER_TICKS(POLL_INTERVAL_MS), NULL);
    if (err_code != NRF_SUCCESS) {
        NRF_LOG_ERROR("Buttons: Poll timer start failed: %d", err_code);
        return err_code;
    }

    NRF_LOG_INFO("Buttons: Init complete, poll=%dms, initial=0x%05X",
                 POLL_INTERVAL_MS, m_button_state);

    // Debug: log all button pins
    NRF_LOG_INFO("Buttons: Pin mapping (NCHORDER_TOTAL_BUTTONS=%d):", NCHORDER_TOTAL_BUTTONS);
    for (int i = 0; i < NCHORDER_TOTAL_BUTTONS && i < 22; i++) {
        uint8_t pin = m_button_pins[i];
        uint8_t port = (pin >= 32) ? 1 : 0;
        uint8_t port_pin = (pin >= 32) ? pin - 32 : pin;
        NRF_LOG_INFO("  [%d] %s = P%d.%02d (pin %d)", i, m_button_names[i], port, port_pin, pin);
    }

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
        if (bitmask & (1UL << i)) {
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

#endif // BUTTON_DRIVER_TRILL
