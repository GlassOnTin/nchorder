/**
 * Northern Chorder - INI Config Parser
 *
 * Minimal INI parser optimized for embedded use.
 * No dynamic allocation, fixed-size buffers.
 */

#include "nchorder_ini.h"
#include <string.h>
#include <stdlib.h>

// Global runtime configuration
static nchorder_config_t g_config;

// Default values
#define DEFAULT_DEBOUNCE_MS      30
#define DEFAULT_POLL_RATE_MS     15
#define DEFAULT_TRILL_THRESHOLD  300
#define DEFAULT_TRILL_PRESCALER  1
#define DEFAULT_CHORD_TIMEOUT_MS 0
#define DEFAULT_CHORD_REPEAT     false
#define DEFAULT_REPEAT_DELAY_MS  500
#define DEFAULT_REPEAT_RATE_MS   50
#define DEFAULT_LED_BRIGHTNESS   128
#define DEFAULT_LED_FEEDBACK     true
#define DEFAULT_DEBUG_RTT        false

// Max line length for parsing
#define MAX_LINE_LEN    128
#define MAX_SECTION_LEN 32
#define MAX_KEY_LEN     32
#define MAX_VALUE_LEN   64

nchorder_config_t* nchorder_config_get(void)
{
    return &g_config;
}

void nchorder_config_reset(void)
{
    g_config.debounce_ms      = DEFAULT_DEBOUNCE_MS;
    g_config.poll_rate_ms     = DEFAULT_POLL_RATE_MS;
    g_config.trill_threshold  = DEFAULT_TRILL_THRESHOLD;
    g_config.trill_prescaler  = DEFAULT_TRILL_PRESCALER;
    g_config.chord_timeout_ms = DEFAULT_CHORD_TIMEOUT_MS;
    g_config.chord_repeat     = DEFAULT_CHORD_REPEAT;
    g_config.repeat_delay_ms  = DEFAULT_REPEAT_DELAY_MS;
    g_config.repeat_rate_ms   = DEFAULT_REPEAT_RATE_MS;
    g_config.led_brightness   = DEFAULT_LED_BRIGHTNESS;
    g_config.led_feedback     = DEFAULT_LED_FEEDBACK;
    g_config.debug_rtt        = DEFAULT_DEBUG_RTT;
}

/**
 * Skip whitespace at start of string
 */
static const char* skip_whitespace(const char *s)
{
    while (*s == ' ' || *s == '\t') {
        s++;
    }
    return s;
}

/**
 * Trim trailing whitespace (modifies string in place)
 */
static void trim_trailing(char *s)
{
    int len = strlen(s);
    while (len > 0 && (s[len-1] == ' ' || s[len-1] == '\t' ||
                       s[len-1] == '\r' || s[len-1] == '\n')) {
        s[--len] = '\0';
    }
}

/**
 * Parse boolean value (true/false, yes/no, 1/0)
 */
static bool parse_bool(const char *value, bool *result)
{
    if (strcmp(value, "true") == 0 || strcmp(value, "yes") == 0 ||
        strcmp(value, "1") == 0 || strcmp(value, "on") == 0) {
        *result = true;
        return true;
    }
    if (strcmp(value, "false") == 0 || strcmp(value, "no") == 0 ||
        strcmp(value, "0") == 0 || strcmp(value, "off") == 0) {
        *result = false;
        return true;
    }
    return false;
}

/**
 * Parse unsigned integer value
 */
static bool parse_uint(const char *value, uint32_t *result, uint32_t max)
{
    char *endptr;
    unsigned long val = strtoul(value, &endptr, 10);
    if (endptr == value || *endptr != '\0') {
        return false;
    }
    if (val > max) {
        val = max;
    }
    *result = (uint32_t)val;
    return true;
}

/**
 * Apply a key=value pair to config based on current section
 */
