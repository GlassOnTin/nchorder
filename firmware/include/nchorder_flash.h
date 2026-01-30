/**
 * Northern Chorder - Flash Storage for Config Persistence
 *
 * Uses Nordic FDS (Flash Data Storage) to persist chord configurations
 * across power cycles. FDS is initialized by peer_manager.
 */

#ifndef NCHORDER_FLASH_H
#define NCHORDER_FLASH_H

#include <stdint.h>
#include <stdbool.h>

// Flash operation status
typedef enum {
    FLASH_OP_IDLE,
    FLASH_OP_SAVE_PENDING,
    FLASH_OP_DONE,
    FLASH_OP_ERROR
} flash_op_status_t;

/**
 * Initialize flash storage module.
 * Call after peer_manager_init() (which initializes FDS).
 * Registers our FDS event handler.
 */
void nchorder_flash_init(void);

/**
 * Check if flash storage is ready for operations.
 * @return true if FDS is initialized and ready
 */
bool nchorder_flash_ready(void);

/**
 * Save config data to flash (async operation).
 * @param data   Config data to save
 * @param size   Size in bytes (max 4096)
 * @return true if save operation was queued successfully
 */
bool nchorder_flash_save_config(const uint8_t *data, uint16_t size);

/**
 * Load config data from flash (sync operation).
 * @param buffer    Buffer to load config into
 * @param max_size  Maximum bytes to load
 * @return Size of loaded config, 0 if no config found
 */
uint16_t nchorder_flash_load_config(uint8_t *buffer, uint16_t max_size);

/**
 * Get status of last flash operation.
 * @return Current operation status
 */
flash_op_status_t nchorder_flash_get_status(void);

/**
 * Clear operation status (call after handling completion).
 */
void nchorder_flash_clear_status(void);

#endif // NCHORDER_FLASH_H
