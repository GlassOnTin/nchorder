/**
 * Twiddler 4 Custom Firmware - HID Report Handling
 *
 * BLE HID keyboard and mouse report management
 */

#ifndef NCHORDER_HID_H
#define NCHORDER_HID_H

#include <stdint.h>
#include <stdbool.h>
#include "nchorder_config.h"

// HID modifier key bits
#define HID_MOD_LCTRL   0x01
#define HID_MOD_LSHIFT  0x02
#define HID_MOD_LALT    0x04
#define HID_MOD_LGUI    0x08
#define HID_MOD_RCTRL   0x10
#define HID_MOD_RSHIFT  0x20
#define HID_MOD_RALT    0x40
#define HID_MOD_RGUI    0x80

// Common HID keycodes
#define HID_KEY_NONE        0x00
#define HID_KEY_A           0x04
#define HID_KEY_B           0x05
#define HID_KEY_C           0x06
#define HID_KEY_D           0x07
#define HID_KEY_E           0x08
#define HID_KEY_F           0x09
#define HID_KEY_G           0x0A
#define HID_KEY_H           0x0B
#define HID_KEY_I           0x0C
#define HID_KEY_J           0x0D
#define HID_KEY_K           0x0E
#define HID_KEY_L           0x0F
#define HID_KEY_M           0x10
#define HID_KEY_N           0x11
#define HID_KEY_O           0x12
#define HID_KEY_P           0x13
#define HID_KEY_Q           0x14
#define HID_KEY_R           0x15
#define HID_KEY_S           0x16
#define HID_KEY_T           0x17
#define HID_KEY_U           0x18
#define HID_KEY_V           0x19
#define HID_KEY_W           0x1A
#define HID_KEY_X           0x1B
#define HID_KEY_Y           0x1C
#define HID_KEY_Z           0x1D
#define HID_KEY_1           0x1E
#define HID_KEY_2           0x1F
#define HID_KEY_3           0x20
#define HID_KEY_4           0x21
#define HID_KEY_5           0x22
#define HID_KEY_6           0x23
#define HID_KEY_7           0x24
#define HID_KEY_8           0x25
#define HID_KEY_9           0x26
#define HID_KEY_0           0x27
#define HID_KEY_ENTER       0x28
#define HID_KEY_ESC         0x29
#define HID_KEY_BACKSPACE   0x2A
#define HID_KEY_TAB         0x2B
#define HID_KEY_SPACE       0x2C
#define HID_KEY_MINUS       0x2D
#define HID_KEY_EQUAL       0x2E
#define HID_KEY_LBRACKET    0x2F
#define HID_KEY_RBRACKET    0x30
#define HID_KEY_BACKSLASH   0x31
#define HID_KEY_SEMICOLON   0x33
#define HID_KEY_QUOTE       0x34
#define HID_KEY_GRAVE       0x35
#define HID_KEY_COMMA       0x36
#define HID_KEY_PERIOD      0x37
#define HID_KEY_SLASH       0x38
#define HID_KEY_CAPSLOCK    0x39
#define HID_KEY_F1          0x3A
#define HID_KEY_F2          0x3B
#define HID_KEY_F3          0x3C
#define HID_KEY_F4          0x3D
#define HID_KEY_F5          0x3E
#define HID_KEY_F6          0x3F
#define HID_KEY_F7          0x40
#define HID_KEY_F8          0x41
#define HID_KEY_F9          0x42
#define HID_KEY_F10         0x43
#define HID_KEY_F11         0x44
#define HID_KEY_F12         0x45
#define HID_KEY_DELETE      0x4C
#define HID_KEY_RIGHT       0x4F
#define HID_KEY_LEFT        0x50
#define HID_KEY_DOWN        0x51
#define HID_KEY_UP          0x52
#define HID_KEY_HOME        0x4A
#define HID_KEY_END         0x4D
#define HID_KEY_PAGEUP      0x4B
#define HID_KEY_PAGEDOWN    0x4E

// Consumer control codes
#define HID_CONSUMER_NONE           0x0000
#define HID_CONSUMER_PLAY_PAUSE     0x00CD
#define HID_CONSUMER_STOP           0x00B7
#define HID_CONSUMER_SCAN_NEXT      0x00B5
#define HID_CONSUMER_SCAN_PREV      0x00B6
#define HID_CONSUMER_VOLUME_UP      0x00E9
#define HID_CONSUMER_VOLUME_DOWN    0x00EA
#define HID_CONSUMER_MUTE           0x00E2
#define HID_CONSUMER_BRIGHTNESS_UP  0x006F
#define HID_CONSUMER_BRIGHTNESS_DN  0x0070

// Keyboard report structure (matches original Twiddler)
typedef struct {
    uint8_t modifiers;              // Modifier key bits
    uint8_t reserved;               // Reserved byte
    uint8_t keycodes[HID_MAX_KEYCODES];  // Currently pressed keycodes
} __attribute__((packed)) keyboard_report_t;

// Consumer control report structure
typedef struct {
    uint16_t consumer_code;         // Consumer control code
} __attribute__((packed)) consumer_report_t;

// Mouse report structure
typedef struct {
    uint8_t buttons;                // Button bits
    int8_t x;                       // X movement
    int8_t y;                       // Y movement
    int8_t wheel;                   // Wheel movement
} __attribute__((packed)) mouse_report_t;

// Initialize HID subsystem
void nchorder_hid_init(void);

// Send keyboard key press
// Returns 0 on success, error code on failure
uint32_t nchorder_hid_key_press(uint8_t modifiers, uint8_t keycode);

// Send keyboard key release (all keys up)
uint32_t nchorder_hid_key_release(void);

// Send consumer control code
uint32_t nchorder_hid_consumer_press(uint16_t consumer_code);

// Send consumer control release
uint32_t nchorder_hid_consumer_release(void);

// Send mouse report
uint32_t nchorder_hid_mouse_report(uint8_t buttons, int8_t x, int8_t y, int8_t wheel);

// Check if HID is ready to send
bool nchorder_hid_is_ready(void);

// Process any pending HID transmissions
// Call this from main loop or event handler
void nchorder_hid_process(void);

#endif // NCHORDER_HID_H
