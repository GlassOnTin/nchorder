/**
 * Northern Chorder - Trill Capacitive Sensor Driver Implementation
 *
 * I2C communication with Bela Trill sensors.
 * Protocol based on Trill-Arduino library (BSD-3-Clause).
 */

#include "nchorder_trill.h"
#include "nchorder_i2c.h"
#include "nchorder_config.h"
#include "nrf_log.h"
#include "SEGGER_RTT.h"

// Simple busy-wait delay (avoid nrf_delay_ms which may hang without DWT init)
static void simple_delay_ms(uint32_t ms)
{
    // Approximate delay - 64MHz CPU, ~10 cycles per iteration
    volatile uint32_t count = ms * 6400;
    while (count--) {
        __NOP();
    }
}

// ============================================================================
// INTERNAL HELPERS
// ============================================================================

/**
 * Send a command to the Trill sensor
 */
static ret_code_t trill_send_command(uint8_t addr, uint8_t cmd, const uint8_t *params, size_t param_len)
{
    uint8_t buf[8];

    if (param_len > 6) {
        return NRF_ERROR_INVALID_LENGTH;
    }

    buf[0] = TRILL_OFFSET_COMMAND;  // Write to command offset
    buf[1] = cmd;                    // Command byte

    for (size_t i = 0; i < param_len; i++) {
        buf[2 + i] = params[i];
    }

    return nchorder_i2c_write(addr, buf, 2 + param_len);
}

/**
 * Prepare sensor for data read by setting read pointer
 */
static ret_code_t trill_prepare_read(uint8_t addr)
{
    // Read from offset 0 to get all data including any header bytes
    uint8_t offset = 0;
    return nchorder_i2c_write(addr, &offset, 1);
}

/**
 * Read 16-bit big-endian value from buffer
 */
static uint16_t read_be16(const uint8_t *buf)
{
    return ((uint16_t)buf[0] << 8) | buf[1];
}

// ============================================================================
// PUBLIC API
// ============================================================================

ret_code_t trill_init(trill_sensor_t *sensor, uint8_t i2c_addr)
{
    ret_code_t err;
    uint8_t identify_buf[4];

    if (sensor == NULL) {
        return NRF_ERROR_NULL;
    }

    // Clear sensor state
    memset(sensor, 0, sizeof(trill_sensor_t));
    sensor->i2c_addr = i2c_addr;

    // Read device info from offset 0
    // Format: FE <type> <fw_ver> <checksum?>
    uint8_t zero_offset = 0;
    err = nchorder_i2c_write(i2c_addr, &zero_offset, 1);  // Set read pointer to 0
    if (err != NRF_SUCCESS) {
        NRF_LOG_ERROR("Trill set read ptr failed: 0x%08X", err);
        return err;
    }

    err = nchorder_i2c_read(i2c_addr, identify_buf, 4);
    if (err != NRF_SUCCESS) {
        NRF_LOG_ERROR("Trill identify read failed: 0x%08X", err);
        return err;
    }

    NRF_LOG_INFO("Trill raw: %02X %02X %02X %02X",
                 identify_buf[0], identify_buf[1], identify_buf[2], identify_buf[3]);

    // Check for FE header and extract device info from bytes 1-2
    if (identify_buf[0] != 0xFE) {
        NRF_LOG_ERROR("Trill unexpected header: 0x%02X (expected 0xFE)", identify_buf[0]);
        return NRF_ERROR_NOT_FOUND;
    }

    sensor->device_type = identify_buf[1];      // Type at byte 1
    sensor->firmware_version = identify_buf[2]; // FW version at byte 2

    // Check for valid device type
    if (sensor->device_type == 0 || sensor->device_type > TRILL_TYPE_FLEX) {
        NRF_LOG_ERROR("Trill unknown type: %d (raw: %02X %02X %02X %02X)",
                      sensor->device_type, identify_buf[0], identify_buf[1],
                      identify_buf[2], identify_buf[3]);
        return NRF_ERROR_NOT_FOUND;
    }

    // Mark as 2D sensor
    sensor->is_2d = (sensor->device_type == TRILL_TYPE_SQUARE ||
                     sensor->device_type == TRILL_TYPE_HEX);

    NRF_LOG_INFO("Trill %s detected (addr=0x%02X, fw=%d)",
                 trill_type_name(sensor->device_type),
                 i2c_addr, sensor->firmware_version);

    // Step 2: Set mode to CENTROID
    err = trill_set_mode(sensor, TRILL_MODE_CENTROID);
    if (err != NRF_SUCCESS) {
        return err;
    }

    // Step 3: Configure scan settings (speed=0 ultra fast, resolution=12 bits)
    uint8_t scan_params[2] = {0, 12};
    err = trill_send_command(i2c_addr, TRILL_CMD_SCAN_SETTINGS, scan_params, 2);
    if (err != NRF_SUCCESS) {
        NRF_LOG_WARNING("Trill scan settings failed: 0x%08X", err);
        // Non-fatal, continue
    }

    simple_delay_ms(5);

    // Step 3.5: Enable auto-scan (sensor continuously updates data)
    uint8_t auto_scan_param = 1;  // 1 = enable
    err = trill_send_command(i2c_addr, TRILL_CMD_AUTO_SCAN, &auto_scan_param, 1);
    if (err != NRF_SUCCESS) {
        NRF_LOG_WARNING("Trill auto-scan enable failed: 0x%08X", err);
        // Non-fatal, continue
    }

    simple_delay_ms(5);

    // Step 4: Update baseline
    err = trill_update_baseline(sensor);
    if (err != NRF_SUCCESS) {
        NRF_LOG_WARNING("Trill baseline update failed: 0x%08X", err);
        // Non-fatal, continue
    }

    sensor->initialized = true;
    return NRF_SUCCESS;
}

