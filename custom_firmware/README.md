# Northern Chorder Custom Firmware

Open-source BLE/USB HID keyboard firmware for nRF52840-based chorded keyboards.

## Features

- **BLE HID Keyboard**: Full Bluetooth Low Energy keyboard using Nordic S140 SoftDevice
- **USB HID Keyboard**: USB connectivity via Nordic USBD library
- **Config File Loading**: v7 format configs loaded from flash (FDS)
- **Multi-character Macros**: String output with timed keystroke sequences
- **Chord Detection**: 16-button state machine with debouncing
- **Mouse Chord Support**: Mouse actions mapped to keyboard keys (workaround)

## Hardware

Targets the **EByte E73-2G4M08S1C** module:

| Spec | Value |
|------|-------|
| MCU | Nordic nRF52840 (ARM Cortex-M4F @ 64MHz) |
| Flash | 1 MB |
| RAM | 256 KB |
| BLE | 4.2 / 5.0 |
| Buttons | 16 direct GPIO inputs (active low) |

See `MEMORY_MAP.md` for detailed memory layout.

## Building

Requires ARM GCC toolchain and Nordic nRF5 SDK 17.1.0.

```bash
# Install toolchain (Ubuntu/Debian)
sudo apt install gcc-arm-none-eabi

# Build firmware
cd custom_firmware/pca10056/s140/armgcc
make

# Output: _build/nrf52840_xxaa.hex (~108KB)
```

## Flashing

### Via USB Bootloader

1. Enter bootloader: Hold T14 + T0 while connecting USB
2. Device appears as mass storage
3. Copy `_build/nrf52840_xxaa.bin` to drive
4. Wait for LED to stop blinking
5. Power cycle

### Via J-Link (SWD)

```bash
nrfjprog --eraseall
nrfjprog --program s140_nrf52_7.2.0_softdevice.hex
nrfjprog --program _build/nrf52840_xxaa.hex
nrfjprog --reset
```

## File Structure

```
custom_firmware/
├── main.c                      # Main firmware - BLE/USB HID integration
├── MEMORY_MAP.md               # Hardware & memory documentation
├── pca10056/s140/armgcc/
│   ├── Makefile                # Build system
│   └── config/sdk_config.h     # Nordic SDK configuration
├── include/
│   ├── nchorder_buttons.h      # Button GPIO scanning API
│   ├── nchorder_chords.h       # Chord detection + macros API
│   ├── nchorder_config.h       # GPIO pin definitions
│   ├── nchorder_hid.h          # HID keycodes
│   ├── nchorder_storage.h      # FDS flash storage API
│   └── nchorder_usb.h          # USB HID API
└── src/
    ├── nchorder_buttons.c      # Button GPIO + debouncing (10ms)
    ├── nchorder_chords.c       # Chord state machine + config parsing
    ├── nchorder_storage.c      # FDS flash storage for configs
    └── nchorder_usb.c          # USB HID keyboard class
```

## Configuration

Configs are stored in flash using Nordic FDS (Flash Data Storage). The firmware reads v7 format binary configs compatible with community layouts.

To load a config, use the Python tools:
```bash
nchorder convert layout.csv output.cfg
# Then flash config via USB mass storage or custom upload tool
```

## Verification

Test with evtest after flashing:

```bash
sudo evtest /dev/input/eventX  # Select keyboard device
# Press a chord
# Expected: ONE press + ONE release per chord
```

## Not Yet Implemented

- **Consumer Control**: Media keys (volume, play/pause)
- **RGB LEDs**: WS2812 status indicators via I2S
- **True Mouse HID**: Actual mouse reports (currently mapped to keys)
- **Touchpad**: IQS5xx touch controller support
- **System Chords**: Config switching, sleep mode

## Contributing

Contributions welcome for:
- Consumer control HID descriptor and report handling
- RGB LED driver (WS2812/SK6812 via I2S peripheral)
- True mouse HID reports with separate report ID
- Touchpad I2C driver for IQS5xx

## Safety

- The SoftDevice (BLE stack) is separate from application firmware
- Bootloader is preserved at 0xF0000+
- Easy rollback: just flash original firmware via USB bootloader

## License

MIT License - See LICENSE file

## Disclaimer

This is unofficial community firmware. Use at your own risk.
Keep a backup of your original firmware before flashing.
