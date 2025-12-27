# Northern Chorder

Open-source firmware and tools for nRF52840-based chorded keyboards.

## Overview

This project provides:
- **Firmware** for nRF52840-based chorded keyboards (BLE + USB HID)
- **Python tools** for config file management, conversion, and analysis
- **Community layouts** including MirrorWalk and variants
- **Hardware documentation** for nRF52840-based chorded keyboards

## Project Status

| Component | Status |
|-----------|--------|
| BLE HID keyboard | Working |
| USB HID keyboard | Working |
| Config file loading (FDS) | Working |
| Multi-character macros | Working |
| Chord detection (16 buttons) | Working |
| Python tools (79 tests) | Working |
| Consumer control (media keys) | Not implemented |
| RGB LEDs | Not implemented |
| Touchpad | Not implemented |

## Hardware

Designed for chorded keyboards using the **EByte E73-2G4M08S1C** module:

| Spec | Value |
|------|-------|
| MCU | Nordic nRF52840 (ARM Cortex-M4F @ 64MHz) |
| Flash | 1 MB |
| RAM | 256 KB |
| Connectivity | BLE 5.0, USB 2.0 |
| Buttons | 16 direct GPIO inputs |

## Installation

### Python Tools

```bash
pip install -e .
```

### Firmware

Requires ARM GCC toolchain and Nordic nRF5 SDK 17.1.0.

```bash
cd firmware/pca10056/s140/armgcc
make
```

See `firmware/README.md` for detailed build and flash instructions.

## Python Tools

```bash
# Show config info
nchorder info config.cfg

# Convert CSV layout to binary config
nchorder convert layout.csv output.cfg

# Convert with system chords included
nchorder convert layout.csv output.cfg -s system.cfg

# Export to JSON (for web tutor apps)
nchorder json config.cfg -o layout.json

# Export with macros
nchorder json config.cfg --include-macros -o layout.json

# Dump config as human-readable text
nchorder dump config.cfg

# Find chord conflicts
nchorder conflicts config.cfg

# Show unmapped chord combinations
nchorder unmapped config.cfg

# Compare two configs
nchorder diff old.cfg new.cfg
```

## Config Files

The `configs/` directory contains:

| File | Description |
|------|-------------|
| `mirrorwalk.cfg` | MirrorWalk layout (BSD-3-Clause, by Griatch) |
| `mirrorwalk_nomcc.cfg` | MirrorWalk without multi-character chords |
| `mirrorwalk_source.csv` | Source CSV for MirrorWalk |
| `mirrorwalk.json` | JSON export for web tutor |

## Documentation

The `docs/` directory contains hardware documentation:

- **01-PRODUCT_OVERVIEW** - Chorded keyboard basics
- **02-HARDWARE_RE** - PCB teardown and component ID
- **03-GPIO_DISCOVERY** - Button-to-pin mapping
- **04-I2C_ANALYSIS** - I2C bus and device identification
- **06-CONFIG_FORMAT** - Binary config file structure

## Directory Structure

```
nchorder/
├── src/nchorder_tools/     # Python config tools
├── firmware/        # nRF52840 BLE/USB HID firmware
├── configs/                # Community chord layouts
├── docs/                   # Hardware documentation
├── photos/                 # Hardware teardown photos
└── tests/                  # Python test suite
```

## License

MIT - See [LICENSE.md](LICENSE.md) for details and third-party attribution.

## Trademark Notice

"Twiddler" is a trademark of Tek Gear Inc. This project is not affiliated with, endorsed by, or sponsored by Tek Gear Inc.
