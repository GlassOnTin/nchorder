/**
 * Northern Chorder - USB CDC Driver
 *
 * Implements serial communication protocol for configuration app.
 * Provides touch streaming, config read/write, and chord upload.
 */

#include "nchorder_cdc.h"
#include "nchorder_config.h"

#include "app_usbd.h"
#include "app_usbd_core.h"
#include "app_usbd_cdc_acm.h"
#include "nrf_log.h"

// CDC interface and endpoint numbers
// HID keyboard uses interface 0, endpoint 1
// CDC uses interfaces 1-2, endpoints 2-4
#define CDC_COMM_INTERFACE  1
#define CDC_DATA_INTERFACE  2
#define CDC_COMM_EPIN       NRF_DRV_USBD_EPIN2
#define CDC_DATA_EPIN       NRF_DRV_USBD_EPIN3
#define CDC_DATA_EPOUT      NRF_DRV_USBD_EPOUT3

// RX/TX buffer sizes
#define CDC_RX_BUFFER_SIZE  64
#define CDC_TX_BUFFER_SIZE  64

// Forward declaration
static void cdc_acm_user_ev_handler(app_usbd_class_inst_t const *p_inst,
                                    app_usbd_cdc_acm_user_event_t event);

// CDC ACM instance
APP_USBD_CDC_ACM_GLOBAL_DEF(m_app_cdc_acm,
                            cdc_acm_user_ev_handler,
                            CDC_COMM_INTERFACE,
                            CDC_DATA_INTERFACE,
                            CDC_COMM_EPIN,
                            CDC_DATA_EPIN,
                            CDC_DATA_EPOUT,
                            APP_USBD_CDC_COMM_PROTOCOL_NONE);

// State
static bool m_cdc_port_open = false;
static bool m_cdc_streaming = false;
static uint8_t m_cdc_stream_rate = 60;  // Hz

// Buffers
static uint8_t m_rx_buffer[CDC_RX_BUFFER_SIZE];
static uint8_t m_tx_buffer[CDC_TX_BUFFER_SIZE];
static volatile bool m_tx_busy = false;

// Runtime configuration with defaults
static cdc_config_t m_config = {
    .threshold_press = 500,
    .threshold_release = 250,
    .debounce_ms = 30,
    .poll_rate_ms = 15,
    .mouse_speed = 10,
    .mouse_accel = 3,
    .volume_sensitivity = 5,
    .reserved = {0}
};

/**
 * Send response data
 */
static ret_code_t cdc_send(const uint8_t *data, size_t len)
{
    if (!m_cdc_port_open || m_tx_busy) {
        return NRF_ERROR_INVALID_STATE;
    }

    if (len > CDC_TX_BUFFER_SIZE) {
        len = CDC_TX_BUFFER_SIZE;
    }

    memcpy(m_tx_buffer, data, len);
    m_tx_busy = true;

    ret_code_t ret = app_usbd_cdc_acm_write(&m_app_cdc_acm, m_tx_buffer, len);
    if (ret != NRF_SUCCESS) {
        m_tx_busy = false;
    }
    return ret;
}

/**
 * Send single byte response
 */
static void cdc_send_ack(void)
{
    uint8_t ack = CDC_RSP_ACK;
    cdc_send(&ack, 1);
}

static void cdc_send_nak(void)
{
    uint8_t nak = CDC_RSP_NAK;
    cdc_send(&nak, 1);
}

/**
 * Process received command
 */
static void process_command(const uint8_t *data, size_t len)
{
    if (len < 1) return;

    uint8_t cmd = data[0];

    switch (cmd) {
        case CDC_CMD_GET_VERSION: {
            cdc_version_t ver = {
                .major = CDC_PROTOCOL_VERSION_MAJOR,
                .minor = CDC_PROTOCOL_VERSION_MINOR,
                .hw_rev = 1  // XIAO nRF52840
            };
            cdc_send((uint8_t*)&ver, sizeof(ver));
            break;
        }

        case CDC_CMD_GET_CONFIG: {
            cdc_send((uint8_t*)&m_config, sizeof(m_config));
            break;
        }

        case CDC_CMD_SET_CONFIG: {
            if (len >= 4) {
                uint8_t config_id = data[1];
                uint16_t value = data[2] | (data[3] << 8);
                if (nchorder_cdc_set_config(config_id, value)) {
                    cdc_send_ack();
                } else {
                    cdc_send_nak();
                }
            } else {
                cdc_send_nak();
            }
            break;
        }

        case CDC_CMD_GET_TOUCHES: {
            // Single touch frame request - will be filled by caller
            // For now send empty frame
            cdc_touch_frame_t frame = {0};
            frame.sync = CDC_STREAM_SYNC;
            cdc_send((uint8_t*)&frame, sizeof(frame));
            break;
        }

        case CDC_CMD_STREAM_START: {
            if (len >= 2) {
                m_cdc_stream_rate = data[1];
                if (m_cdc_stream_rate < 1) m_cdc_stream_rate = 1;
                if (m_cdc_stream_rate > 100) m_cdc_stream_rate = 100;
            }
            m_cdc_streaming = true;
            NRF_LOG_INFO("CDC: Stream started at %d Hz", m_cdc_stream_rate);
            cdc_send_ack();
            break;
        }

        case CDC_CMD_STREAM_STOP: {
            m_cdc_streaming = false;
            NRF_LOG_INFO("CDC: Stream stopped");
            cdc_send_ack();
            break;
        }

        case CDC_CMD_GET_CHORDS: {
            // TODO: Implement chord readback
            // For now, send NAK
            cdc_send_nak();
            break;
        }

        case CDC_CMD_SET_CHORDS: {
            // TODO: Implement chord upload
            // data[1-2] = offset, data[3-4] = count, data[5+] = chord data
            cdc_send_nak();
            break;
        }

        case CDC_CMD_SAVE_FLASH: {
            // TODO: Implement flash save
            cdc_send_nak();
            break;
        }

        case CDC_CMD_LOAD_FLASH: {
            // TODO: Implement flash load
            cdc_send_nak();
            break;
        }

        case CDC_CMD_RESET_DEFAULT: {
            // Reset config to defaults
            m_config.threshold_press = 500;
            m_config.threshold_release = 250;
            m_config.debounce_ms = 30;
            m_config.poll_rate_ms = 15;
            m_config.mouse_speed = 10;
            m_config.mouse_accel = 3;
            m_config.volume_sensitivity = 5;
            cdc_send_ack();
            break;
        }

        default:
            NRF_LOG_WARNING("CDC: Unknown command 0x%02X", cmd);
            cdc_send_nak();
            break;
    }
}

