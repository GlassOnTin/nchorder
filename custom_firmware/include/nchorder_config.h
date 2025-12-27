/**
 * Northern Chorder - Main Configuration
 *
 * This file provides the main configuration by including the appropriate
 * board-specific header and defining common settings.
 *
 * Supported boards:
 * - BOARD_TWIDDLER4:      Actual Twiddler 4 hardware (GPIO buttons)
 * - BOARD_PCA10056:       Nordic nRF52840-DK for testing (GPIO buttons)
 * - BOARD_XIAO_NRF52840:  Seeed XIAO nRF52840 with Trill sensors
 */

#ifndef NCHORDER_CONFIG_H
#define NCHORDER_CONFIG_H

// ============================================================================
// BOARD SELECTION
// ============================================================================
// Define the target board. This can be set in the Makefile via CFLAGS.
// Default to XIAO if not specified.

#if defined(BOARD_XIAO_NRF52840)
    #include "boards/board_xiao_nrf52840.h"
#elif defined(BOARD_IS_DK)
    #include "boards/board_pca10056.h"
#elif defined(BOARD_TWIDDLER4)
    #include "boards/board_twiddler4.h"
#else
    // Default to XIAO for production builds
    #include "boards/board_xiao_nrf52840.h"
#endif

// ============================================================================
// DEVICE IDENTIFICATION (can be overridden by board header)
// ============================================================================
#ifndef BOARD_NAME
#define BOARD_NAME                "Twiddler4"
#endif

#define NCHORDER_MANUFACTURER     "Twiddler Community"
#define NCHORDER_DEVICE_NAME      BOARD_NAME
#define NCHORDER_MODEL_NUMBER     "4"
#define NCHORDER_FW_VERSION       "custom-0.3"

// ============================================================================
// BUTTON LAYOUT CONSTANTS
// ============================================================================
// These are logical constants, not pin assignments

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
// TIMING CONFIGURATION
// ============================================================================

#define CHORD_DEBOUNCE_MS         10
#define CHORD_RELEASE_DELAY_MS    50

// ============================================================================
// HID REPORT CONFIGURATION
// ============================================================================

#define HID_KEYBOARD_REPORT_ID    1
#define HID_CONSUMER_REPORT_ID    2
#define HID_MAX_KEYCODES          3  // 3-key rollover (matches original)

#endif // NCHORDER_CONFIG_H
