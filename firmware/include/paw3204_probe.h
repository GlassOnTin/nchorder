/**
 * PAW3204 Optical Sensor Probe
 */

#ifndef PAW3204_PROBE_H
#define PAW3204_PROBE_H

#include <stdint.h>
#include <stdbool.h>

/**
 * Probe for PAW3204 sensor
 * @return true if sensor detected (Product ID = 0x30)
 */
bool paw3204_probe(void);

/**
 * Read motion deltas
 * @param dx  Output: X movement (-128 to 127)
 * @param dy  Output: Y movement (-128 to 127)
 */
void paw3204_read_motion(int8_t *dx, int8_t *dy);

/**
 * Read register
 */
uint8_t paw3204_read_reg(uint8_t reg);

#endif // PAW3204_PROBE_H
