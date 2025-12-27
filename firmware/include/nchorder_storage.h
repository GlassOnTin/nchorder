/**
 * @file nchorder_storage.h
 * @brief Flash storage for chord configuration.
 *
 * Uses Nordic FDS (Flash Data Storage) to persist chord mappings.
 * Configs are stored as a single record containing v7 format binary data.
 */

#ifndef NCHORDER_STORAGE_H
#define NCHORDER_STORAGE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "sdk_errors.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum config size (8KB should cover any reasonable config) */
#define NCHORDER_CONFIG_MAX_SIZE    8192

/** FDS file ID for chord config */
#define NCHORDER_FDS_FILE_ID        0x1001

/** FDS record key for chord config */
#define NCHORDER_FDS_RECORD_KEY     0x0001

/**
 * @brief Initialize flash storage system.
 *
 * Must be called before any other storage functions.
 * Waits for FDS initialization to complete.
 *
 * @return NRF_SUCCESS on success, error code otherwise.
 */
ret_code_t nchorder_storage_init(void);

/**
 * @brief Load chord configuration from flash.
 *
 * @param[out] p_data       Pointer to receive config data pointer.
 *                          Points to flash memory, do not modify.
 * @param[out] p_size       Pointer to receive config size in bytes.
 *
 * @return NRF_SUCCESS if config found and loaded.
 * @return NRF_ERROR_NOT_FOUND if no config stored.
 * @return Other error codes on failure.
 */
ret_code_t nchorder_storage_load(const uint8_t **p_data, size_t *p_size);

/**
 * @brief Save chord configuration to flash.
 *
 * @param[in] p_data        Pointer to config data to save.
 * @param[in] size          Size of config data in bytes.
 *
 * @return NRF_SUCCESS on success.
 * @return NRF_ERROR_INVALID_LENGTH if size > NCHORDER_CONFIG_MAX_SIZE.
 * @return Other error codes on failure.
 */
ret_code_t nchorder_storage_save(const uint8_t *p_data, size_t size);

/**
 * @brief Delete stored configuration.
 *
 * @return NRF_SUCCESS on success or if no config exists.
 */
ret_code_t nchorder_storage_delete(void);

/**
 * @brief Check if a configuration is stored.
 *
 * @return true if config exists in flash.
 */
bool nchorder_storage_exists(void);

#ifdef __cplusplus
}
#endif

#endif // NCHORDER_STORAGE_H
