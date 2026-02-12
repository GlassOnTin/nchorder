# Firmware Development Guide

[← Back to Index](README.md)

This document describes the clean-room firmware development process for the Northern Chorder project - an open-source firmware for nRF52840-based chorded keyboards.

## Development Environment

### Prerequisites

| Tool | Version | Purpose |
|------|---------|---------|
| arm-none-eabi-gcc | 14.x | ARM cross-compiler |
| Nordic nRF5 SDK | 17.1.0 | SoftDevice, drivers, BLE stack |
| nrfjprog | 7.x+ | Flash programming |
| J-Link | Any | SWD debug probe |

### SDK Setup

```bash
# Extract SDK to expected location
cd /path/to/nchorder
mkdir -p sdk
cd sdk
unzip nRF5_SDK_17.1.0.zip
```

The Makefile expects SDK at: `sdk/nRF5_SDK_17.1.0_ddde560/`

## Building

```bash
cd firmware/pca10056/s140/armgcc

# Clean build
make clean

# Build firmware
make -j4

# Output files:
# _build/nrf52840_xxaa.hex  - Intel HEX (for nrfjprog)
# _build/nrf52840_xxaa.bin  - Raw binary
```

### Build Output (typical)

```
   text    data     bss     dec     hex filename
 111096     896   11100  123092   1e0d4 _build/nrf52840_xxaa.out
```

- ~111KB code fits easily in 808KB available flash
- Warnings from SDK code are normal

## Flashing

### Hardware Setup

Connect J-Link to target's SWD header (J2 on Twiddler):

| J-Link | Target J2 |
|--------|-----------|
| VTref | Pin 1 (VDD) |
| GND | Pin 2 (GND) |
| SWCLK | Pin 3 |
| SWDIO | Pin 4 |

### Flash Commands

```bash
# 1. (Optional) Backup existing firmware first
nrfjprog -f nrf52 --readcode firmware_backup.hex

# 2. If device has readback protection, unlock it
# WARNING: This erases everything!
nrfjprog --recover

# 3. Flash SoftDevice (BLE stack)
make flash_softdevice

# 4. Flash application
make flash
```

### Memory Layout

```
0x00000000 - 0x00000FFF   MBR (Master Boot Record)
0x00001000 - 0x00026FFF   SoftDevice s140 v7.2.0
0x00027000 - 0x000EFFFF   Application (this firmware)
0x000F0000 - 0x000FFFFF   Bootloader (preserved)
```

## Debugging

### RTT Logging

The firmware uses Segger RTT for debug output. Enable in `sdk_config.h`:

```c
#define NRF_LOG_BACKEND_RTT_ENABLED 1
```

View logs with:
```bash
JLinkRTTLogger -device NRF52840_XXAA -if SWD -speed 4000 -RTTChannel 0 /dev/stdout
```

### GDB Debugging

```bash
# Terminal 1: Start GDB server
JLinkGDBServer -device NRF52840_XXAA -if SWD -speed 4000

# Terminal 2: Connect GDB
arm-none-eabi-gdb _build/nrf52840_xxaa.out
(gdb) target remote :2331
(gdb) monitor reset
(gdb) continue
```

### Common Debug Commands

```bash
# Check if J-Link sees the target
nrfjprog --ids

# Read chip info
nrfjprog --memrd 0x10000100 --n 4   # FICR CODEPAGESIZE
nrfjprog --memrd 0x10000104 --n 4   # FICR CODESIZE

# Reset device
nrfjprog --reset

# Full chip erase (WARNING: erases everything including bootloader)
nrfjprog --eraseall
```

## Testing on nRF52840-DK

The firmware can be tested on Nordic's nRF52840-DK (pca10056) development board.

### Known Limitations on DK

**GPIO Conflicts**: The Twiddler GPIO pin assignments conflict with DK hardware:
- DK buttons use pins 11, 12, 24, 25
- Twiddler config uses some of these for different purposes
- This causes initialization errors when running Twiddler firmware on DK

**To test on DK**, you would need to:
1. Create a separate board configuration for DK testing
2. Or temporarily modify `nchorder_config.h` to use DK-compatible pins

**BLE/USB Testing**: Core BLE and USB functionality should work on DK if GPIO conflicts are resolved.

### DK-Specific Setup

The DK has two USB ports:
- **J2**: J-Link interface (for flashing/debugging)
- **J3**: nRF52840 USB (for testing USB HID)

## Code Architecture

### Module Overview

