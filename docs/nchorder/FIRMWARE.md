# Northern Chorder Firmware Development

This document explains the firmware architecture and development process for the Northern Chorder, aimed at undergraduate computer engineering students.

## Prerequisites

Before diving in, you should be familiar with:
- C programming (pointers, structs, function pointers)
- Basic digital electronics (GPIO, I2C, interrupts)
- Command-line tools (make, gcc)

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                      Application Layer                       │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐  │
│  │   main.c    │  │  Chording   │  │   BLE/USB HID       │  │
│  │  (startup)  │  │   Engine    │  │   (host interface)  │  │
│  └──────┬──────┘  └──────┬──────┘  └──────────┬──────────┘  │
├─────────┼────────────────┼────────────────────┼─────────────┤
│         │        Hardware Abstraction Layer   │             │
│  ┌──────▼──────┐  ┌──────▼──────┐  ┌──────────▼──────────┐  │
│  │   Buttons   │  │    LEDs     │  │      Storage        │  │
│  │   Driver    │  │   Driver    │  │      (Flash)        │  │
│  └──────┬──────┘  └──────┬──────┘  └─────────────────────┘  │
├─────────┼────────────────┼──────────────────────────────────┤
│         │    Board-Specific Drivers                         │
│  ┌──────▼──────┐  ┌──────▼──────┐                           │
│  │ GPIO/Trill  │  │  GPIO/I2S   │                           │
│  │  (buttons)  │  │   (LEDs)    │                           │
│  └──────┬──────┘  └──────┬──────┘                           │
├─────────┼────────────────┼──────────────────────────────────┤
│         │         Nordic nRF5 SDK                           │
│  ┌──────▼──────┐  ┌──────▼──────┐  ┌─────────────────────┐  │
│  │   nrfx_*    │  │  SoftDevice │  │    FDS (Flash)      │  │
│  │  (drivers)  │  │    (BLE)    │  │                     │  │
│  └──────┬──────┘  └──────┬──────┘  └─────────────────────┘  │
├─────────┼────────────────┼──────────────────────────────────┤
│         │           Hardware                                │
│  ┌──────▼─────────────────▼─────────────────────────────┐   │
│  │              nRF52840 Microcontroller                │   │
│  │   (ARM Cortex-M4, 1MB Flash, 256KB RAM, BLE 5.0)    │   │
│  └──────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

## Build System

### Toolchain

The firmware uses the ARM GCC toolchain:

```bash
# Install on Ubuntu/Debian
sudo apt install gcc-arm-none-eabi

# Verify installation
arm-none-eabi-gcc --version
```

### Building

```bash
cd custom_firmware

# Build for different targets
make BOARD=xiao       # Northern Chorder (Trill sensors)
make BOARD=twiddler4  # Twiddler 4 hardware
make BOARD=dk         # nRF52840 Development Kit

# Clean and rebuild
make clean && make BOARD=xiao

# See all options
make help
```

### Output Files

After building, you'll find in `_build/`:
- `nrf52840_xxaa.out` - ELF executable (for debugging)
- `nrf52840_xxaa.hex` - Intel HEX (for flashing)
- `nrf52840_xxaa.bin` - Raw binary

## The nRF5 SDK

Nordic's SDK is a collection of drivers, libraries, and examples for their chips. Key directories:

```
sdk/nRF5_SDK_17.1.0/
├── components/
│   ├── ble/           # Bluetooth stack helpers
│   ├── libraries/     # Utility libraries (FDS, scheduler, etc.)
│   └── softdevice/    # BLE protocol stack binary
├── modules/
│   └── nrfx/          # Low-level peripheral drivers
│       ├── drivers/   # TWIM, I2S, GPIO, etc.
│       └── hal/       # Hardware abstraction
└── integration/
    └── nrfx/legacy/   # Compatibility shims (important!)
```

### The SoftDevice

The nRF52840 runs a "SoftDevice" - a precompiled BLE stack that occupies the first ~150KB of flash. Your application runs alongside it:

```
Flash Memory Map:
┌──────────────────┐ 0x100000 (1MB)
│   Application    │
│   (your code)    │
├──────────────────┤ ~0x27000
│   SoftDevice     │
│   (BLE stack)    │
├──────────────────┤ 0x1000
│   MBR            │
│ (Master Boot Rec)│
└──────────────────┘ 0x00000
```

