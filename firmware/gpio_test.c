/**
 * Minimal GPIO Test - Twiddler 4
 *
 * Bare-metal test: no SoftDevice, no USB, just GPIO polling with RTT output.
 */

#include "nrf52840.h"
#include "SEGGER_RTT.h"
#include <stdint.h>

// UICR REGOUT0 = 3.3V (value 5). Placed at 0x10001304 by linker.
// Without this, chip erase resets REGOUT0 to 1.8V default, making
// GPIO pull-ups too weak for reliable button reads.
const uint32_t UICR_REGOUT0 __attribute__((section(".uicr_regout0"), used)) = 0x00000005;

// UICR NFCPINS: disable NFC so P0.09/P0.10 are available as GPIO (F3R/F3M)
const uint32_t UICR_NFCPINS __attribute__((section(".uicr_nfcpins"), used)) = 0xFFFFFFFE;

// Stub implementations for RTT critical regions (no SoftDevice)
void app_util_critical_region_enter(uint8_t *p_nested) {
    __disable_irq();
    (void)p_nested;
}

void app_util_critical_region_exit(uint8_t nested) {
    (void)nested;
    __enable_irq();
}

// PIN_CNF register value for input with pull-up
// DIR=0 (input), INPUT=0 (connected), PULL=3 (pull-up)
#define PIN_CNF_INPUT_PULLUP  0x0000000C

// PIN_CNF for output: DIR=1, INPUT=1 (disconnect), PULL=0, DRIVE=0 (S0S1)
#define PIN_CNF_OUTPUT  0x00000003

// Button pin definitions (port << 5 | pin)
#define P(port, pin) ((port << 5) | pin)

// Button pin definitions - from physical tracing + E73/nRF52840 datasheet cross-reference
// E73 pinout: https://www.cdebyte.com/products/E73-2G4M08S1E/2#Pin
// nRF52840 pins: https://docs.nordicsemi.com/bundle/ps_nrf52840/page/pin.html
// See docs/twiddler4/02-HARDWARE_RE.md for full mapping table

// Thumb buttons
#define PIN_T1   P(0, 0)   // E73 pin 11 (XL1) -> P0.00
#define PIN_T2   P(0, 4)   // E73 pin 18 (AI2) -> P0.04
#define PIN_T3   P(0, 8)   // E73 pin 16 (P0.08) -> P0.08
#define PIN_T4   P(0, 13)  // E73 pin 33 (P0.13) -> P0.13

// Finger row 0 (top/index) - optional, not in original 16-button layout
// #define PIN_F0L  P(1, 0)   // E73 pin 36 (P1.00) -> P1.00
// #define PIN_F0M  P(0, 24)  // E73 pin 35 (P0.24) -> P0.24
// #define PIN_F0R  P(0, 26)  // E73 pin 12 (P0.26) -> P0.26

// Finger row 1
#define PIN_F1L  P(0, 3)   // E73 pin 3 (P0.03) -> P0.03
#define PIN_F1M  P(0, 2)   // E73 pin 7 (AI0) -> P0.02
#define PIN_F1R  P(0, 1)   // E73 pin 13 (XL2) -> P0.01

// Finger row 2
#define PIN_F2L  P(0, 7)   // E73 pin 22 (P0.07) -> P0.07
#define PIN_F2M  P(0, 6)   // E73 pin 14 (P0.06) -> P0.06
#define PIN_F2R  P(0, 5)   // E73 pin 15 (AI3) -> P0.05

// Finger row 3 - NOTE: F3M/F3R are NFC pins, require UICR.NFCPINS=0xFFFFFFFE
#define PIN_F3L  P(0, 12)  // E73 pin 20 (P0.12) -> P0.12
#define PIN_F3M  P(0, 10)  // E73 pin 43 (NF2) -> P0.10
#define PIN_F3R  P(0, 9)   // E73 pin 41 (NF1) -> P0.09

