/**
 * Northern Chorder - Battery Measurement
 *
 * One-shot SAADC measurement of VDD.
 * Each measurement: init → configure → sample → uninit
 * This avoids SAADC idle current (~700uA continuous).
 *
 * VDD channel with 1/6 gain, internal 0.6V reference, 10-bit resolution.
 * Formula: voltage_mV = raw * 3600 / 1024
 * (0.6V reference * 6 gain = 3.6V max range, 1024 counts at 10-bit)
 */

#include "nchorder_battery.h"
#include "nrfx_saadc.h"
#include "nrf_log.h"

static bool m_initialized = false;
static volatile bool m_saadc_done = false;
static nrf_saadc_value_t m_sample;

/**
 * @brief SAADC event handler for one-shot measurement.
 */
static void saadc_event_handler(nrfx_saadc_evt_t const * p_event)
{
    if (p_event->type == NRFX_SAADC_EVT_DONE) {
        m_saadc_done = true;
    }
}

ret_code_t nchorder_battery_init(void)
{
    m_initialized = true;
    NRF_LOG_INFO("Battery measurement initialized");
    return NRF_SUCCESS;
}

uint16_t nchorder_battery_measure(void)
{
    if (!m_initialized) {
        return 0;
    }

    ret_code_t err_code;

    // Init SAADC with low-power defaults
    nrfx_saadc_config_t saadc_config = NRFX_SAADC_DEFAULT_CONFIG;
    saadc_config.resolution = NRF_SAADC_RESOLUTION_10BIT;
    saadc_config.low_power_mode = true;

    err_code = nrfx_saadc_init(&saadc_config, saadc_event_handler);
    if (err_code != NRF_SUCCESS) {
        NRF_LOG_WARNING("SAADC init failed: %d", err_code);
        return 0;
    }

    // Configure VDD channel: 1/6 gain, internal 0.6V reference
    nrf_saadc_channel_config_t channel_config = NRFX_SAADC_DEFAULT_CHANNEL_CONFIG_SE(NRF_SAADC_INPUT_VDD);
    channel_config.gain = NRF_SAADC_GAIN1_6;
    channel_config.reference = NRF_SAADC_REFERENCE_INTERNAL;
    channel_config.acq_time = NRF_SAADC_ACQTIME_10US;

    err_code = nrfx_saadc_channel_init(0, &channel_config);
    if (err_code != NRF_SUCCESS) {
        NRF_LOG_WARNING("SAADC channel init failed: %d", err_code);
        nrfx_saadc_uninit();
        return 0;
    }

    // Set up buffer and trigger conversion
    m_saadc_done = false;
    err_code = nrfx_saadc_buffer_convert(&m_sample, 1);
    if (err_code != NRF_SUCCESS) {
        NRF_LOG_WARNING("SAADC buffer failed: %d", err_code);
        nrfx_saadc_uninit();
        return 0;
    }

    err_code = nrfx_saadc_sample();
    if (err_code != NRF_SUCCESS) {
        NRF_LOG_WARNING("SAADC sample failed: %d", err_code);
        nrfx_saadc_uninit();
        return 0;
    }

    // Wait for conversion (takes ~40us at 10-bit, timeout after 1ms)
    uint32_t timeout = 1000;
    while (!m_saadc_done && timeout > 0) {
        timeout--;
        __NOP();
    }

    // Uninit SAADC to save power
    nrfx_saadc_uninit();

    if (!m_saadc_done) {
        NRF_LOG_WARNING("SAADC timeout");
        return 0;
    }

    // Clamp negative values (can happen with noise)
    int16_t raw = (m_sample < 0) ? 0 : m_sample;

    // Convert: voltage_mV = raw * 3600 / 1024
    // 0.6V ref * 6 (1/6 gain) = 3.6V full scale = 3600mV
    uint16_t voltage_mv = (uint16_t)((uint32_t)raw * 3600 / 1024);

    NRF_LOG_DEBUG("Battery: raw=%d, %dmV", raw, voltage_mv);

    return voltage_mv;
}

uint8_t nchorder_battery_level_percent(uint16_t voltage_mv)
{
    // REGOUT0 is set to 3.0V, so VDD reads ~3000mV when battery is healthy.
    // Below 2700mV the regulator is dropping out.
    // Map 2700-3000mV to 0-100%.
    if (voltage_mv >= 3000) {
        return 100;
    }
    if (voltage_mv <= 2700) {
        return 0;
    }

    // Linear interpolation: (voltage - 2700) / (3000 - 2700) * 100
    return (uint8_t)((uint32_t)(voltage_mv - 2700) * 100 / 300);
}
