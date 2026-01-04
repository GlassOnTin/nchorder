/**
 * Northern Chorder - PAW-A350/ADBM-A350 Optical Sensor Driver
 *
 * Uses hardware I2C (TWI) to communicate with optical finger navigation sensor.
 *
 * Pin mapping (verified working):
 * - P0.30 = SCL (clock) - flex bottom pin 4, E73 pin 10
 * - P0.31 = SDA (data)  - flex bottom pin 3, E73 pin 9
 * - P1.11 = SHUTDOWN    - flex bottom pin 6, E73 pin 1 (LOW = enabled)
 *
 * I2C address: 0x33 (different from mbed reference's 0x57)
 * Product ID: 0x88
 */

#include "nchorder_optical.h"
#include "boards.h"
#include "nrf_gpio.h"
#include "nrf_delay.h"
#include "nrf_log.h"
#include "nrf_drv_twi.h"

// I2C pins (verified working)
#define PIN_OPTICAL_SCL     NRF_GPIO_PIN_MAP(0, 30)  // Flex bottom pin 4, E73 pin 10
#define PIN_OPTICAL_SDA     NRF_GPIO_PIN_MAP(0, 31)  // Flex bottom pin 3, E73 pin 9
#define PIN_OPTICAL_SHUTDOWN NRF_GPIO_PIN_MAP(1, 11) // Flex bottom pin 6, E73 pin 1 (LOW = enabled)

// I2C address - found at 0x33 on actual hardware (not 0x57 as in mbed reference)
#define OPTICAL_I2C_ADDR    0x33

// TWI instance (use instance 1 since 0 might be used by touch sensor)
static const nrf_drv_twi_t m_twi = NRF_DRV_TWI_INSTANCE(1);

// State
static bool m_initialized = false;
static bool m_twi_initialized = false;
static uint8_t m_product_id = 0;

/**
 * Write single register
 */
static bool optical_write_reg(uint8_t reg, uint8_t value)
{
    uint8_t tx_buf[2] = {reg, value};
    ret_code_t err = nrf_drv_twi_tx(&m_twi, OPTICAL_I2C_ADDR, tx_buf, 2, false);

    if (err != NRF_SUCCESS) {
        NRF_LOG_DEBUG("Optical: I2C write failed: %d", err);
        return false;
    }
    return true;
}

/**
 * Read single register
 */
static bool optical_read_reg(uint8_t reg, uint8_t *value)
{
    ret_code_t err;

    // Write register address
    err = nrf_drv_twi_tx(&m_twi, OPTICAL_I2C_ADDR, &reg, 1, true);  // no stop
    if (err != NRF_SUCCESS) {
        NRF_LOG_DEBUG("Optical: I2C addr write failed: %d", err);
        return false;
    }

    // Read value
    err = nrf_drv_twi_rx(&m_twi, OPTICAL_I2C_ADDR, value, 1);
    if (err != NRF_SUCCESS) {
        NRF_LOG_DEBUG("Optical: I2C read failed: %d", err);
        return false;
    }

    return true;
}

/**
 * Read burst data starting from motion register
 */
static bool optical_read_burst(uint8_t *data, size_t len)
{
    ret_code_t err;
    uint8_t reg = OPTICAL_REG_MOTION;

    // Write register address
    err = nrf_drv_twi_tx(&m_twi, OPTICAL_I2C_ADDR, &reg, 1, true);
    if (err != NRF_SUCCESS) {
        return false;
    }

    // Read multiple bytes
    err = nrf_drv_twi_rx(&m_twi, OPTICAL_I2C_ADDR, data, len);
    if (err != NRF_SUCCESS) {
        return false;
    }

    return true;
}

