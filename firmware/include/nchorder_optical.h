/**
 * Northern Chorder - PAW-A350/ADBM-A350 Optical Sensor Driver
 *
 * PixArt PAW-A350 optical finger navigation sensor.
 * Compatible with AVAGO ADBS-A350/ADBM-A350.
 *
 * Key specs:
 * - I2C address: 0x57 (7-bit)
 * - Resolution: 125-1250 CPI
 * - 1.8V or 2.8V operation
 */

#ifndef NCHORDER_OPTICAL_H
#define NCHORDER_OPTICAL_H

#include <stdint.h>
#include <stdbool.h>

// I2C address (7-bit, with IO_SEL and CS/MOSI pins HIGH)
#define OPTICAL_I2C_ADDR            0x57

// Register map (based on ADBM-A350 datasheet)
#define OPTICAL_REG_PRODUCT_ID      0x00    // Product ID (read-only)
#define OPTICAL_REG_REVISION_ID     0x01    // Revision ID (read-only)
#define OPTICAL_REG_MOTION          0x02    // Motion status (burst trigger)
#define OPTICAL_REG_DELTA_X         0x03    // X movement (signed 8-bit)
#define OPTICAL_REG_DELTA_Y         0x04    // Y movement (signed 8-bit)
#define OPTICAL_REG_SQUAL           0x05    // Surface quality
#define OPTICAL_REG_SHUTTER_UPPER   0x06    // Shutter upper byte
#define OPTICAL_REG_SHUTTER_LOWER   0x07    // Shutter lower byte
#define OPTICAL_REG_PIXEL_MAX       0x08    // Maximum pixel value
#define OPTICAL_REG_PIXEL_SUM       0x09    // Pixel sum
#define OPTICAL_REG_PIXEL_MIN       0x0A    // Minimum pixel value
#define OPTICAL_REG_CPI_X           0x0D    // CPI setting X
#define OPTICAL_REG_CPI_Y           0x0E    // CPI setting Y

// Motion register bit masks
#define MOTION_BIT_MOT              0x80    // Motion detected since last read
#define MOTION_BIT_OVF              0x10    // Motion overflow

// Expected Product ID (may vary by revision)
#define OPTICAL_PRODUCT_ID_A350     0x0D    // PixArt PAW-A350 / AVAGO ADBS-A350

/**
 * Motion data structure
 */
typedef struct {
    int8_t dx;          // X delta (positive = right)
    int8_t dy;          // Y delta (positive = down)
    uint8_t squal;      // Surface quality (0-255, higher = better)
    bool motion;        // Motion was detected
    bool overflow;      // Motion overflow occurred
} optical_motion_t;

/**
 * Initialize optical sensor
 *
 * Configures SHUTDOWN pin (P0.29), initializes I2C, verifies sensor presence.
 *
 * @return true if sensor detected and initialized
 */
bool nchorder_optical_init(void);

/**
 * Check if sensor is initialized and responding
 */
bool nchorder_optical_is_ready(void);

/**
 * Read product ID register
 *
 * @return Product ID value, or 0 on error
 */
uint8_t nchorder_optical_get_product_id(void);

/**
 * Read motion data from sensor
 *
 * Reading the motion register clears accumulated deltas.
 * Call this periodically (e.g., every 10ms) to get movement.
 *
 * @param[out] motion  Motion data structure
 * @return true on success
 */
bool nchorder_optical_read_motion(optical_motion_t *motion);

/**
 * Set sensor resolution (CPI)
 *
 * @param cpi  Counts per inch (125-1250, quantized to nearest step)
 * @return true on success
 */
bool nchorder_optical_set_cpi(uint16_t cpi);

/**
 * Enter low-power mode
 */
void nchorder_optical_sleep(void);

/**
 * Wake from low-power mode
 */
void nchorder_optical_wake(void);

#endif // NCHORDER_OPTICAL_H
