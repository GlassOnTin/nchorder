/**
 * Northern Chorder - RGB LED Driver
 *
 * WS2812/SK6812 addressable LED control via I2S peripheral.
 *
 * I2S Configuration for WS2812:
 * - MCK = 3.2MHz (32MHz / 10)
 * - Each I2S bit = 312.5ns
 * - Each WS2812 bit = 4 I2S bits = 1.25us (800kHz)
 * - Logic 0: 0b1000 (high 312.5ns, low 937.5ns)
 * - Logic 1: 0b1110 (high 937.5ns, low 312.5ns)
 *
 * Buffer layout:
 * - 3 LEDs x 24 bits (GRB) = 72 WS2812 bits
 * - 72 x 4 I2S bits = 288 bits = 36 bytes
 * - Plus reset period (50us low) = ~160 I2S bits = 20 bytes
 * - Total buffer = 64 bytes (aligned)
 */

#include "nchorder_led.h"
#include "nchorder_config.h"
#include "nrfx_i2s.h"
#include "nrf_gpio.h"
#include "nrf_delay.h"
#include <string.h>

#include "nrf_log.h"
#include "nrf_log_ctrl.h"

// I2S configuration for WS2812 timing
// MCK = 32MHz / 10 = 3.2MHz, bit period = 312.5ns
// 4 bits per WS2812 bit = 1.25us period
#define I2S_MCK_FREQ     NRF_I2S_MCK_32MDIV10  // 3.2 MHz

// WS2812 bit encoding (4 I2S bits per WS2812 bit)
// Note: MSB first, so bits are reversed in byte
#define WS_BIT_0         0x8   // 0b1000 -> high-low-low-low
#define WS_BIT_1         0xE   // 0b1110 -> high-high-high-low

// Buffer sizes
// 3 LEDs x 24 bits = 72 WS2812 bits
// 72 bits x 4 I2S bits = 288 I2S bits
// 288 bits / 8 = 36 bytes for LED data
// Plus 24 bytes for reset pulse (~75us at 3.2MHz)
// Total = 60 bytes, round up to 64 for alignment
#define LED_DATA_BYTES       36
#define RESET_BYTES          24
#define I2S_BUFFER_SIZE      ((LED_DATA_BYTES + RESET_BYTES + 3) / 4)  // Size in 32-bit words

// Color buffer (GRB order for WS2812)
typedef struct {
    uint8_t g;
    uint8_t r;
    uint8_t b;
} led_color_t;

// Module state
static led_color_t m_colors[NCHORDER_LED_COUNT];
static uint32_t m_i2s_buffer[I2S_BUFFER_SIZE];
static volatile bool m_transfer_done = true;
static bool m_initialized = false;

/**
 * @brief Encode a single byte into I2S buffer.
 *
 * Each bit in the byte becomes 4 I2S bits (nibble).
 * MSB first as required by WS2812.
 *
 * @param[in]  byte     The byte to encode.
 * @param[out] p_buffer Pointer to 4-byte output buffer.
 */
static void encode_byte(uint8_t byte, uint8_t *p_buffer)
{
    // Each byte becomes 4 bytes (32 I2S bits = 8 WS2812 bits)
    // Process 2 bits at a time, producing 1 byte (2 nibbles)
    for (int i = 0; i < 4; i++) {
        uint8_t bit_high = (byte & 0x80) ? WS_BIT_1 : WS_BIT_0;
        uint8_t bit_low  = (byte & 0x40) ? WS_BIT_1 : WS_BIT_0;
        p_buffer[i] = (bit_high << 4) | bit_low;
        byte <<= 2;
    }
}

/**
 * @brief Encode all LED colors into I2S buffer.
 */
static void encode_colors(void)
{
    uint8_t *p_buf = (uint8_t *)m_i2s_buffer;

    // Clear buffer (reset period will be zeros at end)
    memset(m_i2s_buffer, 0, sizeof(m_i2s_buffer));

    // Encode each LED's GRB data
    for (int led = 0; led < NCHORDER_LED_COUNT; led++) {
        encode_byte(m_colors[led].g, &p_buf[led * 12 + 0]);  // Green
        encode_byte(m_colors[led].r, &p_buf[led * 12 + 4]);  // Red
        encode_byte(m_colors[led].b, &p_buf[led * 12 + 8]);  // Blue
    }
    // Remaining bytes are already zero (reset pulse)
}

/**
 * @brief I2S event handler.
 */
static void i2s_handler(nrfx_i2s_buffers_t const *p_released, uint32_t status)
{
    // We don't need continuous transfer - just mark done
    // The handler is called when buffers are released
    if (p_released != NULL) {
        m_transfer_done = true;
    }
}

