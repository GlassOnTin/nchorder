/**
 * Northern Chorder - PAW-A350/ADBM-A350 Optical Sensor Driver
 *
 * Uses I2C interface at address 0x57.
 * Pin P0.29 controls SHUTDOWN (active low = powered on).
 */

#include "nchorder_optical.h"
#include "nchorder_i2c.h"
#include "boards.h"
#include "nrf_gpio.h"
#include "nrf_delay.h"
#include "nrf_log.h"

// SHUTDOWN pin - active low (LOW = sensor enabled, HIGH = shutdown)
#ifndef PIN_SENSOR_SHTDN
#define PIN_SENSOR_SHTDN    NRF_GPIO_PIN_MAP(0, 29)
#endif

// State
static bool m_initialized = false;
static uint8_t m_product_id = 0;

/**
 * Write single register
 */
static bool optical_write_reg(uint8_t reg, uint8_t value)
{
    uint8_t data[2] = { reg | 0x80, value };  // MSB=1 for write
    ret_code_t err = nchorder_i2c_write(OPTICAL_I2C_ADDR, data, 2);
    return (err == NRF_SUCCESS);
}

/**
 * Read single register
 */
static bool optical_read_reg(uint8_t reg, uint8_t *value)
{
    uint8_t addr = reg & 0x7F;  // MSB=0 for read
    ret_code_t err = nchorder_i2c_write_read(OPTICAL_I2C_ADDR, &addr, 1, value, 1);
    return (err == NRF_SUCCESS);
}

/**
 * Read burst data starting from motion register
 */
static bool optical_read_burst(uint8_t *data, size_t len)
{
    uint8_t addr = OPTICAL_REG_MOTION & 0x7F;
    ret_code_t err = nchorder_i2c_write_read(OPTICAL_I2C_ADDR, &addr, 1, data, len);
    return (err == NRF_SUCCESS);
}

bool nchorder_optical_init(void)
{
    if (m_initialized) {
        return true;
    }

    // Ensure I2C is initialized
    if (!nchorder_i2c_is_initialized()) {
        ret_code_t err = nchorder_i2c_init();
        if (err != NRF_SUCCESS) {
            NRF_LOG_ERROR("Optical: I2C init failed");
            return false;
        }
    }

    // Configure SHUTDOWN pin as output
    nrf_gpio_cfg_output(PIN_SENSOR_SHTDN);

    // Wake sensor: SHUTDOWN = LOW (not shutdown)
    NRF_LOG_INFO("Optical: Waking sensor (SHTDN=LOW)");
    nrf_gpio_pin_clear(PIN_SENSOR_SHTDN);
    nrf_delay_ms(50);  // Wait for sensor to wake up

    // Try to read product ID
    uint8_t id = 0;
    if (!optical_read_reg(OPTICAL_REG_PRODUCT_ID, &id)) {
        // Try with SHUTDOWN HIGH (inverted logic?)
        NRF_LOG_INFO("Optical: Trying SHTDN=HIGH...");
        nrf_gpio_pin_set(PIN_SENSOR_SHTDN);
        nrf_delay_ms(50);

        if (!optical_read_reg(OPTICAL_REG_PRODUCT_ID, &id)) {
            NRF_LOG_WARNING("Optical: No response at I2C 0x%02X", OPTICAL_I2C_ADDR);
            // Don't do full I2C scan here - it blocks too long
            return false;
        }
    }

    m_product_id = id;
    NRF_LOG_INFO("Optical: Product ID = 0x%02X", id);

    // Check if it matches expected
    if (id != OPTICAL_PRODUCT_ID_A350 && id != 0x00 && id != 0xFF) {
        NRF_LOG_WARNING("Optical: Unexpected Product ID (got 0x%02X, expected 0x%02X)",
                        id, OPTICAL_PRODUCT_ID_A350);
        // Continue anyway - might be different revision
    }

    // Read initial motion to clear any pending data
    optical_motion_t dummy;
    nchorder_optical_read_motion(&dummy);

    m_initialized = true;
    NRF_LOG_INFO("Optical: Initialized successfully");
    return true;
}

bool nchorder_optical_is_ready(void)
{
    return m_initialized;
}

uint8_t nchorder_optical_get_product_id(void)
{
    return m_product_id;
}

bool nchorder_optical_read_motion(optical_motion_t *motion)
{
    if (!m_initialized || motion == NULL) {
        return false;
    }

    // Burst read: Motion, Delta_X, Delta_Y, SQUAL
    // Note: Some sensors return Delta_Y before Delta_X - check if needed
    uint8_t data[4];
    if (!optical_read_burst(data, 4)) {
        return false;
    }

    motion->motion = (data[0] & MOTION_BIT_MOT) != 0;
    motion->overflow = (data[0] & MOTION_BIT_OVF) != 0;
    motion->dx = (int8_t)data[1];  // Delta_X
    motion->dy = (int8_t)data[2];  // Delta_Y
    motion->squal = data[3];       // Surface quality

    if (motion->motion) {
        NRF_LOG_DEBUG("Optical: dx=%d dy=%d squal=%d",
                      motion->dx, motion->dy, motion->squal);
    }

    return true;
}

bool nchorder_optical_set_cpi(uint16_t cpi)
{
    if (!m_initialized) {
        return false;
    }

    // CPI is typically encoded as a register value
    // For A350: CPI = (reg_value + 1) * 125
    // So reg_value = (CPI / 125) - 1
    if (cpi < 125) cpi = 125;
    if (cpi > 1250) cpi = 1250;

    uint8_t reg_val = (cpi / 125) - 1;

    // Set X and Y CPI to same value
    if (!optical_write_reg(OPTICAL_REG_CPI_X, reg_val)) {
        NRF_LOG_ERROR("Optical: Failed to set CPI_X");
        return false;
    }

    if (!optical_write_reg(OPTICAL_REG_CPI_Y, reg_val)) {
        NRF_LOG_ERROR("Optical: Failed to set CPI_Y");
        return false;
    }

    NRF_LOG_INFO("Optical: CPI set to %d", cpi);
    return true;
}

void nchorder_optical_sleep(void)
{
    if (m_initialized) {
        // SHUTDOWN = HIGH to enter low power mode
        nrf_gpio_pin_set(PIN_SENSOR_SHTDN);
        NRF_LOG_INFO("Optical: Entering sleep mode");
    }
}

void nchorder_optical_wake(void)
{
    if (m_initialized) {
        // SHUTDOWN = LOW to wake
        nrf_gpio_pin_clear(PIN_SENSOR_SHTDN);
        nrf_delay_ms(10);  // Wait for sensor to stabilize
        NRF_LOG_INFO("Optical: Waking from sleep");
    }
}
