/**
 * Northern Chorder - USB Mass Storage Class Driver
 *
 * Provides a 64KB FAT16 RAM disk for config file upload via USB.
 * Works as part of a composite USB device (HID keyboard + MSC).
 */

#ifndef NCHORDER_MSC_H
#define NCHORDER_MSC_H

#include <stdint.h>
#include <stdbool.h>
#include "sdk_errors.h"

/**
 * @brief Initialize USB Mass Storage Class
 *
 * Creates a 64KB RAM block device and registers it as USB MSC.
 * Must be called after nchorder_usb_init() but before USB is started.
 *
 * @return NRF_SUCCESS on success, error code otherwise
 */
ret_code_t nchorder_msc_init(void);

/**
 * @brief Check if MSC is connected and active
 *
 * @return true if USB MSC is connected to host
 */
bool nchorder_msc_is_connected(void);

/**
 * @brief Sync pending writes to storage
 *
 * Call before USB disconnect to ensure all data is written.
 *
 * @return true if sync successful
 */
bool nchorder_msc_sync(void);

/**
 * @brief Handle USB disconnect - reload config from disk
 *
 * Called when USB is disconnected. Remounts FatFS and loads
 * the active config file (determined by ACTIVE.TXT or default 0.CFG).
 *
 * File format: Twiddler .cfg binary format
 * - 0.CFG through 9.CFG: Config slots
 * - ACTIVE.TXT: Contains single digit 0-9 for active slot
 */
void nchorder_msc_on_disconnect(void);

/**
 * @brief Get the currently active config slot
 *
 * @return Config slot number (0-9), or -1 if no config loaded
 */
int nchorder_msc_get_active_slot(void);

/**
 * @brief Process pending MSC operations
 *
 * Call from main loop to handle deferred config reload.
 * This is needed because FatFS operations cannot run in interrupt context.
 */
void nchorder_msc_process(void);

/**
 * @brief Mark USB as active
 *
 * Call when USB is successfully connected and started.
 * This prevents spurious config reloads during initial USB negotiation.
 */
void nchorder_msc_set_active(void);

#endif // NCHORDER_MSC_H
