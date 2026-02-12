# Optical Thumb Sensor: PixArt PAW-A350

[← Back to Index](README.md) | [Previous: LED Control](04-LED_CONTROL.md)

This document covers the optical finger navigation sensor on the Twiddler 4 thumb board.

## Sensor Identification

The sensor was identified via a label on the flex cable connector:

![PAW-A350 label on flex cable](../../photos/twiddler4/28_paw_a350_label_j5.jpg)

**Label text**: `PAW-A350 N14001 YMB40001 @ENG`

The same label is visible on the back of the sensor module:

![PAW-A350 module back with label](../../photos/twiddler4/29_paw_a350_module_back_label.jpg)

### PixArt PAW-A350 Specifications

| Parameter | Value |
|-----------|-------|
| Manufacturer | PixArt Imaging |
| Part Number | PAW-A350 |
| Type | Optical Finger Navigation (OFN) |
| Resolution | 125-1250 CPI (adjustable) |
| Interface | I2C or 4-wire SPI |
| Illumination | IR LED |
| Lens | ADBL-A321 |
| Compatibility | AVAGO ADBS-A350 |

**Key feature**: Low-power design with automatic power management modes.

## Hardware Overview

### Thumb Board Assembly

![Thumb board with RGB LEDs and sensor](../../photos/twiddler4/23_thumb_board_overview_rgb_leds.jpg)

The thumb board contains:
- PAW-A350 optical sensor module (center, raised)
- 3 RGB LEDs (visible as R/G/B colors)
- Thumb buttons (black domes)
- FFC connector to main board

![Thumb board back showing connectors](../../photos/twiddler4/35_thumb_board_back_j4_j5_j6.jpg)

Back of the thumb board showing J4 (FFC to main board), J5 (sensor module), J6 (sensor flex cable), and L1/L2/L3 RGB LEDs.

### Sensor Module

![Sensor module front view](../../photos/twiddler4/26_sensor_module_front_lens_j6.jpg)

The sensor module sits in a raised housing with a lens aperture facing down toward the thumb surface.

![Sensor module side view](../../photos/twiddler4/27_sensor_module_side_view.jpg)

The module connects via a small flex cable to connector J5 on the thumb PCB.

![PAW-A350 module front showing lens](../../photos/twiddler4/30_paw_a350_module_front_lens.jpg)

The front of the module shows the lens assembly and FPC flex cable with exposed traces.

![PAW-A350 side view with LED wires](../../photos/twiddler4/31_paw_a350_side_view_led_wires.jpg)

Side view showing the IR LED illumination (visible as magenta glow through the housing).

### FFC Connection

![FFC connector closeup](../../photos/twiddler4/24_ffc_connector_pins_closeup.jpg)

![FPC connector pins closeup](../../photos/twiddler4/32_fpc_connector_pins_closeup.jpg)

Closeup of the FPC connector showing the gold-plated contact pads.

![FPC flex under microscope](../../photos/twiddler4/33_fpc_flex_microscope_traces.jpg)

![FPC flex test pads](../../photos/twiddler4/34_fpc_flex_test_pads.jpg)

Microscope views of the FPC flex cable showing trace routing and test pads.

The 12-pin FFC cable (J6) connects the thumb board to the main PCB (see [02-HARDWARE_RE.md](02-HARDWARE_RE.md) for complete J6 pinout).

**Note**: FFC pin numbering appears reversed between main board (J6) and thumb board (J5) only if one board is viewed upside down. With both boards oriented the same way, pin numbering is consistent.

### J4 Sensor Connector Pinout (17 pins)

J4 connects the sensor module flex PCB to the thumb board. Verified by continuity testing with sensor module and J4 desoldered (Feb 2026).

| J4 Pin | E73 Pad | nRF52840 | Signal |
|--------|---------|----------|--------|
| 1 | - | - | NC |
| 2 | - | GND | Ground |
| 3 | - | VCC | 3.3V power |
| 4 | E73.9 | P0.31 | I2C SDA |
| 5 | - | GND | Ground |
| 6 | E73.8 | P0.29 | T0 button (on sensor flex PCB) |
| 7 | E73.1 | P1.11 | SHUTDOWN (active LOW) |
| 8 | - | GND | Ground |
| 9 | - | GND | Ground |
| 10 | - | VCC | 3.3V power |
| 11 | E73.10 | P0.30 | I2C SCL |
| 12 | - | - | NC |
| 13 | - | - | NC |
| 14 | - | VCC | 3.3V power |
| 15 | - | GND | Ground |
| 16 | - | GND | Ground |
| 17 | - | - | NC |

Summary: 6x GND, 3x VCC, 1x SDA, 1x SCL, 1x SHUTDOWN, 1x T0 button, 4x NC.

**Note**: Signal pins verified by direct continuity from J4 pads to E73 module pads on the main board (J4.4→E73.9, J4.6→E73.8, J4.7→E73.1, J4.11→E73.10). J5/J6 FFC connector screen print is inverted between main board and thumb board, making intermediate pin counting unreliable - always verify against E73 pads.

## Pin Mapping

**VERIFIED WORKING**: I2C communication confirmed with PAW-A350 sensor.

| Signal | E73 Pin | nRF52840 | Function |
|--------|---------|----------|----------|
| SCL | 10 | P0.30 | I2C clock |
| SDA | 9 | P0.31 | I2C data |
| SHUTDOWN | 1 | P1.11 | Sensor enable (LOW = active) |