The SoftDevice handles all BLE radio operations. Your code communicates with it through "SoftDevice calls" (sd_* functions).

## Driver Architecture

### Why Abstraction Layers?

The Northern Chorder supports multiple hardware configurations:

| Board | Button Input | LED Output |
|-------|-------------|------------|
| Twiddler 4 | GPIO (mechanical switches) | I2S (WS2812B addressable LEDs) |
| nRF52840-DK | GPIO (dev board buttons) | I2S (WS2812B) |
| XIAO + Trill | I2C (capacitive sensors) | GPIO (single LED) |

Rather than duplicating code, we use **abstraction**:

```c
// nchorder_buttons.h - Common interface
void buttons_init(void);
uint16_t buttons_scan(void);  // Returns 16-bit button mask
void buttons_set_callback(buttons_callback_t cb);

// Implementations selected at compile time:
// - nchorder_buttons.c (GPIO driver)
// - button_driver_trill.c (Trill I2C driver)
```

### Conditional Compilation

The Makefile selects which source files to include:

```makefile
ifeq ($(BOARD),xiao)
  SRC_FILES += src/nchorder_led_gpio.c    # Simple GPIO LED
  SRC_FILES += src/nchorder_i2c.c         # I2C bus driver
  SRC_FILES += src/nchorder_trill.c       # Trill sensor driver
else
  SRC_FILES += src/nchorder_led.c         # WS2812B via I2S
endif
```

Board-specific defines control behavior within shared code:

```c
// nchorder_buttons.c
#if defined(BUTTON_DRIVER_TRILL)
    // Use Trill I2C sensors
    #include "nchorder_trill.h"
    // ... Trill-specific implementation
#else
    // Use GPIO buttons
    // ... GPIO-specific implementation
#endif
```

## I2C Communication

### What is I2C?

I2C (Inter-Integrated Circuit) is a two-wire serial bus:
- **SDA** (Serial Data) - bidirectional data line
- **SCL** (Serial Clock) - clock signal from master

```
       Master (MCU)              Slave (Sensor)
      ┌───────────┐             ┌───────────┐
      │           │─────SDA─────│           │
      │  nRF52840 │─────SCL─────│   Trill   │
      │           │             │           │
      └───────────┘             └───────────┘
            │                         │
           ─┴─ Pull-up resistors ────┴─
```

Each device has a 7-bit address. Multiple devices share the bus:

```
MCU ──┬── SDA ──┬── Trill (0x20) ──┬── MUX (0x70)
      │         │                   │
      └── SCL ──┴───────────────────┘
```

### I2C Multiplexer

Problem: All Trill sensors have the same address (0x20).

Solution: PCA9548 I2C multiplexer - a chip that routes I2C signals to one of 8 downstream channels:

```
                    ┌─────────────┐
     MCU ──SDA/SCL──│   PCA9548   │── Ch0 ── Trill Square (thumb)
                    │   (0x70)    │── Ch1 ── Trill Bar 1 (index)
                    │             │── Ch2 ── Trill Bar 2 (middle)
                    │             │── Ch3 ── Trill Bar 3 (ring/pinky)
                    └─────────────┘
```

To read a sensor, first select its channel by writing to the mux:

```c
// Select channel 0 (thumb sensor)
uint8_t channel_mask = (1 << 0);  // 0x01
nchorder_i2c_write(0x70, &channel_mask, 1);

// Now address 0x20 reaches the thumb sensor
nchorder_i2c_read(0x20, buffer, length);
```

### TWIM vs TWI

The nRF52840 has two I2C implementations:
- **TWI** - Basic I2C, CPU handles each byte
- **TWIM** - I2C with EasyDMA, transfers happen in background

We use TWIM for efficiency. The CPU sets up a transfer, then DMA handles the bytes while the CPU does other work.

## Case Study: The TWIM Configuration Bug

This illustrates a common embedded development challenge: fighting the SDK.

### The Symptom

```
error: 'NRFX_TWIM0_INST_IDX' undeclared
```

The TWIM driver wasn't being compiled, even though we enabled it in `sdk_config.h`:

```c
#define NRFX_TWIM_ENABLED 1
#define NRFX_TWIM0_ENABLED 1
```

