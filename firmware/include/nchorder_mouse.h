/**
 * Northern Chorder - USB HID Mouse Driver
 *
 * Implements mouse control via the square Trill sensor.
 * Mouse always runs in parallel with chord typing.
 */

#ifndef NCHORDER_MOUSE_H
#define NCHORDER_MOUSE_H

#include <stdint.h>
#include <stdbool.h>

/**
 * Initialize HID mouse class
 * Must be called after nchorder_usb_init() and before nchorder_usb_start()
 */
uint32_t nchorder_mouse_init(void);

/**
 * Check if mouse is ready to send reports
 */
bool nchorder_mouse_is_ready(void);

/**
 * Move mouse by relative offset
 * Called from touch driver when square sensor position changes
 *
 * @param dx  X movement (-127 to 127)
 * @param dy  Y movement (-127 to 127)
 */
uint32_t nchorder_mouse_move(int8_t dx, int8_t dy);

/**
 * Move scroll wheel
 *
 * @param delta  Scroll amount (-127 to 127, positive = up)
 */
uint32_t nchorder_mouse_scroll(int8_t delta);

/**
 * Set mouse button state
 *
 * @param button  Button index (0=left, 1=right, 2=middle)
 * @param pressed True if pressed
 */
uint32_t nchorder_mouse_button(uint8_t button, bool pressed);

#endif // NCHORDER_MOUSE_H
