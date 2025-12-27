/**
 * Northern Chorder - USB Mass Storage Class Driver
 *
 * Provides a 192KB FAT16 RAM disk for config file upload via USB.
 * Part of composite USB device (HID keyboard uses interface 0, MSC uses interface 1).
 */

#include "nchorder_msc.h"
#include "nchorder_config.h"
#include "nchorder_chords.h"

#include "app_usbd.h"
#include "app_usbd_msc.h"
#include "nrf_block_dev.h"
#include "nrf_block_dev_ram.h"
#include "nrf_log.h"

#include "ff.h"
#include "diskio_blkdev.h"

// ============================================================================
// CONFIGURATION
// ============================================================================

/**
 * @brief RAM block device size (192KB = 384 sectors @ 512 bytes)
 *
 * Minimum ~190KB required for FAT16 compatibility with Windows/Android.
 * This size should work on Android OTG, Windows, Mac, and Linux.
 */
#define RAM_BLOCK_DEVICE_SIZE   (192 * 1024)

/**
 * @brief MSC work buffer size
 */
#define MSC_WORKBUFFER_SIZE     1024

/**
 * @brief USB interface number for MSC (HID uses 0)
 */
#define MSC_INTERFACE_NUM       1

// ============================================================================
// BLOCK DEVICE
// ============================================================================

/**
 * @brief RAM buffer for the block device
 */
static uint8_t m_block_dev_ram_buff[RAM_BLOCK_DEVICE_SIZE];

/**
 * @brief RAM block device definition
 */
NRF_BLOCK_DEV_RAM_DEFINE(
    m_block_dev_ram,
    NRF_BLOCK_DEV_RAM_CONFIG(512, m_block_dev_ram_buff, sizeof(m_block_dev_ram_buff)),
    NFR_BLOCK_DEV_INFO_CONFIG("nChorder", "Config", "1.00")
);

// ============================================================================
// FATFS
// ============================================================================

static FATFS m_filesystem;
static bool m_fatfs_mounted = false;

/**
 * @brief Block device list for FatFS diskio
 */
static diskio_blkdev_t m_drives[] = {
    DISKIO_BLOCKDEV_CONFIG(NRF_BLOCKDEV_BASE_ADDR(m_block_dev_ram, block_dev), NULL)
};

// ============================================================================
// MSC CLASS
// ============================================================================

/**
 * @brief Block device list for MSC (single RAM device)
 */
#define BLOCKDEV_LIST() (                                   \
    NRF_BLOCKDEV_BASE_ADDR(m_block_dev_ram, block_dev)      \
)

/**
 * @brief Endpoint list for MSC
 *
 * Uses endpoint 2 (HID uses endpoint 1)
 * Format: APP_USBD_MSC_ENDPOINT_LIST(IN_EP, OUT_EP)
 */
#define ENDPOINT_LIST() APP_USBD_MSC_ENDPOINT_LIST(2, 2)

// Forward declaration
static void msc_user_ev_handler(app_usbd_class_inst_t const * p_inst,
                                app_usbd_msc_user_event_t event);

/**
 * @brief MSC class instance
 */
APP_USBD_MSC_GLOBAL_DEF(m_app_msc,
                        MSC_INTERFACE_NUM,
                        msc_user_ev_handler,
                        ENDPOINT_LIST(),
                        BLOCKDEV_LIST(),
                        MSC_WORKBUFFER_SIZE);

// ============================================================================
// STATE
// ============================================================================

static bool m_msc_connected = false;
static volatile bool m_config_reload_pending = false;
static bool m_usb_was_active = false;  // True after USB has been actually used

// ============================================================================
// FATFS HELPERS
// ============================================================================

/**
 * @brief Initialize FatFS and format disk if needed
 */