### The Investigation

The nrfx drivers use preprocessor conditionals to include hardware support:

```c
// nrfx_twim.c (simplified)
#if NRFX_TWIM0_ENABLED
    // ... TWIM0 instance code
#endif
```

Using the preprocessor to check the actual value:

```c
#if NRFX_TWIM0_ENABLED
#pragma message "TWIM0 is ENABLED"
#else
#pragma message "TWIM0 is DISABLED"  // <-- This printed!
#endif
```

Something was overriding our setting.

### The Root Cause

Tracing through the include chain:

```
nchorder_i2c.c
  → sdk_common.h
    → nrfx_glue.h
      → apply_old_config.h   ← HERE!
```

The `apply_old_config.h` file exists for backward compatibility with older Nordic code. It contains:

```c
// apply_old_config.h, line 1147
#if defined(TWI_PRESENT) && defined(TWIM_PRESENT)
    // nRF52840 has both old TWI and new TWIM
    #undef NRFX_TWIM0_ENABLED
    #define NRFX_TWIM0_ENABLED (TWI0_ENABLED && TWI0_USE_EASY_DMA)
#endif
```

The SDK **undefines** our setting and replaces it with a formula based on legacy TWI settings!

### The Fix

Enable the legacy settings that the formula reads:

```c
// sdk_config.h
#define TWI_ENABLED 1
#define TWI0_ENABLED 1
#define TWI0_USE_EASY_DMA 1
```

Now `(TWI0_ENABLED && TWI0_USE_EASY_DMA)` evaluates to `(1 && 1)` = `1`, and TWIM0 is enabled.

### Lessons Learned

1. **Read the SDK source** - Configuration isn't always straightforward
2. **Use preprocessor diagnostics** - `#pragma message` and `#if` checks reveal actual values
3. **Trace include chains** - Headers can modify settings in unexpected ways
4. **Legacy compatibility layers are tricky** - They exist for good reasons but add complexity

## Button-to-Chord Mapping

### The 16-Button Layout

Chording keyboards map simultaneous button presses to characters. The Twiddler layout uses 16 buttons:

```
        Thumb           Fingers (4 rows × 3 columns)
      ┌───────┐        ┌─────┬─────┬─────┐
      │T1  T2 │        │ F1L │ F1M │ F1R │  Index
      │       │        ├─────┼─────┼─────┤
      │T3  T4 │        │ F2L │ F2M │ F2R │  Middle
      └───────┘        ├─────┼─────┼─────┤
                       │ F3L │ F3M │ F3R │  Ring
                       ├─────┼─────┼─────┤
                       │ F4L │ F4M │ F4R │  Pinky
                       └─────┴─────┴─────┘
```

### Bitmask Representation

Button state is a 16-bit integer where each bit represents one button:

```
Bit:  15  14  13  12  11  10   9   8   7   6   5   4   3   2   1   0
      F4R F4M F4L T4  F3R F3M F3L T3  F2R F2M F2L T2  F1R F1M F1L T1
```

Example: Pressing T1 + F1M = `0b0000000000000101` = `0x0005`

### Trill Sensor Mapping

The Trill Bar is a 1D touch sensor that reports position (0-3200). We divide it into zones:

```
Trill Bar
┌────────────┬────────────┬────────────┬────────────┐
│  Zone 0    │  Zone 1    │  Zone 2    │  Zone 3    │
│  0-799     │  800-1599  │ 1600-2399  │ 2400-3200  │
│    ↓       │     ↓      │     ↓      │     ↓      │
│   F*L      │    F*M     │  (unused)  │    F*R     │
└────────────┴────────────┴────────────┴────────────┘
```

The Trill Square (2D) is divided into quadrants for thumb buttons:

```
Trill Square
┌─────────────┬─────────────┐
│     T1      │     T2      │
│  (x<1600,   │  (x≥1600,   │
│   y<1600)   │   y<1600)   │
├─────────────┼─────────────┤
│     T3      │     T4      │
│  (x<1600,   │  (x≥1600,   │
│   y≥1600)   │   y≥1600)   │
└─────────────┴─────────────┘
```

## Debugging Techniques

### RTT (Real-Time Transfer)

RTT provides printf-style debugging without UART. Output appears in J-Link tools:

