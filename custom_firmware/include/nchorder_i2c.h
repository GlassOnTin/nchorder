/**
 * Northern Chorder - I2C Bus Driver
 *
 * Provides I2C communication and PCA9548 multiplexer control for
 * accessing multiple Trill sensors on the same I2C bus.
 */

#ifndef NCHORDER_I2C_H
#define NCHORDER_I2C_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "sdk_errors.h"

/**
 * Initialize I2C bus (TWIM0)
 * Configures pins from board header and initializes Nordic TWI driver.
 * Also initializes MUX reset pin as output.
 *
 * @return NRF_SUCCESS on success, error code on failure
 */
ret_code_t nchorder_i2c_init(void);

/**
 * Write data to I2C device
 *
 * @param addr    7-bit I2C address
 * @param p_data  Pointer to data buffer
 * @param len     Number of bytes to write
 * @return NRF_SUCCESS on success, error code on failure
 */
ret_code_t nchorder_i2c_write(uint8_t addr, const uint8_t *p_data, size_t len);

/**
 * Read data from I2C device
 *
 * @param addr    7-bit I2C address
 * @param p_data  Pointer to receive buffer
 * @param len     Number of bytes to read
 * @return NRF_SUCCESS on success, error code on failure
 */
ret_code_t nchorder_i2c_read(uint8_t addr, uint8_t *p_data, size_t len);

/**
 * Write then read (combined transaction)
 * Writes tx_data, then reads rx_len bytes without releasing bus.
 *
 * @param addr      7-bit I2C address
 * @param p_tx      Pointer to transmit buffer
 * @param tx_len    Number of bytes to write
 * @param p_rx      Pointer to receive buffer
 * @param rx_len    Number of bytes to read
 * @return NRF_SUCCESS on success, error code on failure
 */
ret_code_t nchorder_i2c_write_read(uint8_t addr,
                                    const uint8_t *p_tx, size_t tx_len,
                                    uint8_t *p_rx, size_t rx_len);

/**
 * Select I2C multiplexer channel
 * Writes channel bitmask to PCA9548 at I2C_ADDR_MUX.
 *
 * @param channel  Channel number (0-7), or 0xFF to disable all
 * @return NRF_SUCCESS on success, error code on failure
 */
ret_code_t nchorder_i2c_mux_select(uint8_t channel);

/**
 * Reset I2C multiplexer
 * Pulses the MUX reset pin low, then high.
 * Clears all channel selections and resets internal state.
 */
void nchorder_i2c_mux_reset(void);

/**
 * Scan I2C bus for devices (debug utility)
 * Attempts to address each device from 0x08 to 0x77.
 * Logs found devices via NRF_LOG.
 *
 * @return Number of devices found
 */
uint8_t nchorder_i2c_scan(void);

/**
 * Check if I2C is initialized
 *
 * @return true if initialized
 */
bool nchorder_i2c_is_initialized(void);

#endif // NCHORDER_I2C_H
