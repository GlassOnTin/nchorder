/**
 * Northern Chorder - Button Scanning Driver
 *
 * GPIO-based button input handling with polling detection
 * Twiddler 4: 19 buttons (4 thumb + 5x3 finger) with internal pull-ups (active-low)
 */

#ifndef NCHORDER_BUTTONS_H
#define NCHORDER_BUTTONS_H

#include <stdint.h>
#include <stdbool.h>
#include "nchorder_config.h"

/**
 * Button callback type
 * Called when button state changes (after debouncing)
 *
 * @param button_state 32-bit bitmask of currently pressed buttons
 *                     Bit 0 = T1, Bit 1 = F1L, ... Bit 16 = F0L, etc.
 *                     1 = pressed, 0 = released
 */
typedef void (*buttons_callback_t)(uint32_t button_state);

/**
 * Initialize button GPIO pins
 * Configures all button pins as inputs with pull-ups
 *
 * @return 0 on success, error code on failure
 */
uint32_t buttons_init(void);

/**
 * Scan current button state
 * Reads all GPIO pins and returns active-high bitmask
 * (GPIO is active-low, this function inverts for convenience)
 *
 * @return 32-bit bitmask of pressed buttons (1 = pressed)
 */
uint32_t buttons_scan(void);

/**
 * Register callback for button state changes
 * Only one callback can be registered at a time
 *
 * @param callback Function to call on button changes, or NULL to disable
 */
void buttons_set_callback(buttons_callback_t callback);

/**
 * Check if any button is currently pressed
 *
 * @return true if at least one button is pressed
 */
bool buttons_any_pressed(void);

/**
 * Get string representation of button bitmask (for debugging)
 * Returns static buffer, not thread-safe
 *
 * @param bitmask Button state bitmask
 * @return Human-readable string like "T1+F1M+F2R"
 */
const char* buttons_to_string(uint32_t bitmask);

#endif // NCHORDER_BUTTONS_H
