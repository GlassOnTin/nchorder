# I2C Bus Analysis

[← Back to Index](README.md) | [Previous: GPIO Discovery](03-GPIO_DISCOVERY.md)

This section covers finding I2C bus pins and identifying connected devices on the Twiddler 4.

## I2C Background

I2C (Inter-Integrated Circuit) is a two-wire serial protocol for connecting peripherals:

- **SCL** (Serial Clock): Clock signal, driven by master
- **SDA** (Serial Data): Bidirectional data line

**Electrical characteristics**:
- Open-drain with pull-up resistors
- Idle state is HIGH (both lines pulled up to VCC)
- Data changes when clock is LOW
- Multiple devices share the same bus, addressed by 7-bit address

## The Challenge

The Twiddler 4 has two I2C devices:
- Touchpad controller (Azoteq IQS5xx)
- RGB LEDs on thumb board

**Question**: Which GPIO pins are SCL and SDA?

## Step 1: Candidate Identification

### Typical nRF52840 I2C Pins

Nordic SDK examples commonly use:
- P0.26 for SDA
- P0.27 for SCL

However, the nRF52840 has flexible pin mapping - any GPIO can be configured for I2C.

### Module Constraint Discovery

Checking the E73 module pinout:
- P0.26 is exposed at pin 12 ✓
- P0.27 is **NOT exposed** ✗

**Conclusion**: The Twiddler cannot use the typical P0.26/P0.27 pair because P0.27 isn't available on the E73 module.

This is a reminder to never assume "typical" pin assignments. Always verify against actual hardware.

## Step 2: J3 Header Analysis

![J3 debug header closeup](../../photos/twiddler4/17_closeup_j3_pins_c1c4.jpg)

The J3 debug header exposes several GPIO pins. If the designers wanted to provide I2C access for debugging, they would route it here.

| J3 Pin | Signal | Potential Use |
|--------|--------|---------------|
| 1 | VDD | Power |
| 2 | P0.31 | I2C candidate |
| 3 | P0.30 | I2C candidate |
| 4 | GND | Ground |
| 5 | P0.28 | I2C candidate |
| 6 | P1.09 | GPIO |
| 7 | VDH | Battery |

P0.30 and P0.31 are adjacent, commonly used together, and both have ADC capability (useful for analog functions, but also indicates general-purpose use).

## Step 3: Oscilloscope Verification

### Setup

1. Connect scope probe to J3 pin 2 (P0.31)
2. Connect second probe to J3 pin 3 (P0.30)
3. Connect ground clip to J3 pin 4 (GND)
4. Power on device

### Observations

**P0.31 and P0.30 (J3 pins 2-3)**:
- Both lines idle HIGH (~3.3V)
- Regular pulses dropping to 0V observed
- Behavior consistent with I2C (open-drain with pull-ups)

**Note**: Data patterns were not decoded on the oscilloscope. A logic analyzer with I2C decode would be needed to confirm protocol and identify device addresses.

### Without an Oscilloscope

You can use a multimeter to check:
- Both lines should read ~3.3V DC when idle (pull-ups active)
- With device active, voltage may fluctuate slightly

A logic analyzer is even better - it can decode I2C protocol and show device addresses.

## I2C Devices on the Bus

### Touchpad: Azoteq IQS5xx

| Parameter | Value |
|-----------|-------|
| Device | Azoteq IQS5xx (IQS550/572/525-B000) |
| Default I2C Address | 0x74 (7-bit) - from datasheet |
| Write Address | 0xE8 (0x74 << 1 \| 0) |
| Read Address | 0xE9 (0x74 << 1 \| 1) |
| Function | Optical touchpad controller |

**Datasheet**: https://www.azoteq.com/images/stories/pdf/iqs5xx-b000_trackpad_datasheet.pdf

**Note**: Address is from datasheet default. Actual address not verified via logic analyzer.

### RGB LEDs: Addressable (WS2812/SK6812)

The thumb board has three RGB LEDs (L1, L2, L3). Analysis suggests these are **addressable LEDs** (WS2812B or SK6812), NOT I2C-controlled.

| Parameter | Value |
|-----------|-------|
| Device | WS2812B or SK6812 (likely) |
| Protocol | Single-wire, 800kHz data |
| Interface | I2S peripheral |
| Function | Status indication RGB LEDs |

**Evidence for addressable LEDs:**
1. **FFC constraint**: J6 has 12 pins (4 buttons, 2 I2C, 2 power, 4 remaining). Single-wire addressable LEDs need only 1 data line for all 3 LEDs.
2. **LED package**: Photos show 4-pin SMD packages consistent with WS2812B/SK6812 (VCC, GND, DIN, DOUT).
3. **PCB traces**: Routing pattern shows daisy-chain compatible layout from J6 to L1→L2→L3.

**To verify**: Probe the LED data line with oscilloscope during LED animation. WS2812 protocol has distinctive 800kHz timing with 0.4µs/0.8µs pulse widths.

## I2C Address Calculation

I2C uses 7-bit addresses, but the byte on the wire includes a R/W bit:

```
7-bit address:  0x74 = 0111 0100

Write byte: (0x74 << 1) | 0 = 0xE8 = 1110 1000
Read byte:  (0x74 << 1) | 1 = 0xE9 = 1110 1001
```

When searching firmware for I2C addresses, look for both forms.

## I2C Protocol Basics

A typical I2C transaction:

```
START | Address+W | ACK | Register | ACK | Data | ACK | STOP
       |<- Master sends ->| |<- Slave ACKs ->|

START | Address+W | ACK | Register | ACK | RESTART | Address+R | ACK | Data | NACK | STOP
       |<- Write register address ->|              |<- Read data from register ->|
```

**Key observations on oscilloscope**:
- START: SDA falls while SCL is HIGH
- STOP: SDA rises while SCL is HIGH
- Data: SDA stable while SCL is HIGH
- ACK: Receiver pulls SDA LOW

## Likely I2C Configuration

Based on oscilloscope observations and pin availability:

| Parameter | Value |
|-----------|-------|
| SCL Pin | P0.31 (E73 pin 9, J3 pin 2) - likely |
| SDA Pin | P0.30 (E73 pin 10, J3 pin 3) - likely |
| Pull-ups | External (on PCB) |
| Speed | Unknown (typical 100-400 kHz) |

**To confirm**: Use logic analyzer with I2C decode on J3 pins 2-3.

## Why Non-Standard Pins?

The Twiddler uses P0.30/P0.31 instead of the typical P0.26/P0.27 because:

1. **P0.27 is not exposed** on the E73 module
2. **P0.30/P0.31 are exposed** at pins 9-10
3. **J3 provides easy access** for debugging

This is a great example of how module constraints drive design decisions.

## Next Steps for I2C Analysis

To fully reverse-engineer the I2C devices:

1. **Capture traffic** - Logic analyzer on J3 pins 2-3
2. **Decode addresses** - Identify all devices on the bus
3. **Map registers** - Correlate writes with device behavior
4. **Compare to datasheet** - Match with known device protocols

## Key Lessons

1. **Don't assume standard pins** - Verify against actual hardware
2. **Debug headers help** - Look for exposed I2C on test points
3. **Understand addressing** - 7-bit vs 8-bit can cause confusion
4. **Protocol knowledge helps** - Knowing I2C lets you interpret signals

---

[← Back to Index](README.md) | [Next: Config Format →](06-CONFIG_FORMAT.md)
