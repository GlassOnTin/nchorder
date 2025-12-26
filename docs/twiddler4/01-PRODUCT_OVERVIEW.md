# Product Overview: Twiddler 4

[← Back to Index](README.md)

## What is a Chording Keyboard?

A **chording keyboard** produces characters by pressing multiple keys simultaneously (a "chord"), rather than pressing individual keys sequentially like a standard keyboard.

**Example**: On a Twiddler, pressing buttons N+A+E together might produce the letter "t", while N alone produces "n".

**Advantages**:
- One-handed operation - useful for wearable computing, accessibility
- Compact form factor - all keys reachable without moving the hand
- Potentially faster than hunt-and-peck typing once learned

**Trade-offs**:
- Steep learning curve (must memorize chord combinations)
- Lower maximum speed than touch typing for most users
- Specialized device with limited availability

## Twiddler 4 Specifications

| Specification | Value |
|---------------|-------|
| Manufacturer | Tek Gear |
| Model | Twiddler 4 |
| Buttons | 16 total (4 thumb + 12 finger) |
| Connectivity | USB-C, Bluetooth 5.0 BLE |
| Microcontroller | Nordic nRF52840 (ARM Cortex-M4F @ 64MHz) |
| Flash | 1MB internal |
| RAM | 256KB |
| Battery | Li-ion AAA-size (SACINO 10440, 3.7V 350mAh) |
| Touchpad | Azoteq IQS5xx optical controller |
| Storage | FAT12 filesystem (flash partition) |
| USB Interface | Composite: HID Keyboard + Mouse + Consumer Control + Mass Storage |

## Physical Layout

The Twiddler 4 consists of two PCB assemblies connected by a 12-pin flat flex cable (FFC):

### Main PCB

![Main PCB front with LED on](../../photos/twiddler4/02_main_pcb_front_led_on.jpg)

The main PCB contains:
- **U1** (center): EByte E73-2G4M08S1C module containing the nRF52840 SoC. This is the "brain" of the device - a small RF module with a ceramic antenna, about 18mm × 13mm.
- **J2** (top edge): 4-pin SWD debug header for firmware flashing and debugging
- **J3** (right edge): 7-pin extended debug header exposing I2C bus and additional GPIO
- **J1** (bottom): USB-C connector for charging and data
- **Battery holder**: Accepts AAA-sized Li-ion cell
- **12 finger buttons** (back side): Arranged in a 4×3 grid (4 rows × 3 columns: L/M/R)

### Thumb Board

![Thumb board with RGB LEDs](../../photos/twiddler4/06_thumb_board_l1l2l3_leds.jpg)

Connected via J6 (12-pin FFC), the thumb board contains:
- **4 thumb buttons**: T1 (N), T2 (A), T3 (E), T4 (SP) - labels shown are for left-hand models
- **L1, L2, L3**: RGB status LEDs (I2C controlled)
- **Touchpad**: Optical sensor for mouse cursor control

## Button Layout

![Back of device showing finger buttons](../../photos/twiddler4/07_back_12_finger_buttons_usbc.jpg)

```
Thumb buttons (top):     T1   T2   T3   T4
                        (N)  (A)  (E)  (SP)

Finger buttons (grid):   F1L  F1M  F1R    (index finger - top row)
                        F2L  F2M  F2R    (middle finger)
                        F3L  F3M  F3R    (ring finger)
                        F4L  F4M  F4R    (pinky finger - bottom row)
```

Each button position has a unique bit in the 16-bit chord bitmask used in configuration files.

## USB Interface

When connected via USB, the Twiddler 4 appears as a **composite device** with four interfaces:

1. **HID Keyboard**: Standard keyboard with modifier keys + 3 simultaneous keycodes
2. **HID Consumer Control**: Media keys (volume, play/pause, etc.)
3. **HID Mouse**: 3-button mouse with X/Y movement and scroll wheel (via touchpad)
4. **Mass Storage**: FAT12 filesystem containing configuration files

The mass storage interface exposes:
- `0.CFG`, `1.CFG`, `2.CFG`: Chord configuration files (switchable)
- `SETTINGS.TXT`: Device settings (Bluetooth enable, key repeat, etc.)
- `INFO.TXT`: Device information and button press statistics

## Block Diagram

```
                    ┌─────────────────────────────────────┐
                    │         Main PCB                    │
                    │  ┌─────────────────────────────┐    │
USB-C ──────────────┤  │    E73-2G4M08S1C Module     │    │
                    │  │    ┌─────────────────┐      │    │
                    │  │    │   nRF52840      │      │    │
                    │  │    │  ARM Cortex-M4F │      │    │
                    │  │    │  BLE 5.0        │      │    │
                    │  │    │  USB 2.0        │      │    │
                    │  │    │  1MB Flash      │      │    │
                    │  │    └────────┬────────┘      │    │
                    │  │             │               │    │
                    │  │    Ceramic Antenna          │    │
                    │  └─────────────│───────────────┘    │
                    │               GPIO                  │
                    │                │                    │
                    │    ┌───────────┴───────────┐        │
                    │    │                       │        │
                    │    ▼                       │        │
                    │ ┌────────────────┐         │        │
                    │ │  12 Finger     │         │        │
                    │ │  Buttons       │         │        │
                    │ │  (direct GPIO) │         │        │
                    │ └────────────────┘         │        │
                    │                            │        │
                    │              J6 (12-pin FFC)        │
                    │               GPIO + I2C + Power    │
                    └────────────────────┼────────────────┘
                                         │
                    ┌────────────────────┼────────────────┐
                    │          Thumb PCB                  │
                    │    ┌───────────────┴───────────┐    │
                    │    │      GPIO          I2C    │    │
                    │    │        │            │     │    │
                    │    │        ▼            ▼     │    │
                    │    │  ┌──────────┐  ┌───────┐  │    │
                    │    │  │ 4 Thumb  │  │Touch- │  │    │
                    │    │  │ Buttons  │  │pad    │  │    │
                    │    │  └──────────┘  │IQS5xx │  │    │
                    │    │                └───────┘  │    │
                    │    │  ┌────────────────────┐   │    │
                    │    │  │  RGB LEDs L1-L3    │   │    │
                    │    │  │  (I2S data line)   │   │    │
                    │    │  └────────────────────┘   │    │
                    │    └───────────────────────────┘    │
                    └─────────────────────────────────────┘
```

## Next Steps

- **To tear down the device**: Continue to [02-HARDWARE_RE.md](02-HARDWARE_RE.md)
- **To understand configuration files**: See [06-CONFIG_FORMAT.md](06-CONFIG_FORMAT.md)

---

[← Back to Index](README.md) | [Next: Hardware Documentation →](02-HARDWARE_RE.md)
