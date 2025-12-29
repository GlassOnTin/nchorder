/**
 * Northern Chorder - INI Config Parser
 *
 * Simple INI file parser for human-readable configuration.
 * Supports sections, key=value pairs, and # comments.
 */

#ifndef NCHORDER_INI_H
#define NCHORDER_INI_H

#include <stdint.h>
#include <stdbool.h>

/**
 * Runtime configuration structure
 * These values can be modified via CONFIG.INI on the USB disk
 */
typedef struct {
    // Timing settings
    uint16_t debounce_ms;       // Button debounce delay (default: 30)
    uint16_t poll_rate_ms;      // Sensor poll interval (default: 15)

    // Trill sensor settings
    uint16_t trill_threshold;   // Touch detection threshold (default: 50)
    uint16_t trill_prescaler;   // Scan speed 0-4, lower=faster (default: 1)

    // Chord settings
    uint16_t chord_timeout_ms;  // Max time to build chord (default: 0=disabled)
    bool     chord_repeat;      // Enable key repeat on hold (default: false)
    uint16_t repeat_delay_ms;   // Initial repeat delay (default: 500)
    uint16_t repeat_rate_ms;    // Repeat interval (default: 50)

    // LED settings
    uint8_t  led_brightness;    // 0-255 brightness (default: 128)
    bool     led_feedback;      // Flash LED on chord (default: true)

    // Debug settings
    bool     debug_rtt;         // Enable RTT debug output (default: false)
} nchorder_config_t;

/**
 * Get pointer to global runtime config
 * Config is initialized with defaults, then updated from CONFIG.INI if present
 */
nchorder_config_t* nchorder_config_get(void);

/**
 * Reset config to defaults
 */
void nchorder_config_reset(void);

/**
 * Parse INI file content and update config
 *
 * @param data   Pointer to INI file content (null-terminated string)
 * @param len    Length of data
 * @return       Number of settings successfully parsed
 */
int nchorder_ini_parse(const char *data, uint32_t len);

/**
 * Generate default INI file content
 *
 * @param buf    Buffer to write INI content
 * @param buflen Size of buffer
 * @return       Number of bytes written (excluding null terminator)
 */
int nchorder_ini_generate_default(char *buf, uint32_t buflen);

#endif // NCHORDER_INI_H
