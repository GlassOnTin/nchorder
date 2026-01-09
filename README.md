# Northern Chorder

Open-source firmware and tools for nRF52840-based chorded keyboards.

## Quick Start - GUI App

Pre-built apps are available for all platforms:

| Platform | Download | Install |
|----------|----------|---------|
| **Windows** | [nchorder-gui-windows-x64.zip](https://github.com/GlassOnTin/nchorder/releases/latest) | Extract zip, run `nchorder-gui.exe` |
| **macOS** | [nchorder-gui-macos-arm64.zip](https://github.com/GlassOnTin/nchorder/releases/latest) | Extract, move .app to Applications |
| **Linux** | [nchorder-gui-linux-x86_64](https://github.com/GlassOnTin/nchorder/releases/latest) | `chmod +x` and run |
| **Android** | [nchorder-gui-android.apk](https://github.com/GlassOnTin/nchorder/releases/latest) | Install APK, allow unknown sources |

### What You Can Do

The GUI works standalone (no keyboard required) for:

- **Cheat Sheet** - Visual reference for all chords in your layout, organized by category
- **Exercise Mode** - Practice chords with scrolling prompts and real-time feedback
- **Chord Editor** - Browse and modify chord mappings

With a keyboard connected via USB:

- **Touch Visualizer** - Real-time display of button states and detected chords
- **Config Tuning** - Adjust thresholds, debounce, and mouse settings with live sliders
- **Debug View** - GPIO diagnostics for hardware troubleshooting

### Included Layouts

The app bundles community chord layouts in the `configs/` folder:

- **MirrorWalk** - Efficient layout optimized for English text (by Griatch)
- **TabSpace** - Alternative layout with different finger assignments

## Overview

This project provides:
- **Cross-platform GUI** for configuration, practice, and visualization
- **Firmware** for nRF52840-based chorded keyboards (BLE + USB HID)
- **Python tools** for config file management, conversion, and analysis
- **Community layouts** including MirrorWalk and variants
- **Hardware documentation** for nRF52840-based chorded keyboards

## Project Status

| Component | Status |
|-----------|--------|
| Cross-platform GUI | Working (Win/Mac/Linux/Android) |
| BLE HID keyboard | Working |
| USB HID keyboard | Working |
| Config file loading (FDS) | Working |
| Multi-character macros | Working |
| Chord detection (16 buttons) | Working |
| Python tools (79 tests) | Working |
| Consumer control (media keys) | Not implemented |
| RGB LEDs | Not implemented |
| Optical sensor (mouse) | Not implemented |

## Supported Hardware

| Board | Description | Build |
|-------|-------------|-------|
| **nChorder XIAO** | Seeed XIAO nRF52840 + Trill capacitive sensors | `make` (default) |
| **nRF52840-DK** | Nordic development kit (4 buttons for testing) | `make BOARD=dk` |
| **Twiddler 4** | Commercial chorded keyboard (EByte E73 module) | `make BOARD=twiddler4` |

All boards use the Nordic nRF52840 SoC (ARM Cortex-M4F @ 64MHz, 1MB flash, 256KB RAM).

## Firmware

The [firmware/](firmware/) directory contains the nRF52840 BLE/USB HID implementation.

### Quick Start

```bash
# Install ARM GCC toolchain
sudo apt install gcc-arm-none-eabi

# Download Nordic SDK 17.1.0
cd firmware
wget https://nsscprodmedia.blob.core.windows.net/prod/software-and-other-downloads/sdks/nrf5/binaries/nrf5_sdk_17.1.0_ddde560.zip
unzip nrf5_sdk_17.1.0_ddde560.zip -d sdk/

# Build (default: XIAO with Trill sensors)
make

# Flash via J-Link
make flash
```

### Build Options

```bash
make                    # XIAO nRF52840 (default)
make BOARD=dk           # nRF52840-DK
make BOARD=twiddler4    # Twiddler 4
make flash              # Flash application (preserves SoftDevice)
make flash_softdevice   # Flash Nordic BLE stack
```

See [firmware/README.md](firmware/README.md) for detailed instructions.

## Python Tools

### Installation

```bash
pip install -e .
```

### Usage

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

See [docs/README.md](docs/README.md) for full documentation.

- [**nchorder/**](docs/nchorder/) - XIAO hardware wiring and firmware notes
- [**twiddler4/**](docs/twiddler4/) - Twiddler 4 reverse engineering (GPIO, I2C, config format)

## Directory Structure

```
nchorder/
├── firmware/           # nRF52840 BLE/USB HID firmware
├── src/nchorder_tools/ # Python config tools
├── configs/            # Community chord layouts
├── docs/               # Hardware documentation
├── photos/             # Hardware photos
└── tests/              # Python test suite
```

## License

MIT - See [LICENSE.md](LICENSE.md) for details and third-party attribution.

## Trademark Notice

"Twiddler" is a trademark of Tek Gear Inc. This project is not affiliated with, endorsed by, or sponsored by Tek Gear Inc.