ret_code_t trill_read(trill_sensor_t *sensor)
{
    ret_code_t err;
    uint8_t buf[36];  // Enough for centroid data (2D needs 34 bytes)
    size_t read_len;

    if (sensor == NULL || !sensor->initialized) {
        return NRF_ERROR_INVALID_STATE;
    }

    // Prepare for data read
    err = trill_prepare_read(sensor->i2c_addr);
    if (err != NRF_SUCCESS) {
        return err;
    }

    // Read centroid data (including 4-byte header from offset 0)
    // 1D sensors: 4 header + 20 data bytes
    // 2D sensors: 4 header + 30 data bytes
    read_len = sensor->is_2d ? 34 : 24;

    err = nchorder_i2c_read(sensor->i2c_addr, buf, read_len);
    if (err != NRF_SUCCESS) {
        return err;
    }

    // Debug: dump raw bytes for 2D sensor (header + data)
    if (sensor->is_2d) {
        static uint32_t dbg_cnt = 0;
        if ((dbg_cnt++ % 50) == 0) {
            // Header: bytes 0-3
            SEGGER_RTT_printf(0, "RAW2D:[%02X%02X%02X%02X]",
                buf[0], buf[1], buf[2], buf[3]);
            // Y positions: bytes 4-13
            SEGGER_RTT_printf(0, "Y:%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X-",
                buf[4], buf[5], buf[6], buf[7], buf[8],
                buf[9], buf[10], buf[11], buf[12], buf[13]);
            // X positions: bytes 14-23
            SEGGER_RTT_printf(0, "X:%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X-",
                buf[14], buf[15], buf[16], buf[17], buf[18],
                buf[19], buf[20], buf[21], buf[22], buf[23]);
            // Sizes: bytes 24-33
            SEGGER_RTT_printf(0, "S:%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\n",
                buf[24], buf[25], buf[26], buf[27], buf[28],
                buf[29], buf[30], buf[31], buf[32], buf[33]);
        }
    }

    // Parse centroid data
    sensor->num_touches = 0;

    if (sensor->is_2d) {
        // Trill Square 2D centroid format (after 4-byte header):
        // Bytes 4-13:  Horizontal (Y) positions for touches 0-4 (5 * 2 bytes)
        // Bytes 14-23: Vertical (X) positions for touches 0-4 (5 * 2 bytes)
        // Bytes 24-33: Touch sizes for touches 0-4 (5 * 2 bytes)
        const int DATA_OFFSET = 4;  // Skip header
        for (int i = 0; i < TRILL_MAX_TOUCHES_2D; i++) {
            uint16_t y = read_be16(&buf[DATA_OFFSET + i * 2]);         // Horizontal = Y
            uint16_t x = read_be16(&buf[DATA_OFFSET + 10 + i * 2]);    // Vertical = X
            uint16_t size = read_be16(&buf[DATA_OFFSET + 20 + i * 2]);

            // Filter invalid touches: size or position of 0 or 0xFFFF means no touch
            if (size > 0 && size != 0xFFFF &&
                x != 0xFFFF && y != 0xFFFF) {
                sensor->touches_2d[sensor->num_touches].x = x;
                sensor->touches_2d[sensor->num_touches].y = y;
                sensor->touches_2d[sensor->num_touches].size = size;
                sensor->num_touches++;
            }
        }
    } else {
        // 1D format (after 4-byte header):
        // Bytes 4-13:  Positions for touches 0-4 (5 * 2 bytes)
        // Bytes 14-23: Sizes for touches 0-4 (5 * 2 bytes)
        const int DATA_OFFSET = 4;  // Skip header
        for (int i = 0; i < TRILL_MAX_TOUCHES_1D; i++) {
            uint16_t pos = read_be16(&buf[DATA_OFFSET + i * 2]);
            uint16_t size = read_be16(&buf[DATA_OFFSET + 10 + i * 2]);

            // Filter invalid touches: size or position of 0 or 0xFFFF means no touch
            if (size > 0 && size != 0xFFFF && pos != 0xFFFF) {
                sensor->touches[sensor->num_touches].position = pos;
                sensor->touches[sensor->num_touches].size = size;
                sensor->num_touches++;
            }
        }
    }

    return NRF_SUCCESS;
}

