# LED Control: RGB Addressable LEDs

[← Back to Index](README.md) | [Previous: GPIO Discovery](03-GPIO_DISCOVERY.md)

This document describes the RGB LED system on the Twiddler 4 and how to control it with custom firmware.

## Hardware Overview

The Twiddler 4 has **3 RGB addressable LEDs** on the thumb board, daisy-chained on a single data line.

### Pin Configuration

| Function | E73 Pin | GPIO | Description |
|----------|---------|------|-------------|
| Power Enable | 2 | P1.10 | Controls Q1 transistor for LED power |
| Data | 6 | P1.13 | Serial data line to LED strip |

### Power Architecture

Unlike typical WS2812 setups where LEDs are always powered, the Twiddler 4 uses a **transistor-controlled power supply**:

```
Battery ──┬── Q1 (transistor) ──── LED Strip VCC
          │
P1.10 ────┴── Q1 Base (control)

P1.13 ────────────────────────── LED Strip Data In
```

**Key discovery**: The LEDs won't light unless P1.10 is set HIGH first!

This design allows the MCU to completely cut power to the LEDs for battery saving.

## Protocol Discovery

### Initial Assumptions (Wrong)

We initially assumed these were standard WS2812/SK6812 LEDs based on:
- Single-wire data protocol
- 3 addressable RGB LEDs in series
- Similar form factor

WS2812 uses **GRB color order** (Green, Red, Blue).

### Empirical Testing

Through systematic testing, we discovered:

1. **Holding data line HIGH** → Blue LED illuminates
2. **Sending GRB data {0x00, 0xFF, 0x00}** expecting red → Got **green**
3. **Sending RGB data {0xFF, 0x00, 0x00}** → Got **red** ✓

**Conclusion**: These LEDs use **RGB order**, not GRB like WS2812!

### Timing Discovery

Our first attempts with `__NOP()` timing failed silently. Through experimentation:

| Approach | Result |
|----------|--------|
| `__NOP()` based timing | No output (timing too fast/inconsistent) |
| `nrf_delay_us(1)` based timing | Works reliably |

The nRF52840 runs at 64MHz, making `__NOP()` approximately 15.6ns per instruction. The required timing precision for the LED protocol was better achieved with `nrf_delay_us()`.

## Working Implementation

### Initialization

```c
// Enable LED power via Q1 transistor
nrf_gpio_cfg_output(PIN_LED_POWER);  // P1.10
nrf_gpio_pin_set(PIN_LED_POWER);      // HIGH = power on
nrf_delay_ms(10);  // Let power stabilize

// Configure data pin
nrf_gpio_cfg_output(PIN_LED_DATA);    // P1.13
nrf_gpio_pin_clear(PIN_LED_DATA);
```

### Bit-Bang Protocol

```c
static void send_byte(uint8_t byte)
{
    for (int bit = 7; bit >= 0; bit--) {
        if (byte & (1 << bit)) {
            // '1' bit: longer high pulse
            nrf_gpio_pin_set(PIN_LED_DATA);
            nrf_delay_us(1);
            nrf_gpio_pin_clear(PIN_LED_DATA);
            // Short low (function call overhead is sufficient)
        } else {
            // '0' bit: shorter high pulse
            nrf_gpio_pin_set(PIN_LED_DATA);
            // Very short high (function call overhead)
            nrf_gpio_pin_clear(PIN_LED_DATA);
            nrf_delay_us(1);
        }
    }
}
```

### Sending Data to All 3 LEDs

```c
// Reset pulse (>50us low)
nrf_gpio_pin_clear(PIN_LED_DATA);
nrf_delay_us(80);

// Send RGB data for each LED (RGB order, NOT GRB!)
for (int led = 0; led < 3; led++) {
    send_byte(colors[led].r);  // Red first
    send_byte(colors[led].g);  // Green second
    send_byte(colors[led].b);  // Blue third
}

// Latch pulse (>50us low)
nrf_gpio_pin_clear(PIN_LED_DATA);
nrf_delay_us(80);
```

## Color Definitions

Since these LEDs use RGB order, color macros are straightforward:

```c
// RGB order (R, G, B)
#define LED_COLOR_OFF     0x00, 0x00, 0x00
#define LED_COLOR_RED     0xFF, 0x00, 0x00
#define LED_COLOR_GREEN   0x00, 0xFF, 0x00
#define LED_COLOR_BLUE    0x00, 0x00, 0xFF
#define LED_COLOR_WHITE   0xFF, 0xFF, 0xFF
#define LED_COLOR_YELLOW  0xFF, 0xFF, 0x00
#define LED_COLOR_CYAN    0x00, 0xFF, 0xFF
#define LED_COLOR_MAGENTA 0xFF, 0x00, 0xFF

// Dimmed versions (25% brightness)
#define LED_DIM_RED       0x40, 0x00, 0x00
#define LED_DIM_GREEN     0x00, 0x40, 0x00
#define LED_DIM_BLUE      0x00, 0x00, 0x40
#define LED_DIM_WHITE     0x40, 0x40, 0x40
```

## Debugging Journey

This section documents the troubleshooting process for educational purposes.

### Problem 1: LEDs Not Lighting

**Symptom**: GPIO configured, data sent, but LEDs stay dark.

**Investigation**:
1. Measured P1.13 with multimeter - showed 0V
2. Traced LED circuit on PCB
3. Found trace going to Q1 transistor, not directly to LEDs

**Solution**: Discovered P1.10 controls power via Q1. Set P1.10 HIGH before sending data.

### Problem 2: Wrong Colors

**Symptom**: Sent green, got... something else.

**Initial theory**: Timing is off (all bits reading as 1 = white).

**Testing**:
1. Sent blue data → Got blue ✓
2. Sent green data → Got green... wait, that worked?
3. Sent red data with GRB order → Got green!

**Solution**: LEDs use RGB order, not GRB. Swapped byte order.

### Problem 3: Intermittent Failures

**Symptom**: Code worked once, then stopped working after refactoring.

**Investigation**:
1. Inline test code worked
2. Same logic in a function didn't work
3. `__NOP()` timing was being optimized differently

**Solution**: Switched to `nrf_delay_us()` for consistent timing regardless of compiler optimization.

### Problem 4: Static HIGH = Blue

**Observation**: Holding P1.13 HIGH continuously shows blue on LED 1.

**Explanation**: With no protocol timing, the LED interprets continuous HIGH as... something. This isn't normal WS2812 behavior but helped confirm the data pin was connected.

## Key Lessons

1. **Don't assume standard protocols** - These LEDs look like WS2812 but use RGB order
2. **Check for power gating** - The transistor-controlled power was not obvious
3. **Timing matters** - `__NOP()` timing is fragile; `nrf_delay_us()` is more reliable
4. **Test incrementally** - Start with static GPIO states before protocol timing
5. **Document your failures** - The debugging journey is educational

## Hardware Identification

The exact LED part number is unknown. Characteristics:
- 3 LEDs in series on single data line
- RGB byte order (not GRB)
- Compatible with ~1µs bit timing
- Requires reset pulse of ~50µs

Likely candidates:
- SK6805 (RGB order variant)
- Custom/generic addressable LED

## Files

- `firmware/include/boards/board_twiddler4.h` - Pin definitions
- `firmware/include/nchorder_led.h` - LED driver API and color macros
- `firmware/src/nchorder_led.c` - LED driver implementation

---

[← Back to Index](README.md) | [Next: Config Format →](06-CONFIG_FORMAT.md)
