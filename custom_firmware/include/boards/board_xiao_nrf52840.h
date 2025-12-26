/**
 * Northern Chorder - Seeed XIAO nRF52840 Board Configuration
 *
 * Pin mappings for XIAO nRF52840-Plus with Trill capacitive sensors.
 * Uses I2C multiplexer (PCA9548) to address 4 Trill sensors.
 *
 * Hardware:
 *   - Seeed XIAO nRF52840-Plus (2886:8045)
 *   - Adafruit PCA9548 I2C Mux at 0x70
 *   - Trill Square on mux ch0 (thumb)
 *   - Trill Bar x3 on mux ch1-3 (finger rows)
 *
 * Pin mapping source: https://docs.zephyrproject.org/latest/build/dts/api/bindings/gpio/seeed-xiao-header.html
 */

#ifndef BOARD_XIAO_NRF52840_H
#define BOARD_XIAO_NRF52840_H

#include "nrf_gpio.h"

// ============================================================================
// DEVICE IDENTIFICATION
// ============================================================================
#define BOARD_NAME                "nChorder-XIAO"
#define BOARD_MANUFACTURER        "Northern Chorder"
#define BOARD_MODEL               "xiao-nrf52840"

// ============================================================================
// INPUT DRIVER SELECTION
// ============================================================================
// Use Trill capacitive sensors instead of GPIO buttons
#define BUTTON_DRIVER_TRILL       1

// ============================================================================
// I2C BUS CONFIGURATION
// ============================================================================
// XIAO Arduino pin mapping to nRF52840 GPIO:
//   D4 = P0.04 (I2C SDA)
//   D5 = P0.05 (I2C SCL)
//   D6 = P1.11 (MUX Reset)

#define PIN_I2C_SDA               NRF_GPIO_PIN_MAP(0, 4)   // D4 = P0.04
#define PIN_I2C_SCL               NRF_GPIO_PIN_MAP(0, 5)   // D5 = P0.05
#define PIN_MUX_RESET             NRF_GPIO_PIN_MAP(1, 11)  // D6 = P1.11

// I2C frequency
#define I2C_FREQUENCY             NRF_TWIM_FREQ_400K

// ============================================================================
// I2C DEVICE ADDRESSES
// ============================================================================
#define I2C_ADDR_MUX              0x70  // PCA9548 I2C multiplexer
#define I2C_ADDR_TRILL            0x20  // All Trill sensors (selected via mux)

// ============================================================================
// I2C MUX CHANNEL ASSIGNMENTS
// ============================================================================
// Wiring: 3 Trill Bars oriented as columns (L/M/R), not rows
//
//         Bar 1 (L)    Bar 2 (M)    Bar 3 (R)
//         ch1          ch2          ch3
// Zone 0: F1L          F1M          F1R    ← Index finger
// Zone 1: F2L          F2M          F2R    ← Middle finger
// Zone 2: F3L          F3M          F3R    ← Ring finger
// Zone 3: F4L          F4M          F4R    ← Pinky finger

#define MUX_CH_THUMB              0     // Trill Square - thumb control
#define MUX_CH_COL_L              1     // Trill Bar 1 - Left column
#define MUX_CH_COL_M              2     // Trill Bar 2 - Middle column
#define MUX_CH_COL_R              3     // Trill Bar 3 - Right column

#define MUX_NUM_CHANNELS          4     // Total sensors

// ============================================================================
// TRILL SENSOR CONFIGURATION
// ============================================================================
// Trill Bar: 26 electrodes, position range 0-3200 in centroid mode
// Divided into 4 zones for finger rows (index, middle, ring, pinky)

#define TRILL_BAR_POS_MAX         3200

// Zone boundaries (4 fingers per bar column)
#define TRILL_ZONE_0_START        0
#define TRILL_ZONE_0_END          800     // Index finger
#define TRILL_ZONE_1_START        800
#define TRILL_ZONE_1_END          1600    // Middle finger
#define TRILL_ZONE_2_START        1600
#define TRILL_ZONE_2_END          2400    // Ring finger
#define TRILL_ZONE_3_START        2400
#define TRILL_ZONE_3_END          3200    // Pinky finger

// Trill Square: 2D touch surface for thumb
// Divided into 4 quadrants for thumb buttons T1-T4
#define TRILL_SQUARE_CENTER       1600  // Midpoint for quadrant detection

// Touch size threshold (filter out light/accidental touches)
#define TRILL_TOUCH_SIZE_MIN      100

// ============================================================================
// BUTTON MAPPING
// ============================================================================
// Total 16 buttons to match Twiddler 4 layout:
//   4 thumb buttons (T1-T4) from Trill Square quadrants
//   12 finger buttons from 3 Trill Bars (columns) x 4 zones (finger rows)
//
// Column-oriented mapping:
//   Bar 1 (ch1) zones 0-3 → F1L, F2L, F3L, F4L (Left column)
//   Bar 2 (ch2) zones 0-3 → F1M, F2M, F3M, F4M (Middle column)
//   Bar 3 (ch3) zones 0-3 → F1R, F2R, F3R, F4R (Right column)

