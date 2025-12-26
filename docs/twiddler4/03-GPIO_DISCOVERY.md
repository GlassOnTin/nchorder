# GPIO Discovery: Button Mapping

[← Back to Index](README.md) | [Previous: Hardware RE](02-HARDWARE_RE.md)

This section demonstrates how to discover which GPIO pins connect to physical buttons, using the Twiddler 4's 16-button layout as an example.

## The Challenge

The Twiddler 4 has 16 buttons:
- 4 thumb buttons: T1 (N), T2 (A), T3 (E), T4 (SP)
- 12 finger buttons: 4 rows × 3 columns (Left, Middle, Right)

![Back of device showing finger buttons](../../photos/twiddler4/07_back_12_finger_buttons_usbc.jpg)

**Question**: Which nRF52840 GPIO pin connects to each button?

**Why this matters**: To write custom firmware or understand existing code, you need to know the physical-to-logical mapping.

## Background: How Buttons Connect to MCUs

There are two common approaches:

### Direct GPIO (One Pin Per Button)
```
Button 1 ──┬── GND
           │
MCU Pin ───┴── (internal pull-up)

When pressed: Pin reads LOW
When released: Pin reads HIGH (pulled up)
```

**Pros**: Simple, fast to scan, no ghosting issues
**Cons**: Uses many GPIO pins (16 buttons = 16 pins)

### Matrix Scanning (Rows × Columns)
```
       Col1  Col2  Col3
        │     │     │
Row1 ───┼─────┼─────┼───
        │     │     │
Row2 ───┼─────┼─────┼───

Drive one row LOW, read columns
Repeat for each row
```

**Pros**: Fewer pins (4×4 matrix = 8 pins for 16 buttons)
**Cons**: More complex firmware, potential ghosting

## Step 1: Determine Architecture (Direct vs Matrix)

### Method: Continuity Testing

With power disconnected:
1. Place one multimeter probe on a button pad
2. Test continuity to ground (the other button pad)
3. Test continuity to other buttons

**Findings for Twiddler 4**:
- Each button has one pad connected directly to GND
- Each button's other pad connects to a unique trace (no shared rows/columns)
- No continuity between different buttons

**Conclusion**: Direct GPIO architecture, not a matrix.

**Why direct GPIO here?** The nRF52840 has plenty of GPIO pins, and 16 buttons doesn't require a matrix. Direct GPIO is simpler and has lower latency.

## Step 2: Trace Accessible Buttons

### Identifying E73 Module Pads

![E73 module closeup showing edge pads](../../photos/twiddler4/04_closeup_u1_module_r3r9_c1c4.jpg)

The E73 module has edge pads that are accessible for probing. From the pinout table:
- Odd-numbered pins (1, 3, 5, etc.) are on one edge
- Even-numbered pins (2, 4, 6, etc.) are on the opposite edge

Some pins are under the module and cannot be probed without desoldering.

### Probing Methodology

1. Set multimeter to continuity mode
2. Place one probe on a button's signal pad (not the GND pad)
3. Probe E73 module edge pads until you get a beep
4. Record the mapping

### Results: Confirmed Mappings

| Button | GPIO | E73 Pin | Location |
|--------|------|---------|----------|
| T1 (N) | P0.00 | 11 | Edge - accessible |
| F1R | P0.01 | 13 | Edge - accessible |
| F1M | P0.02 | 7 | Edge - accessible |
| F1L | P0.03 | 3 | Edge - accessible |
| F2R | P0.05 | 15 | Edge - accessible |

**Pattern observed**: Low-numbered P0 pins (0-5) are used for buttons with accessible pads.

### The Previously Inaccessible Buttons

The remaining 11 buttons route to E73 pads underneath the module, inaccessible without desoldering.

**Method used to verify**:

We hot-air desoldered the E73 module to expose all PCB traces:
- Used hot air rework station at ~350°C
- Applied flux to module edges
- Heated evenly until solder reflowed
- Lifted module with tweezers
- Performed continuity testing on all 16 button traces
- All mappings confirmed to match the pattern-inferred values

**Pattern confirmed**:
- Finger rows follow descending pin order: L=high, M=mid, R=low (e.g., F1: P0.03, P0.02, P0.01)
- Thumb buttons interleave between finger rows
- Pins P0.11, P0.14, P0.16, P0.18, P0.19 reserved for USB/power

## Step 3: Active Probing (Alternative Method)

With the device powered on:

1. Connect oscilloscope or logic analyzer to accessible GPIO pads
2. Press each button and observe which signal changes
3. Buttons are active-low: signal goes from HIGH (~3.3V) to LOW (0V) when pressed

**Tip**: The internal pull-up resistor means the signal idles HIGH. A pressed button pulls it to ground.

## Button Bitmask Mapping