```c
#include "nrf_log.h"

NRF_LOG_INFO("Button mask: 0x%04X", button_state);
NRF_LOG_ERROR("I2C failed: 0x%08X", err_code);
```

View output with:
```bash
JLinkRTTViewer
# or
JLinkRTTClient
```

### Common Error Codes

| Code | Meaning |
|------|---------|
| 0x00000000 | NRF_SUCCESS |
| 0x00000004 | NRF_ERROR_NO_MEM |
| 0x00000007 | NRF_ERROR_INVALID_PARAM |
| 0x00000008 | NRF_ERROR_INVALID_STATE |
| 0x0000000D | NRF_ERROR_TIMEOUT |
| 0x00008200 | NRF_ERROR_DRV_TWI_ERR_ANACK (I2C address not acknowledged) |
| 0x00008201 | NRF_ERROR_DRV_TWI_ERR_DNACK (I2C data not acknowledged) |

### I2C Bus Scan

The firmware includes a debug function to find devices:

```c
// Scans addresses 0x08-0x77, logs found devices
uint8_t found = nchorder_i2c_scan();
// Output: "Found device at 0x20", "Found device at 0x70", etc.
```

## Flashing the Firmware

### Using J-Link

```bash
# Flash application (after SoftDevice is installed)
nrfjprog --program _build/nrf52840_xxaa.hex --sectorerase --verify

# Full erase and flash (need to reflash SoftDevice too)
nrfjprog --eraseall
nrfjprog --program s140_nrf52_7.2.0_softdevice.hex
nrfjprog --program _build/nrf52840_xxaa.hex --sectorerase

# Reset to start running
nrfjprog --reset
```

### Using UF2 Bootloader (XIAO)

If your XIAO has a UF2 bootloader:
1. Double-tap reset to enter bootloader (drive appears)
2. Copy the .uf2 file to the drive
3. Device reboots automatically

## Troubleshooting & Lessons Learned

This section documents issues encountered during development and their solutions.

### CRITICAL: Never Block in BLE Applications

**Symptom:** Device crashes with SoftDevice fault 0x4001 after sending BLE HID keypress.

**Root Cause:** The SoftDevice (BLE stack) requires regular processing of its event queue. Blocking delays (even 500ms) prevent this, causing TX buffer exhaustion.

```c
// BAD - Will crash!
void send_key(uint8_t keycode) {
    ble_hid_send_key(keycode);
    nrf_delay_ms(500);        // Blocks BLE event processing
    ble_hid_send_release();   // By now, SoftDevice has crashed
}

// GOOD - Non-blocking
void send_key(uint8_t keycode) {
    ble_hid_send_key(keycode);
    // Key release handled by app_timer callback
}
```

**Rule:** In BLE applications, never use blocking delays longer than a few milliseconds. Use app_timers for delayed operations.

### LFCLK Configuration (XIAO nRF52840)

**Symptom:** Device hangs at startup, never reaches main loop.

**Root Cause:** The XIAO nRF52840 has no external 32.768 kHz crystal. The default SDK config expects one.

**Fix:** Configure LFCLK to use the internal RC oscillator in `sdk_config.h`:

```c
#define NRFX_CLOCK_CONFIG_LF_SRC 0           // 0 = RC oscillator
#define NRF_SDH_CLOCK_LF_SRC 0               // For SoftDevice
#define NRF_SDH_CLOCK_LF_RC_CTIV 16          // Calibration interval
#define NRF_SDH_CLOCK_LF_RC_TEMP_CTIV 2      // Temperature compensation
```

### nrf_delay_ms Hangs

**Symptom:** `nrf_delay_ms()` hangs indefinitely.

**Root Cause:** The delay function uses the DWT (Debug Watchpoint and Trace) cycle counter. If DWT isn't initialized, it waits forever.

**Workaround:** Use a simple busy-wait delay instead:

```c
static void simple_delay_ms(uint32_t ms)
{
    // Approximate delay - 64MHz CPU, ~10 cycles per iteration
    volatile uint32_t count = ms * 6400;
    while (count--) {
        __NOP();
    }
}
```

**Note:** This is less accurate than `nrf_delay_ms()` but doesn't depend on DWT.

### BLE Bonds Cleared on Reset

