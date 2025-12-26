# Northern Chorder Documentation

Documentation for chording keyboard development: reverse engineering the Twiddler 4 and building custom hardware.

## Sections

| Section | Description |
|---------|-------------|
| [twiddler4/](twiddler4/) | Twiddler 4 hardware reverse engineering, GPIO mapping, config format |
| [nchorder/](nchorder/) | Custom capacitive touch chording keyboard design |

## Project Overview

This project has two parts:

1. **Twiddler 4 Analysis** - Reverse engineering an existing commercial chording keyboard to understand:
   - nRF52840-based hardware design
   - GPIO button mapping
   - I2C peripherals (touchpad)
   - Configuration file format

2. **Northern Chorder Build** - Custom hardware using:
   - Seeed XIAO nRF52840-Plus (MCU)
   - Bela Trill capacitive touch sensors (instead of mechanical buttons)
   - I2C multiplexer for sensor addressing

## Quick Links

**Twiddler 4:**
- [Hardware overview](twiddler4/02-HARDWARE_RE.md)
- [GPIO pin mapping](twiddler4/03-GPIO_DISCOVERY.md)
- [Config file format](twiddler4/06-CONFIG_FORMAT.md)
- [Firmware development](twiddler4/07-FIRMWARE_DEVELOPMENT.md)

**Northern Chorder:**
- [Hardware wiring plan](nchorder/HARDWARE.md)
