# Optical Thumb Sensor: PixArt PAW-A350

[← Back to Index](README.md) | [Previous: LED Control](04-LED_CONTROL.md)

This document covers the optical finger navigation sensor on the Twiddler 4 thumb board.

## Sensor Identification

The sensor was identified via a label on the flex cable connector:

![PAW-A350 label on flex cable](../../photos/twiddler4/28_paw_a350_label_j5.jpg)

**Label text**: `PAW-A350 N14001 YMB40001 @ENG`

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

### Sensor Module

![Sensor module front view](../../photos/twiddler4/26_sensor_module_front_lens_j6.jpg)

The sensor module sits in a raised housing with a lens aperture facing down toward the thumb surface.

![Sensor module side view](../../photos/twiddler4/27_sensor_module_side_view.jpg)

The module connects via a small flex cable to connector J5 on the thumb PCB.

### FFC Connection

![FFC connector closeup](../../photos/twiddler4/24_ffc_connector_pins_closeup.jpg)

The 12-pin FFC cable (J6) connects the thumb board to the main PCB, carrying:
- 4 thumb button GPIO lines
- 4 sensor signals (P0.29, P0.30, P0.31, P1.11)
- LED data line (P1.13)
- Power (VCC, GND)

## Pin Mapping

Based on PCB tracing and E73 module pinout:

| Signal | E73 Pin | nRF52840 | FFC Pin | Function |
|--------|---------|----------|---------|----------|
| ? | 8 | P0.29 | - | Unknown (CS? INT? NRST?) |
| SDA/MOSI | 10 | P0.30 | - | I2C SDA or SPI MOSI |
| SCL/SCLK | 9 | P0.31 | - | I2C SCL or SPI SCLK |
| MISO | 1 | P1.11 | - | SPI MISO (if SPI mode) |

**Note**: P0.29's function is unconfirmed. It may be:
- Chip select (NCS) for SPI mode
- Motion interrupt output
- Reset input (NRST)

## Interface Options

The PAW-A350 supports two communication modes:

### I2C Mode

Standard I2C with 7-bit addressing.

**Discovered I2C address**: `0x57` (from [mbed reference implementation](https://os.mbed.com/teams/PixArt/code/ADBM-A350_referenceCode/))

**I2C configuration** (per mbed guide):
- With CS and MOSI pins HIGH: address is 0x57
- SHUTDOWN pin (P0.29) controls sensor power - LOW = enabled

**Current status**: No response at 0x57. Possible issues:
- Sensor requires 1.8V power supply (thumb board may have regulator)
- Specific initialization sequence needed
- SHUTDOWN pin logic may be inverted

### 4-Wire SPI Mode

Standard SPI with dedicated MOSI/MISO lines:
- SCLK: Serial clock
- MOSI: Master out, slave in
- MISO: Master in, slave out
- NCS: Chip select (active low)

**SPI probe result**: No response (0xFF on all reads)

Tested configurations:
- SPI Mode 0 (CPOL=0, CPHA=0)
- SPI Mode 3 (CPOL=1, CPHA=1)
- P0.29 HIGH and LOW

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

## Investigation Status

### What Works
- Sensor physically identified as PAW-A350
- FFC pinout mapped
- Driver implemented with correct I2C address (0x57)
- USB HID mouse class enabled for Twiddler4

### What Doesn't Work Yet
- No response from sensor at I2C address 0x57
- Unable to read Product ID register

### Next Steps

1. **Logic analyzer capture**: Sniff original firmware's I2C communication
2. **Check voltage levels**: Sensor may need 1.8V I/O (thumb board likely has level shifter)
3. **Verify SHUTDOWN pin**: May need to pulse or use different polarity
4. **Wake sequence**: Try dummy write or specific register sequence

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

## References

- [PixArt PAW-A350 Product Page](https://www.codico.com/en/en/current/news/paw-a350-optical-finger-navigation-chip-by-pixart)
- [mbed ADBM-A350 Reference Code](https://os.mbed.org/teams/PixArt/code/ADBM-A350_referenceCode/)
- [ADBS-A350 Datasheet (DigiKey)](https://media.digikey.com/pdf/Data%20Sheets/Avago%20PDFs/ADBS-A350.pdf)

## Files

- `firmware/include/nchorder_optical.h` - Optical sensor driver API
- `firmware/src/nchorder_optical.c` - Driver implementation
- `firmware/include/boards/board_twiddler4.h` - Pin definitions
- `firmware/src/nchorder_mouse.c` - USB HID mouse integration

---

[← Back to Index](README.md) | [Next: Config Format →](06-CONFIG_FORMAT.md)
