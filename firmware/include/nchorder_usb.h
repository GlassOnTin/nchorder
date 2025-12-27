/**
 * Northern Chorder - USB HID Driver
 *
 * USB keyboard HID support for wired operation
 * Uses Nordic SDK app_usbd_hid_kbd class
 */

#ifndef NCHORDER_USB_H
#define NCHORDER_USB_H

#include <stdint.h>
#include <stdbool.h>

/**
 * Initialize USB HID subsystem
 * Sets up USB device with HID keyboard class
 *
 * @return 0 on success, error code on failure
 */
uint32_t nchorder_usb_init(void);

/**
 * Check if USB is connected and ready
 *
 * @return true if USB is connected and enumerated
 */
bool nchorder_usb_is_connected(void);

/**
 * Send keyboard key press via USB HID
 *
 * @param modifiers HID modifier byte (Ctrl, Shift, Alt, GUI)
 * @param keycode   HID keycode (0x04 = 'a', etc.)
 * @return 0 on success, error code on failure
 */
uint32_t nchorder_usb_key_press(uint8_t modifiers, uint8_t keycode);

/**
 * Send keyboard key release via USB HID
 *
 * @return 0 on success, error code on failure
 */
uint32_t nchorder_usb_key_release(void);

/**
 * Process USB events
 * Call this from main loop
 */
void nchorder_usb_process(void);

#endif // NCHORDER_USB_H
