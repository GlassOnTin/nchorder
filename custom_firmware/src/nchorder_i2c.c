/**
 * Northern Chorder - I2C Bus Driver Implementation
 *
 * Uses Nordic nrfx_twim driver for I2C master communication.
 * Includes PCA9548 multiplexer control for multi-sensor support.
 */

// sdk_config.h must be included before nrfx drivers
#include "sdk_common.h"

#include "nchorder_i2c.h"
#include "nchorder_config.h"
#include "nrf_gpio.h"
#include "nrfx_twim.h"
#include "nrf_delay.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"

// I2C instance (TWIM0)
static const nrfx_twim_t m_twim = NRFX_TWIM_INSTANCE(0);

// Initialization state
static bool m_initialized = false;

// Current mux channel (for optimization)
static uint8_t m_current_mux_channel = 0xFF;

// Transfer complete flag for blocking operations
static volatile bool m_xfer_done = false;
static volatile ret_code_t m_xfer_result = NRF_SUCCESS;

/**
 * TWIM event handler
 */
static void twim_handler(nrfx_twim_evt_t const *p_event, void *p_context)
{
    switch (p_event->type) {
        case NRFX_TWIM_EVT_DONE:
            m_xfer_result = NRF_SUCCESS;
            m_xfer_done = true;
            break;

        case NRFX_TWIM_EVT_ADDRESS_NACK:
            m_xfer_result = NRF_ERROR_DRV_TWI_ERR_ANACK;
            m_xfer_done = true;
            break;

        case NRFX_TWIM_EVT_DATA_NACK:
            m_xfer_result = NRF_ERROR_DRV_TWI_ERR_DNACK;
            m_xfer_done = true;
            break;

        default:
            m_xfer_result = NRF_ERROR_INTERNAL;
            m_xfer_done = true;
            break;
    }
}

/**
 * Wait for transfer to complete with timeout
 */
static ret_code_t wait_for_xfer(uint32_t timeout_ms)
{
    uint32_t timeout_count = timeout_ms * 1000;  // Convert to ~us loops

    while (!m_xfer_done && timeout_count > 0) {
        __WFE();  // Wait for event (low power)
        timeout_count--;
    }

    if (!m_xfer_done) {
        nrfx_twim_disable(&m_twim);
        nrfx_twim_enable(&m_twim);
        return NRF_ERROR_TIMEOUT;
    }

    return m_xfer_result;
}

ret_code_t nchorder_i2c_init(void)
{
    ret_code_t err_code;

    if (m_initialized) {
        return NRF_SUCCESS;
    }

    // Configure MUX reset pin as output, initially high (not reset)
    nrf_gpio_cfg_output(PIN_MUX_RESET);
    nrf_gpio_pin_set(PIN_MUX_RESET);

    // Configure TWIM
    nrfx_twim_config_t config = {
        .scl                = PIN_I2C_SCL,
        .sda                = PIN_I2C_SDA,
        .frequency          = NRF_TWIM_FREQ_400K,
        .interrupt_priority = NRFX_TWIM_DEFAULT_CONFIG_IRQ_PRIORITY,
        .hold_bus_uninit    = false
    };

    err_code = nrfx_twim_init(&m_twim, &config, twim_handler, NULL);
    if (err_code != NRF_SUCCESS) {
        NRF_LOG_ERROR("I2C init failed: 0x%08X", err_code);
        return err_code;
    }

    nrfx_twim_enable(&m_twim);

    m_initialized = true;
    m_current_mux_channel = 0xFF;

    NRF_LOG_INFO("I2C initialized (SDA=P%d.%02d, SCL=P%d.%02d)",
                 (PIN_I2C_SDA >> 5), (PIN_I2C_SDA & 0x1F),
                 (PIN_I2C_SCL >> 5), (PIN_I2C_SCL & 0x1F));

    return NRF_SUCCESS;
}

ret_code_t nchorder_i2c_write(uint8_t addr, const uint8_t *p_data, size_t len)
{
    if (!m_initialized) {
        return NRF_ERROR_INVALID_STATE;
    }

    if (p_data == NULL || len == 0) {
        return NRF_ERROR_INVALID_PARAM;
    }

    nrfx_twim_xfer_desc_t xfer = {
        .type = NRFX_TWIM_XFER_TX,
        .address = addr,
        .primary_length = len,
        .p_primary_buf = (uint8_t *)p_data,
        .secondary_length = 0,
        .p_secondary_buf = NULL
    };

    m_xfer_done = false;
    ret_code_t err_code = nrfx_twim_xfer(&m_twim, &xfer, 0);

    if (err_code != NRF_SUCCESS) {
        return err_code;
    }

    return wait_for_xfer(100);  // 100ms timeout
}

