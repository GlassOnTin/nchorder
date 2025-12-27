# Hardware Reverse Engineering

[← Back to Index](README.md) | [Previous: Product Overview](01-PRODUCT_OVERVIEW.md)

This section covers the methodology for analyzing the Twiddler 4 PCB - identifying components, understanding the circuit, and documenting findings.

## Methodology Overview

Hardware reverse engineering follows a systematic approach:

1. **Document the device unopened** - Photos, measurements, visible labels
2. **Open carefully** - Non-destructive if possible, document how
3. **Photograph everything** - Multiple angles, before touching anything
4. **Identify major ICs** - Part numbers, datasheets
5. **Find debug interfaces** - JTAG, SWD, UART headers
6. **Trace connections** - Multimeter continuity, visual inspection
7. **Power analysis** - Voltage rails, power sequencing

## Opening the Device

The Twiddler 4 uses:
- 6 Phillips head screws on the exterior case
- Internal screws holding PCB to housing
- FFC cable connecting thumb board to main PCB

**Tip**: Before removing any cables, photograph their orientation. FFC cables in particular can be inserted backwards.

## Component Identification

### Step 1: Major ICs

![U1 Module closeup](../../photos/twiddler4/04_closeup_u1_module_r3r9_c1c4.jpg)

The largest component is labeled **U1** - an EByte E73-2G4M08S1C RF module. This module contains:
- Nordic nRF52840 SoC
- Crystal oscillator
- RF matching network
- Ceramic antenna

