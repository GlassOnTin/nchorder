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
- 6 Phillips head screws on the exterior case (**M2.5 x 6mm**)
- Internal screws holding PCB to housing (also M2.5 x 6mm)
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

| Pin | Signal | Connects To |
|-----|--------|-------------|
| 1 | VDD | 3.3V power |
| 2 | P0.31 | Thumb sensor C (E73 pin 9) |
| 3 | P0.30 | Thumb sensor D (E73 pin 10) |
| 4 | GND | Ground |
| 5 | P0.28 | E73 pin 4 |
| 6 | P1.09 | E73 pin 17 |
| 7 | VDH | Battery voltage |

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
| J3 | Header | Extended debug (GPIO exposed) |
| J6 | FFC | 12-pin connection to thumb board |

**J6 FFC Pinout** (12 pins, pin 1 = left side near "J" silkscreen):

| J6 Pin | E73 Pad | nRF52840 | Signal |
|--------|---------|----------|--------|
| 1 | 33 | P0.13 | T4 (SP) |
| 2 | 16 | P0.08 | T3 (E) |
| 3 | 18 | P0.04 | T1 (N) |
| 4 | 11 | P0.00 | T2 (A) |
| 5 | 1 | P1.11 | Sensor SHUTDOWN (active LOW) |
| 6 | 8 | P0.29 | T0 (thumb button) |
| 7 | 10 | P0.30 | I2C SCL (optical sensor) |
| 8 | 9 | P0.31 | I2C SDA (optical sensor) |
| 9 | 5 | GND | Ground |
| 10 | 19 | VCC | 3.3V power |
| 11 | 6 | P1.13 | LED Data |
| 12 | Q2 col | - | LED Power (switched via Q1/Q2, P1.10) |

Verified by continuity testing with E73 module desoldered (Feb 2026).

![Thumb board front with optical sensor and J4 desoldered](../../photos/twiddler4/06a_thumb_board_desoldered.jpg)

Thumb board front view with optical sensor module and J4 connector removed, showing J5 (sensor FPC connector), J4 footprint, 4 thumb buttons, and L1/L2/L3 RGB LEDs.

## E73-2G4M08S1C Module Pinout

The E73 module exposes most nRF52840 GPIO pins on edge pads. Pinout determined by:
1. Physical continuity testing from E73 module pads to button contacts
2. Cross-referencing E73 datasheet pin names to nRF52840 ball positions
3. Bare-metal GPIO polling to verify button-to-pin assignments

![Desoldered E73 module showing even-numbered pads](../../photos/twiddler4/22_closeup_u1_module_desoldered.jpg)

### Cross-Reference Methodology

The E73 module uses non-standard pin naming (AI0-AI7, XL1/XL2, etc.) that must be mapped to nRF52840 GPIO numbers:

