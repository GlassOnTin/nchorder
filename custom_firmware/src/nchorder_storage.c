/**
 * @file nchorder_storage.c
 * @brief Flash storage implementation using Nordic FDS.
 */

#include "nchorder_storage.h"
#include "fds.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "app_scheduler.h"
#include "nrf_pwr_mgmt.h"

/** Flag set when FDS operation completes */
static volatile bool m_fds_initialized = false;
static volatile bool m_fds_op_pending = false;
static volatile ret_code_t m_fds_op_result = NRF_SUCCESS;

/** Cached record descriptor for loaded config */
static fds_record_desc_t m_record_desc;
static bool m_record_found = false;

/**
 * @brief FDS event handler.
 */
static void fds_evt_handler(fds_evt_t const *p_evt)
{
    switch (p_evt->id)
    {
        case FDS_EVT_INIT:
            if (p_evt->result == NRF_SUCCESS)
            {
                m_fds_initialized = true;
                NRF_LOG_DEBUG("FDS initialized");
            }
            else
            {
                NRF_LOG_ERROR("FDS init failed: %d", p_evt->result);
            }
            break;

        case FDS_EVT_WRITE:
        case FDS_EVT_UPDATE:
            m_fds_op_result = p_evt->result;
            m_fds_op_pending = false;
            if (p_evt->result == NRF_SUCCESS)
            {
                NRF_LOG_DEBUG("FDS write complete");
            }
            else
            {
                NRF_LOG_ERROR("FDS write failed: %d", p_evt->result);
            }
            break;

        case FDS_EVT_DEL_RECORD:
            m_fds_op_result = p_evt->result;
            m_fds_op_pending = false;
            NRF_LOG_DEBUG("FDS delete complete: %d", p_evt->result);
            break;

        case FDS_EVT_GC:
            m_fds_op_result = p_evt->result;
            m_fds_op_pending = false;
            NRF_LOG_DEBUG("FDS garbage collection complete");
            break;

        default:
            break;
    }
}

ret_code_t nchorder_storage_init(void)
{
    ret_code_t err_code;

    // Register FDS event handler
    err_code = fds_register(fds_evt_handler);
    if (err_code != NRF_SUCCESS)
    {
        NRF_LOG_ERROR("FDS register failed: %d", err_code);
        return err_code;
    }

    // Initialize FDS
    err_code = fds_init();
    if (err_code != NRF_SUCCESS)
    {
        NRF_LOG_ERROR("FDS init call failed: %d", err_code);
        return err_code;
    }

    // Wait for initialization to complete
    while (!m_fds_initialized)
    {
        // Process pending events while waiting
        app_sched_execute();
        NRF_LOG_PROCESS();
        nrf_pwr_mgmt_run();
    }

    NRF_LOG_INFO("Storage: FDS ready");
    return NRF_SUCCESS;
}

ret_code_t nchorder_storage_load(const uint8_t **p_data, size_t *p_size)
{
    ret_code_t err_code;
    fds_find_token_t ftok = {0};
    fds_flash_record_t flash_record;

    if (p_data == NULL || p_size == NULL)
    {
        return NRF_ERROR_NULL;
    }

    // Find the config record
    err_code = fds_record_find(NCHORDER_FDS_FILE_ID, NCHORDER_FDS_RECORD_KEY,
                               &m_record_desc, &ftok);
    if (err_code == FDS_ERR_NOT_FOUND)
    {
        NRF_LOG_INFO("Storage: No config found");
        m_record_found = false;
        return NRF_ERROR_NOT_FOUND;
    }
    else if (err_code != NRF_SUCCESS)
    {
        NRF_LOG_ERROR("Storage: Find failed: %d", err_code);
        return err_code;
    }

    // Open the record to access data
    err_code = fds_record_open(&m_record_desc, &flash_record);
    if (err_code != NRF_SUCCESS)
    {
        NRF_LOG_ERROR("Storage: Open failed: %d", err_code);
        return err_code;
    }

    // Return pointer to flash data (FDS stores data in flash directly)
    *p_data = (const uint8_t *)flash_record.p_data;
    *p_size = flash_record.p_header->length_words * sizeof(uint32_t);

    // Close record (data remains valid until deleted/updated)
    err_code = fds_record_close(&m_record_desc);
    if (err_code != NRF_SUCCESS)
    {
        NRF_LOG_WARNING("Storage: Close warning: %d", err_code);
    }

    m_record_found = true;
    NRF_LOG_INFO("Storage: Loaded config (%d bytes)", *p_size);
    return NRF_SUCCESS;
}

ret_code_t nchorder_storage_save(const uint8_t *p_data, size_t size)
{
    ret_code_t err_code;
    fds_record_t record;

    if (p_data == NULL)
    {
        return NRF_ERROR_NULL;
    }

    if (size > NCHORDER_CONFIG_MAX_SIZE || size == 0)
    {
        return NRF_ERROR_INVALID_LENGTH;
    }

    // Prepare record
    record.file_id = NCHORDER_FDS_FILE_ID;
    record.key = NCHORDER_FDS_RECORD_KEY;
    record.data.p_data = p_data;
    // FDS stores in words (4 bytes), round up
    record.data.length_words = (size + 3) / sizeof(uint32_t);

    m_fds_op_pending = true;

    if (m_record_found)
    {
        // Update existing record
        err_code = fds_record_update(&m_record_desc, &record);
    }
    else
    {
        // Write new record
        err_code = fds_record_write(NULL, &record);
    }

    if (err_code != NRF_SUCCESS)
    {
        m_fds_op_pending = false;
        NRF_LOG_ERROR("Storage: Write call failed: %d", err_code);
        return err_code;
    }

    // Wait for operation to complete
    while (m_fds_op_pending)
    {
        NRF_LOG_PROCESS();
        __WFE();
    }

    if (m_fds_op_result == NRF_SUCCESS)
    {
        m_record_found = true;
        NRF_LOG_INFO("Storage: Saved config (%d bytes)", size);
    }

    return m_fds_op_result;
}

ret_code_t nchorder_storage_delete(void)
{
    ret_code_t err_code;
    fds_find_token_t ftok = {0};

    // Find the record first
    err_code = fds_record_find(NCHORDER_FDS_FILE_ID, NCHORDER_FDS_RECORD_KEY,
                               &m_record_desc, &ftok);
    if (err_code == FDS_ERR_NOT_FOUND)
    {
        m_record_found = false;
        return NRF_SUCCESS;  // Nothing to delete
    }
    else if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    m_fds_op_pending = true;

    err_code = fds_record_delete(&m_record_desc);
    if (err_code != NRF_SUCCESS)
    {
        m_fds_op_pending = false;
        return err_code;
    }

    // Wait for deletion
    while (m_fds_op_pending)
    {
        NRF_LOG_PROCESS();
        __WFE();
    }

    m_record_found = false;
    NRF_LOG_INFO("Storage: Config deleted");

    // Run garbage collection to reclaim space
    m_fds_op_pending = true;
    err_code = fds_gc();
    if (err_code == NRF_SUCCESS)
    {
        while (m_fds_op_pending)
        {
            NRF_LOG_PROCESS();
            __WFE();
        }
    }

    return m_fds_op_result;
}

bool nchorder_storage_exists(void)
{
    fds_find_token_t ftok = {0};
    fds_record_desc_t desc;

    ret_code_t err_code = fds_record_find(NCHORDER_FDS_FILE_ID,
                                          NCHORDER_FDS_RECORD_KEY,
                                          &desc, &ftok);
    return (err_code == NRF_SUCCESS);
}