| Module | File | Purpose |
|--------|------|---------|
| Config | `nchorder_config.h` | GPIO pins, timing constants |
| Buttons | `nchorder_buttons.c` | GPIO polling via NRF_P0->IN register |
| Chords | `nchorder_chords.c` | Chord detection state machine |
| USB | `nchorder_usb.c` | USB HID keyboard |
| CDC | `nchorder_cdc.c` | USB CDC debug console |
| Flash | `nchorder_flash.c` | FDS flash storage for configs |
| LED | `nchorder_led.c` | WS2812 RGB LED via I2S |
| Battery | `nchorder_battery.c` | SAADC battery monitoring |
| Optical | `nchorder_optical.c` | PAW-A350 thumb sensor (I2C) |
| Mouse | `nchorder_mouse.c` | USB HID mouse integration |

### Initialization Flow

```c
main()
├── QSPI disable              // Release P0.22/P0.23 for GPIO
├── SWO trace disable          // Release P1.00 for F0L button
├── log_init()
├── scheduler_init()           // Must be before timers_init
├── timers_init()
├── wdt_init()                 // Watchdog (5s timeout)
├── buttons_leds_init()        // BSP (board buttons/LEDs)
├── power_management_init()
├── ble_stack_init()           // SoftDevice
├── gap_params_init()
├── gatt_init()
├── advertising_init()
├── services_init()            // BLE HIDS, BAS, DIS
├── battery_init()             // SAADC battery monitoring
├── conn_params_init()
├── buffer_init()
├── peer_manager_init()
├── nchorder_flash_init()      // FDS flash storage
├── nchorder_led_init()        // RGB LED driver (I2S/WS2812)
├── LED test (R/G/B, 500ms)
├── nchorder_init()            // Chord system
│   ├── chord_init()           // Load default mappings
│   ├── nchorder_flash_load_config()  // Load config from flash
│   ├── chord_load_config()    // Parse .cfg format
│   ├── buttons_init()         // Configure GPIO (or Trill sensors on XIAO)
│   ├── buttons_set_callback()
│   ├── nchorder_usb_init()    // USB HID (Twiddler4/XIAO only)
│   └── nchorder_usb_start()
├── timers_start()
├── advertising_start(false)   // Keep bonds across resets
└── main loop
    ├── wdt_feed()
    ├── nchorder_usb_process()
    ├── nchorder_usb_check_disconnect()
    ├── nchorder_cdc_process()
    └── idle_state_handle()
        ├── app_sched_execute()
        ├── nchorder_usb_process()
        ├── NRF_LOG_PROCESS()
        └── nrf_pwr_mgmt_run()
```

### Chord Processing Flow

```
Timer tick → poll NRF_P0->IN → debounce filter → button state update
                                                        ↓
                                                chord_update()
                                                        ↓
                                                chord completed?
                                                        ↓ yes
                                                chord_lookup_*()
                                                        ↓
                                                send via BLE or USB
```

**Note**: Button reading uses direct GPIO register polling, not GPIOTE interrupts. This was found to be more reliable for scanning multiple buttons simultaneously.

## Configuration File Format

The firmware parses Twiddler .cfg files. See [06-CONFIG_FORMAT.md](06-CONFIG_FORMAT.md) for format details.

Key structures:
- Header: 128 bytes with chord count, string table offset
- Chord entries: 8 bytes each (bitmask, modifier, keycode)
- String table: For multi-character macros

## Implemented Features

- [x] 16-button GPIO input with debouncing
- [x] Chord detection state machine
- [x] Twiddler .cfg config file parsing
- [x] Single-key chord mappings
- [x] Multi-character macro support
- [x] Consumer control (media keys)
- [x] BLE HID keyboard
- [x] USB HID keyboard (secondary)
- [x] Flash storage for configs (FDS)
- [x] RGB LED driver (I2S/WS2812)
- [x] PAW-A350 optical sensor driver (I2C at 0x33, disabled pending I2C bus sharing fix)
- [x] USB HID mouse integration
- [x] USB CDC debug console
- [x] SAADC battery monitoring
- [x] Watchdog timer (5s)
- [x] BLE bond/peer management

## Not Yet Implemented

- [ ] System chords (config switching, sleep)
- [ ] USB mass storage (config editing)
- [ ] Mouse button chords (partial)
- [ ] Optical sensor enabled in production build (I2C bus contention with main loop)

## Legal Notes

This firmware is a **clean-room implementation**:
- Uses only public Nordic SDK and documentation
- GPIO mappings determined via hardware probing (multimeter continuity) **and empirical firmware testing**
- Config format derived from community documentation
- No proprietary code or reverse-engineered binaries included

**Important**: The E73 module datasheet has incorrect GPIO mappings for pins 38/40/42. Empirical testing with bare-metal firmware was required to discover the correct assignments (P0.15/P0.20/P0.17 instead of P1.02/P1.04/P1.06). See [03-GPIO_DISCOVERY.md](03-GPIO_DISCOVERY.md) for details.

---

[← Back to Index](README.md)