static bool fatfs_init(void)
{
    FRESULT ff_result;
    DSTATUS disk_state;

    // Register block device with diskio
    diskio_blockdev_register(m_drives, ARRAY_SIZE(m_drives));

    // Initialize disk
    disk_state = disk_initialize(0);
    if (disk_state)
    {
        NRF_LOG_ERROR("FatFS: disk_initialize failed: %d", disk_state);
        return false;
    }

    // Try to mount existing filesystem
    ff_result = f_mount(&m_filesystem, "", 1);
    if (ff_result == FR_NO_FILESYSTEM)
    {
        // No filesystem - create one
        NRF_LOG_INFO("FatFS: Formatting RAM disk as FAT16...");

        static uint8_t work_buf[512];
        ff_result = f_mkfs("", FM_FAT, 0, work_buf, sizeof(work_buf));
        if (ff_result != FR_OK)
        {
            NRF_LOG_ERROR("FatFS: f_mkfs failed: %d", ff_result);
            return false;
        }

        // Mount the new filesystem
        ff_result = f_mount(&m_filesystem, "", 1);
        if (ff_result != FR_OK)
        {
            NRF_LOG_ERROR("FatFS: f_mount after format failed: %d", ff_result);
            return false;
        }

        // Create a README file to show the disk is working
        FIL file;
        ff_result = f_open(&file, "README.TXT", FA_CREATE_ALWAYS | FA_WRITE);
        if (ff_result == FR_OK)
        {
            const char *readme =
                "Northern Chorder Config Disk\r\n"
                "============================\r\n"
                "\r\n"
                "Place config files here:\r\n"
                "  0.CFG - 9.CFG  (chord layouts)\r\n"
                "  ACTIVE.TXT     (active config: 0-9)\r\n"
                "\r\n"
                "Disconnect USB to apply changes.\r\n";

            UINT bytes_written;
            f_write(&file, readme, strlen(readme), &bytes_written);
            f_close(&file);
            NRF_LOG_INFO("FatFS: Created README.TXT");
        }
    }
    else if (ff_result != FR_OK)
    {
        NRF_LOG_ERROR("FatFS: f_mount failed: %d", ff_result);
        return false;
    }

    m_fatfs_mounted = true;
    NRF_LOG_INFO("FatFS: Mounted successfully");
    return true;
}

/**
 * @brief Unmount FatFS (before USB host access)
 */
static void fatfs_uninit(void)
{
    if (m_fatfs_mounted)
    {
        f_mount(NULL, "", 0);
        m_fatfs_mounted = false;
        NRF_LOG_DEBUG("FatFS: Unmounted");
    }
}

// ============================================================================
// EVENT HANDLER
// ============================================================================

/**
 * @brief MSC class event handler
 *
 * Note: SDK 17.1.0 MSC class only defines APP_USBD_MSC_USER_EVT_NONE (dummy).
 * All block device I/O is handled internally by the MSC class.
 */
static void msc_user_ev_handler(app_usbd_class_inst_t const * p_inst,
                                app_usbd_msc_user_event_t event)
{
    UNUSED_PARAMETER(p_inst);
    UNUSED_PARAMETER(event);

    // MSC class handles all I/O internally
    // Event handler is required but no user events are defined in SDK 17.1.0
}

// ============================================================================
// PUBLIC API
// ============================================================================

ret_code_t nchorder_msc_init(void)
{
    ret_code_t ret;

    NRF_LOG_INFO("MSC: Initializing (192KB RAM disk)");

    // Format disk with FAT16 before USB exposes it
    if (!fatfs_init())
    {
        NRF_LOG_WARNING("MSC: FatFS init failed (disk will be unformatted)");
        // Continue anyway - host can format it
    }

    // Unmount FatFS so USB host has exclusive access
    fatfs_uninit();

    // Get MSC class instance
    app_usbd_class_inst_t const * class_inst_msc = app_usbd_msc_class_inst_get(&m_app_msc);

    // Append to USB device (after HID has been added)
    ret = app_usbd_class_append(class_inst_msc);
    if (ret != NRF_SUCCESS)
    {
        NRF_LOG_ERROR("MSC: class_append failed: %d", ret);
        return ret;
    }

    NRF_LOG_INFO("MSC: Init complete");
    return NRF_SUCCESS;
}

bool nchorder_msc_is_connected(void)
{
    return m_msc_connected;
}

bool nchorder_msc_sync(void)
{
    // For RAM block device, no sync needed
    // Data is already in RAM
    return true;
}

// ============================================================================
// CONFIG FILE LOADING
// ============================================================================

/**
 * @brief Maximum config file size (16KB should be plenty)
 */
#define MAX_CONFIG_SIZE     (16 * 1024)

/**
 * @brief Config file buffer (static to avoid stack overflow)
 */
static uint8_t m_config_buffer[MAX_CONFIG_SIZE];

/**
 * @brief Currently active config slot (-1 = none)
 */
static int m_active_slot = -1;

/**
 * @brief Read ACTIVE.TXT to get active config slot
 *
 * @return Config slot 0-9, or 0 if file doesn't exist or invalid
 */
