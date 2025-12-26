# Twiddler 4 Hardware Documentation

Hardware documentation for the Twiddler 4 chording keyboard, useful for custom firmware development and hardware modifications.

## About This Documentation

**Target audience**: Developers creating custom firmware for the Twiddler 4

**What you'll find**:
- Component identification and specifications
- GPIO pin mapping for buttons
- I2C bus configuration and connected devices
- Configuration file format specification

**The device**: Twiddler 4 is a one-handed chording keyboard with 16 buttons, Bluetooth/USB connectivity, and a touchpad. It uses a Nordic nRF52840 microcontroller.

## Document Index

| Topic | Document | Description |
|-------|----------|-------------|
| Device overview | [01-PRODUCT_OVERVIEW.md](01-PRODUCT_OVERVIEW.md) | What is a chording keyboard, specs, physical layout |
| Hardware | [02-HARDWARE_RE.md](02-HARDWARE_RE.md) | Component identification, debug headers, PCB layout |
| GPIO mapping | [03-GPIO_DISCOVERY.md](03-GPIO_DISCOVERY.md) | Button-to-pin mapping |
| I2C bus | [04-I2C_ANALYSIS.md](04-I2C_ANALYSIS.md) | I2C pins and connected devices |
| Config format | [06-CONFIG_FORMAT.md](06-CONFIG_FORMAT.md) | Binary configuration file structure |
| Firmware dev | [07-FIRMWARE_DEVELOPMENT.md](07-FIRMWARE_DEVELOPMENT.md) | Building, flashing, debugging custom firmware |
| APPROTECT bypass | [08-APPROTECT_BYPASS.md](08-APPROTECT_BYPASS.md) | Voltage glitching to extract original firmware |

## Hardware Summary

| Category | Finding |
|----------|---------|
| MCU | Nordic nRF52840 (ARM Cortex-M4F @ 64MHz) in EByte E73-2G4M08S1C module |
| Buttons | 16 direct GPIO inputs (NOT a matrix), active low with internal pull-ups |
| I2C | P0.30 (SDA), P0.31 (SCL) - exposed on J3 debug header |
| Touchpad | Azoteq IQS5xx @ I2C address 0x74 |
| RGB LEDs | WS2812/SK6812 addressable LEDs (L1, L2, L3 on thumb board) |
| Config | Binary format with header, chord table, and optional string table |

## Prerequisites

- **Electronics**: Can use a multimeter, understand GPIO, pull-ups, I2C basics
- **Programming**: Comfortable with C (for custom firmware development)

## Photo Reference

Key reference photos:

| Photo | Description |
|-------|-------------|
| ![Full disassembly](../../photos/twiddler4/01_full_disassembly_overview.jpg) | Complete teardown: main PCB, thumb board, battery compartment |
| ![Main PCB front](../../photos/twiddler4/02_main_pcb_front_led_on.jpg) | Main PCB front with U1 module, debug headers J2/J3 |
| ![U1 module closeup](../../photos/twiddler4/04_closeup_u1_module_r3r9_c1c4.jpg) | E73 module close-up with component designators |
| ![Thumb board](../../photos/twiddler4/06_thumb_board_l1l2l3_leds.jpg) | Thumb board with RGB LEDs L1-L3 |
| ![Back with buttons](../../photos/twiddler4/07_back_12_finger_buttons_usbc.jpg) | Back of device showing 12 finger buttons |
| ![J3 debug header](../../photos/twiddler4/17_closeup_j3_pins_c1c4.jpg) | J3 debug header with I2C pins labeled |

## Getting Started

1. **New to the device?** Start with [01-PRODUCT_OVERVIEW.md](01-PRODUCT_OVERVIEW.md)
2. **Need hardware details?** See [02-HARDWARE_RE.md](02-HARDWARE_RE.md)
3. **Building custom firmware?** See [07-FIRMWARE_DEVELOPMENT.md](07-FIRMWARE_DEVELOPMENT.md)
4. **GPIO mapping?** See [03-GPIO_DISCOVERY.md](03-GPIO_DISCOVERY.md)
5. **Working with configs?** Format spec in [06-CONFIG_FORMAT.md](06-CONFIG_FORMAT.md)
6. **Extracting original firmware?** See [08-APPROTECT_BYPASS.md](08-APPROTECT_BYPASS.md)

---

*This documentation supports the Northern Chorder custom firmware project.*