ret_code_t nchorder_i2c_read(uint8_t addr, uint8_t *p_data, size_t len)
{
    if (!m_initialized) {
        return NRF_ERROR_INVALID_STATE;
    }

    if (p_data == NULL || len == 0) {
        return NRF_ERROR_INVALID_PARAM;
    }

    nrfx_twim_xfer_desc_t xfer = {
        .type = NRFX_TWIM_XFER_RX,
        .address = addr,
        .primary_length = len,
        .p_primary_buf = p_data,
        .secondary_length = 0,
        .p_secondary_buf = NULL
    };

    m_xfer_done = false;
    ret_code_t err_code = nrfx_twim_xfer(&m_twim, &xfer, 0);

    if (err_code != NRF_SUCCESS) {
        return err_code;
    }

    return wait_for_xfer(100);
}

ret_code_t nchorder_i2c_write_read(uint8_t addr,
                                    const uint8_t *p_tx, size_t tx_len,
                                    uint8_t *p_rx, size_t rx_len)
{
    if (!m_initialized) {
        return NRF_ERROR_INVALID_STATE;
    }

    if (p_tx == NULL || tx_len == 0 || p_rx == NULL || rx_len == 0) {
        return NRF_ERROR_INVALID_PARAM;
    }

    nrfx_twim_xfer_desc_t xfer = {
        .type = NRFX_TWIM_XFER_TXRX,
        .address = addr,
        .primary_length = tx_len,
        .p_primary_buf = (uint8_t *)p_tx,
        .secondary_length = rx_len,
        .p_secondary_buf = p_rx
    };

    m_xfer_done = false;
    ret_code_t err_code = nrfx_twim_xfer(&m_twim, &xfer, 0);

    if (err_code != NRF_SUCCESS) {
        return err_code;
    }

    return wait_for_xfer(100);
}

ret_code_t nchorder_i2c_mux_select(uint8_t channel)
{
    if (!m_initialized) {
        return NRF_ERROR_INVALID_STATE;
    }

    // Optimization: skip if already on this channel
    if (channel == m_current_mux_channel) {
        return NRF_SUCCESS;
    }

    uint8_t data;

    if (channel == 0xFF) {
        // Disable all channels
        data = 0x00;
    } else if (channel < 8) {
        // Enable single channel
        data = (1 << channel);
    } else {
        return NRF_ERROR_INVALID_PARAM;
    }

    ret_code_t err_code = nchorder_i2c_write(I2C_ADDR_MUX, &data, 1);

    if (err_code == NRF_SUCCESS) {
        m_current_mux_channel = channel;
        NRF_LOG_DEBUG("MUX channel %d selected", channel);
    } else {
        NRF_LOG_WARNING("MUX select ch%d failed: 0x%08X", channel, err_code);
        m_current_mux_channel = 0xFF;  // Unknown state
    }

    return err_code;
}

void nchorder_i2c_mux_reset(void)
{
    NRF_LOG_INFO("Resetting I2C mux");

    // Pulse reset low for at least 6ns (datasheet minimum)
    // We use 10us to be safe
    nrf_gpio_pin_clear(PIN_MUX_RESET);
    nrf_delay_us(10);
    nrf_gpio_pin_set(PIN_MUX_RESET);
    nrf_delay_us(10);

    // Reset tracking
    m_current_mux_channel = 0xFF;
}

uint8_t nchorder_i2c_scan(void)
{
    if (!m_initialized) {
        NRF_LOG_ERROR("I2C not initialized");
        return 0;
    }

    uint8_t found = 0;
    uint8_t dummy;

    NRF_LOG_INFO("I2C bus scan starting...");

    // Scan valid 7-bit addresses (0x08 to 0x77)
    for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
        // Try to read one byte
        ret_code_t err = nchorder_i2c_read(addr, &dummy, 1);

        if (err == NRF_SUCCESS || err == NRF_ERROR_DRV_TWI_ERR_DNACK) {
            // Device ACKed its address
            NRF_LOG_INFO("  Found device at 0x%02X", addr);
            found++;
        }
    }

    NRF_LOG_INFO("I2C scan complete: %d device(s) found", found);
    return found;
}

bool nchorder_i2c_is_initialized(void)
{
    return m_initialized;
}