**I2C Configuration**:
- **Address**: 0x33 (7-bit, verified via bus scan)
- **Product ID**: 0x88 (confirmed via register 0x00)
- **Frequency**: 100kHz (tested working)

**Note**: P0.29 connects to thumb button T0, which is physically mounted on the underside of the sensor module and soldered to the same flex PCB. It routes through J4 pin 6.

**Pull-up resistors**: SDA and SCL have ~600Ω pull-ups to 3.3V on the sensor module PCB.

## Interface

The sensor uses **standard I2C** at address **0x33**, communicating via the nRF52840 TWI1 hardware peripheral at 100kHz. The thumb board has on-board circuitry tying CS, MOSI, IO_SEL, and NRST to fixed levels, so only SDA, SCL, and SHUTDOWN are routed to the main board.

After deasserting SHUTDOWN (drive LOW to enable), wait at least 50ms (`tWAKEUP`) before I2C access.

**Power requirements** (from [mbed ADBM-A350 guide](https://os.mbed.com/teams/PixArt/code/ADBM-A350_referenceCode/wiki/Guide-for-nRF52-DK-Platform)):
- VDD (core): 1.7-2.1V (typical 1.8V)
- VDDIO (I/O): 1.65-3.6V — 3.3V is within spec

### Investigation Notes

The mbed reference documents address 0x57 (with CS and MOSI tied HIGH) and the eval board uses 5+ GPIO pins. The actual Twiddler 4 hardware uses address 0x33 — discovered via I2C bus scan after the correct pins (P0.30/P0.31, not P0.29/P1.11) were identified.

SPI and PAW3204-style 2-wire protocols were also tested during investigation and produced no response — the sensor only responds to standard I2C.

## Register Map (Typical for PixArt OFN)

Based on similar PixArt sensors:

| Register | Address | Description |
|----------|---------|-------------|
| Product_ID | 0x00 | Device identification |
| Motion | 0x02 | Motion status (bit 7 = motion detected) |
| Delta_X | 0x03 | X-axis movement (signed 8-bit) |
| Delta_Y | 0x04 | Y-axis movement (signed 8-bit) |
| SQUAL | 0x05 | Surface quality indicator |
| Config | 0x06 | Configuration register |

**Note**: Actual register map may differ. Datasheet required for confirmation.

## Implementation Status

### Working ✓
- Sensor physically identified as PAW-A350 (Product ID: 0x88)
- Pin mapping verified: P0.30=SCL, P0.31=SDA, P1.11=SHUTDOWN
- I2C communication at address 0x33 (100kHz)
- SHUTDOWN control: drive LOW to enable sensor
- Motion register reading (dx, dy, squal)
- USB HID mouse integration (sends motion reports)
- Driver: `firmware/src/nchorder_optical.c`

### Key Findings
- I2C address is 0x33 (not 0x57 as documented in mbed reference)
- SDA/SCL were swapped from initial assumption
- P1.11 is SHUTDOWN pin (not MOTION interrupt)
- P0.29 connects to thumb button T0, not sensor

## Driver Implementation

The firmware includes a full optical sensor driver:

```c
// Initialize and detect sensor
bool nchorder_optical_init(void);

// Read motion data (call in main loop)
bool nchorder_optical_read_motion(optical_motion_t *motion);

// Control sensor power
void nchorder_optical_sleep(void);
void nchorder_optical_wake(void);
```

Motion data is automatically sent to USB HID mouse when detected.

## Sensor Internals

![PAW-A350 disassembly showing lens, die, and module](../../photos/twiddler4/36_paw_a350_disassembly_lens_die.jpg)

Disassembly of the PAW-A350 module showing (left to right): lens assembly, sensor die with bond wires visible, and complete module with FPC attached.

![PAW-A350 sensor, lens, and housing separated](../../photos/twiddler4/37_paw_a350_sensor_lens_housing.jpg)

The three components separated: sensor package with bond wires and IC pins (left), ADBL-A321 lens assembly (center), and plastic housing with IR filter window (right).

## References

- [PixArt PAW-A350 Product Page](https://www.codico.com/en/en/current/news/paw-a350-optical-finger-navigation-chip-by-pixart)
- [mbed ADBM-A350 Reference Code](https://os.mbed.com/teams/PixArt/code/ADBM-A350_referenceCode/)
- [mbed nRF52-DK Setup Guide](https://os.mbed.com/teams/PixArt/code/ADBM-A350_referenceCode/wiki/Guide-for-nRF52-DK-Platform)
- [ADBM-A350 Datasheet (DigiKey)](https://media.digikey.com/pdf/data%20sheets/avago%20pdfs/adbm-a350.pdf)
- [ADBS-A350 Datasheet (DigiKey)](https://media.digikey.com/pdf/Data%20Sheets/Avago%20PDFs/ADBS-A350.pdf)
- [PAW3204 Datasheet](https://www.alldatasheet.com/datasheet-pdf/pdf/333267/PIXART/PAW3204.html) - 2-wire protocol reference

## Files

- `firmware/include/nchorder_optical.h` - Optical sensor driver API
- `firmware/src/nchorder_optical.c` - Driver implementation
- `firmware/include/boards/board_twiddler4.h` - Pin definitions
- `firmware/src/nchorder_mouse.c` - USB HID mouse integration

---

[← Back to Index](README.md) | [Next: Config Format →](06-CONFIG_FORMAT.md)