// Finger row 4 - CORRECTED via empirical testing (not P1 as originally mapped!)
#define PIN_F4L  P(0, 15)  // Empirically verified
#define PIN_F4M  P(0, 20)  // Empirically verified
#define PIN_F4R  P(0, 17)  // Empirically verified

static const uint8_t button_pins[] = {
    PIN_T1, PIN_F1L, PIN_F1M, PIN_F1R,
    PIN_T2, PIN_F2L, PIN_F2M, PIN_F2R,
    PIN_T3, PIN_F3L, PIN_F3M, PIN_F3R,
    PIN_T4, PIN_F4L, PIN_F4M, PIN_F4R
};

static const char* button_names[] = {
    "T1", "F1L", "F1M", "F1R",
    "T2", "F2L", "F2M", "F2R",
    "T3", "F3L", "F3M", "F3R",
    "T4", "F4L", "F4M", "F4R"
};

#define NUM_BUTTONS 16

// Simple delay
static void delay(volatile uint32_t count) {
    while (count--) __NOP();
}

// Configure pin as input with pull-up
static void config_input_pullup(uint8_t pin) {
    uint8_t port = pin >> 5;
    uint8_t port_pin = pin & 0x1F;

    if (port == 0) {
        NRF_P0->PIN_CNF[port_pin] = PIN_CNF_INPUT_PULLUP;
    } else {
        NRF_P1->PIN_CNF[port_pin] = PIN_CNF_INPUT_PULLUP;
    }
}

// Drive pin as output HIGH or LOW (keep for future debugging)
__attribute__((unused))
static void drive_pin(uint8_t pin, int high) {
    uint8_t port = pin >> 5;
    uint8_t port_pin = pin & 0x1F;
    uint32_t mask = 1UL << port_pin;

    if (port == 0) {
        NRF_P0->PIN_CNF[port_pin] = PIN_CNF_OUTPUT;
        if (high) NRF_P0->OUTSET = mask;
        else NRF_P0->OUTCLR = mask;
    } else {
        NRF_P1->PIN_CNF[port_pin] = PIN_CNF_OUTPUT;
        if (high) NRF_P1->OUTSET = mask;
        else NRF_P1->OUTCLR = mask;
    }
}

