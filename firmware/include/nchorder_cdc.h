/**
 * Northern Chorder - USB CDC Protocol
 *
 * Provides serial communication for configuration app.
 * Runs alongside HID keyboard as composite USB device.
 */

#ifndef NCHORDER_CDC_H
#define NCHORDER_CDC_H

#include <stdint.h>
#include <stdbool.h>

// Protocol version
#define CDC_PROTOCOL_VERSION_MAJOR  1
#define CDC_PROTOCOL_VERSION_MINOR  0

// Command codes
#define CDC_CMD_GET_VERSION     0x01
#define CDC_CMD_GET_TOUCHES     0x02
#define CDC_CMD_GET_CONFIG      0x03
#define CDC_CMD_SET_CONFIG      0x04
#define CDC_CMD_GET_CHORDS      0x05
#define CDC_CMD_SET_CHORDS      0x06
#define CDC_CMD_SAVE_FLASH      0x07
#define CDC_CMD_LOAD_FLASH      0x08
#define CDC_CMD_RESET_DEFAULT   0x09
#define CDC_CMD_STREAM_START    0x10
#define CDC_CMD_STREAM_STOP     0x11

// Config upload commands (chunked transfer for .cfg files)
#define CDC_CMD_UPLOAD_START    0x12  // [total_size_lo, total_size_hi] -> ACK/NAK
#define CDC_CMD_UPLOAD_DATA     0x13  // [data...] -> ACK/NAK
#define CDC_CMD_UPLOAD_COMMIT   0x14  // [] -> ACK/NAK (parses and activates config)
#define CDC_CMD_UPLOAD_ABORT    0x15  // [] -> ACK (cancels in-progress upload)

// Response codes
#define CDC_RSP_ACK             0x06
#define CDC_RSP_NAK             0x15
#define CDC_RSP_ERROR           0xFF

// Config IDs for SET_CONFIG
#define CDC_CFG_THRESHOLD_PRESS     0x01
#define CDC_CFG_THRESHOLD_RELEASE   0x02
#define CDC_CFG_DEBOUNCE_MS         0x03
#define CDC_CFG_POLL_RATE_MS        0x04
#define CDC_CFG_MOUSE_SPEED         0x05
#define CDC_CFG_MOUSE_ACCEL         0x06
#define CDC_CFG_VOLUME_SENSITIVITY  0x07

// Touch stream sync byte
#define CDC_STREAM_SYNC         0xAA

// Maximum touches per bar sensor
#define CDC_MAX_BAR_TOUCHES     5

// Bar touch data (4 bytes per touch)
typedef struct __attribute__((packed)) {
    uint16_t pos;           // Position (0xFFFF = no touch)
    uint16_t size;          // Touch size/pressure
} cdc_bar_touch_t;

// Touch stream frame (71 bytes) - multitouch support
typedef struct __attribute__((packed)) {
    uint8_t  sync;          // 0xAA
    uint16_t thumb_x;       // Square sensor X (0-1800)
    uint16_t thumb_y;       // Square sensor Y (0-1800)
    uint16_t thumb_size;    // Touch pressure/size
    cdc_bar_touch_t bar0[CDC_MAX_BAR_TOUCHES];  // Left column (5 touches)
    cdc_bar_touch_t bar1[CDC_MAX_BAR_TOUCHES];  // Middle column (5 touches)
    cdc_bar_touch_t bar2[CDC_MAX_BAR_TOUCHES];  // Right column (5 touches)
    uint32_t buttons;       // 32-bit button bitmask (20 buttons used)
} cdc_touch_frame_t;

// Runtime config structure
typedef struct __attribute__((packed)) {
    uint16_t threshold_press;       // Touch detection threshold
    uint16_t threshold_release;     // Release threshold (hysteresis)
    uint16_t debounce_ms;           // Debounce time
    uint16_t poll_rate_ms;          // Sensor poll interval
    uint16_t mouse_speed;           // Mouse movement multiplier
    uint16_t mouse_accel;           // Acceleration curve
    uint16_t volume_sensitivity;    // Volume gesture sensitivity
    uint16_t reserved[4];           // Future use
} cdc_config_t;

// Version response
typedef struct __attribute__((packed)) {
    uint8_t major;
    uint8_t minor;
    uint8_t hw_rev;
} cdc_version_t;

/**
 * Initialize CDC interface
 * Must be called before nchorder_usb_start()
 */
uint32_t nchorder_cdc_init(void);

/**
 * Process CDC data (call from main loop)
 */
void nchorder_cdc_process(void);

/**
 * Check if CDC port is open (DTR set by host)
 */
bool nchorder_cdc_is_open(void);

/**
 * Send touch frame (for streaming mode)
 */
void nchorder_cdc_send_touch_frame(const cdc_touch_frame_t *frame);

/**
 * Get current runtime config
 */
const cdc_config_t* nchorder_cdc_get_config(void);

/**
 * Set a config value by ID
 */
bool nchorder_cdc_set_config(uint8_t config_id, uint16_t value);

/**
 * Check if touch streaming is enabled
 */
bool nchorder_cdc_is_streaming(void);

/**
 * Debug print via CDC (for development)
 */
void nchorder_cdc_debug(const char *fmt, ...);

#endif // NCHORDER_CDC_H
