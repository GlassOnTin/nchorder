/**
 * Twiddler 4 Custom Firmware - Board Configuration
 *
 * Pin mappings and hardware configuration for nRF52840
 * Based on reverse engineering of official firmware v3.8.0
 *
 * GPIO Mapping Source:
 * - Hardware probing: 5 buttons confirmed via multimeter
 * - Firmware analysis: 16-pin array at 0x17C56
 * - Pattern recognition: finger rows use descending pin order (L=high, M=mid, R=low)
 */

#ifndef NCHORDER_CONFIG_H
#define NCHORDER_CONFIG_H

#include "nrf_gpio.h"

// Device identification
#define NCHORDER_MANUFACTURER     "Tek Gear"
#define NCHORDER_DEVICE_NAME      "Twiddler"
#define NCHORDER_MODEL_NUMBER     "4"
#define NCHORDER_FW_VERSION       "custom-0.2"

// ============================================================================
// BUTTON CONFIGURATION
// ============================================================================
// Architecture: Direct GPIO (NOT matrix scanning)
// All 16 buttons on GPIO Port 0 with internal pull-ups
// Active-low: pressed = 0, released = 1

#define NCHORDER_NUM_THUMB_BUTTONS    4
#define NCHORDER_NUM_FINGER_COLUMNS   3
#define NCHORDER_NUM_FINGER_ROWS      4
#define NCHORDER_TOTAL_BUTTONS        16

// Button bitmask indices (matches config file format)
// Interleaved: T1, F1L, F1M, F1R, T2, F2L, F2M, F2R, ...
#define BTN_T1    0    // Thumb 1 - Num
#define BTN_F1L   1    // Finger Row 1 - Left
#define BTN_F1M   2    // Finger Row 1 - Middle
#define BTN_F1R   3    // Finger Row 1 - Right
#define BTN_T2    4    // Thumb 2 - Alt
#define BTN_F2L   5    // Finger Row 2 - Left
#define BTN_F2M   6    // Finger Row 2 - Middle
#define BTN_F2R   7    // Finger Row 2 - Right
#define BTN_T3    8    // Thumb 3 - Ctrl/Enter
#define BTN_F3L   9    // Finger Row 3 - Left
#define BTN_F3M   10   // Finger Row 3 - Middle
#define BTN_F3R   11   // Finger Row 3 - Right
#define BTN_T4    12   // Thumb 4 - Shift/Space
#define BTN_F4L   13   // Finger Row 4 - Left
#define BTN_F4M   14   // Finger Row 4 - Middle
#define BTN_F4R   15   // Finger Row 4 - Right

// ============================================================================
// GPIO PIN ASSIGNMENTS - All on Port 0
// ============================================================================
// All 16 mappings verified by continuity testing after desoldering E73 module

// Thumb buttons
#define PIN_BTN_T1      NRF_GPIO_PIN_MAP(0, 0)   // P0.00 (E73 pin 33)
#define PIN_BTN_T2      NRF_GPIO_PIN_MAP(0, 4)   // P0.04 (E73 pin 40)
#define PIN_BTN_T3      NRF_GPIO_PIN_MAP(0, 8)   // P0.08 (E73 pin 38)
#define PIN_BTN_T4      NRF_GPIO_PIN_MAP(0, 13)  // P0.13 (E73 pin 54)

// Finger Row 1 (index finger)
#define PIN_BTN_F1L     NRF_GPIO_PIN_MAP(0, 3)   // P0.03 (E73 pin 25)
#define PIN_BTN_F1M     NRF_GPIO_PIN_MAP(0, 2)   // P0.02 (E73 pin 29)
#define PIN_BTN_F1R     NRF_GPIO_PIN_MAP(0, 1)   // P0.01 (E73 pin 35)

// Finger Row 2 (middle finger)
#define PIN_BTN_F2L     NRF_GPIO_PIN_MAP(0, 7)   // P0.07 (E73 pin 22)
#define PIN_BTN_F2M     NRF_GPIO_PIN_MAP(0, 6)   // P0.06 (E73 pin 36)
#define PIN_BTN_F2R     NRF_GPIO_PIN_MAP(0, 5)   // P0.05 (E73 pin 37)

// Finger Row 3 (ring finger)
#define PIN_BTN_F3L     NRF_GPIO_PIN_MAP(0, 12)  // P0.12 (E73 pin 42)
#define PIN_BTN_F3M     NRF_GPIO_PIN_MAP(0, 10)  // P0.10 (E73 pin 64)
#define PIN_BTN_F3R     NRF_GPIO_PIN_MAP(0, 9)   // P0.09 (E73 pin 62)

// Finger Row 4 (pinky)
#define PIN_BTN_F4L     NRF_GPIO_PIN_MAP(0, 20)  // P0.20 (E73 pin 53)
#define PIN_BTN_F4M     NRF_GPIO_PIN_MAP(0, 17)  // P0.17 (E73 pin 51)
#define PIN_BTN_F4R     NRF_GPIO_PIN_MAP(0, 15)  // P0.15 (E73 pin 49)

// Button pin array for iteration (indexed by bitmask position)
#define BUTTON_PINS { \
    PIN_BTN_T1,  PIN_BTN_F1L, PIN_BTN_F1M, PIN_BTN_F1R, \
    PIN_BTN_T2,  PIN_BTN_F2L, PIN_BTN_F2M, PIN_BTN_F2R, \
    PIN_BTN_T3,  PIN_BTN_F3L, PIN_BTN_F3M, PIN_BTN_F3R, \
    PIN_BTN_T4,  PIN_BTN_F4L, PIN_BTN_F4M, PIN_BTN_F4R  \
}

// ============================================================================
// I2C BUS (TWI0) - Touchpad and LEDs
// ============================================================================
// Non-standard pins: P0.27 not exposed on E73 module

#define PIN_I2C_SDA       NRF_GPIO_PIN_MAP(0, 30)  // J3 pin 3
#define PIN_I2C_SCL       NRF_GPIO_PIN_MAP(0, 31)  // J3 pin 2
#define TOUCHPAD_I2C_ADDR 0x74                     // Azoteq IQS5xx

// ============================================================================
// LED PINS
// ============================================================================
// RGB LEDs (L1-L3) on thumb board: WS2812B/SK6812 addressable LEDs
// Driven via I2S peripheral, single data wire through FFC J6
// Main board has single status LED (D1)

// LED data pin - PLACEHOLDER, needs verification via hardware probing
// This pin routes through FFC J6 to the thumb board LEDs
// TODO: Verify correct pin by probing FFC during LED animation
#define PIN_LED_DATA      NRF_GPIO_PIN_MAP(0, 26)  // [?] Placeholder - verify!

#define PIN_LED_STATUS    NRF_GPIO_PIN_MAP(0, 11)  // [?] Main board LED D1 (placeholder)

// Timing configuration
#define CHORD_DEBOUNCE_MS         10
#define CHORD_RELEASE_DELAY_MS    50

// HID report configuration
#define HID_KEYBOARD_REPORT_ID    1
#define HID_CONSUMER_REPORT_ID    2
#define HID_MAX_KEYCODES          3  // 3-key rollover (matches original)

#endif // NCHORDER_CONFIG_H