bool nchorder_optical_init(void)
{
    if (m_initialized) {
        return true;
    }

    NRF_LOG_INFO("Optical: Init (SCL=P0.30, SDA=P0.31, SHTDN=P1.11, addr=0x%02X)", OPTICAL_I2C_ADDR);

    // Configure SHUTDOWN pin (P1.11) - drive LOW to enable sensor
    // Per mbed reference: shutdown = 0 enables the sensor
    nrf_gpio_cfg_output(PIN_OPTICAL_SHUTDOWN);
    nrf_gpio_pin_clear(PIN_OPTICAL_SHUTDOWN);  // Enable sensor
    NRF_LOG_INFO("Optical: SHUTDOWN=LOW (sensor enabled)");
    nrf_delay_ms(50);  // Wait for sensor to wake up (tWAKEUP)

    // Initialize TWI
    if (!m_twi_initialized) {
        const nrf_drv_twi_config_t twi_config = {
            .scl                = PIN_OPTICAL_SCL,
            .sda                = PIN_OPTICAL_SDA,
            .frequency          = NRF_DRV_TWI_FREQ_100K,
            .interrupt_priority = APP_IRQ_PRIORITY_LOW,
            .clear_bus_init     = true,
        };

        ret_code_t err = nrf_drv_twi_init(&m_twi, &twi_config, NULL, NULL);
        if (err != NRF_SUCCESS && err != NRF_ERROR_INVALID_STATE) {
            NRF_LOG_ERROR("Optical: TWI init failed: %d", err);
            return false;
        }

        nrf_drv_twi_enable(&m_twi);
        m_twi_initialized = true;
        NRF_LOG_INFO("Optical: TWI initialized");
    }

    // Wait for sensor to be ready after power-up
    nrf_delay_ms(100);

    // Scan I2C bus for devices
    NRF_LOG_INFO("Optical: Scanning I2C bus...");
    for (uint8_t addr = 0x08; addr < 0x78; addr++) {
        uint8_t dummy;
        ret_code_t err = nrf_drv_twi_rx(&m_twi, addr, &dummy, 1);
        if (err == NRF_SUCCESS) {
            NRF_LOG_INFO("Optical: Found device at 0x%02X", addr);
        }
    }
    NRF_LOG_INFO("Optical: I2C scan complete");

    // Soft reset (from mbed reference: write 0x5A to register 0x3A)
    NRF_LOG_INFO("Optical: Sending soft reset...");
    optical_write_reg(OPTICAL_REG_SOFT_RESET, OPTICAL_SOFT_RESET_CMD);
    nrf_delay_ms(50);  // Wait for reset to complete

    // Try to read product ID (should be 0x88)
    uint8_t id = 0;
    bool read_ok = optical_read_reg(OPTICAL_REG_PRODUCT_ID, &id);
    NRF_LOG_INFO("Optical: Product ID read %s = 0x%02X (expect 0x88)", read_ok ? "OK" : "FAIL", id);

    // If first read failed or got 0xFF, try again after longer delay
    if (!read_ok || id == 0xFF || id == 0x00) {
        nrf_delay_ms(100);

        read_ok = optical_read_reg(OPTICAL_REG_PRODUCT_ID, &id);
        NRF_LOG_INFO("Optical: Product ID read attempt 2 %s = 0x%02X", read_ok ? "OK" : "FAIL", id);
    }

    m_product_id = id;

    // Check if it matches expected
    if (!read_ok) {
        NRF_LOG_WARNING("Optical: I2C communication failed");
    } else if (id == 0xFF || id == 0x00) {
        NRF_LOG_WARNING("Optical: No valid response (got 0x%02X)", id);
    } else if (id != OPTICAL_PRODUCT_ID_A350) {
        NRF_LOG_INFO("Optical: Product ID = 0x%02X (expected 0x%02X)", id, OPTICAL_PRODUCT_ID_A350);
    } else {
        NRF_LOG_INFO("Optical: PAW-A350 detected!");
    }

    // If we got a valid response, initialize OFN engine
    if (read_ok && id != 0xFF && id != 0x00) {
        // OFN engine settings (from mbed reference)
        optical_write_reg(OPTICAL_REG_OFN_ENGINE, OPTICAL_OFN_ENGINE_INIT);
        NRF_LOG_INFO("Optical: OFN engine initialized");
    }

    // Read initial motion to clear any pending data
    optical_motion_t dummy;
    nchorder_optical_read_motion(&dummy);

    m_initialized = true;
    NRF_LOG_INFO("Optical: Initialized (ID=0x%02X)", id);
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

    // Read each register individually (burst read not working)
    uint8_t motion_reg, dx_reg, dy_reg, squal_reg;
    if (!optical_read_reg(OPTICAL_REG_MOTION, &motion_reg)) {
        return false;
    }
    if (!optical_read_reg(OPTICAL_REG_DELTA_X, &dx_reg)) {
        return false;
    }
    if (!optical_read_reg(OPTICAL_REG_DELTA_Y, &dy_reg)) {
        return false;
    }
    if (!optical_read_reg(OPTICAL_REG_SQUAL, &squal_reg)) {
        return false;
    }

    motion->motion = (motion_reg & MOTION_BIT_MOT) != 0;
    motion->overflow = (motion_reg & MOTION_BIT_OVF) != 0;
    motion->dx = (int8_t)dx_reg;
    motion->dy = (int8_t)dy_reg;
    motion->squal = squal_reg;

    // Log motion events
    if (motion->motion) {
        NRF_LOG_DEBUG("Optical: dx=%d dy=%d squal=%d", motion->dx, motion->dy, motion->squal);
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
        nrf_gpio_pin_set(PIN_OPTICAL_SHUTDOWN);  // HIGH = shutdown
        NRF_LOG_INFO("Optical: Entering sleep mode");
    }
}

void nchorder_optical_wake(void)
{
    if (m_initialized) {
        nrf_gpio_pin_clear(PIN_OPTICAL_SHUTDOWN);  // LOW = enabled
        nrf_delay_ms(50);  // Wait for sensor to wake (tWAKEUP)
        NRF_LOG_INFO("Optical: Waking from sleep");
    }
}