1. **E73 Datasheet**: [Ebyte E73-2G4M08S1E Pin Table](https://www.cdebyte.com/products/E73-2G4M08S1E/2#Pin)
   - Provides: E73 Pin Number → Pin Name → "Corresponding chip pin number" (BGA ball position)

2. **nRF52840 Datasheet**: [Nordic Pin Assignments](https://docs.nordicsemi.com/bundle/ps_nrf52840/page/pin.html)
   - Provides: BGA Ball Position → GPIO Port.Pin

**Example**: E73 Pin 18 → "AI2" → Ball "J1" → nRF52840 P0.04

### E73 Pin to nRF52840 GPIO Mapping

| E73 Pin | E73 Name | Ball | nRF52840 | Twiddler 4 Connection |
|---------|----------|------|----------|----------------------|
| 1 | P1.11 | B19 | P1.11 | Sensor SHUTDOWN → J6 pin 5 (was listed as NC) |
| 2 | P1.10 | A20 | P1.10 | LED data / Transistor Q1 |
| 3 | P0.03 | B13 | P0.03 | Button F1L |
| 4 | AI4 | B11 | P0.28 | Header J3 pin 5 |
| 5 | GND | - | GND | Ground |
| 6 | P1.13 | A16 | P1.13 | LED Signal |
| 7 | AI0 | A12 | P0.02 | Button F1M |
| 8 | AI5 | A10 | P0.29 | Thumb sensor T0C |
| 9 | AI7 | A8 | P0.31 | Thumb sensor T0B |
| 10 | AI6 | B9 | P0.30 | Thumb sensor T0A |
| 11 | XL1 | D2 | P0.00 | Button T2 (Thumb 2) |
| 12 | P0.26 | G1 | P0.26 | Button F0R |
| 13 | XL2 | F2 | P0.01 | Button F1R |
| 14 | P0.06 | L1 | P0.06 | Button F2M |
| 15 | AI3 | K2 | P0.05 | Button F2R |
| 16 | P0.08 | N1 | P0.08 | Button T3 (Thumb 3) |
| 17 | P1.09 | R1 | P1.09 | Header J3 pin 6 |
| 18 | AI2 | J1 | P0.04 | Button T1 (Thumb 1) |
| 19 | VCC | - | VDD | 3.3V power |
| 20 | P0.12 | U1 | P0.12 | Button F3L |
| 21 | GND | - | GND | Ground |
| 22 | P0.07 | M2 | P0.07 | Button F2L |
| 23 | VDH | - | VDDH | Battery+ |
| 24 | GND | - | GND | Ground |
| 25 | DCH | - | DCCH | DC/DC output |
| 26 | RST | - | RESET | NC (Reset) |
| 27 | VBS | - | VBUS | USB 5V |
| 28 | P15 | AD10 | P0.15 | NC |
| 29 | D- | - | D- | USB D- |
| 30 | P17 | AD12 | P0.17 | NC |
| 31 | D+ | - | D+ | USB D+ |
| 32 | P0.20 | AD16 | P0.20 | NC |
| 33 | P0.13 | AD8 | P0.13 | Button T4 (Thumb 4) |
| 34 | P0.22 | AD18 | P0.22 | NC (datasheet) - see pin 36 note |
| 35 | P0.24 | AD20 | P0.24 | Button F0M |
| 36 | P1.00 | AD22 | **P0.22** | Button F0L (E73 internal routing differs from datasheet!) |
| 37 | SWD | - | SWDIO | SWD Data (J2 pin 4) |
| 38 | P1.02 | - | **P0.15** | Button F4L (E73 internal routing differs from datasheet!) |
| 39 | SWC | AC24 | SWDCLK | SWD Clock (J2 pin 3) |
| 40 | P1.04 | U24 | **P0.20** | Button F4M (E73 internal routing differs from datasheet!) |
| 41 | NF1 | L24 | P0.09 | Button F3R (NFC pin) |
| 42 | P1.06 | R24 | **P0.17** | Button F4R (E73 internal routing differs from datasheet!) |
| 43 | NF2 | J24 | P0.10 | Button F3M (NFC pin) |

### Button GPIO Summary

**Thumb Buttons:**

| Button | E73 Pin | nRF52840 | Notes |
|--------|---------|----------|-------|
| T1 | 18 | P0.04 | AI2 (analog input) |
| T2 | 11 | P0.00 | XL1 (32kHz crystal pin) |
| T3 | 16 | P0.08 | - |
| T4 | 33 | P0.13 | - |

**Finger Buttons (Row 0-4, Left/Middle/Right):**

| Button | E73 Pin | nRF52840 | Notes |
|--------|---------|----------|-------|
| F0L | 36 | P0.22 | E73 routes pin 36 to P0.22, not P1.00! |
| F0M | 35 | P0.24 | - |
| F0R | 12 | P0.26 | - |
| F1L | 3 | P0.03 | - |
| F1M | 7 | P0.02 | AI0 (analog input) |
| F1R | 13 | P0.01 | XL2 (32kHz crystal pin) |
| F2L | 22 | P0.07 | - |
| F2M | 14 | P0.06 | - |
| F2R | 15 | P0.05 | AI3 (analog input) |
| F3L | 20 | P0.12 | - |
| F3M | 43 | P0.10 | NFC2 pin - requires UICR.NFCPINS=0xFFFFFFFE |
| F3R | 41 | P0.09 | NFC1 pin - requires UICR.NFCPINS=0xFFFFFFFE |
| F4L | 38 | P0.15 | E73 routes pin 38 to P0.15, not P1.02! |
| F4M | 40 | P0.20 | E73 routes pin 40 to P0.20, not P1.04! |
| F4R | 42 | P0.17 | E73 routes pin 42 to P0.17, not P1.06! |

**Thumb Sensor (Optical/Touch):**

| Signal | E73 Pin | nRF52840 | Notes |
|--------|---------|----------|-------|
| T0A | 10 | P0.30 | AI6 (analog input) |
| T0B | 9 | P0.31 | AI7 (analog input) |
| T0C | 8 | P0.29 | AI5 (analog input) |

### Important Notes

1. **NFC Pins**: P0.09 (F3R) and P0.10 (F3M) are NFC antenna pins by default. To use as GPIO, write `0xFFFFFFFE` to UICR.NFCPINS (address 0x1000120C) and reset.

2. **Crystal Pins**: P0.00 (T2) and P0.01 (F1R) are 32.768 kHz crystal pins (XL1/XL2). Can be used as GPIO when external crystal is not populated.

3. **Analog Inputs**: Several button pins (P0.02, P0.04, P0.05, P0.29-P0.31) are analog-capable, which may be intentional for future features or simply convenient routing.

**Key pins:**
- Debug: Pin 37 (SWDIO), Pin 39 (SWDCLK)
- Thumb sensor: Pins 8-10 (P0.29-P0.31) - analog inputs, protocol TBD
- LED: Pin 6 (P1.13), Pin 2 (P1.10 - data line)

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

## Peripheral Usage (Likely)

| Peripheral | Base Address | Purpose |
|------------|--------------|---------|
| USBD | 0x40027000 | USB HID + Mass Storage |
| GPIO P0/P1 | 0x50000000 | Direct button inputs |
| GPIOTE | 0x40006000 | Button interrupts |
| SAADC | 0x40007000 | Battery monitoring |

**Note**: Thumb sensor and LED protocols not yet determined.

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
