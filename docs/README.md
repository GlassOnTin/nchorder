# Northern Chorder Documentation

Firmware for chording keyboards based on the nRF52840 SoC.

## Supported Hardware

| Board | Description | Build Command |
|-------|-------------|---------------|
| [nChorder XIAO](nchorder/) | Custom capacitive touch chorder (XIAO nRF52840 + Trill sensors) | `make` (default) |
| [nRF52840-DK](nchorder/) | Nordic development kit for testing (4 buttons) | `make BOARD=dk` |
| [Twiddler 4](twiddler4/) | Commercial chording keyboard by Tek Gear | `make BOARD=twiddler4` |

## Hardware Documentation

### Northern Chorder (nChorder XIAO)

Custom chording keyboard using capacitive touch sensors:
- **MCU**: Seeed XIAO nRF52840-Plus
- **Input**: Bela Trill capacitive touch sensors (1 Square + 3 Bars)
- **Interface**: I2C via PCA9548 multiplexer
- **Connection**: BLE HID (USB disabled due to power event crash)

Documentation:
- [Hardware wiring](nchorder/HARDWARE.md)
- [Firmware notes](nchorder/FIRMWARE.md)

### nRF52840-DK

Nordic's development kit for firmware testing:
- **MCU**: nRF52840 (on-board)
- **Input**: 4 buttons (SW1-SW4) mapped to thumb buttons T1-T4
- **LEDs**: 4 LEDs (active-low)
- **Debug**: On-board J-Link, RTT logging

Useful for testing BLE HID without custom hardware.

### Twiddler 4

Commercial chording keyboard hardware:
- **MCU**: nRF52840 via EByte E73-2G4M08S1C module
- **Input**: 16 mechanical buttons (4 thumb + 12 finger)
- **I2C**: Optical motion sensor (P0.30/P0.31) - unidentified
- **LEDs**: WS2812B/SK6812 addressable RGB (via I2S)
- **Debug**: SWD header (J2), I2C header (J3)

Documentation:
- [Product overview](twiddler4/01-PRODUCT_OVERVIEW.md)
- [Hardware reverse engineering](twiddler4/02-HARDWARE_RE.md)
- [GPIO pin mapping](twiddler4/03-GPIO_DISCOVERY.md)
- [I2C bus analysis](twiddler4/04-I2C_ANALYSIS.md)
- [Config file format](twiddler4/06-CONFIG_FORMAT.md)
- [Firmware development](twiddler4/07-FIRMWARE_DEVELOPMENT.md)
- [APPROTECT bypass](twiddler4/08-APPROTECT_BYPASS.md)

## Quick Start

```bash
# Build for default board (XIAO with Trill sensors)
make

# Build for nRF52840-DK
make BOARD=dk

# Build for Twiddler 4
make BOARD=twiddler4

# Flash (requires nrfjprog)
make flash
```

## Features

- BLE HID keyboard with LESC pairing
- USB HID (Twiddler 4 only, disabled on XIAO/DK)
- Twiddler-compatible chord configuration
- Persistent bond storage
- RTT debug logging
