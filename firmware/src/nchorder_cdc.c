/**
 * Northern Chorder - USB CDC Driver
 *
 * Implements serial communication protocol for configuration app.
 * Provides touch streaming, config read/write, and chord upload.
 */

#include "nchorder_cdc.h"
#include "nchorder_config.h"
#include "nchorder_chords.h"
#include "nchorder_flash.h"

#include <stdarg.h>
#include "nrf_delay.h"
#include <stdio.h>
#include "app_usbd.h"
#include "app_usbd_core.h"
#include "app_usbd_cdc_acm.h"
#include "app_scheduler.h"
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
#define CDC_TX_BUFFER_SIZE  128  // Must be >= touch frame size (71 bytes)

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

// Config upload buffer (4KB max - typical configs are 1-3KB)
#define CONFIG_UPLOAD_MAX_SIZE  4096
static uint8_t m_upload_buffer[CONFIG_UPLOAD_MAX_SIZE];
static uint16_t m_upload_expected_size = 0;
static uint16_t m_upload_received = 0;
static bool m_upload_in_progress = false;
static volatile bool m_flash_save_pending = false;  // Deferred to main loop

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
    if (!m_cdc_port_open) {
        return NRF_ERROR_INVALID_STATE;
    }

    // Wait for previous TX to complete (needed for rapid upload ACKs)
    uint32_t timeout = 1000;  // ~1ms at 1us delay
    while (m_tx_busy && timeout--) {
        nrf_delay_us(1);
    }
    if (m_tx_busy) {
        return NRF_ERROR_BUSY;
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
 * Debug print function - sends text via CDC
 * Only sends when streaming (debug view) - suppressed during command mode
 * to avoid corrupting command/response protocol.
 */
void nchorder_cdc_debug(const char *fmt, ...)
{
    if (m_tx_busy || !m_cdc_streaming) return;

    char buf[128];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (len > 0 && len < CDC_TX_BUFFER_SIZE) {
        memcpy(m_tx_buffer, buf, len);
        m_tx_busy = true;
        ret_code_t ret = app_usbd_cdc_acm_write(&m_app_cdc_acm, m_tx_buffer, len);
        if (ret != NRF_SUCCESS) {
            m_tx_busy = false;
        }
    }
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
            // Legacy command - redirect to new upload protocol
            // For now, return NAK - use UPLOAD_START/DATA/COMMIT instead
            cdc_send_nak();
            break;
        }

        case CDC_CMD_SAVE_FLASH: {
            // Defer flash save to main loop (FDS requires non-interrupt context)
            if (m_upload_received > 0) {
                m_flash_save_pending = true;
                NRF_LOG_INFO("CDC: Flash save deferred to main loop (%d bytes)",
                            m_upload_received);
                cdc_send_ack();
            } else {
                NRF_LOG_WARNING("CDC: No data to save");
                cdc_send_nak();
            }
            break;
        }

        case CDC_CMD_LOAD_FLASH: {
            // Load config from flash
            uint16_t loaded = nchorder_flash_load_config(m_upload_buffer, CONFIG_UPLOAD_MAX_SIZE);
            if (loaded > 0) {
                m_upload_received = loaded;
                m_upload_expected_size = loaded;
                chord_load_config(m_upload_buffer, loaded);
                cdc_send_ack();
            } else {
                cdc_send_nak();
            }
            break;
        }

        case CDC_CMD_UPLOAD_START: {
            // Start config upload: [size_lo, size_hi]
            if (len < 3) {
                cdc_send_nak();
                break;
            }
            uint16_t total_size = data[1] | (data[2] << 8);
            if (total_size == 0 || total_size > CONFIG_UPLOAD_MAX_SIZE) {
                NRF_LOG_WARNING("CDC: Upload size invalid: %d", total_size);
                cdc_send_nak();
                break;
            }
            m_upload_expected_size = total_size;
            m_upload_received = 0;
            m_upload_in_progress = true;
            NRF_LOG_INFO("CDC: Upload started, expecting %d bytes", total_size);
            cdc_send_ack();
            break;
        }

        case CDC_CMD_UPLOAD_DATA: {
            // Append data chunk: [data...]
            if (!m_upload_in_progress) {
                NRF_LOG_WARNING("CDC: Upload data without start");
                cdc_send_nak();
                break;
            }
            uint16_t chunk_size = len - 1;  // Exclude command byte
            if (m_upload_received + chunk_size > m_upload_expected_size) {
                NRF_LOG_WARNING("CDC: Upload overflow");
                m_upload_in_progress = false;
                cdc_send_nak();
                break;
            }
            memcpy(&m_upload_buffer[m_upload_received], &data[1], chunk_size);
            m_upload_received += chunk_size;
            NRF_LOG_DEBUG("CDC: Received %d/%d bytes", m_upload_received, m_upload_expected_size);
            cdc_send_ack();
            break;
        }

        case CDC_CMD_UPLOAD_COMMIT: {
            // Finalize upload and parse config
            if (!m_upload_in_progress) {
                NRF_LOG_WARNING("CDC: Commit without active upload");
                cdc_send_nak();
                break;
            }
            if (m_upload_received != m_upload_expected_size) {
                NRF_LOG_WARNING("CDC: Incomplete upload: %d/%d",
                               m_upload_received, m_upload_expected_size);
                m_upload_in_progress = false;
                cdc_send_nak();
                break;
            }
            // Parse the config
            chord_load_config(m_upload_buffer, m_upload_received);
            uint16_t key_count = chord_get_mapping_count();
            uint16_t macro_count = chord_get_multichar_count();
            uint16_t consumer_count = chord_get_consumer_count();
            NRF_LOG_INFO("CDC: Config loaded: %d keys, %d macros, %d consumer",
                        key_count, macro_count, consumer_count);
            m_upload_in_progress = false;
            cdc_send_ack();
            break;
        }

        case CDC_CMD_UPLOAD_ABORT: {
            // Cancel upload
            if (m_upload_in_progress) {
                NRF_LOG_INFO("CDC: Upload aborted");
            }
            m_upload_in_progress = false;
            m_upload_received = 0;
            m_upload_expected_size = 0;
            cdc_send_ack();
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
    // Handle deferred flash save (must run in main loop context for FDS)
    if (m_flash_save_pending) {
        m_flash_save_pending = false;

        if (nchorder_flash_save_config(m_upload_buffer, m_upload_received)) {
            // Wait for async FDS completion
            bool success = false;
            for (int i = 0; i < 500; i++) {
                app_sched_execute();
                flash_op_status_t status = nchorder_flash_get_status();
                if (status == FLASH_OP_DONE) {
                    nchorder_flash_clear_status();
                    success = true;
                    break;
                } else if (status == FLASH_OP_ERROR) {
                    nchorder_flash_clear_status();
                    break;
                }
                nrf_delay_ms(10);
            }
            if (success) {
                NRF_LOG_INFO("CDC: Config saved to flash");
            } else {
                NRF_LOG_WARNING("CDC: Flash save timeout/failed");
            }
        } else {
            NRF_LOG_WARNING("CDC: Flash save_config failed");
        }
    }
}

bool nchorder_cdc_is_open(void)
{
    return m_cdc_port_open;
}

bool nchorder_cdc_is_streaming(void)
{
    return m_cdc_streaming && m_cdc_port_open;
}

// Verify frame size at compile time
_Static_assert(sizeof(cdc_touch_frame_t) == 71, "Frame size mismatch!");

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
