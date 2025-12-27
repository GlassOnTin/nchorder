/**
 * Northern Chorder - Twiddler 4 Board Configuration
 *
 * Pin mappings for the actual Twiddler 4 hardware.
 * All 16 GPIO mappings verified by continuity testing after desoldering E73 module.
 */

#ifndef BOARD_TWIDDLER4_H
#define BOARD_TWIDDLER4_H

#include "nrf_gpio.h"

// ============================================================================
// DEVICE IDENTIFICATION
// ============================================================================
#define BOARD_NAME                "Twiddler4"
#define BOARD_MANUFACTURER        "Tek Gear"
#define BOARD_MODEL               "4"

// ============================================================================
// BUTTON CONFIGURATION
// ============================================================================
// Architecture: Direct GPIO (NOT matrix scanning)
// All 16 buttons on GPIO Port 0 with internal pull-ups
// Active-low: pressed = 0, released = 1

#define BOARD_NUM_BUTTONS         16

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
// I2C BUS (TWI0) - Optical sensor and LEDs
// ============================================================================
#define PIN_I2C_SDA       NRF_GPIO_PIN_MAP(0, 30)  // J3 pin 3
#define PIN_I2C_SCL       NRF_GPIO_PIN_MAP(0, 31)  // J3 pin 2
#define TOUCHPAD_I2C_ADDR 0x74                     // Optical sensor (unidentified)

// ============================================================================
// LED PINS
// ============================================================================
// RGB LEDs (L1-L3) on thumb board: WS2812B/SK6812 addressable LEDs
#define PIN_LED_DATA      NRF_GPIO_PIN_MAP(0, 26)  // Placeholder - verify!
#define PIN_LED_STATUS    NRF_GPIO_PIN_MAP(0, 11)  // Main board LED D1 (placeholder)

#endif // BOARD_TWIDDLER4_H
