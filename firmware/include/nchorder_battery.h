/**
 * Northern Chorder - Battery Measurement
 *
 * Real battery voltage measurement via SAADC.
 * Uses one-shot pattern: init SAADC, sample VDD, uninit each measurement
 * to minimize idle current draw.
 *
 * Hardware: nRF52840 internal VDD channel with 1/6 gain and 0.6V reference.
 */

#ifndef NCHORDER_BATTERY_H
#define NCHORDER_BATTERY_H

#include <stdint.h>
#include "sdk_errors.h"

/**
 * @brief Initialize the battery measurement module.
 *
 * Does not start SAADC - that happens on each measurement.
 *
 * @return NRF_SUCCESS on success.
 */
ret_code_t nchorder_battery_init(void);

/**
 * @brief Measure battery voltage.
 *
 * Performs a one-shot SAADC measurement of VDD.
 * SAADC is initialized, sampled, and uninitialized each call
 * to avoid idle current draw.
 *
 * @return Battery voltage in millivolts, or 0 on error.
 */
uint16_t nchorder_battery_measure(void);

/**
 * @brief Convert battery voltage to percentage.
 *
 * Linear approximation for LiPo battery:
 * - 4200mV = 100%
 * - 3000mV = 0%
 *
 * Note: REGOUT0 is set to 3.0V, so VDD measurement reflects
 * the regulator output, not raw battery. With battery > 3.0V,
 * VDD reads ~3000mV. Battery depletion shows as VDD dropping
 * below 3000mV.
 *
 * @param[in] voltage_mv  Battery voltage in millivolts.
 * @return Battery level as percentage (0-100).
 */
uint8_t nchorder_battery_level_percent(uint16_t voltage_mv);

#endif // NCHORDER_BATTERY_H