static int read_active_slot(void)
{
    FIL file;
    FRESULT ff_result;
    char buf[8];
    UINT bytes_read;

    ff_result = f_open(&file, "ACTIVE.TXT", FA_READ);
    if (ff_result != FR_OK)
    {
        // No ACTIVE.TXT - default to slot 0
        return 0;
    }

    ff_result = f_read(&file, buf, sizeof(buf) - 1, &bytes_read);
    f_close(&file);

    if (ff_result != FR_OK || bytes_read == 0)
    {
        return 0;
    }

    buf[bytes_read] = '\0';

    // Parse first digit
    char c = buf[0];
    if (c >= '0' && c <= '9')
    {
        return c - '0';
    }

    return 0;
}

/**
 * @brief Load config file from specified slot
 *
 * @param slot Config slot 0-9
 * @return true if config loaded successfully
 */
static bool load_config_slot(int slot)
{
    FIL file;
    FRESULT ff_result;
    UINT bytes_read;
    char filename[8];

    // Build filename: "0.CFG" through "9.CFG"
    filename[0] = '0' + slot;
    filename[1] = '.';
    filename[2] = 'C';
    filename[3] = 'F';
    filename[4] = 'G';
    filename[5] = '\0';

    NRF_LOG_INFO("MSC: Loading config from %s", filename);

    ff_result = f_open(&file, filename, FA_READ);
    if (ff_result != FR_OK)
    {
        NRF_LOG_WARNING("MSC: Config file %s not found", filename);
        return false;
    }

    // Get file size
    FSIZE_t file_size = f_size(&file);
    if (file_size > MAX_CONFIG_SIZE)
    {
        NRF_LOG_WARNING("MSC: Config file too large (%d bytes)", (int)file_size);
        f_close(&file);
        return false;
    }

    // Read entire file
    ff_result = f_read(&file, m_config_buffer, (UINT)file_size, &bytes_read);
    f_close(&file);

    if (ff_result != FR_OK || bytes_read != file_size)
    {
        NRF_LOG_WARNING("MSC: Failed to read config file");
        return false;
    }

    // Parse config using chord module
    chord_load_config(m_config_buffer, bytes_read);

    uint16_t key_count = chord_get_mapping_count();
    uint16_t mouse_count = chord_get_mouse_mapping_count();
    uint16_t multichar_count = chord_get_multichar_count();
    uint16_t consumer_count = chord_get_consumer_count();

    NRF_LOG_INFO("MSC: Loaded %d keys, %d mouse, %d multichar, %d consumer",
                 key_count, mouse_count, multichar_count, consumer_count);

    // Report skipped chords
    uint16_t skipped = chord_get_skipped_count();
    if (skipped > 0)
    {
        uint16_t sys, mc, unk;
        chord_get_skipped_details(&sys, &mc, &unk);
        NRF_LOG_WARNING("MSC: Skipped %d chords (sys=%d, unknown=%d)",
                        skipped, sys, unk);
    }

    m_active_slot = slot;
    return true;
}

void nchorder_msc_on_disconnect(void)
{
    // Only reload if USB was actually used (avoid spurious reload on boot)
    if (!m_usb_was_active)
    {
        NRF_LOG_DEBUG("MSC: Ignoring disconnect (USB not yet active)");
        return;
    }

    // Set flag for main loop to process (don't do FatFS ops in interrupt context)
    m_config_reload_pending = true;
    NRF_LOG_INFO("MSC: Config reload requested");
}

void nchorder_msc_set_active(void)
{
    if (!m_usb_was_active)
    {
        m_usb_was_active = true;
        NRF_LOG_INFO("MSC: USB active");
    }
}

void nchorder_msc_process(void)
{
    if (!m_config_reload_pending)
    {
        return;
    }
    m_config_reload_pending = false;

    NRF_LOG_INFO("MSC: Processing config reload");

    // Remount FatFS
    FRESULT ff_result = f_mount(&m_filesystem, "", 1);
    if (ff_result != FR_OK)
    {
        NRF_LOG_WARNING("MSC: Failed to remount filesystem: %d", ff_result);
        m_fatfs_mounted = false;
        return;
    }
    m_fatfs_mounted = true;

    // Read active slot
    int slot = read_active_slot();
    NRF_LOG_INFO("MSC: Active slot = %d", slot);

    // Try to load config from active slot
    if (!load_config_slot(slot))
    {
        // If active slot fails, try slot 0
        if (slot != 0)
        {
            NRF_LOG_INFO("MSC: Trying fallback to slot 0");
            load_config_slot(0);
        }
    }

    // Unmount FatFS (USB might reconnect)
    f_mount(NULL, "", 0);
    m_fatfs_mounted = false;
    NRF_LOG_INFO("MSC: Config reload complete");
}

int nchorder_msc_get_active_slot(void)
{
    return m_active_slot;
}
