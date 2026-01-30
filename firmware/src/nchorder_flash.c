/**
 * Northern Chorder - Flash Storage Implementation
 *
 * Uses Nordic FDS to store chord configurations persistently.
 * Config is stored as: 4-byte header (size + padding) + config data.
 */

#include "nchorder_flash.h"
#include "fds.h"
#include "nrf_log.h"
#include "nrf_delay.h"
#include "app_scheduler.h"
#include <string.h>

// FDS file and record identifiers
#define CONFIG_FILE_ID      0x0001
#define CONFIG_RECORD_KEY   0x0001

// Maximum config size (must match CDC upload buffer)
#define CONFIG_MAX_SIZE     4096

// Config storage format (word-aligned for FDS)
typedef struct __attribute__((packed, aligned(4))) {
    uint16_t size;      // Actual config size in bytes
    uint16_t reserved;  // Padding for word alignment
    uint8_t  data[];    // Config data (flexible array)
} flash_config_t;

// Static buffer for write operations (must persist until FDS callback)
static uint32_t m_flash_buffer[(CONFIG_MAX_SIZE + sizeof(flash_config_t) + 3) / 4];

// Module state
static volatile bool m_fds_initialized = false;
static volatile flash_op_status_t m_op_status = FLASH_OP_IDLE;
static volatile uint32_t m_last_error = 0;

/**
 * FDS event handler - called for all FDS events
 */
static void fds_evt_handler(fds_evt_t const *p_evt)
{
    switch (p_evt->id) {
        case FDS_EVT_INIT:
            if (p_evt->result == NRF_SUCCESS) {
                m_fds_initialized = true;
                NRF_LOG_INFO("Flash: FDS initialized");
            } else {
                NRF_LOG_ERROR("Flash: FDS init failed: %d", p_evt->result);
            }
            break;

        case FDS_EVT_WRITE:
        case FDS_EVT_UPDATE:
            if (p_evt->result == NRF_SUCCESS) {
                m_op_status = FLASH_OP_DONE;
                NRF_LOG_INFO("Flash: Config saved");
            } else {
                m_op_status = FLASH_OP_ERROR;
                NRF_LOG_ERROR("Flash: Save failed: %d", p_evt->result);
            }
            break;

        case FDS_EVT_DEL_RECORD:
        case FDS_EVT_DEL_FILE:
            // Not used, but handle for completeness
            break;

        case FDS_EVT_GC:
            NRF_LOG_DEBUG("Flash: Garbage collection complete");
            break;

        default:
            break;
    }
}

void nchorder_flash_init(void)
{
    ret_code_t ret;

    // Register our event handler
    ret = fds_register(fds_evt_handler);
    if (ret != NRF_SUCCESS) {
        NRF_LOG_ERROR("Flash: Failed to register FDS handler: %d", ret);
        return;
    }

    // FDS should already be initialized by peer_manager
    // Check if it's ready
    fds_stat_t stat;
    ret = fds_stat(&stat);
    if (ret == NRF_SUCCESS) {
        m_fds_initialized = true;
        NRF_LOG_INFO("Flash: Ready (pages=%d, records=%d)",
                     stat.pages_available, stat.valid_records);
    } else {
        NRF_LOG_WARNING("Flash: FDS not ready yet (ret=%d)", ret);
        // Will be set when FDS_EVT_INIT fires
    }
}

bool nchorder_flash_ready(void)
{
    return m_fds_initialized;
}

bool nchorder_flash_save_config(const uint8_t *data, uint16_t size)
{
    if (!m_fds_initialized) {
        NRF_LOG_ERROR("Flash: Not initialized");
        return false;
    }

    if (data == NULL || size == 0 || size > CONFIG_MAX_SIZE) {
        NRF_LOG_ERROR("Flash: Invalid config size: %d", size);
        return false;
    }

    // Prepare word-aligned buffer with size header
    flash_config_t *cfg = (flash_config_t *)m_flash_buffer;
    cfg->size = size;
    cfg->reserved = 0;
    memcpy(cfg->data, data, size);

    // Calculate word-aligned length (header + data)
    uint16_t total_bytes = sizeof(flash_config_t) + size;
    uint16_t length_words = (total_bytes + 3) / 4;

    // Prepare FDS record
    fds_record_t record = {
        .file_id = CONFIG_FILE_ID,
        .key = CONFIG_RECORD_KEY,
        .data = {
            .p_data = m_flash_buffer,
            .length_words = length_words
        }
    };

    // Check if record already exists
    fds_record_desc_t desc = {0};
    fds_find_token_t tok = {0};
    ret_code_t ret;

    if (fds_record_find(CONFIG_FILE_ID, CONFIG_RECORD_KEY, &desc, &tok) == NRF_SUCCESS) {
        // Update existing record
        ret = fds_record_update(&desc, &record);
        NRF_LOG_DEBUG("Flash: Updating existing record");
    } else {
        // Write new record
        ret = fds_record_write(&desc, &record);
        NRF_LOG_DEBUG("Flash: Writing new record");
    }

    if (ret == NRF_SUCCESS) {
        m_op_status = FLASH_OP_SAVE_PENDING;
        NRF_LOG_INFO("Flash: Save queued (%d bytes)", size);
        return true;
    } else {
        NRF_LOG_ERROR("Flash: Save failed to queue: 0x%x (%d words)", ret, length_words);
        m_last_error = ret;
        return false;
    }
}

uint16_t nchorder_flash_load_config(uint8_t *buffer, uint16_t max_size)
{
    if (!m_fds_initialized) {
        NRF_LOG_WARNING("Flash: Not initialized, can't load");
        return 0;
    }

    if (buffer == NULL || max_size == 0) {
        return 0;
    }

    // Find the config record
    fds_record_desc_t desc = {0};
    fds_find_token_t tok = {0};

    if (fds_record_find(CONFIG_FILE_ID, CONFIG_RECORD_KEY, &desc, &tok) != NRF_SUCCESS) {
        NRF_LOG_INFO("Flash: No saved config found");
        return 0;
    }

    // Open the record for reading
    fds_flash_record_t flash_rec = {0};
    if (fds_record_open(&desc, &flash_rec) != NRF_SUCCESS) {
        NRF_LOG_ERROR("Flash: Failed to open record");
        return 0;
    }

    // Read size from header
    flash_config_t const *cfg = (flash_config_t const *)flash_rec.p_data;
    uint16_t size = cfg->size;

    if (size > max_size) {
        NRF_LOG_WARNING("Flash: Config too large (%d > %d), truncating", size, max_size);
        size = max_size;
    }

    if (size > CONFIG_MAX_SIZE) {
        NRF_LOG_ERROR("Flash: Invalid stored size: %d", size);
        fds_record_close(&desc);
        return 0;
    }

    // Copy config data
    memcpy(buffer, cfg->data, size);

    // Close record
    fds_record_close(&desc);

    NRF_LOG_INFO("Flash: Loaded %d byte config", size);
    return size;
}

flash_op_status_t nchorder_flash_get_status(void)
{
    return m_op_status;
}

void nchorder_flash_clear_status(void)
{
    m_op_status = FLASH_OP_IDLE;
}
