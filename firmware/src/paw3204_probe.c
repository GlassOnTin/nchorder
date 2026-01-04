/**
 * PAW3204 Optical Sensor Probe
 *
 * Simple bit-bang driver to detect PAW3204 sensor.
 * Protocol: Proprietary 2-wire serial (NOT I2C)
 * - SCK idles HIGH
 * - Data is bidirectional on SDA
 * - Write: Address byte (MSB=1), then data
 * - Read: Address byte (MSB=0), then clock in data
 */

#include "nrf_gpio.h"
#include "nrf_delay.h"
#include "nrf_log.h"
#include "boards.h"

// Pin definitions - adjust based on actual wiring
#define PAW_SCK     NRF_GPIO_PIN_MAP(0, 31)  // Clock
#define PAW_SDA     NRF_GPIO_PIN_MAP(0, 30)  // Bidirectional data (or MOSI)
#define PAW_MISO    NRF_GPIO_PIN_MAP(1, 11)  // MISO for 4-wire SPI

// Timing (conservative - PAW3204 runs up to 360kHz)
#define PAW_DELAY_US  5

// Registers
#define PAW_REG_PRODUCT_ID   0x00
#define PAW_REG_MOTION       0x02
#define PAW_REG_DELTA_X      0x03
#define PAW_REG_DELTA_Y      0x04

// Expected product ID
#define PAW3204_PRODUCT_ID   0x30

static void paw_sda_output(void)
{
    nrf_gpio_cfg_output(PAW_SDA);
}

static void paw_sda_input(void)
{
    nrf_gpio_cfg_input(PAW_SDA, NRF_GPIO_PIN_PULLUP);
}

static void paw_write_byte(uint8_t data)
{
    paw_sda_output();

    for (int i = 7; i >= 0; i--) {
        // Set data bit
        if (data & (1 << i)) {
            nrf_gpio_pin_set(PAW_SDA);
        } else {
            nrf_gpio_pin_clear(PAW_SDA);
        }
        nrf_delay_us(PAW_DELAY_US);

        // Clock low
        nrf_gpio_pin_clear(PAW_SCK);
        nrf_delay_us(PAW_DELAY_US);

        // Clock high (data sampled on rising edge)
        nrf_gpio_pin_set(PAW_SCK);
        nrf_delay_us(PAW_DELAY_US);
    }
}

static uint8_t paw_read_byte(void)
{
    uint8_t data = 0;

    paw_sda_input();
    nrf_delay_us(PAW_DELAY_US);

    for (int i = 7; i >= 0; i--) {
        // Clock low
        nrf_gpio_pin_clear(PAW_SCK);
        nrf_delay_us(PAW_DELAY_US);

        // Read data bit
        if (nrf_gpio_pin_read(PAW_SDA)) {
            data |= (1 << i);
        }

        // Clock high
        nrf_gpio_pin_set(PAW_SCK);
        nrf_delay_us(PAW_DELAY_US);
    }

    return data;
}

// 4-wire SPI read using separate MISO (P1.11)
static uint8_t paw_read_byte_4wire(void)
{
    uint8_t data = 0;

    nrf_gpio_cfg_input(PAW_MISO, NRF_GPIO_PIN_PULLUP);
    nrf_delay_us(PAW_DELAY_US);

    for (int i = 7; i >= 0; i--) {
        // Clock low
        nrf_gpio_pin_clear(PAW_SCK);
        nrf_delay_us(PAW_DELAY_US);

        // Read data bit from MISO (P1.11)
        if (nrf_gpio_pin_read(PAW_MISO)) {
            data |= (1 << i);
        }

        // Clock high
        nrf_gpio_pin_set(PAW_SCK);
        nrf_delay_us(PAW_DELAY_US);
    }

    return data;
}

uint8_t paw3204_read_reg(uint8_t reg)
{
    // Address byte with read bit (MSB = 0)
    paw_write_byte(reg & 0x7F);

    // Small delay between address and data
    nrf_delay_us(PAW_DELAY_US * 2);

    // Read response (2-wire: bidirectional on SDA)
    return paw_read_byte();
}

// 4-wire SPI version
uint8_t paw3204_read_reg_4wire(uint8_t reg)
{
    // Address byte with read bit (MSB = 0)
    paw_write_byte(reg & 0x7F);

    // Small delay between address and data
    nrf_delay_us(PAW_DELAY_US * 2);

    // Read response from separate MISO pin (P1.11)
    return paw_read_byte_4wire();
}

void paw3204_write_reg(uint8_t reg, uint8_t data)
{
    // Address byte with write bit (MSB = 1)
    paw_write_byte(reg | 0x80);

    // Write data
    paw_write_byte(data);
}

// P0.29 might be chip select or power enable
#define PAW_NCS     NRF_GPIO_PIN_MAP(0, 29)