static bool apply_setting(const char *section, const char *key, const char *value)
{
    uint32_t uval;
    bool bval;

    // [timing] section
    if (strcmp(section, "timing") == 0) {
        if (strcmp(key, "debounce_ms") == 0) {
            if (parse_uint(value, &uval, 1000)) {
                g_config.debounce_ms = (uint16_t)uval;
                return true;
            }
        }
        else if (strcmp(key, "poll_rate_ms") == 0) {
            if (parse_uint(value, &uval, 1000)) {
                g_config.poll_rate_ms = (uint16_t)uval;
                return true;
            }
        }
    }

    // [trill] section
    else if (strcmp(section, "trill") == 0) {
        if (strcmp(key, "threshold") == 0) {
            if (parse_uint(value, &uval, 1000)) {
                g_config.trill_threshold = (uint16_t)uval;
                return true;
            }
        }
        else if (strcmp(key, "prescaler") == 0) {
            if (parse_uint(value, &uval, 4)) {
                g_config.trill_prescaler = (uint16_t)uval;
                return true;
            }
        }
    }

    // [chord] section
    else if (strcmp(section, "chord") == 0) {
        if (strcmp(key, "timeout_ms") == 0) {
            if (parse_uint(value, &uval, 10000)) {
                g_config.chord_timeout_ms = (uint16_t)uval;
                return true;
            }
        }
        else if (strcmp(key, "repeat") == 0) {
            if (parse_bool(value, &bval)) {
                g_config.chord_repeat = bval;
                return true;
            }
        }
        else if (strcmp(key, "repeat_delay_ms") == 0) {
            if (parse_uint(value, &uval, 5000)) {
                g_config.repeat_delay_ms = (uint16_t)uval;
                return true;
            }
        }
        else if (strcmp(key, "repeat_rate_ms") == 0) {
            if (parse_uint(value, &uval, 1000)) {
                g_config.repeat_rate_ms = (uint16_t)uval;
                return true;
            }
        }
    }

    // [led] section
    else if (strcmp(section, "led") == 0) {
        if (strcmp(key, "brightness") == 0) {
            if (parse_uint(value, &uval, 255)) {
                g_config.led_brightness = (uint8_t)uval;
                return true;
            }
        }
        else if (strcmp(key, "feedback") == 0) {
            if (parse_bool(value, &bval)) {
                g_config.led_feedback = bval;
                return true;
            }
        }
    }

    // [debug] section
    else if (strcmp(section, "debug") == 0) {
        if (strcmp(key, "rtt") == 0) {
            if (parse_bool(value, &bval)) {
                g_config.debug_rtt = bval;
                return true;
            }
        }
    }

    return false;
}

int nchorder_ini_parse(const char *data, uint32_t len)
{
    char line[MAX_LINE_LEN];
    char section[MAX_SECTION_LEN] = "";
    char key[MAX_KEY_LEN];
    char value[MAX_VALUE_LEN];
    int parsed_count = 0;

    const char *ptr = data;
    const char *end = data + len;

    while (ptr < end) {
        // Read one line
        int line_len = 0;
        while (ptr < end && *ptr != '\n' && line_len < MAX_LINE_LEN - 1) {
            line[line_len++] = *ptr++;
        }
        line[line_len] = '\0';

        // Skip the newline
        if (ptr < end && *ptr == '\n') {
            ptr++;
        }

        // Skip leading whitespace
        const char *p = skip_whitespace(line);

        // Skip empty lines and comments
        if (*p == '\0' || *p == '#' || *p == ';') {
            continue;
        }

        // Check for section header [section]
        if (*p == '[') {
            p++;
            int i = 0;
            while (*p && *p != ']' && i < MAX_SECTION_LEN - 1) {
                section[i++] = *p++;
            }
            section[i] = '\0';
            trim_trailing(section);
            continue;
        }

        // Parse key=value
        int ki = 0;
        while (*p && *p != '=' && ki < MAX_KEY_LEN - 1) {
            key[ki++] = *p++;
        }
        key[ki] = '\0';
        trim_trailing(key);

        if (*p != '=') {
            continue;  // Invalid line, skip
        }
        p++;  // Skip '='

        p = skip_whitespace(p);
        int vi = 0;
        while (*p && vi < MAX_VALUE_LEN - 1) {
            value[vi++] = *p++;
        }
        value[vi] = '\0';
        trim_trailing(value);

        // Apply the setting
        if (apply_setting(section, key, value)) {
            parsed_count++;
        }
    }

    return parsed_count;
}

int nchorder_ini_generate_default(char *buf, uint32_t buflen)
{
    static const char default_ini[] =
        "# nChorder Configuration\n"
        "# Edit this file to customize settings.\n"
        "# Changes take effect after USB reconnect.\n"
        "\n"
        "[timing]\n"
        "# Button debounce delay in milliseconds\n"
        "debounce_ms = 30\n"
        "\n"
        "# Sensor polling rate in milliseconds\n"
        "poll_rate_ms = 15\n"
        "\n"
        "[chord]\n"
        "# Chord timeout in ms (0 = disabled, wait forever)\n"
        "timeout_ms = 0\n"
        "\n"
        "# Enable key repeat when chord is held\n"
        "repeat = false\n"
        "\n"
        "# Initial delay before repeat starts (ms)\n"
        "repeat_delay_ms = 500\n"
        "\n"
        "# Interval between repeats (ms)\n"
        "repeat_rate_ms = 50\n"
        "\n"
        "[led]\n"
        "# LED brightness 0-255\n"
        "brightness = 128\n"
        "\n"
        "# Flash LED on chord input\n"
        "feedback = true\n"
        "\n"
        "[debug]\n"
        "# Enable RTT debug output\n"
        "rtt = false\n"
        "\n"
        "# --- Hardware-specific settings below ---\n"
        "\n"
        "[trill]\n"
        "# Touch detection threshold (higher = less sensitive)\n"
        "threshold = 300\n"
        "\n"
        "# Scan prescaler 0-4 (lower = faster, more power)\n"
        "prescaler = 1\n";

    uint32_t len = sizeof(default_ini) - 1;  // Exclude null terminator
    if (len >= buflen) {
        len = buflen - 1;
    }
    memcpy(buf, default_ini, len);
    buf[len] = '\0';
    return (int)len;
}
