/**
 * Northern Chorder - Simple GPIO LED Driver
 *
 * For boards without WS2812B addressable LEDs.
 * Uses a single GPIO pin with on/off control only.
 */

#include "nchorder_led.h"
#include "nchorder_config.h"
#include "nrf_gpio.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"

// Module state
static bool m_initialized = false;
static bool m_led_on = false;

ret_code_t nchorder_led_init(void)
{
    if (m_initialized) {
        return NRF_SUCCESS;
    }

    // Configure status LED as output
    nrf_gpio_cfg_output(PIN_LED_STATUS);

    // LED off initially (active low on most boards)
    nrf_gpio_pin_set(PIN_LED_STATUS);

    m_initialized = true;
    m_led_on = false;

    NRF_LOG_INFO("LED driver initialized (GPIO mode, pin %d)", PIN_LED_STATUS);

    return NRF_SUCCESS;
}

void nchorder_led_set(uint8_t led_index, uint8_t r, uint8_t g, uint8_t b)
{
    (void)led_index;

    // Simple on/off based on any color component
    m_led_on = (r > 0 || g > 0 || b > 0);
}

void nchorder_led_set_all(uint8_t r, uint8_t g, uint8_t b)
{
    m_led_on = (r > 0 || g > 0 || b > 0);
}

ret_code_t nchorder_led_update(void)
{
    if (!m_initialized) {
        return NRF_ERROR_INVALID_STATE;
    }

    // Active low LED control
    if (m_led_on) {
        nrf_gpio_pin_clear(PIN_LED_STATUS);
    } else {
        nrf_gpio_pin_set(PIN_LED_STATUS);
    }

    return NRF_SUCCESS;
}

void nchorder_led_off(void)
{
    nchorder_led_set_all(LED_COLOR_OFF);
    nchorder_led_update();
}

void nchorder_led_indicate_ble_connected(void)
{
    // Solid on for connected
    m_led_on = true;
    nchorder_led_update();
}

void nchorder_led_indicate_ble_advertising(void)
{
    // Simple on for advertising (could add blink pattern with timer)
    m_led_on = true;
    nchorder_led_update();
}

void nchorder_led_indicate_usb_connected(void)
{
    m_led_on = true;
    nchorder_led_update();
}

void nchorder_led_indicate_error(void)
{
    m_led_on = true;
    nchorder_led_update();
}

bool nchorder_led_is_ready(void)
{
    return true;  // GPIO is always ready
}
