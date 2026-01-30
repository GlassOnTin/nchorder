/**
 * Northern Chorder - Trill Capacitive Sensor Driver
 *
 * I2C driver for Bela Trill Bar and Square sensors.
 * Supports reading touch position and size in CENTROID mode.
 *
 * Protocol reference: https://github.com/BelaPlatform/Trill-Arduino
 */

#ifndef NCHORDER_TRILL_H
#define NCHORDER_TRILL_H

#include <stdint.h>
#include <stdbool.h>
#include "sdk_errors.h"

// ============================================================================
// TRILL I2C PROTOCOL CONSTANTS
// ============================================================================

// Command codes (written to offset 0)
#define TRILL_CMD_NONE              0
#define TRILL_CMD_MODE              1
#define TRILL_CMD_SCAN_SETTINGS     2
#define TRILL_CMD_PRESCALER         3
#define TRILL_CMD_NOISE_THRESHOLD   4
#define TRILL_CMD_IDAC              5
#define TRILL_CMD_BASELINE_UPDATE   6
#define TRILL_CMD_MINIMUM_SIZE      7
#define TRILL_CMD_EVENT_MODE        9
#define TRILL_CMD_CHANNEL_MASK_LOW  10
#define TRILL_CMD_CHANNEL_MASK_HIGH 11
#define TRILL_CMD_RESET             12
#define TRILL_CMD_FORMAT            13
#define TRILL_CMD_TIMER_PERIOD      14
#define TRILL_CMD_SCAN_TRIGGER      15
#define TRILL_CMD_AUTO_SCAN         16
#define TRILL_CMD_ACK               254
#define TRILL_CMD_IDENTIFY          255

// Buffer offsets
#define TRILL_OFFSET_COMMAND        0
#define TRILL_OFFSET_DATA           4

// Sensor modes
#define TRILL_MODE_CENTROID         0
#define TRILL_MODE_RAW              1
#define TRILL_MODE_BASELINE         2
#define TRILL_MODE_DIFF             3

// Sensor types (returned by identify)
#define TRILL_TYPE_UNKNOWN          0
#define TRILL_TYPE_BAR              1
#define TRILL_TYPE_SQUARE           2
#define TRILL_TYPE_CRAFT            3
#define TRILL_TYPE_RING             4
#define TRILL_TYPE_HEX              5
#define TRILL_TYPE_FLEX             6

// Default I2C addresses per sensor type
#define TRILL_ADDR_BAR              0x20
#define TRILL_ADDR_SQUARE           0x28
#define TRILL_ADDR_CRAFT            0x30
#define TRILL_ADDR_RING             0x38
#define TRILL_ADDR_HEX              0x40
#define TRILL_ADDR_FLEX             0x48

// Touch data limits
#define TRILL_MAX_TOUCHES_1D        5   // Bar, Ring, etc.
#define TRILL_MAX_TOUCHES_2D        5   // Square, Hex

// Position range
#define TRILL_POS_MAX               3200

// ============================================================================
// DATA STRUCTURES
// ============================================================================

/**
 * Touch data for a single touch point
 */
typedef struct {
    uint16_t position;  // 0-3200 (or 0 if no touch)
    uint16_t size;      // Touch size/pressure proxy
} trill_touch_t;

/**
 * 2D touch data (for Square sensor)
 */
typedef struct {
    uint16_t x;         // Horizontal position
    uint16_t y;         // Vertical position
    uint16_t size;      // Touch size
} trill_touch_2d_t;

/**
 * Sensor state
 */
typedef struct {
    uint8_t i2c_addr;           // I2C address (usually 0x20)
    uint8_t device_type;        // TRILL_TYPE_*
    uint8_t firmware_version;   // Firmware version from identify
    uint8_t num_touches;        // Current number of active touches
    bool initialized;           // Sensor successfully initialized
    bool is_2d;                 // True for Square/Hex (2D sensors)

    // Touch data (1D sensors)
    trill_touch_t touches[TRILL_MAX_TOUCHES_1D];

    // Touch data (2D sensors)
    trill_touch_2d_t touches_2d[TRILL_MAX_TOUCHES_2D];
} trill_sensor_t;

// ============================================================================
// API FUNCTIONS
// ============================================================================

/**
 * Initialize a Trill sensor
 * Performs identify, sets CENTROID mode, configures scan settings.
 * Must call nchorder_i2c_mux_select() first if using mux.
 *
 * @param sensor    Pointer to sensor state structure
 * @param i2c_addr  I2C address (usually 0x20 when using mux)
 * @return NRF_SUCCESS on success, error code on failure
 */
ret_code_t trill_init(trill_sensor_t *sensor, uint8_t i2c_addr);

/**
 * Read touch data from sensor
 * Updates sensor->touches[] and sensor->num_touches.
 * Must call nchorder_i2c_mux_select() first if using mux.
 *
 * @param sensor    Pointer to sensor state structure
 * @return NRF_SUCCESS on success, error code on failure
 */
ret_code_t trill_read(trill_sensor_t *sensor);

/**
 * Set sensor mode
 *
 * @param sensor    Pointer to sensor state structure
 * @param mode      TRILL_MODE_* constant
 * @return NRF_SUCCESS on success, error code on failure
 */
ret_code_t trill_set_mode(trill_sensor_t *sensor, uint8_t mode);

/**
 * Update baseline calibration
 * Call this when sensor is not being touched.
 *
 * @param sensor    Pointer to sensor state structure
 * @return NRF_SUCCESS on success, error code on failure
 */
ret_code_t trill_update_baseline(trill_sensor_t *sensor);

/**
 * Set minimum touch size threshold
 * Touches smaller than this are ignored.
 *
 * @param sensor    Pointer to sensor state structure
 * @param min_size  Minimum size (0-255)
 * @return NRF_SUCCESS on success, error code on failure
 */
ret_code_t trill_set_min_size(trill_sensor_t *sensor, uint8_t min_size);

/**
 * Get string name for sensor type
 *
 * @param type  TRILL_TYPE_* constant
 * @return Human-readable string
 */
const char* trill_type_name(uint8_t type);

/**
 * Check if sensor has any active touches
 *
 * @param sensor    Pointer to sensor state structure
 * @return true if at least one touch is active
 */
bool trill_is_touched(const trill_sensor_t *sensor);

/**
 * Get primary touch position (first touch)
 * Returns 0 if no touches.
 *
 * @param sensor    Pointer to sensor state structure
 * @return Position 0-3200, or 0 if no touch
 */
uint16_t trill_get_position(const trill_sensor_t *sensor);

/**
 * Get primary touch size
 *
 * @param sensor    Pointer to sensor state structure
 * @return Size value, or 0 if no touch
 */
uint16_t trill_get_size(const trill_sensor_t *sensor);

#endif // NCHORDER_TRILL_H