ret_code_t trill_set_mode(trill_sensor_t *sensor, uint8_t mode)
{
    if (sensor == NULL) {
        return NRF_ERROR_NULL;
    }

    uint8_t param = mode;
    ret_code_t err = trill_send_command(sensor->i2c_addr, TRILL_CMD_MODE, &param, 1);

    if (err == NRF_SUCCESS) {
        simple_delay_ms(5);  // Allow mode change to take effect
    }

    return err;
}

ret_code_t trill_update_baseline(trill_sensor_t *sensor)
{
    if (sensor == NULL) {
        return NRF_ERROR_NULL;
    }

    ret_code_t err = trill_send_command(sensor->i2c_addr, TRILL_CMD_BASELINE_UPDATE, NULL, 0);

    if (err == NRF_SUCCESS) {
        simple_delay_ms(10);  // Allow baseline update to complete
    }

    return err;
}

ret_code_t trill_set_min_size(trill_sensor_t *sensor, uint8_t min_size)
{
    if (sensor == NULL) {
        return NRF_ERROR_NULL;
    }

    uint8_t param = min_size;
    return trill_send_command(sensor->i2c_addr, TRILL_CMD_MINIMUM_SIZE, &param, 1);
}

const char* trill_type_name(uint8_t type)
{
    switch (type) {
        case TRILL_TYPE_BAR:     return "Bar";
        case TRILL_TYPE_SQUARE:  return "Square";
        case TRILL_TYPE_CRAFT:   return "Craft";
        case TRILL_TYPE_RING:    return "Ring";
        case TRILL_TYPE_HEX:     return "Hex";
        case TRILL_TYPE_FLEX:    return "Flex";
        default:                 return "Unknown";
    }
}

bool trill_is_touched(const trill_sensor_t *sensor)
{
    if (sensor == NULL) {
        return false;
    }
    return sensor->num_touches > 0;
}

uint16_t trill_get_position(const trill_sensor_t *sensor)
{
    if (sensor == NULL || sensor->num_touches == 0) {
        return 0;
    }

    if (sensor->is_2d) {
        // For 2D, return X position of first touch
        return sensor->touches_2d[0].x;
    } else {
        return sensor->touches[0].position;
    }
}

uint16_t trill_get_size(const trill_sensor_t *sensor)
{
    if (sensor == NULL || sensor->num_touches == 0) {
        return 0;
    }

    if (sensor->is_2d) {
        return sensor->touches_2d[0].size;
    } else {
        return sensor->touches[0].size;
    }
}
