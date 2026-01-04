/**
 * Northern Chorder - Twiddler 4 Board Configuration
 *
 * Pin mappings for the actual Twiddler 4 hardware.
 * All 19 GPIO mappings verified by continuity testing after desoldering E73 module.
 */

#ifndef BOARD_TWIDDLER4_H
#define BOARD_TWIDDLER4_H

// ============================================================================
// BUTTON CONFIGURATION (must be before nrf_gpio.h for dependency order)
// ============================================================================
// Architecture: Direct GPIO (NOT matrix scanning)
// 19 buttons traced: 4 thumb + 5 finger rows (F0-F4) x 3 columns
// Active-low: pressed = 0, released = 1
// Pin mappings verified by continuity testing after desoldering E73 module (Jan 2026)

#define BOARD_NUM_BUTTONS         22  // 5 thumb + 5x3 finger (F0-F4) + 2 expansion

#include "nrf_gpio.h"

// ============================================================================
// DEVICE IDENTIFICATION
// ============================================================================
#define BOARD_NAME                "Twiddler4"
#define BOARD_MANUFACTURER        "Tek Gear"
#define BOARD_MODEL               "4"

// Thumb buttons
#define PIN_BTN_T0      NRF_GPIO_PIN_MAP(0, 29)  // P0.29 - empirically verified
#define PIN_BTN_T1      NRF_GPIO_PIN_MAP(0, 0)   // P0.00 (E73 pin 11)
#define PIN_BTN_T2      NRF_GPIO_PIN_MAP(0, 4)   // P0.04 (E73 pin 18)
#define PIN_BTN_T3      NRF_GPIO_PIN_MAP(0, 8)   // P0.08 (E73 pin 16)
#define PIN_BTN_T4      NRF_GPIO_PIN_MAP(0, 13)  // P0.13 (E73 pin 33)

// Finger Row 0 (mouse buttons)
#define PIN_BTN_F0L     NRF_GPIO_PIN_MAP(1, 0)   // P1.00 (E73 pin 36)
#define PIN_BTN_F0M     NRF_GPIO_PIN_MAP(0, 24)  // P0.24 (E73 pin 35)
#define PIN_BTN_F0R     NRF_GPIO_PIN_MAP(0, 26)  // P0.26 (E73 pin 12)

// Finger Row 1 (index finger)
#define PIN_BTN_F1L     NRF_GPIO_PIN_MAP(0, 3)   // P0.03 (E73 pin 3)
#define PIN_BTN_F1M     NRF_GPIO_PIN_MAP(0, 2)   // P0.02 (E73 pin 7)
#define PIN_BTN_F1R     NRF_GPIO_PIN_MAP(0, 1)   // P0.01 (E73 pin 13)

// Finger Row 2 (middle finger)
#define PIN_BTN_F2L     NRF_GPIO_PIN_MAP(0, 7)   // P0.07 (E73 pin 22)
#define PIN_BTN_F2M     NRF_GPIO_PIN_MAP(0, 6)   // P0.06 (E73 pin 14)
#define PIN_BTN_F2R     NRF_GPIO_PIN_MAP(0, 5)   // P0.05 (E73 pin 15)

// Finger Row 3 (ring finger)
#define PIN_BTN_F3L     NRF_GPIO_PIN_MAP(0, 12)  // P0.12 (E73 pin 20)
#define PIN_BTN_F3M     NRF_GPIO_PIN_MAP(0, 10)  // P0.10 (E73 pin 43)
#define PIN_BTN_F3R     NRF_GPIO_PIN_MAP(0, 9)   // P0.09 (E73 pin 41)

// Finger Row 4 (pinky) - CORRECTED via empirical GPIO testing (Jan 2026)
#define PIN_BTN_F4L     NRF_GPIO_PIN_MAP(0, 15)  // P0.15 - empirically verified
#define PIN_BTN_F4M     NRF_GPIO_PIN_MAP(0, 20)  // P0.20 - empirically verified
#define PIN_BTN_F4R     NRF_GPIO_PIN_MAP(0, 17)  // P0.17 - empirically verified

// Expansion GPIOs on J3 header (active-low, directly accessible for bodge wires)
#define PIN_BTN_EXT1    NRF_GPIO_PIN_MAP(0, 28)  // P0.28 (E73 pin 4, J3) - can bodge to F0L
#define PIN_BTN_EXT2    NRF_GPIO_PIN_MAP(1, 9)   // P1.09 (E73 pin 17, J3) - spare expansion

// Button pin array for iteration (indexed by bitmask position)
// 22-button layout: T0-T4 + F0-F4 rows + 2 expansion GPIOs
#define BUTTON_PINS { \
    PIN_BTN_T1,  PIN_BTN_F1L, PIN_BTN_F1M, PIN_BTN_F1R, \
    PIN_BTN_T2,  PIN_BTN_F2L, PIN_BTN_F2M, PIN_BTN_F2R, \
    PIN_BTN_T3,  PIN_BTN_F3L, PIN_BTN_F3M, PIN_BTN_F3R, \
    PIN_BTN_T4,  PIN_BTN_F4L, PIN_BTN_F4M, PIN_BTN_F4R, \
    PIN_BTN_F0L, PIN_BTN_F0M, PIN_BTN_F0R, PIN_BTN_T0,  \
    PIN_BTN_EXT1, PIN_BTN_EXT2 \
}

// ============================================================================
// I2C BUS (TWI0) - General purpose I2C on J3 header
// ============================================================================
#define PIN_I2C_SDA       NRF_GPIO_PIN_MAP(0, 30)  // J3 pin 3
#define PIN_I2C_SCL       NRF_GPIO_PIN_MAP(0, 31)  // J3 pin 2

// ============================================================================
// OPTICAL THUMB SENSOR - SPI interface (FFC J6 to thumb board)
// ============================================================================
// I2C scan found no devices - likely SPI protocol
// Pinout via FFC: P0.29 (CS?), P0.30 (MOSI?), P0.31 (SCK?), P1.11 (MISO?)
// TODO: Identify sensor chip and implement driver
#define PIN_SENSOR_CS     NRF_GPIO_PIN_MAP(0, 29)  // E73 pin 8 - chip select (unverified)
#define PIN_SENSOR_CLK    NRF_GPIO_PIN_MAP(0, 31)  // E73 pin 9 - SPI clock (unverified)
#define PIN_SENSOR_MOSI   NRF_GPIO_PIN_MAP(0, 30)  // E73 pin 10 - data out (unverified)
#define PIN_SENSOR_MISO   NRF_GPIO_PIN_MAP(1, 11)  // Via FFC - data in (unverified)

// ============================================================================
// LED PINS - WS2812/SK6812 addressable RGB strip (3 LEDs)
// ============================================================================
// Power controlled via Q1 transistor on P1.10, data on P1.13
#define PIN_LED_POWER     NRF_GPIO_PIN_MAP(1, 10)  // P1.10 (E73 pin 2) - Q1 power enable
#define PIN_LED_DATA      NRF_GPIO_PIN_MAP(1, 13)  // P1.13 (E73 pin 6) - WS2812 data
#define PIN_LED_STATUS    PIN_LED_DATA             // Alias for compatibility
#define LED_COUNT         3                        // 3 RGB LEDs on strip

// ============================================================================
// I2C MUX (not present on Twiddler4 - stub definitions for compile)
// ============================================================================
#define PIN_MUX_RESET     0xFF                     // Not connected
#define I2C_ADDR_MUX      0x70                     // Dummy address (not used)

#endif // BOARD_TWIDDLER4_H
