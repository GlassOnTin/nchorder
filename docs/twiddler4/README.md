# Twiddler 4 Hardware Documentation

Hardware documentation for the Twiddler 4 chording keyboard, useful for custom firmware development and hardware modifications.

## About This Documentation

**Target audience**: Developers creating custom firmware for the Twiddler 4

**What you'll find**:
- Component identification and specifications
- GPIO pin mapping for buttons and peripherals
- Configuration file format specification

**The device**: Twiddler 4 is a one-handed chording keyboard with 16 buttons, Bluetooth/USB connectivity, and an optical motion sensor for mouse control. It uses a Nordic nRF52840 microcontroller.

## Document Index

| Topic | Document | Description |
|-------|----------|-------------|
| Device overview | [01-PRODUCT_OVERVIEW.md](01-PRODUCT_OVERVIEW.md) | What is a chording keyboard, specs, physical layout |
| Hardware | [02-HARDWARE_RE.md](02-HARDWARE_RE.md) | Component identification, E73 pinout, debug headers |
| GPIO mapping | [03-GPIO_DISCOVERY.md](03-GPIO_DISCOVERY.md) | Button-to-GPIO mapping |
| LED control | [04-LED_CONTROL.md](04-LED_CONTROL.md) | RGB LED driver (bit-bang protocol, RGB order) |
| Optical sensor | [05-OPTICAL_SENSOR.md](05-OPTICAL_SENSOR.md) | PixArt PAW-A350 thumb sensor |
| Config format | [06-CONFIG_FORMAT.md](06-CONFIG_FORMAT.md) | Binary configuration file structure |
| Firmware dev | [07-FIRMWARE_DEVELOPMENT.md](07-FIRMWARE_DEVELOPMENT.md) | Building, flashing, debugging custom firmware |
| APPROTECT bypass | [08-APPROTECT_BYPASS.md](08-APPROTECT_BYPASS.md) | Voltage glitching to extract original firmware |

## Hardware Summary

| Category | Finding |
|----------|---------|
| MCU | Nordic nRF52840 (ARM Cortex-M4F @ 64MHz) in EByte E73-2G4M08S1C module |
| Buttons | 22 direct GPIO inputs (NOT a matrix), active low with internal pull-ups |
| Button reading | Direct GPIO register polling (`NRF_P0->IN`), not GPIOTE interrupts |
| Thumb sensor | PixArt PAW-A350 OFN, I2C/SPI, 125-1250 CPI, pins P0.29-P0.31 + P1.11 |
| RGB LEDs | 3x addressable LEDs, P1.10 (power enable), P1.13 (data), RGB byte order |
| Config | Binary format with header, chord table, and optional string table |

### Critical Note: E73 Module GPIO Routing

The E73-2G4M08S1C datasheet has **incorrect GPIO mappings** for pins 38/40/42:
- Datasheet claims: P1.02, P1.04, P1.06
- **Actual (verified)**: P0.15, P0.20, P0.17

This was discovered through empirical GPIO testing after continuity tracing gave wrong results. Always verify GPIO assignments with working firmware, not just multimeter tracing!

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
| ![J3 debug header](../../photos/twiddler4/17_closeup_j3_pins_c1c4.jpg) | J3 debug header with GPIO pins labeled |
| ![Desoldered module](../../photos/twiddler4/22_closeup_u1_module_desoldered.jpg) | E73 module desoldered, exposing even-numbered pads for tracing |
| ![Thumb board RGB](../../photos/twiddler4/23_thumb_board_overview_rgb_leds.jpg) | Thumb board with RGB LEDs lit (R/G/B test pattern) |
| ![Sensor front](../../photos/twiddler4/26_sensor_module_front_lens_j6.jpg) | PAW-A350 sensor module front view with lens |
| ![PAW-A350 label](../../photos/twiddler4/28_paw_a350_label_j5.jpg) | Sensor label: PAW-A350 N14001 |

## Getting Started

1. **New to the device?** Start with [01-PRODUCT_OVERVIEW.md](01-PRODUCT_OVERVIEW.md)
2. **Need hardware details?** See [02-HARDWARE_RE.md](02-HARDWARE_RE.md)
3. **Building custom firmware?** See [07-FIRMWARE_DEVELOPMENT.md](07-FIRMWARE_DEVELOPMENT.md)
4. **GPIO mapping?** See [03-GPIO_DISCOVERY.md](03-GPIO_DISCOVERY.md)
5. **Working with configs?** Format spec in [06-CONFIG_FORMAT.md](06-CONFIG_FORMAT.md)
6. **Extracting original firmware?** See [08-APPROTECT_BYPASS.md](08-APPROTECT_BYPASS.md)

---

*This documentation supports the Northern Chorder custom firmware project.*