// Try reading with SCK idle LOW (SPI mode 0)
static uint8_t paw_read_byte_mode0(void)
{
    uint8_t data = 0;
    nrf_gpio_cfg_input(PAW_MISO, NRF_GPIO_PIN_PULLUP);

    for (int i = 7; i >= 0; i--) {
        // Clock high - sample data
        nrf_gpio_pin_set(PAW_SCK);
        nrf_delay_us(PAW_DELAY_US);
        if (nrf_gpio_pin_read(PAW_MISO)) {
            data |= (1 << i);
        }
        // Clock low
        nrf_gpio_pin_clear(PAW_SCK);
        nrf_delay_us(PAW_DELAY_US);
    }
    return data;
}

static void paw_write_byte_mode0(uint8_t data)
{
    nrf_gpio_cfg_output(PAW_SDA);

    for (int i = 7; i >= 0; i--) {
        // Set data
        if (data & (1 << i)) {
            nrf_gpio_pin_set(PAW_SDA);
        } else {
            nrf_gpio_pin_clear(PAW_SDA);
        }
        // Clock high
        nrf_gpio_pin_set(PAW_SCK);
        nrf_delay_us(PAW_DELAY_US);
        // Clock low
        nrf_gpio_pin_clear(PAW_SCK);
        nrf_delay_us(PAW_DELAY_US);
    }
}

bool paw3204_probe(void)
{
    uint8_t id;

    // Initialize clock pin - try idle LOW first (SPI mode 0)
    nrf_gpio_cfg_output(PAW_SCK);
    nrf_gpio_pin_clear(PAW_SCK);  // SCK idles LOW
    nrf_gpio_cfg_output(PAW_SDA);
    nrf_gpio_cfg_input(PAW_MISO, NRF_GPIO_PIN_PULLUP);

    // Try P0.29 as chip select or power enable
    nrf_gpio_cfg_output(PAW_NCS);

    NRF_LOG_INFO("PAW3204: Probing on SCK=P0.31, SDA=P0.30, NCS=P0.29");

    // Attempt 1: P0.29 HIGH (power enable style)
    nrf_gpio_pin_set(PAW_NCS);
    nrf_delay_ms(50);
    id = paw3204_read_reg(PAW_REG_PRODUCT_ID);
    NRF_LOG_INFO("PAW3204: P0.29=HIGH -> ID=0x%02X", id);

    if (id == PAW3204_PRODUCT_ID) {
        NRF_LOG_INFO("PAW3204: Sensor detected (P0.29=HIGH)!");
        return true;
    }

    // Attempt 2: P0.29 LOW (chip select style)
    nrf_gpio_pin_clear(PAW_NCS);
    nrf_delay_ms(50);
    id = paw3204_read_reg(PAW_REG_PRODUCT_ID);
    NRF_LOG_INFO("PAW3204: P0.29=LOW -> ID=0x%02X", id);

    if (id == PAW3204_PRODUCT_ID) {
        NRF_LOG_INFO("PAW3204: Sensor detected (P0.29=LOW)!");
        return true;
    }

    // Try 4-wire SPI with P1.11 as MISO (mode 3: idle high)
    NRF_LOG_INFO("PAW3204: Trying 4-wire SPI mode 3 (MISO=P1.11)...");
    nrf_gpio_pin_clear(PAW_NCS);
    nrf_delay_ms(10);
    id = paw3204_read_reg_4wire(PAW_REG_PRODUCT_ID);
    NRF_LOG_INFO("PAW3204: Mode 3 ID = 0x%02X", id);

    if (id == PAW3204_PRODUCT_ID || (id != 0xFF && id != 0x00)) {
        NRF_LOG_INFO("PAW3204: Response! ID=0x%02X", id);
        return (id == PAW3204_PRODUCT_ID);
    }

    // Try SPI mode 0 (clock idles LOW)
    NRF_LOG_INFO("PAW3204: Trying SPI mode 0 (SCK idle LOW)...");
    nrf_gpio_pin_clear(PAW_SCK);  // Idle low
    nrf_gpio_pin_clear(PAW_NCS);
    nrf_delay_ms(10);
    paw_write_byte_mode0(PAW_REG_PRODUCT_ID & 0x7F);
    nrf_delay_us(PAW_DELAY_US * 2);
    id = paw_read_byte_mode0();
    NRF_LOG_INFO("PAW3204: Mode 0 ID = 0x%02X", id);

    if (id == PAW3204_PRODUCT_ID || (id != 0xFF && id != 0x00)) {
        NRF_LOG_INFO("PAW3204: Response! ID=0x%02X", id);
        return (id == PAW3204_PRODUCT_ID);
    }

    NRF_LOG_INFO("PAW3204: No sensor detected - may need logic analyzer");
    return false;
}

void paw3204_read_motion(int8_t *dx, int8_t *dy)
{
    // Read motion register first (clears delta registers)
    uint8_t motion = paw3204_read_reg(PAW_REG_MOTION);

    if (motion & 0x80) {  // Motion detected
        *dx = (int8_t)paw3204_read_reg(PAW_REG_DELTA_X);
        *dy = (int8_t)paw3204_read_reg(PAW_REG_DELTA_Y);
    } else {
        *dx = 0;
        *dy = 0;
    }
}