/**
 * CDC ACM event handler
 */
static void cdc_acm_user_ev_handler(app_usbd_class_inst_t const *p_inst,
                                    app_usbd_cdc_acm_user_event_t event)
{
    switch (event) {
        case APP_USBD_CDC_ACM_USER_EVT_PORT_OPEN:
            NRF_LOG_INFO("CDC: Port opened");
            m_cdc_port_open = true;
            m_cdc_streaming = false;
            // Start receiving
            app_usbd_cdc_acm_read_any(&m_app_cdc_acm, m_rx_buffer, sizeof(m_rx_buffer));
            break;

        case APP_USBD_CDC_ACM_USER_EVT_PORT_CLOSE:
            NRF_LOG_INFO("CDC: Port closed");
            m_cdc_port_open = false;
            m_cdc_streaming = false;
            break;

        case APP_USBD_CDC_ACM_USER_EVT_TX_DONE:
            m_tx_busy = false;
            break;

        case APP_USBD_CDC_ACM_USER_EVT_RX_DONE: {
            size_t rx_size = app_usbd_cdc_acm_rx_size(&m_app_cdc_acm);
            if (rx_size > 0) {
                process_command(m_rx_buffer, rx_size);
            }
            // Continue receiving
            app_usbd_cdc_acm_read_any(&m_app_cdc_acm, m_rx_buffer, sizeof(m_rx_buffer));
            break;
        }

        default:
            break;
    }
}

uint32_t nchorder_cdc_init(void)
{
    ret_code_t ret;

    app_usbd_class_inst_t const *class_cdc_acm =
        app_usbd_cdc_acm_class_inst_get(&m_app_cdc_acm);

    ret = app_usbd_class_append(class_cdc_acm);
    if (ret != NRF_SUCCESS) {
        NRF_LOG_ERROR("CDC: Failed to append class: %d", ret);
        return ret;
    }

    NRF_LOG_INFO("CDC: Initialized");
    return NRF_SUCCESS;
}

void nchorder_cdc_process(void)
{
    // Main loop processing - streaming is handled in button driver callback
}

bool nchorder_cdc_is_open(void)
{
    return m_cdc_port_open;
}

bool nchorder_cdc_is_streaming(void)
{
    return m_cdc_streaming && m_cdc_port_open;
}

void nchorder_cdc_send_touch_frame(const cdc_touch_frame_t *frame)
{
    if (!m_cdc_streaming || !m_cdc_port_open || m_tx_busy) {
        return;
    }

    cdc_send((const uint8_t*)frame, sizeof(cdc_touch_frame_t));
}

const cdc_config_t* nchorder_cdc_get_config(void)
{
    return &m_config;
}

bool nchorder_cdc_set_config(uint8_t config_id, uint16_t value)
{
    switch (config_id) {
        case CDC_CFG_THRESHOLD_PRESS:
            if (value >= 100 && value <= 1000) {
                m_config.threshold_press = value;
                return true;
            }
            break;

        case CDC_CFG_THRESHOLD_RELEASE:
            if (value >= 50 && value <= 500) {
                m_config.threshold_release = value;
                return true;
            }
            break;

        case CDC_CFG_DEBOUNCE_MS:
            if (value >= 10 && value <= 100) {
                m_config.debounce_ms = value;
                return true;
            }
            break;

        case CDC_CFG_POLL_RATE_MS:
            if (value >= 5 && value <= 50) {
                m_config.poll_rate_ms = value;
                return true;
            }
            break;

        case CDC_CFG_MOUSE_SPEED:
            if (value >= 1 && value <= 20) {
                m_config.mouse_speed = value;
                return true;
            }
            break;

        case CDC_CFG_MOUSE_ACCEL:
            if (value <= 10) {
                m_config.mouse_accel = value;
                return true;
            }
            break;

        case CDC_CFG_VOLUME_SENSITIVITY:
            if (value >= 1 && value <= 10) {
                m_config.volume_sensitivity = value;
                return true;
            }
            break;

        default:
            break;
    }

    return false;
}
