/**
 * Northern Chorder - RGB LED Driver
 *
 * WS2812/SK6812 addressable LED control via GPIO bit-bang.
 * Hardware: 3 RGB LEDs (L1-L3) on thumb board, daisy-chained.
 *
 * Pin Configuration (Twiddler 4):
 * - P1.10 (PIN_LED_POWER): Q1 transistor for power enable
 * - P1.13 (PIN_LED_DATA): WS2812 data line
 */

#ifndef NCHORDER_LED_H
#define NCHORDER_LED_H

#include <stdint.h>
#include <stdbool.h>
#include "sdk_errors.h"

// Number of RGB LEDs on thumb board
#define NCHORDER_LED_COUNT      3

// LED indices
#define LED_L1                  0
#define LED_L2                  1
#define LED_L3                  2

// Common colors (RGB order - these LEDs are NOT WS2812)
#define LED_COLOR_OFF           0x00, 0x00, 0x00
#define LED_COLOR_RED           0xFF, 0x00, 0x00
#define LED_COLOR_GREEN         0x00, 0xFF, 0x00
#define LED_COLOR_BLUE          0x00, 0x00, 0xFF
#define LED_COLOR_WHITE         0xFF, 0xFF, 0xFF
#define LED_COLOR_YELLOW        0xFF, 0xFF, 0x00
#define LED_COLOR_CYAN          0x00, 0xFF, 0xFF
#define LED_COLOR_MAGENTA       0xFF, 0x00, 0xFF

// Dimmed versions for status indication (~6% brightness, saves power)
#define LED_DIM_RED             0x10, 0x00, 0x00
#define LED_DIM_GREEN           0x00, 0x10, 0x00
#define LED_DIM_BLUE            0x00, 0x00, 0x10
#define LED_DIM_WHITE           0x10, 0x10, 0x10

/**
 * @brief Initialize the LED driver.
 *
 * Configures power enable and data pins, initializes LEDs to off.
 *
 * @return NRF_SUCCESS on success, error code otherwise.
 */
ret_code_t nchorder_led_init(void);

/**
 * @brief Set color of a single LED.
 *
 * Color is buffered until nchorder_led_update() is called.
 *
 * @param[in] led_index  LED index (0-2).
 * @param[in] r          Red intensity (0-255).
 * @param[in] g          Green intensity (0-255).
 * @param[in] b          Blue intensity (0-255).
 */
void nchorder_led_set(uint8_t led_index, uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief Set all LEDs to the same color.
 *
 * Color is buffered until nchorder_led_update() is called.
 *
 * @param[in] r  Red intensity (0-255).
 * @param[in] g  Green intensity (0-255).
 * @param[in] b  Blue intensity (0-255).
 */
void nchorder_led_set_all(uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief Update LEDs with buffered colors.
 *
 * Transmits color data to WS2812 LEDs via GPIO bit-bang.
 * This function is blocking (~100us for 3 LEDs).
 *
 * @return NRF_SUCCESS on success, error code otherwise.
 */
ret_code_t nchorder_led_update(void);

/**
 * @brief Turn off all LEDs.
 *
 * Convenience function that sets all LEDs to black and updates.
 */
void nchorder_led_off(void);

/**
 * @brief Indicate BLE connected state.
 *
 * Shows steady green on L1.
 */
void nchorder_led_indicate_ble_connected(void);

/**
 * @brief Indicate BLE advertising state.
 *
 * Shows dim blue on L1.
 */
void nchorder_led_indicate_ble_advertising(void);

/**
 * @brief Indicate USB connected state.
 *
 * Shows dim white on L2.
 */
void nchorder_led_indicate_usb_connected(void);

/**
 * @brief Indicate error state.
 *
 * Shows red on all LEDs.
 */
void nchorder_led_indicate_error(void);

/**
 * @brief Check if LED driver is ready for new update.
 *
 * @return true if ready, false if previous transfer still in progress.
 */
bool nchorder_led_is_ready(void);

/**
 * @brief Power off LEDs completely.
 *
 * Sends all-zero data, then clears Q1 transistor power enable pin.
 * Saves ~20-60mA when LEDs are not needed.
 */
void nchorder_led_power_off(void);

/**
 * @brief Power on LEDs.
 *
 * Sets Q1 transistor power enable pin with stabilization delay.
 */
void nchorder_led_power_on(void);

/**
 * @brief Display current LED colors for a timed duration, then auto-off.
 *
 * Calls nchorder_led_update() then starts a one-shot timer.
 * When the timer expires, LEDs are powered off via nchorder_led_power_off().
 *
 * @param[in] ms  Duration in milliseconds before auto-off.
 */
void nchorder_led_show_timed(uint32_t ms);

#endif // NCHORDER_LED_H