#define BOARD_NUM_BUTTONS         16

// Button bitmask positions (same as Twiddler 4 for chord compatibility):
//   Bit 0:  T1  (thumb)      Bit 4:  T2  (thumb)      Bit 8:  T3  (thumb)      Bit 12: T4  (thumb)
//   Bit 1:  F1L (index)      Bit 5:  F2L (middle)     Bit 9:  F3L (ring)       Bit 13: F4L (pinky)
//   Bit 2:  F1M (index)      Bit 6:  F2M (middle)     Bit 10: F3M (ring)       Bit 14: F4M (pinky)
//   Bit 3:  F1R (index)      Bit 7:  F2R (middle)     Bit 11: F3R (ring)       Bit 15: F4R (pinky)

// Thumb button indices (from Trill Square quadrants)
#define BTN_T1                    0   // Top-left quadrant
#define BTN_T2                    4   // Top-right quadrant
#define BTN_T3                    8   // Bottom-left quadrant
#define BTN_T4                    12  // Bottom-right quadrant

// Finger row 1 (index) - from Trill Bar 1
#define BTN_F1L                   1   // Zone 0
#define BTN_F1M                   2   // Zone 1
#define BTN_F1R                   3   // Zone 3 (skip zone 2)

// Finger row 2 (middle) - from Trill Bar 2
#define BTN_F2L                   5   // Zone 0
#define BTN_F2M                   6   // Zone 1
#define BTN_F2R                   7   // Zone 3

// Finger row 3 (ring) - from Trill Bar 3 zones 0-1
#define BTN_F3L                   9   // Zone 0
#define BTN_F3M                   10  // Zone 1
#define BTN_F3R                   11  // Zone 2

// Finger row 4 (pinky) - from Trill Bar 3 zone 3 (shared bar)
// NOTE: With only 3 Trill Bars, row 4 shares Bar 3 with row 3
// This limits combinations but matches physical layout
#define BTN_F4L                   13  // Zone 0 (same as F3L - cannot chord together)
#define BTN_F4M                   14  // Zone 1 (same as F3M)
#define BTN_F4R                   15  // Zone 3

// ============================================================================
// GPIO PIN DEFINITIONS (for compatibility - not used with Trill driver)
// ============================================================================
// Dummy definitions so existing code that references BUTTON_PINS compiles
#define PIN_UNUSED                NRF_GPIO_PIN_MAP(1, 15)

#define PIN_BTN_T1                PIN_UNUSED
#define PIN_BTN_T2                PIN_UNUSED
#define PIN_BTN_T3                PIN_UNUSED
#define PIN_BTN_T4                PIN_UNUSED
#define PIN_BTN_F1L               PIN_UNUSED
#define PIN_BTN_F1M               PIN_UNUSED
#define PIN_BTN_F1R               PIN_UNUSED
#define PIN_BTN_F2L               PIN_UNUSED
#define PIN_BTN_F2M               PIN_UNUSED
#define PIN_BTN_F2R               PIN_UNUSED
#define PIN_BTN_F3L               PIN_UNUSED
#define PIN_BTN_F3M               PIN_UNUSED
#define PIN_BTN_F3R               PIN_UNUSED
#define PIN_BTN_F4L               PIN_UNUSED
#define PIN_BTN_F4M               PIN_UNUSED
#define PIN_BTN_F4R               PIN_UNUSED

#define BUTTON_PINS { \
    PIN_BTN_T1,  PIN_BTN_F1L, PIN_BTN_F1M, PIN_BTN_F1R, \
    PIN_BTN_T2,  PIN_BTN_F2L, PIN_BTN_F2M, PIN_BTN_F2R, \
    PIN_BTN_T3,  PIN_BTN_F3L, PIN_BTN_F3M, PIN_BTN_F3R, \
    PIN_BTN_T4,  PIN_BTN_F4L, PIN_BTN_F4M, PIN_BTN_F4R  \
}

// ============================================================================
// LED CONFIGURATION
// ============================================================================
// XIAO has a built-in LED, but we'll primarily use RTT for debug output
// The XIAO doesn't have WS2812 LEDs like Twiddler 4

#define PIN_LED_STATUS            NRF_GPIO_PIN_MAP(0, 26)  // Built-in LED (active low)
#define PIN_LED_DATA              PIN_UNUSED               // No addressable LEDs

// ============================================================================
// USB CONFIGURATION
// ============================================================================
// XIAO nRF52840 has native USB support
// USB VID/PID from device (2886:8045 for Seeed XIAO nRF52840)

#define USB_VID                   0x2886
#define USB_PID                   0x8045

#endif // BOARD_XIAO_NRF52840_H
