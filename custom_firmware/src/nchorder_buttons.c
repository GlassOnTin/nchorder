/**
 * Twiddler 4 Custom Firmware - Button Scanning Driver
 *
 * Implements GPIO-based button input with GPIOTE interrupts
 * All 16 buttons on GPIO Port 0, active-low with internal pull-ups
 */

#include "nchorder_buttons.h"
#include "nrf_gpio.h"
#include "nrfx_gpiote.h"
#include "app_timer.h"
#include "nrf_log.h"

// Debounce timer
APP_TIMER_DEF(m_debounce_timer);

// Button pin array (indexed by bitmask position)
static const uint8_t m_button_pins[NCHORDER_TOTAL_BUTTONS] = BUTTON_PINS;

// Button names for debug output
static const char* const m_button_names[NCHORDER_TOTAL_BUTTONS] = {
    "T1", "F1L", "F1M", "F1R",
    "T2", "F2L", "F2M", "F2R",
    "T3", "F3L", "F3M", "F3R",
    "T4", "F4L", "F4M", "F4R"
};

// Current debounced button state
static uint16_t m_button_state = 0;

// Raw button state (before debounce)
static uint16_t m_raw_state = 0;

// Callback for button state changes
static buttons_callback_t m_callback = NULL;

// Debounce in progress flag
static volatile bool m_debounce_pending = false;

/**
 * Read raw GPIO state and convert to button bitmask
 * GPIO is active-low, returns active-high bitmask
 */
static uint16_t read_gpio_state(void)
{
    uint32_t port0_state = NRF_P0->IN;
    uint16_t bitmask = 0;

    for (int i = 0; i < NCHORDER_TOTAL_BUTTONS; i++) {
        uint8_t pin = m_button_pins[i];

        // Skip pins on Port 1 (unused/disabled buttons)
        if (pin >= 32) {
            continue;
        }

        // Active-low: pin LOW means button pressed
        if (!(port0_state & (1UL << pin))) {
            bitmask |= (1 << i);
        }
    }

    return bitmask;
}

/**
 * Debounce timer handler
 * Called after debounce delay to check if button state is stable
 */
static void debounce_timer_handler(void *p_context)
{
    (void)p_context;

    uint16_t current_state = read_gpio_state();

    // Only update if state matches raw state (stable)
    if (current_state == m_raw_state) {
        if (current_state != m_button_state) {
            m_button_state = current_state;

            // Call callback if registered
            if (m_callback != NULL) {
                m_callback(m_button_state);
            }
        }
    }

    m_debounce_pending = false;
}

/**
 * GPIOTE event handler
 * Called on any button GPIO state change
 */
static void gpiote_event_handler(nrfx_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
    (void)pin;
    (void)action;

    // Read current raw state
    m_raw_state = read_gpio_state();

    // Start/restart debounce timer
    if (!m_debounce_pending) {
        m_debounce_pending = true;
        app_timer_start(m_debounce_timer, APP_TIMER_TICKS(CHORD_DEBOUNCE_MS), NULL);
    } else {
        // Restart timer if already pending
        app_timer_stop(m_debounce_timer);
        app_timer_start(m_debounce_timer, APP_TIMER_TICKS(CHORD_DEBOUNCE_MS), NULL);
    }
}

uint32_t buttons_init(void)
{
    ret_code_t err_code;

    NRF_LOG_INFO("Buttons: Initializing %d buttons", NCHORDER_TOTAL_BUTTONS);

    // Initialize GPIOTE if not already initialized
    if (!nrfx_gpiote_is_init()) {
        err_code = nrfx_gpiote_init();
        if (err_code != NRF_SUCCESS && err_code != NRFX_ERROR_INVALID_STATE) {
            NRF_LOG_ERROR("Buttons: GPIOTE init failed: %d", err_code);
            return err_code;
        }
    }

    // Create debounce timer
    err_code = app_timer_create(&m_debounce_timer,
                                APP_TIMER_MODE_SINGLE_SHOT,
                                debounce_timer_handler);
    if (err_code != NRF_SUCCESS) {
        NRF_LOG_ERROR("Buttons: Timer create failed: %d", err_code);
        return err_code;
    }

    // Configure each button pin
    // Track which pins we've already initialized to avoid duplicates
    // (e.g., when multiple buttons map to same PIN_UNUSED)
    uint64_t initialized_pins = 0;
    nrfx_gpiote_in_config_t in_config = NRFX_GPIOTE_CONFIG_IN_SENSE_TOGGLE(false);
    in_config.pull = NRF_GPIO_PIN_PULLUP;

    for (int i = 0; i < NCHORDER_TOTAL_BUTTONS; i++) {
        uint8_t pin = m_button_pins[i];

        // Skip pins on Port 1 (unused/disabled buttons on DK)
        // PIN_UNUSED is on Port 1, real buttons are on Port 0
        if (pin >= 32) {
            NRF_LOG_DEBUG("Buttons: Skipping %s (pin %d on Port 1, disabled)", m_button_names[i], pin);
            continue;
        }

        // Skip if we already initialized this pin
        if (initialized_pins & (1ULL << pin)) {
            NRF_LOG_DEBUG("Buttons: Skipping %s (pin %d already initialized)", m_button_names[i], pin);
            continue;
        }

        err_code = nrfx_gpiote_in_init(pin, &in_config, gpiote_event_handler);
        if (err_code != NRF_SUCCESS) {
            NRF_LOG_ERROR("Buttons: Pin %d init failed: %d", pin, err_code);
            return err_code;
        }

        nrfx_gpiote_in_event_enable(pin, true);
        initialized_pins |= (1ULL << pin);
        NRF_LOG_DEBUG("Buttons: Pin P0.%02d configured for %s", pin, m_button_names[i]);
    }

    // Read initial state
    m_button_state = read_gpio_state();
    m_raw_state = m_button_state;

    NRF_LOG_INFO("Buttons: Init complete, initial state: 0x%04X", m_button_state);

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
