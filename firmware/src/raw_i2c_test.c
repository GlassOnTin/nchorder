/**
 * Raw I2C Test - Minimal Trill sensor identification
 *
 * SIMPLIFIED TEST: Direct connection to Trill Square at 0x28, NO MUX.
 * Connect Trill directly to I2C pins (SDA=P0.04, SCL=P0.05).
 *
 * Protocol:
 * 1. Write 1 byte (0x00) to set read offset
 * 2. Read 4 bytes - expect: FE <type> <fw_ver> <checksum>
 *
 * Buffer is pre-filled with 0xFF to verify reads actually write data.
 */

#include "sdk_common.h"
#include "nrf_gpio.h"
#include "nrfx_twim.h"
#include "nrf_delay.h"
#include "SEGGER_RTT.h"

// Pin definitions (XIAO nRF52840)
#define TEST_SDA      NRF_GPIO_PIN_MAP(0, 4)   // D4 = P0.04
#define TEST_SCL      NRF_GPIO_PIN_MAP(0, 5)   // D5 = P0.05

// I2C address - Trill Square default
#define SQUARE_ADDR   0x28

static const nrfx_twim_t m_twim = NRFX_TWIM_INSTANCE(0);
static volatile bool m_done = false;
static volatile ret_code_t m_result = 0;

static void handler(nrfx_twim_evt_t const *evt, void *ctx)
{
    (void)ctx;
    m_result = (evt->type == NRFX_TWIM_EVT_DONE) ? NRF_SUCCESS : NRF_ERROR_INTERNAL;
    m_done = true;
}

static ret_code_t raw_write(uint8_t addr, uint8_t *data, size_t len)
{
    nrfx_twim_xfer_desc_t xfer = {
        .type = NRFX_TWIM_XFER_TX,
        .address = addr,
        .primary_length = len,
        .p_primary_buf = data
    };
    m_done = false;
    ret_code_t err = nrfx_twim_xfer(&m_twim, &xfer, 0);
    if (err != NRF_SUCCESS) return err;
    while (!m_done) __WFE();
    return m_result;
}

static ret_code_t raw_read(uint8_t addr, uint8_t *data, size_t len)
{
    nrfx_twim_xfer_desc_t xfer = {
        .type = NRFX_TWIM_XFER_RX,
        .address = addr,
        .primary_length = len,
        .p_primary_buf = data
    };
    m_done = false;
    ret_code_t err = nrfx_twim_xfer(&m_twim, &xfer, 0);
    if (err != NRF_SUCCESS) return err;
    while (!m_done) __WFE();
    return m_result;
}