**Symptom:** After USB disconnect (which causes a brief power brownout and reset), the device won't reconnect to the previously paired host. Re-pairing is required.

**Root Cause:** `advertising_start(true)` in the boot sequence erases all stored bonds.

**Fix:** Use `advertising_start(false)` to preserve bonds:

```c
// In main() after BLE init
advertising_start(false);  // Keep existing bonds

// Re-pairing is still allowed via PM_EVT_CONN_SEC_CONFIG_REQ handler
```

### Trill Sensor I2C Protocol

**Symptom:** I2C read returns garbage data or wrong device type.

**Root Cause:** Trill sensors require setting the read pointer before reading data.

**Correct Protocol:**

```c
// Step 1: Set read pointer to offset 0
uint8_t zero = 0;
nchorder_i2c_write(TRILL_ADDR, &zero, 1);

// Step 2: Small delay for pointer to take effect
simple_delay_ms(2);

// Step 3: Read data
uint8_t buf[4];
nchorder_i2c_read(TRILL_ADDR, buf, 4);

// Response format: 0xFE <device_type> <firmware_version> <checksum>
// Device types: 1=Bar, 2=Square, 3=Craft, 4=Ring, 5=Hex, 6=Flex
```

### Touch Threshold Tuning

**Symptom:** False positive button presses, especially during USB plug/unplug.

**Root Cause:** Voltage transients cause momentary capacitance changes that register as touches.

**Fix:** Increase the minimum touch size threshold:

```c
// button_driver_trill.c
#define TRILL_MIN_TOUCH_SIZE    300   // Minimum size to register (was 200)
#define TRILL_RELEASE_SIZE      150   // Hysteresis for release (was 100)
```

Higher thresholds reduce sensitivity but eliminate spurious touches.

### J-Link Debugging External Targets

When debugging a standalone XIAO (not connected to the DK's nRF52840):

**Symptom:** J-Link cannot connect: "Could not find core in Coresight setup"

**Solution:** The J-Link needs SWDSEL asserted to know it should use external SWD:

1. Connect J-Link SWD pins (SWDIO, SWDCLK, GND) to XIAO
2. Connect XIAO's VDD to J-Link's VTref (pin 1) - tells J-Link the target voltage
3. Jump SWDSEL to VDD - enables external target mode

```
DK J-Link Header          XIAO
┌─────────────┐           ┌─────┐
│ SWDIO ──────│───────────│ SWD │
│ SWDCLK ─────│───────────│ SCK │
│ GND ────────│───────────│ GND │
│ VTref ──────│───────────│ 3V3 │
│ SWDSEL ─────│──┬────────│ 3V3 │  (jumper to VTref)
└─────────────┘  │
```

### SoftDevice Must Be Flashed

**Symptom:** Device resets immediately after flashing, or BLE doesn't work.

**Root Cause:** `nrfjprog --eraseall` also erases the SoftDevice. The application expects it to be present.

**Fix:** Always flash SoftDevice before the application:

```bash
# Full recovery procedure
nrfjprog --eraseall
nrfjprog --program ../sdk/nRF5_SDK_17.1.0/components/softdevice/s140/hex/s140_nrf52_7.2.0_softdevice.hex
nrfjprog --program _build/nrf52840_xxaa.hex --sectorerase
nrfjprog --reset
```

### Debug RAM Markers

During development, we used fixed RAM addresses for debugging (inspectable via J-Link `mem32` command):

| Address | Purpose |
|---------|---------|
| 0x20030090 | Crash info (fault ID, PC, error code) |

The crash info is written by the fault handlers and survives soft resets, allowing post-mortem debugging:

```bash
# After a crash, before power cycle:
nrfjprog --memrd 0x20030090 --n 16
# Shows: fault_marker, PC, info_ptr, err_code
```

## Further Reading

- [nRF5 SDK Documentation](https://infocenter.nordicsemi.com/topic/sdk_nrf5_v17.1.0/index.html)
- [nRF52840 Product Specification](https://infocenter.nordicsemi.com/pdf/nRF52840_PS_v1.1.pdf)
- [Trill Sensor Documentation](https://learn.bela.io/products/trill/about-trill/)
- [I2C Specification](https://www.nxp.com/docs/en/user-guide/UM10204.pdf)