In the Twiddler configuration format, buttons are represented as a 16-bit bitmask:

```
Bit 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
    |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |
    T4 T3 T2 T1 F4R... (finger buttons)    ...F1L
```

- **T1-T4**: Thumb buttons (T1=N, T2=A, T3=E, T4=SP)
- **F1L-F4R**: Finger buttons (row 1-4, Left/Middle/Right)

The firmware reads GPIO states and constructs this bitmask for chord matching.

## Understanding Pull-Up Resistors

The nRF52840 has configurable internal pull-ups (10-50kΩ typical).

**Why pull-ups?**
- Without a pull-up, an open switch leaves the input floating (undefined)
- Pull-up ensures a defined HIGH state when button is released
- Button press connects to GND, overriding the pull-up

**Configuration in nRF52840**:
```c
nrf_gpio_cfg_input(PIN_NUMBER, NRF_GPIO_PIN_PULLUP);
```

The firmware configures all 16 button pins this way during initialization.

## Interrupt vs Polling

The Twiddler uses GPIOTE (GPIO Tasks and Events) for interrupt-driven button detection:

- **Polling**: Check button state in a loop (wastes CPU cycles)
- **Interrupts**: CPU notified when state changes (efficient)

GPIOTE allows configuring interrupts on rising edge, falling edge, or both.

## Complete GPIO Summary

**Update (Dec 2025)**: Complete 16-button GPIO mapping determined via:
1. Hardware probing (5 pins confirmed)
2. Pattern recognition (finger rows use descending pin order)

**Key Finding**: All 16 buttons are on GPIO Port 0 (P0).

### Complete Button-to-GPIO Mapping

| Bit | Button | GPIO | E73 Pin |
|-----|--------|------|---------|
| 0 | T1 (N) | P0.00 | 33 |
| 1 | F1L | P0.03 | 25 |
| 2 | F1M | P0.02 | 29 |
| 3 | F1R | P0.01 | 35 |
| 4 | T2 (A) | P0.04 | 40 |
| 5 | F2L | P0.07 | 22 |
| 6 | F2M | P0.06 | 36 |
| 7 | F2R | P0.05 | 37 |
| 8 | T3 (E) | P0.08 | 38 |
| 9 | F3L | P0.12 | 42 |
| 10 | F3M | P0.10 | 64 |
| 11 | F3R | P0.09 | 62 |
| 12 | T4 (SP) | P0.13 | 54 |
| 13 | F4L | P0.20 | 53 |
| 14 | F4M | P0.17 | 51 |
| 15 | F4R | P0.15 | 49 |

All 16 GPIO mappings verified by continuity testing after hot-air desoldering the E73 module.

### GPIO Pins Used

All 16 button pins: `P0.00, P0.01, P0.02, P0.03, P0.04, P0.05, P0.06, P0.07, P0.08, P0.09, P0.10, P0.12, P0.13, P0.15, P0.17, P0.20`

Pins NOT used for buttons: `P0.11, P0.14, P0.16, P0.18, P0.19` (reserved for USB, QSPI, etc.)

### Pattern Observed

1. **Thumb buttons** use pins at group boundaries: P0.00, P0.04, P0.08, P0.13
2. **Finger rows** use 3 consecutive pins in **descending** order (L=high, M=mid, R=low):
   - Row 1: P0.03, P0.02, P0.01
   - Row 2: P0.07, P0.06, P0.05
   - Row 3: P0.12, P0.10, P0.09 (skips P0.11)
   - Row 4: P0.20, P0.17, P0.15 (skips several)

## Key Lessons

1. **Check architecture first** - Direct GPIO vs matrix changes your approach
2. **Start with accessible points** - Don't desolder until necessary
3. **Understand pull configuration** - Pull-up/pull-down determines idle state
4. **Document as you go** - Create mapping tables for future reference

## Button Statistics (Bonus)

From `INFO.TXT` on the device, we can see usage patterns:

```json
"button_presses": {
    "T0": 192, "T1": 28, "T2": 73, "T3": 63, "T4": 10,
    "F0L": 20, "F0M": 27, "F0R": 45,
    "F1L": 350, "F1M": 870, "F1R": 428,
    "F2L": 442, "F2M": 717, "F2R": 236,
    "F3L": 335, "F3M": 318, "F3R": 297,
    "F4L": 70, "F4M": 107, "F4R": 116
}
```

**Button naming**:
- T0-T4: Thumb buttons (T0=no thumb, T1=N, T2=A, T3=E, T4=SP)
- F0-F4, L/M/R: Finger rows 0-4, Left/Middle/Right columns

Most-used buttons are in the F1 and F2 rows - the easiest to reach.

---

[← Back to Index](README.md) | [Next: I2C Analysis →](04-I2C_ANALYSIS.md)
