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

### Results: Partial Mappings (Pre-Desoldering)

| Button | GPIO | E73 Pin |
|--------|------|---------|
| T1 (N) | P0.00 | 11 |
| F1R | P0.01 | 13 |
| F1M | P0.02 | 7 |
| F1L | P0.03 | 3 |
| F2R | P0.05 | 15 |

These 5 buttons were confirmed via edge-accessible pads.

### Full Mapping via Module Desoldering

The remaining buttons route to E73 pads underneath the module. Hot-air desoldering exposed all 43 pads for continuity testing.

**Method**:
- Hot air rework station at ~350°C
- Flux applied to module edges
- Heated evenly until solder reflowed
- Lifted module with tweezers
- Continuity testing on all button traces

See [Complete GPIO Summary](#complete-gpio-summary) for full mapping table.

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

## GPIO Reading Technique: Direct Register Polling

The nchorder firmware uses **direct GPIO register polling**, not GPIOTE interrupts:

```c
// Read all 32 GPIO pins on port 0 in one register read
uint32_t p0_in = NRF_P0->IN;

// Check specific button (active-low: bit=0 means pressed)
if ((p0_in & (1 << PIN_NUMBER)) == 0) {
    // Button is pressed
}
```

**Why polling instead of GPIOTE?**
- GPIOTE interrupts were found to be unreliable for button scanning
- Polling is simple and deterministic
- With 16+ buttons, polling a single 32-bit register is efficient
- Button state can be captured as a complete snapshot (avoids race conditions)

The polling loop runs at a fixed rate (typically every 1-10ms) and reads both `NRF_P0->IN` and `NRF_P1->IN` to capture all button states.

**Register addresses**:
- `NRF_P0->IN` (0x50000510): Port 0 input pins
- `NRF_P1->IN` (0x50000810): Port 1 input pins

**Alternative: GPIOTE (not used)**

GPIOTE (GPIO Tasks and Events) provides interrupt-driven detection:
- **Pros**: CPU notified when state changes (efficient when idle)
- **Cons**: More complex, can miss rapid transitions, unreliable for button matrices

GPIOTE remains available for wakeup from low-power sleep modes.

## Complete GPIO Summary

**Update (Jan 2026)**: Complete button GPIO mapping determined by:
1. Desoldering E73 module and continuity testing all 43 pads to PCB traces
2. **Empirical GPIO testing** via bare-metal firmware (critical for F4 row!)

### Critical Finding: E73 Module Routing Discrepancy

The E73-2G4M08S1C datasheet has multiple incorrect pin mappings:
- Pin 36 claimed to be P1.00, **actually routes to P0.22** (F0L button)
- Pin 38 claimed to be P1.02, **actually routes to P0.15** (F4L button)
- Pin 40 claimed to be P1.04, **actually routes to P0.20** (F4M button)
- Pin 42 claimed to be P1.06, **actually routes to P0.17** (F4R button)

This was confirmed by:
- Multimeter: Buttons physically connect to E73 pins 36/38/40/42
- GPIO test firmware: Only P0.22/P0.15/P0.20/P0.17 respond to button presses
- P1.00/P1.02/P1.04/P1.06 never changed regardless of button state

**Lesson learned**: Always verify GPIO mappings empirically - datasheets can be wrong!

### Button-to-GPIO Mapping (Fully Verified Jan 2026)

| Button | E73 Pin | GPIO | Status | Notes |
|--------|---------|------|--------|-------|
| **Thumb Buttons** |
| T0 | 8 | P0.29 | ✅ Verified | Extra thumb button (labeled as sensor in original) |
| T1 (N) | 11 | P0.00 | ✅ Verified | Num modifier |
| T2 (A) | 18 | P0.04 | ✅ Verified | Alt modifier |
| T3 (E) | 16 | P0.08 | ✅ Verified | Ctrl/Enter modifier |
| T4 (SP) | 33 | P0.13 | ✅ Verified | Shift/Space modifier |
| **Finger Row 0 (Mouse)** |
| F0L | 36 | **P0.22** | ✅ Verified | ⚠️ Datasheet says P1.00 - WRONG! |
| F0M | 35 | P0.24 | ✅ Verified | |
| F0R | 12 | P0.26 | ✅ Verified | |
| **Finger Row 1 (Index)** |
| F1L | 3 | P0.03 | ✅ Verified | |
| F1M | 7 | P0.02 | ✅ Verified | |
| F1R | 13 | P0.01 | ✅ Verified | |
| **Finger Row 2 (Middle)** |
| F2L | 22 | P0.07 | ✅ Verified | |
| F2M | 14 | P0.06 | ✅ Verified | |
| F2R | 15 | P0.05 | ✅ Verified | |
| **Finger Row 3 (Ring)** |
| F3L | 20 | P0.12 | ✅ Verified | |
| F3M | 43 | P0.10 | ✅ Verified | NFC2 - requires UICR.NFCPINS=0xFFFFFFFE |
| F3R | 41 | P0.09 | ✅ Verified | NFC1 - requires UICR.NFCPINS=0xFFFFFFFE |
| **Finger Row 4 (Pinky)** |
| F4L | 38 | **P0.15** | ✅ Verified | ⚠️ Datasheet says P1.02 - WRONG! |
| F4M | 40 | **P0.20** | ✅ Verified | ⚠️ Datasheet says P1.04 - WRONG! |
| F4R | 42 | **P0.17** | ✅ Verified | ⚠️ Datasheet says P1.06 - WRONG! |

### Expansion GPIOs (J3 Header)

The J3 header exposes two spare GPIOs that can be used for additional buttons or other purposes:

| Name | E73 Pin | GPIO | J3 Pin | Notes |
|------|---------|------|--------|-------|
| EXT1 | 4 | P0.28 | 5 | Can bodge to broken F0L |
| EXT2 | 17 | P1.09 | 6 | Spare expansion |

These are active-low with internal pull-ups, matching the standard button configuration.

### GPIO Summary by Port

**Port 0 (21 GPIOs)**:
- Thumb: P0.00 (T1), P0.04 (T2), P0.08 (T3), P0.13 (T4), P0.29 (T0)
- F0 row: P0.22 (F0L), P0.24 (F0M), P0.26 (F0R)
- F1 row: P0.01, P0.02, P0.03
- F2 row: P0.05, P0.06, P0.07
- F3 row: P0.09, P0.10, P0.12
- F4 row: P0.15, P0.17, P0.20
- Expansion: P0.28 (EXT1)

**Port 1 (1 GPIO)**:
- P1.09 (EXT2) - Expansion

### Other GPIO Usage

| Function | E73 Pin | GPIO | Notes |
|----------|---------|------|-------|
| LED Power Enable | 2 | P1.10 | Controls Q1 transistor for LED strip power |
| LED Data | 6 | P1.13 | RGB LED data line (see [04-LED_CONTROL.md](04-LED_CONTROL.md)) |
| I2C SDA | - | P0.30 | J3 header, optical sensor |
| I2C SCL | - | P0.31 | J3 header, optical sensor |

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

[← Back to Index](README.md) | [Next: Config Format →](06-CONFIG_FORMAT.md)
