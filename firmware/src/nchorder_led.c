/**
 * Northern Chorder - RGB LED Driver
 *
 * Addressable LED control via GPIO bit-bang.
 *
 * Hardware:
 * - P1.10 (PIN_LED_POWER) controls Q1 transistor for power enable
 * - P1.13 (PIN_LED_DATA) is the data line
 * - 3 RGB LEDs in RGB order (NOT GRB like WS2812), daisy-chained
 *
 * Power management:
 * - LEDs are the dominant current draw (~20-60mA via Q1)
 * - Auto-off timer powers down LEDs after timed display
 * - Status indications show briefly then auto-off
 *
 * Timing: Uses nrf_delay_us for reliable timing
 */

#include "nchorder_led.h"
#include "nchorder_config.h"
#include "nrf_gpio.h"
#include "nrf_delay.h"
#include "app_timer.h"
#include <string.h>

#include "nrf_log.h"
#include "nrf_log_ctrl.h"

// Color buffer (RGB order - NOT GRB!)
typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} led_color_t;

// Module state
static led_color_t m_colors[NCHORDER_LED_COUNT];
static bool m_initialized = false;
static bool m_power_enabled = false;

// Auto-off timer
APP_TIMER_DEF(m_led_off_timer);

/**
 * @brief Timer callback to power off LEDs after timed display.
 */
static void led_off_timer_handler(void * p_context)
{
    UNUSED_PARAMETER(p_context);
    nchorder_led_power_off();
}

/**
 * @brief Send a single byte using delay-based timing.
 */
static void send_byte(uint8_t byte)
{
    for (int bit = 7; bit >= 0; bit--) {
        if (byte & (1 << bit)) {
            // '1' bit: longer high
            nrf_gpio_pin_set(PIN_LED_DATA);
            nrf_delay_us(1);
            nrf_gpio_pin_clear(PIN_LED_DATA);
            // Short low (function call overhead is enough)
        } else {
            // '0' bit: shorter high
            nrf_gpio_pin_set(PIN_LED_DATA);
            // Very short high (function call overhead)
            nrf_gpio_pin_clear(PIN_LED_DATA);
            nrf_delay_us(1);
        }
    }
}

ret_code_t nchorder_led_init(void)
{
    if (m_initialized) {
        return NRF_SUCCESS;
    }

    // Configure power enable pin and turn on LED power
    nrf_gpio_cfg_output(PIN_LED_POWER);
    nrf_gpio_pin_set(PIN_LED_POWER);
    m_power_enabled = true;

    // Let power stabilize
    nrf_delay_ms(10);

    // Configure data pin
    nrf_gpio_cfg_output(PIN_LED_DATA);
    nrf_gpio_pin_clear(PIN_LED_DATA);

    // Initialize colors to off
    memset(m_colors, 0, sizeof(m_colors));

    // Create auto-off timer
    ret_code_t err_code = app_timer_create(&m_led_off_timer,
                                            APP_TIMER_MODE_SINGLE_SHOT,
                                            led_off_timer_handler);
    if (err_code != NRF_SUCCESS) {
        NRF_LOG_WARNING("LED off timer create failed: %d", err_code);
    }

    m_initialized = true;

    NRF_LOG_INFO("LED driver initialized (power=%d, data=%d)", PIN_LED_POWER, PIN_LED_DATA);

    // Turn off LEDs initially
    nchorder_led_off();

    return NRF_SUCCESS;
}

void nchorder_led_set(uint8_t led_index, uint8_t r, uint8_t g, uint8_t b)
{
    if (led_index >= NCHORDER_LED_COUNT) {
        return;
    }

    m_colors[led_index].r = r;
    m_colors[led_index].g = g;
    m_colors[led_index].b = b;
}

void nchorder_led_set_all(uint8_t r, uint8_t g, uint8_t b)
{
    for (int i = 0; i < NCHORDER_LED_COUNT; i++) {
        m_colors[i].r = r;
        m_colors[i].g = g;
        m_colors[i].b = b;
    }
}

ret_code_t nchorder_led_update(void)
{
    if (!m_initialized) {
        return NRF_ERROR_INVALID_STATE;
    }

    // Ensure power is enabled
    if (!m_power_enabled) {
        nrf_gpio_pin_set(PIN_LED_POWER);
        m_power_enabled = true;
        nrf_delay_ms(1);
    }

    // Reset pulse (>50us low)
    nrf_gpio_pin_clear(PIN_LED_DATA);
    nrf_delay_us(80);

    // Send data for all LEDs (RGB order)
    for (int led = 0; led < NCHORDER_LED_COUNT; led++) {
        send_byte(m_colors[led].r);
        send_byte(m_colors[led].g);
        send_byte(m_colors[led].b);
    }

    // Latch pulse (>50us low)
    nrf_gpio_pin_clear(PIN_LED_DATA);
    nrf_delay_us(80);

    return NRF_SUCCESS;
}

void nchorder_led_off(void)
{
    nchorder_led_set_all(LED_COLOR_OFF);
    nchorder_led_update();
}

void nchorder_led_power_off(void)
{
    if (!m_initialized) {
        return;
    }

    // Send all-zero to LEDs first (prevents ghosting on power-up)
    nchorder_led_set_all(LED_COLOR_OFF);
    if (m_power_enabled) {
        nchorder_led_update();
    }

    // Cut power to LED chain via Q1 transistor
    nrf_gpio_pin_clear(PIN_LED_POWER);
    m_power_enabled = false;
}

void nchorder_led_power_on(void)
{
    if (!m_initialized) {
        return;
    }

    if (!m_power_enabled) {
        nrf_gpio_pin_set(PIN_LED_POWER);
        m_power_enabled = true;
        nrf_delay_ms(1);  // Stabilization delay
    }
}

void nchorder_led_show_timed(uint32_t ms)
{
    if (!m_initialized) {
        return;
    }

    // Stop any pending auto-off
    app_timer_stop(m_led_off_timer);

    // Power on and display
    nchorder_led_update();

    // Start auto-off timer
    ret_code_t err_code = app_timer_start(m_led_off_timer,
                                           APP_TIMER_TICKS(ms),
                                           NULL);
    if (err_code != NRF_SUCCESS) {
        NRF_LOG_WARNING("LED off timer start failed: %d", err_code);
    }
}

void nchorder_led_indicate_ble_connected(void)
{
    nchorder_led_set(LED_L1, LED_DIM_GREEN);
    nchorder_led_set(LED_L2, LED_COLOR_OFF);
    nchorder_led_set(LED_L3, LED_COLOR_OFF);
    nchorder_led_show_timed(2000);
}

void nchorder_led_indicate_ble_advertising(void)
{
    nchorder_led_set(LED_L1, LED_DIM_BLUE);
    nchorder_led_set(LED_L2, LED_COLOR_OFF);
    nchorder_led_set(LED_L3, LED_COLOR_OFF);
    nchorder_led_show_timed(2000);
}

void nchorder_led_indicate_usb_connected(void)
{
    nchorder_led_set(LED_L1, LED_COLOR_OFF);
    nchorder_led_set(LED_L2, LED_DIM_WHITE);
    nchorder_led_set(LED_L3, LED_COLOR_OFF);
    nchorder_led_show_timed(2000);
}

void nchorder_led_indicate_error(void)
{
    nchorder_led_set_all(LED_DIM_RED);
    nchorder_led_show_timed(2000);
}

bool nchorder_led_is_ready(void)
{
    return m_initialized;
}