int main(void) {
    // Long delay at start for RTT connection
    for (volatile uint32_t i = 0; i < 5000000; i++) __NOP();

    SEGGER_RTT_Init();

    // Print startup message multiple times to ensure RTT is working
    for (int j = 0; j < 3; j++) {
        SEGGER_RTT_printf(0, "\n\n=== Twiddler 4 GPIO Test ===\n");
        SEGGER_RTT_printf(0, "Bare-metal, no SoftDevice\n\n");
        delay(500000);
    }

    // CRITICAL: Disable TRACE peripheral - P1.02/P1.04 are TRACEDATA2/TRACECLK
    // CLOCK->TRACECONFIG: TRACEMUX bits 16-17 must be 0 to release pins to GPIO
    SEGGER_RTT_printf(0, "CLOCK->TRACECONFIG before = 0x%08X\n", NRF_CLOCK->TRACECONFIG);
    NRF_CLOCK->TRACECONFIG = 0;  // Disable trace, release pins to GPIO
    SEGGER_RTT_printf(0, "CLOCK->TRACECONFIG after  = 0x%08X\n", NRF_CLOCK->TRACECONFIG);

    // Check UICR.NFCPINS
    SEGGER_RTT_printf(0, "UICR.NFCPINS = 0x%08X\n", NRF_UICR->NFCPINS);

    // Configure all button pins as input with pull-up
    SEGGER_RTT_printf(0, "\n=== Initializing button pins ===\n");
    for (int i = 0; i < NUM_BUTTONS; i++) {
        config_input_pullup(button_pins[i]);
    }
    // Configure ALL GPIO pins as inputs with pull-up for full scan
    SEGGER_RTT_printf(0, "Configuring ALL GPIO pins for scanning...\n");
    for (int i = 0; i < 32; i++) {
        // Skip P0.09/P0.10 if NFC enabled, and P0.00/P0.01 (32kHz crystal)
        // Actually, configure them all - we disabled NFC already
        NRF_P0->PIN_CNF[i] = PIN_CNF_INPUT_PULLUP;
    }
    for (int i = 0; i < 16; i++) {
        NRF_P1->PIN_CNF[i] = PIN_CNF_INPUT_PULLUP;
    }
    SEGGER_RTT_printf(0, "Done.\n");

    // Configure all button pins
    SEGGER_RTT_printf(0, "\n=== Configuring pins ===\n");
    for (int i = 0; i < NUM_BUTTONS; i++) {
        uint8_t pin = button_pins[i];
        uint8_t port = pin >> 5;
        uint8_t port_pin = pin & 0x1F;

        config_input_pullup(pin);

        uint32_t cnf = (port == 0) ? NRF_P0->PIN_CNF[port_pin] : NRF_P1->PIN_CNF[port_pin];
        SEGGER_RTT_printf(0, "%s: P%d.%02d CNF=0x%08X\n",
                         button_names[i], port, port_pin, cnf);
    }

    delay(100000);

    // Initial GPIO state
    SEGGER_RTT_printf(0, "\n=== Initial state ===\n");
    SEGGER_RTT_printf(0, "P0.IN = 0x%08X\n", NRF_P0->IN);
    SEGGER_RTT_printf(0, "P1.IN = 0x%08X\n", NRF_P1->IN);

    // Debug P1 pins specifically
    SEGGER_RTT_printf(0, "\n=== P1 Debug (after TRACE disable) ===\n");
    SEGGER_RTT_printf(0, "NRF_P1 base = 0x%08X\n", (uint32_t)NRF_P1);
    SEGGER_RTT_printf(0, "P1.02 CNF = 0x%08X\n", NRF_P1->PIN_CNF[2]);
    SEGGER_RTT_printf(0, "P1.04 CNF = 0x%08X\n", NRF_P1->PIN_CNF[4]);
    SEGGER_RTT_printf(0, "P1.06 CNF = 0x%08X\n", NRF_P1->PIN_CNF[6]);
    SEGGER_RTT_printf(0, "P1.IN = 0x%08X\n", NRF_P1->IN);

    // Give RTT time to sync
    SEGGER_RTT_printf(0, "\nStarting P1 diagnostic tests in 3 seconds...\n");
    delay(3000000);

    // Test 1: Try input with NO pull (floating) to see pin state
    SEGGER_RTT_printf(0, "\n=== Test 1: P1 with NO pull-up (floating) ===\n");
    NRF_P1->PIN_CNF[2] = 0x00000000;  // Input, no pull
    NRF_P1->PIN_CNF[4] = 0x00000000;
    NRF_P1->PIN_CNF[6] = 0x00000000;
    delay(100000);
    SEGGER_RTT_printf(0, "P1.IN (no pull) = 0x%08X\n", NRF_P1->IN);
    uint32_t p1_nopull = NRF_P1->IN;
    SEGGER_RTT_printf(0, "P1.02=%d P1.04=%d P1.06=%d (no pull)\n",
                     (p1_nopull >> 2) & 1, (p1_nopull >> 4) & 1, (p1_nopull >> 6) & 1);

    // Test 2: Try with pull-down instead
    SEGGER_RTT_printf(0, "\n=== Test 2: P1 with pull-DOWN ===\n");
    NRF_P1->PIN_CNF[2] = 0x00000004;  // Input, pull-down (PULL=1)
    NRF_P1->PIN_CNF[4] = 0x00000004;
    NRF_P1->PIN_CNF[6] = 0x00000004;
    delay(100000);
    uint32_t p1_pulldown = NRF_P1->IN;
    SEGGER_RTT_printf(0, "P1.IN (pull-down) = 0x%08X\n", p1_pulldown);
    SEGGER_RTT_printf(0, "P1.02=%d P1.04=%d P1.06=%d (pull-down)\n",
                     (p1_pulldown >> 2) & 1, (p1_pulldown >> 4) & 1, (p1_pulldown >> 6) & 1);

    // Test 3: Back to pull-up
    SEGGER_RTT_printf(0, "\n=== Test 3: P1 with pull-UP ===\n");
    NRF_P1->PIN_CNF[2] = PIN_CNF_INPUT_PULLUP;
    NRF_P1->PIN_CNF[4] = PIN_CNF_INPUT_PULLUP;
    NRF_P1->PIN_CNF[6] = PIN_CNF_INPUT_PULLUP;
    delay(100000);
    uint32_t p1_pullup = NRF_P1->IN;
    SEGGER_RTT_printf(0, "P1.IN (pull-up) = 0x%08X\n", p1_pullup);
    SEGGER_RTT_printf(0, "P1.02=%d P1.04=%d P1.06=%d (pull-up)\n",
                     (p1_pullup >> 2) & 1, (p1_pullup >> 4) & 1, (p1_pullup >> 6) & 1);

    // Test 4: Check LATCH register (might have captured events)
    SEGGER_RTT_printf(0, "\n=== Test 4: P1 LATCH register ===\n");
    SEGGER_RTT_printf(0, "P1.LATCH = 0x%08X\n", NRF_P1->LATCH);

    // Test 5: Store P1 history for offline analysis
    SEGGER_RTT_printf(0, "\n=== Test 5: P1 history capture ===\n");
    SEGGER_RTT_printf(0, "Recording P1.IN for 30 seconds...\n");
    SEGGER_RTT_printf(0, "PRESS F4 BUTTONS DURING THIS TIME!\n");
    SEGGER_RTT_printf(0, "(Will print summary after capture)\n\n");

    // Capture P1.IN history - store unique values
    #define MAX_HISTORY 100
    static uint32_t p1_history[MAX_HISTORY];
    static uint32_t p1_times[MAX_HISTORY];
    int history_idx = 0;
    uint32_t prev_p1 = NRF_P1->IN;
    p1_history[0] = prev_p1;
    p1_times[0] = 0;
    history_idx = 1;

    uint32_t start_time = 0;
    for (int sec = 30; sec > 0; sec--) {
        // Print countdown
        SEGGER_RTT_printf(0, "%2d ", sec);
        if (sec % 10 == 0) SEGGER_RTT_printf(0, "\n");

        for (int ms = 0; ms < 1000; ms++) {
            uint32_t curr_p1 = NRF_P1->IN;
            if (curr_p1 != prev_p1 && history_idx < MAX_HISTORY) {
                p1_history[history_idx] = curr_p1;
                p1_times[history_idx] = (30 - sec) * 1000 + ms;
                history_idx++;
                prev_p1 = curr_p1;
            }
            delay(1000);
        }
        start_time++;
    }

    SEGGER_RTT_printf(0, "\n\n=== P1 History Summary ===\n");
    SEGGER_RTT_printf(0, "Unique P1 values captured: %d\n", history_idx);
    for (int i = 0; i < history_idx && i < 20; i++) {
        SEGGER_RTT_printf(0, "  t=%5dms: P1.IN=0x%04X\n", p1_times[i], p1_history[i] & 0xFFFF);
    }
    if (history_idx == 1) {
        SEGGER_RTT_printf(0, "  >>> NO CHANGES DETECTED <<<\n");
    }

    // Test 6: Try using SENSE functionality
    SEGGER_RTT_printf(0, "\n=== Test 6: GPIO SENSE test ===\n");

    // Configure P1.02 with SENSE for low level (detect when pin goes LOW)
    // PIN_CNF: DIR=0, INPUT=0, PULL=3 (pull-up), SENSE=3 (low level)
    #define PIN_CNF_INPUT_PULLUP_SENSE_LOW  0x00030004 | (3 << 16)  // Actually: 0x0003000C
    // Correct: PULL=3 (bits 2-3), SENSE=3 (bits 16-17) = 0x0003000C
    uint32_t sense_cfg = (0 << 0)   // DIR = input
                       | (0 << 1)   // INPUT = connect
                       | (3 << 2)   // PULL = pull-up
                       | (0 << 8)   // DRIVE = S0S1
                       | (3 << 16); // SENSE = low level
    SEGGER_RTT_printf(0, "SENSE config = 0x%08X\n", sense_cfg);
    NRF_P1->PIN_CNF[2] = sense_cfg;
    NRF_P1->PIN_CNF[4] = sense_cfg;
    NRF_P1->PIN_CNF[6] = sense_cfg;
    delay(100000);

    SEGGER_RTT_printf(0, "P1.02 CNF = 0x%08X\n", NRF_P1->PIN_CNF[2]);

    // Check DETECT register
    SEGGER_RTT_printf(0, "\nDETECT registers:\n");
    SEGGER_RTT_printf(0, "GPIOTE->EVENTS_PORT = 0x%08X\n", NRF_GPIOTE->EVENTS_PORT);

    // Clear LATCH and wait for button press
    NRF_P1->LATCH = 0xFFFFFFFF;  // Clear all latches
    SEGGER_RTT_printf(0, "P1.LATCH after clear = 0x%08X\n", NRF_P1->LATCH);

    SEGGER_RTT_printf(0, "\nPress F4 buttons for 10 seconds (check LATCH):\n");
    for (int sec = 10; sec > 0; sec--) {
        uint32_t latch = NRF_P1->LATCH;
        uint32_t p1_in = NRF_P1->IN;
        SEGGER_RTT_printf(0, "%2d: LATCH=0x%04X IN=0x%04X\n", sec, latch & 0xFFFF, p1_in & 0xFFFF);
        if (latch != 0) {
            SEGGER_RTT_printf(0, "  >>> LATCH DETECTED! <<<\n");
        }
        delay(1000000);
    }

    SEGGER_RTT_printf(0, "\n=== Polling - press buttons ===\n");

    uint32_t last_p0 = NRF_P0->IN;
    uint32_t last_p1 = NRF_P1->IN;
    uint32_t loop = 0;

    while (1) {
        uint32_t p0 = NRF_P0->IN;
        uint32_t p1 = NRF_P1->IN;

        if (p0 != last_p0 || p1 != last_p1) {
            // Report ANY bit changes on P0 and P1
            uint32_t p0_changed = p0 ^ last_p0;
            uint32_t p1_changed = p1 ^ last_p1;

            if (p0_changed) {
                for (int bit = 0; bit < 32; bit++) {
                    if (p0_changed & (1UL << bit)) {
                        int pressed = (p0 & (1UL << bit)) ? 0 : 1;
                        SEGGER_RTT_printf(0, "P0.%02d: %s\n", bit,
                                         pressed ? "LOW" : "HIGH");
                    }
                }
            }
            if (p1_changed) {
                for (int bit = 0; bit < 16; bit++) {
                    if (p1_changed & (1UL << bit)) {
                        int pressed = (p1 & (1UL << bit)) ? 0 : 1;
                        SEGGER_RTT_printf(0, "P1.%02d: %s\n", bit,
                                         pressed ? "LOW" : "HIGH");
                    }
                }
            }

            last_p0 = p0;
            last_p1 = p1;
        }

        // Heartbeat
        if (++loop >= 500000) {
            loop = 0;
            SEGGER_RTT_printf(0, "HB P0=%08X P1=%08X\n", p0, p1);
        }

        delay(100);
    }

    return 0;
}