void raw_i2c_test(void)
{
    ret_code_t err;

    SEGGER_RTT_printf(0, "\n");
    SEGGER_RTT_printf(0, "============================================================\n");
    SEGGER_RTT_printf(0, "RAW I2C TEST - NO MUX - Direct to Trill Square @ 0x28\n");
    SEGGER_RTT_printf(0, "============================================================\n");
    SEGGER_RTT_printf(0, "\n");
    SEGGER_RTT_printf(0, "Hardware: XIAO nRF52840, SDA=P0.04, SCL=P0.05\n");
    SEGGER_RTT_printf(0, "Target: Trill Square at I2C address 0x%02X\n", SQUARE_ADDR);
    SEGGER_RTT_printf(0, "\n");

    // Print the actual code being executed
    SEGGER_RTT_printf(0, "=== CODE ===\n");
    SEGGER_RTT_printf(0, "// Initialize I2C at 100kHz\n");
    SEGGER_RTT_printf(0, "nrfx_twim_config_t cfg = {\n");
    SEGGER_RTT_printf(0, "    .scl = P0.05,\n");
    SEGGER_RTT_printf(0, "    .sda = P0.04,\n");
    SEGGER_RTT_printf(0, "    .frequency = NRF_TWIM_FREQ_100K\n");
    SEGGER_RTT_printf(0, "};\n");
    SEGGER_RTT_printf(0, "nrfx_twim_init(&twim, &cfg, handler, NULL);\n");
    SEGGER_RTT_printf(0, "\n");

    // Init I2C
    nrfx_twim_config_t cfg = {
        .scl = TEST_SCL,
        .sda = TEST_SDA,
        .frequency = NRF_TWIM_FREQ_100K,
        .interrupt_priority = 6,
        .hold_bus_uninit = false
    };

    err = nrfx_twim_init(&m_twim, &cfg, handler, NULL);
    if (err != NRF_SUCCESS) {
        SEGGER_RTT_printf(0, "ERROR: I2C init failed: 0x%08X\n", err);
        return;
    }
    nrfx_twim_enable(&m_twim);
    SEGGER_RTT_printf(0, "I2C initialized OK\n\n");

    nrf_delay_ms(100);  // Let sensor settle

    // Step 1: Write offset byte
    SEGGER_RTT_printf(0, "=== STEP 1: Set read pointer ===\n");
    SEGGER_RTT_printf(0, "// Write single byte 0x00 to address 0x%02X\n", SQUARE_ADDR);
    SEGGER_RTT_printf(0, "uint8_t offset = 0x00;\n");
    SEGGER_RTT_printf(0, "nrfx_twim_xfer(TX, addr=0x%02X, data=&offset, len=1);\n", SQUARE_ADDR);
    SEGGER_RTT_printf(0, "\n");

    uint8_t offset = 0x00;
    err = raw_write(SQUARE_ADDR, &offset, 1);
    SEGGER_RTT_printf(0, "Result: %s (err=0x%08X)\n\n",
                      (err == NRF_SUCCESS) ? "OK" : "FAILED", err);

    if (err != NRF_SUCCESS) {
        SEGGER_RTT_printf(0, "ERROR: No device responding at 0x%02X\n", SQUARE_ADDR);
        SEGGER_RTT_printf(0, "Check wiring: SDA to Trill SDA, SCL to Trill SCL\n");
        nrfx_twim_uninit(&m_twim);
        return;
    }

    nrf_delay_ms(5);

    // Step 2: Read 4 bytes
    SEGGER_RTT_printf(0, "=== STEP 2: Read identification bytes ===\n");
    SEGGER_RTT_printf(0, "// Pre-fill buffer with 0xFF to detect uninitialized reads\n");
    SEGGER_RTT_printf(0, "uint8_t buf[4] = {0xFF, 0xFF, 0xFF, 0xFF};\n");
    SEGGER_RTT_printf(0, "nrfx_twim_xfer(RX, addr=0x%02X, data=buf, len=4);\n", SQUARE_ADDR);
    SEGGER_RTT_printf(0, "\n");

    // Pre-fill with 0xFF - if read fails silently, we'll see 0xFF 0xFF 0xFF 0xFF
    uint8_t buf[4] = {0xFF, 0xFF, 0xFF, 0xFF};

    err = raw_read(SQUARE_ADDR, buf, 4);
    SEGGER_RTT_printf(0, "Result: %s (err=0x%08X)\n",
                      (err == NRF_SUCCESS) ? "OK" : "FAILED", err);
    SEGGER_RTT_printf(0, "Buffer: 0x%02X 0x%02X 0x%02X 0x%02X\n\n",
                      buf[0], buf[1], buf[2], buf[3]);

    // Interpret results
    SEGGER_RTT_printf(0, "=== INTERPRETATION ===\n");
    if (buf[0] == 0xFF && buf[1] == 0xFF && buf[2] == 0xFF && buf[3] == 0xFF) {
        SEGGER_RTT_printf(0, "Buffer unchanged - read did not write to memory!\n");
    } else if (buf[0] == 0xFE) {
        const char *type_names[] = {"?", "Bar", "Square", "Craft", "Ring", "Hex", "Flex"};
        uint8_t type = buf[1];
        const char *type_name = (type <= 6) ? type_names[type] : "Unknown";
        SEGGER_RTT_printf(0, "Valid Trill response:\n");
        SEGGER_RTT_printf(0, "  Header:   0x%02X (expected 0xFE) - OK\n", buf[0]);
        SEGGER_RTT_printf(0, "  Type:     %d (%s)\n", type, type_name);
        SEGGER_RTT_printf(0, "  Firmware: %d\n", buf[2]);
        SEGGER_RTT_printf(0, "  Checksum: 0x%02X\n", buf[3]);
        SEGGER_RTT_printf(0, "\n");
        if (type == 6) {
            SEGGER_RTT_printf(0, "*** PROBLEM: Type=6 (Flex) but this is a Square board! ***\n");
        }
    } else {
        SEGGER_RTT_printf(0, "Unexpected response (header 0x%02X != 0xFE)\n", buf[0]);
    }

    SEGGER_RTT_printf(0, "\n============================================================\n");
    SEGGER_RTT_printf(0, "TEST COMPLETE\n");
    SEGGER_RTT_printf(0, "============================================================\n");

    nrfx_twim_uninit(&m_twim);
}