ret_code_t nchorder_led_init(void)
{
    ret_code_t err_code;

    if (m_initialized) {
        return NRF_SUCCESS;
    }

    // Configure I2S for WS2812 timing
    // Note: SCK and LRCK pins must be assigned even if not used externally
    // because the I2S peripheral requires them for timing
    nrfx_i2s_config_t config = {
        .sck_pin      = NRFX_I2S_PIN_NOT_USED,   // Not needed externally
        .lrck_pin     = NRFX_I2S_PIN_NOT_USED,   // Not needed externally
        .mck_pin      = NRFX_I2S_PIN_NOT_USED,   // Not needed for WS2812
        .sdout_pin    = PIN_LED_DATA,            // LED data line
        .sdin_pin     = NRFX_I2S_PIN_NOT_USED,   // No input needed
        .irq_priority = NRFX_I2S_CONFIG_IRQ_PRIORITY,
        .mode         = NRF_I2S_MODE_MASTER,
        .format       = NRF_I2S_FORMAT_I2S,
        .alignment    = NRF_I2S_ALIGN_LEFT,
        .sample_width = NRF_I2S_SWIDTH_8BIT,
        .channels     = NRF_I2S_CHANNELS_LEFT,   // Only left channel for SDOUT
        .mck_setup    = I2S_MCK_FREQ,
        .ratio        = NRF_I2S_RATIO_32X,       // MCK/LRCK ratio
    };

    err_code = nrfx_i2s_init(&config, i2s_handler);
    if (err_code != NRF_SUCCESS) {
        NRF_LOG_ERROR("I2S init failed: %d", err_code);
        return err_code;
    }

    // Initialize colors to off
    memset(m_colors, 0, sizeof(m_colors));

    m_initialized = true;
    m_transfer_done = true;

    NRF_LOG_INFO("LED driver initialized (pin %d)", PIN_LED_DATA);

    // Turn off LEDs initially
    nchorder_led_off();

    return NRF_SUCCESS;
}

void nchorder_led_set(uint8_t led_index, uint8_t r, uint8_t g, uint8_t b)
{
    if (led_index >= NCHORDER_LED_COUNT) {
        return;
    }

    m_colors[led_index].r = r;
    m_colors[led_index].g = g;
    m_colors[led_index].b = b;
}

void nchorder_led_set_all(uint8_t r, uint8_t g, uint8_t b)
{
    for (int i = 0; i < NCHORDER_LED_COUNT; i++) {
        m_colors[i].r = r;
        m_colors[i].g = g;
        m_colors[i].b = b;
    }
}

ret_code_t nchorder_led_update(void)
{
    if (!m_initialized) {
        return NRF_ERROR_INVALID_STATE;
    }

    // Wait for previous transfer to complete
    uint32_t timeout = 10000;
    while (!m_transfer_done && timeout > 0) {
        timeout--;
    }

    if (!m_transfer_done) {
        NRF_LOG_WARNING("LED update timeout");
        nrfx_i2s_stop();
        m_transfer_done = true;
    }

    // Encode colors into I2S buffer
    encode_colors();

    // Start I2S transfer
    nrfx_i2s_buffers_t buffers = {
        .p_rx_buffer = NULL,
        .p_tx_buffer = m_i2s_buffer,
    };

    m_transfer_done = false;
    ret_code_t err_code = nrfx_i2s_start(&buffers, I2S_BUFFER_SIZE, 0);

    if (err_code != NRF_SUCCESS) {
        m_transfer_done = true;
        NRF_LOG_ERROR("I2S start failed: %d", err_code);
        return err_code;
    }

    // Wait for transfer to complete (blocking for simplicity)
    timeout = 10000;
    while (!m_transfer_done && timeout > 0) {
        timeout--;
    }

    // Stop I2S (one-shot transfer)
    nrfx_i2s_stop();
    m_transfer_done = true;

    return NRF_SUCCESS;
}

void nchorder_led_off(void)
{
    nchorder_led_set_all(LED_COLOR_OFF);
    nchorder_led_update();
}

void nchorder_led_indicate_ble_connected(void)
{
    nchorder_led_set(LED_L1, LED_DIM_GREEN);
    nchorder_led_set(LED_L2, LED_COLOR_OFF);
    nchorder_led_set(LED_L3, LED_COLOR_OFF);
    nchorder_led_update();
}

void nchorder_led_indicate_ble_advertising(void)
{
    nchorder_led_set(LED_L1, LED_DIM_BLUE);
    nchorder_led_set(LED_L2, LED_COLOR_OFF);
    nchorder_led_set(LED_L3, LED_COLOR_OFF);
    nchorder_led_update();
}

void nchorder_led_indicate_usb_connected(void)
{
    nchorder_led_set(LED_L1, LED_COLOR_OFF);
    nchorder_led_set(LED_L2, LED_DIM_WHITE);
    nchorder_led_set(LED_L3, LED_COLOR_OFF);
    nchorder_led_update();
}

void nchorder_led_indicate_error(void)
{
    nchorder_led_set_all(LED_DIM_RED);
    nchorder_led_update();
}

bool nchorder_led_is_ready(void)
{
    return m_transfer_done;
}
