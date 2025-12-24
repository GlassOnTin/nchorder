/**
 * Northern Chorder - nRF52840-DK (pca10056) Board Configuration
 *
 * Pin mappings for testing on Nordic's development kit.
 * DK has only 4 buttons, mapped to thumb buttons T1-T4 for testing.
 * Finger buttons are disabled (set to unused pin).
 */

#ifndef BOARD_PCA10056_H
#define BOARD_PCA10056_H

#include "nrf_gpio.h"

// ============================================================================
// DEVICE IDENTIFICATION
// ============================================================================
#define BOARD_NAME                "Twiddler4-DK"
#define BOARD_MANUFACTURER        "Nordic Semiconductor"
#define BOARD_MODEL               "pca10056"

// ============================================================================
// BUTTON CONFIGURATION
// ============================================================================
// DK has 4 buttons on pins 11, 12, 24, 25
// We map these to thumb buttons only; finger buttons disabled

#define BOARD_NUM_BUTTONS         16  // Still 16 for compatibility, but only 4 active

// DK button pins (directly from pca10056.h)
#define DK_BUTTON_1               11
#define DK_BUTTON_2               12
#define DK_BUTTON_3               24
#define DK_BUTTON_4               25

// Pin that's guaranteed not to conflict (unused on DK)
#define PIN_UNUSED                NRF_GPIO_PIN_MAP(1, 15)

// Thumb buttons - mapped to DK buttons
#define PIN_BTN_T1      NRF_GPIO_PIN_MAP(0, DK_BUTTON_1)  // SW1
#define PIN_BTN_T2      NRF_GPIO_PIN_MAP(0, DK_BUTTON_2)  // SW2
#define PIN_BTN_T3      NRF_GPIO_PIN_MAP(0, DK_BUTTON_3)  // SW3
#define PIN_BTN_T4      NRF_GPIO_PIN_MAP(0, DK_BUTTON_4)  // SW4

// Finger buttons - disabled (use unused pin)
#define PIN_BTN_F1L     PIN_UNUSED
#define PIN_BTN_F1M     PIN_UNUSED
#define PIN_BTN_F1R     PIN_UNUSED
#define PIN_BTN_F2L     PIN_UNUSED
#define PIN_BTN_F2M     PIN_UNUSED
#define PIN_BTN_F2R     PIN_UNUSED
#define PIN_BTN_F3L     PIN_UNUSED
#define PIN_BTN_F3M     PIN_UNUSED
#define PIN_BTN_F3R     PIN_UNUSED
#define PIN_BTN_F4L     PIN_UNUSED
#define PIN_BTN_F4M     PIN_UNUSED
#define PIN_BTN_F4R     PIN_UNUSED

// Button pin array for iteration (indexed by bitmask position)
#define BUTTON_PINS { \
    PIN_BTN_T1,  PIN_BTN_F1L, PIN_BTN_F1M, PIN_BTN_F1R, \
    PIN_BTN_T2,  PIN_BTN_F2L, PIN_BTN_F2M, PIN_BTN_F2R, \
    PIN_BTN_T3,  PIN_BTN_F3L, PIN_BTN_F3M, PIN_BTN_F3R, \
    PIN_BTN_T4,  PIN_BTN_F4L, PIN_BTN_F4M, PIN_BTN_F4R  \
}

// ============================================================================
// I2C BUS - Not connected on DK (no touchpad)
// ============================================================================
#define PIN_I2C_SDA       NRF_GPIO_PIN_MAP(0, 26)  // Arduino header
#define PIN_I2C_SCL       NRF_GPIO_PIN_MAP(0, 27)  // Arduino header
#define TOUCHPAD_I2C_ADDR 0x74                     // Placeholder

// ============================================================================
// LED PINS
// ============================================================================
// DK has 4 LEDs on pins 13, 14, 15, 16
#define DK_LED_1                  13
#define DK_LED_2                  14
#define DK_LED_3                  15
#define DK_LED_4                  16

// Use LED1 for status, LED2-4 as RGB substitute (active low on DK)
#define PIN_LED_STATUS    NRF_GPIO_PIN_MAP(0, DK_LED_1)
#define PIN_LED_DATA      NRF_GPIO_PIN_MAP(0, DK_LED_2)  // Won't work for WS2812, but won't crash

// DK LEDs are directly driven (not WS2812), so LED driver will be non-functional
// This is expected - we just want BLE/USB to work for testing

#endif // BOARD_PCA10056_H