**How we identified it**:
1. Read silkscreen markings on the module
2. Module has "tek gear" branding but form factor matches E73
3. Cross-referenced with [E73 datasheet](https://www.cdebyte.com/products/E73-2G4M08S1C)

**Why use a module instead of bare nRF52840?**
- RF design is difficult - matching network, antenna placement
- Module is pre-certified (FCC, CE)
- Faster time to market
- Trade-off: Higher cost, less flexibility

### Step 2: Debug Headers

![J3 debug header closeup](../../photos/twiddler4/17_closeup_j3_pins_c1c4.jpg)

Most consumer devices have debug interfaces for factory programming and testing. These are often left populated.

**J2 - SWD Debug Header** (4 pins):

| Pin | Signal | E73 Pin | Purpose |
|-----|--------|---------|---------|
| 1 | VDD | 19 | Power supply |
| 2 | GND | 21 | Ground reference |
| 3 | SWDCLK | 39 (SWC) | Debug clock (from debugger) |
| 4 | SWDIO | 37 (SWD) | Bidirectional debug data |

**Note**: E73 pin 26 (RST) provides hardware reset - hold low to keep chip in reset state during SWD operations.

SWD (Serial Wire Debug) is ARM's 2-wire debug protocol. With a J-Link or ST-Link adapter, you can:
- Read/write flash memory
- Set breakpoints
- Single-step through code
- Inspect registers and RAM

**J3 - Extended Debug Header** (7 pins):

| Pin | Signal | Function |
|-----|--------|----------|
| 1 | VDD | 3.3V power |
| 2 | P0.31 | GPIO (likely I2C SCL) |
| 3 | P0.30 | GPIO (likely I2C SDA) |
| 4 | GND | Ground |
| 5 | P0.28 | GPIO / ADC |
| 6 | P1.09 | GPIO |
| 7 | VDH | Battery voltage |

**Oscilloscope observations on P0.30/P0.31**: Both lines idle high at 3.3V with regular pulses dropping to 0V. This is consistent with I2C (open-drain with pull-ups), but data patterns were not decoded to confirm.

### Step 3: Discrete Components

![Discrete components near U1](../../photos/twiddler4/16_closeup_u1_r5r9_area.jpg)

| Designator | Component | Value | Purpose |
|------------|-----------|-------|---------|
| R1 | Resistor | 2KΩ | Unknown |
| R2 | Resistor | 560Ω | Unknown |
| R3 | Resistor | 180Ω | Unknown |
| R4 | Resistor | 10Ω | Current sense? |
| R5 | Resistor | 10Ω | Current sense? |
| R6 | Resistor | 1KΩ | Q2 base resistor |
| R7 | Resistor | 1KΩ | Q1 base resistor |
| Q1 | Transistor | PNP | Power switch |
| Q2 | Transistor | NPN | Power switch |

**Identifying transistor type**: Use multimeter diode mode:
- PNP: Base-emitter and base-collector both show ~0.6V drop (base positive)
- NPN: Same, but base is the negative lead

**The Q1/Q2 Circuit**: Q1 (PNP) and Q2 (NPN) form a complementary pair, likely acting as a power switch. Q2's output connects to Q1's base, allowing a logic-level GPIO to control a higher-current load.

### Step 4: Connectors

| Designator | Type | Purpose |
|------------|------|---------|
| J1 | USB-C | Charging and data |
| J2 | Header | SWD debug |
| J3 | Header | Extended debug (I2C exposed) |
| J6 | FFC | 12-pin connection to thumb board |

**J6 FFC Pinout** (12 pins):
- 4 pins: Thumb button GPIO lines
- 2 pins: I2C (SDA, SCL) for optical sensor
- 1 pin: I2S data for RGB LEDs
- 2 pins: Power (VCC, GND)
- 3 pins: Unknown/reserved

## E73-2G4M08S1C Module Pinout

The E73 module exposes most nRF52840 GPIO pins on edge pads. This pinout is essential for tracing PCB connections.

**Source**: [Ebyte E73-2G4M08S1C User Manual v1.9](https://www.cdebyte.com/products/E73-2G4M08S1C)

| Pin | Name | nRF52840 Pin | Function |
|-----|------|--------------|----------|
| 1 | P1.11 | P1.11 | GPIO |
| 2 | P1.10 | P1.10 | GPIO |
| 3 | P0.03 | P0.03/AIN1 | GPIO/ADC |
| 4 | AI4 | P0.28/AIN4 | GPIO/ADC |
| 5 | GND | - | Ground |
| 6 | P1.13 | P1.13 | GPIO |
| 7 | AI0 | P0.02/AIN0 | GPIO/ADC |
| 8 | AI5 | P0.29/AIN5 | GPIO/ADC |
| 9 | AI7 | P0.31/AIN7 | GPIO/ADC |
| 10 | AI6 | P0.30/AIN6 | GPIO/ADC |
| 11 | XL1 | P0.00/XL1 | 32.768kHz crystal |
| 12 | P0.26 | P0.26 | GPIO |
| 13 | XL2 | P0.01/XL2 | 32.768kHz crystal |
| 14 | P0.06 | P0.06 | GPIO |
| 15 | AI3 | P0.05/AIN3 | GPIO/ADC |
| 16 | P0.08 | P0.08 | GPIO |
| 17 | P1.09 | P1.09 | GPIO |
| 18 | AI2 | P0.04/AIN2 | GPIO/ADC |
| 19 | VDD | - | Power supply (1.7-3.6V) |
| 20 | P12 | P0.12 | GPIO |
| 21 | GND | - | Ground |
| 22 | P0.07 | P0.07 | GPIO |
| 23 | VDH | VDDH | High-voltage supply (1.7-5.5V) |
| 24 | GND | - | Ground |
| 25 | DCH | DCCH | DC/DC converter output |
| 26 | RST | P0.18/RESET | **Reset (active low)** |
| 27 | VBS | VBUS | USB 5V input |
| 28 | P15 | P0.15 | GPIO |
| 29 | D- | D- | USB D- |
| 30 | P17 | P0.17 | GPIO |
| 31 | D+ | D+ | USB D+ |
| 32 | P0.20 | P0.20 | GPIO |
| 33 | P0.13 | P0.13 | GPIO |
| 34 | P0.22 | P0.22 | GPIO/QSPI |
| 35 | P0.24 | P0.24 | GPIO/QSPI |
| 36 | P1.00 | P1.00 | GPIO |
| 37 | SWD | SWDIO | **Debug data** |
| 38 | P1.02 | P1.02 | GPIO |
| 39 | SWC | SWDCLK | **Debug clock** |
| 40 | P1.04 | P1.04 | GPIO |
| 41 | NF1 | P0.09/NFC1 | GPIO/NFC |
| 42 | P1.06 | P1.06 | GPIO |
| 43 | NF2 | P0.10/NFC2 | GPIO/NFC |

**Key pins for debugging:**
- Pin 26 (RST): Reset, directly mapped to P0.18/RESET - hold low to keep chip in reset
- Pin 37 (SWD): SWDIO data line
- Pin 39 (SWC): SWDCLK clock line

**Key observation**: P0.27 (typical I2C SCL) is NOT exposed on this module. This forced the Twiddler designers to use alternative pins for I2C.

## PCB Tracing Techniques

### Continuity Testing

With power disconnected, use multimeter continuity mode to trace connections:

1. Place one probe on a known pin (e.g., E73 module pad)
2. Probe suspected connection points
3. Beep = connection, no beep = no connection

**Tip**: Ground planes will beep with many points. Check resistance - true connections show near-zero ohms, ground plane connections may show slightly higher.

### Visual Tracing

![PCB traces near U1](../../photos/twiddler4/15_closeup_u1_d1_r1r5_c1c4.jpg)

- Follow copper traces from pad to pad
- Look for vias (small holes) that route to other layers
- Silkscreen labels often hint at function

### Power-On Testing

With power applied, use multimeter voltage mode:
- Verify power rails (3.3V, VBUS, VDH)
- Check idle state of GPIO (high = pull-up, low = pull-down)
- I2C lines should idle high (~3.3V)

## Peripheral Usage Summary

| Peripheral | Base Address | Purpose |
|------------|--------------|---------|
| USBD | 0x40027000 | USB HID + Mass Storage |
| GPIO P0/P1 | 0x50000000 | Direct button inputs |
| TWI0 | 0x40003000 | Optical sensor I2C |
| GPIOTE | 0x40006000 | Button interrupts |
| I2S | 0x40025000 | RGB LED data (WS2812) |
| SAADC | 0x40007000 | Battery monitoring |

## What Remains Unverified

- R1-R5 resistor functions
- Q1/Q2 circuit exact purpose (power switch vs level shifter)
- D1 (main board LED) pin assignment
- L1-L3 RGB LED data pin - routed through FFC J6 to thumb board

## Key Lessons

1. **Debug headers are your friend** - Check for unpopulated pads that could be JTAG/SWD
2. **Modules simplify design** - Look up module datasheets for pinouts
3. **Document as you go** - Photos and notes save time later

---

[← Back to Index](README.md) | [Next: GPIO Discovery →](03-GPIO_DISCOVERY.md)
