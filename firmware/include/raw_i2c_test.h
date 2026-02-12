/**
 * Raw I2C Test - Minimal Trill sensor identification
 */

#ifndef RAW_I2C_TEST_H
#define RAW_I2C_TEST_H

/**
 * Run raw I2C test bypassing all Trill driver code.
 * Outputs results via SEGGER RTT.
 * Call early in main() BEFORE any other I2C initialization.
 */
void raw_i2c_test(void);

#endif // RAW_I2C_TEST_H
