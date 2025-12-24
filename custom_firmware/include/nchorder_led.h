/**
 * Northern Chorder - RGB LED Driver
 *
 * WS2812/SK6812 addressable LED control via I2S peripheral.
 * Hardware: 3 RGB LEDs (L1-L3) on thumb board, daisy-chained.
 *
 * I2S Encoding Approach:
 * - I2S at 3.2MHz (4x oversampling of 800kHz WS2812 protocol)
 * - Each WS2812 bit = 4 I2S bits
 * - Logic 0 = 0b1000 (high-low-low-low)
 * - Logic 1 = 0b1110 (high-high-high-low)
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

// Common colors (GRB order for WS2812)
#define LED_COLOR_OFF           0x00, 0x00, 0x00
#define LED_COLOR_RED           0x00, 0xFF, 0x00
#define LED_COLOR_GREEN         0xFF, 0x00, 0x00
#define LED_COLOR_BLUE          0x00, 0x00, 0xFF
#define LED_COLOR_WHITE         0xFF, 0xFF, 0xFF
#define LED_COLOR_YELLOW        0xFF, 0xFF, 0x00
#define LED_COLOR_CYAN          0xFF, 0x00, 0xFF
#define LED_COLOR_MAGENTA       0x00, 0xFF, 0xFF

// Dimmed versions for status indication (25% brightness)
#define LED_DIM_RED             0x00, 0x40, 0x00
#define LED_DIM_GREEN           0x40, 0x00, 0x00
#define LED_DIM_BLUE            0x00, 0x00, 0x40
#define LED_DIM_WHITE           0x40, 0x40, 0x40

/**
 * @brief Initialize the LED driver.
 *
 * Configures I2S peripheral for WS2812 timing and initializes LEDs to off.
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
 * Encodes color data and transmits via I2S to WS2812 LEDs.
 * This function is non-blocking - data transfer happens via DMA.
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

#endif // NCHORDER_LED_H
