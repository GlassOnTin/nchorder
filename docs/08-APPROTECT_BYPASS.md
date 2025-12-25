# APPROTECT Bypass via Voltage Glitching

[← Back to Index](README.md) | [Previous: Firmware Development](07-FIRMWARE_DEVELOPMENT.md)

This section documents attempts to bypass the nRF52840's APPROTECT mechanism using voltage glitching, enabling firmware extraction from the Twiddler 4.

## Background

### What is APPROTECT?

The nRF52840 includes a hardware feature called APPROTECT (Access Port Protection) that disables the SWD debug interface when enabled. This prevents reading or writing flash memory via debug tools.

- **APPROTECT disabled**: Full SWD access to flash, RAM, registers
- **APPROTECT enabled**: SWD blocked, only mass erase available

The Twiddler 4 ships with APPROTECT enabled, preventing firmware extraction.

### Bypass Method: Voltage Glitching

Voltage glitching exploits the fact that digital logic relies on voltage thresholds. By briefly disrupting the core voltage (DEC1 rail) at the precise moment the APPROTECT check occurs during boot, we can potentially corrupt the check and leave the debug port accessible.

**Target**: DEC1 pin (0.9V core regulator output)
**Method**: Crowbar (briefly short to GND via MOSFET)
**Timing**: 1-10ms after power-on, during boot ROM execution

## Hardware Setup

### Components

| Component | Purpose | Notes |
|-----------|---------|-------|
| MKS DLC32 V2.1 | Glitch controller | ESP32-WROOM-32U based CNC board |
| D4184 MOSFET module | Power control | Controls Twiddler VDD via GPIO 4 |
| Built-in spindle MOSFET | DEC1 crowbar | WSF40N10-L125F via GPIO 32 |
| Schottky diode (MBRS340) | Protection | Blocks reverse voltage to DEC1 |
| Twiddler 4 | Target | nRF52840 revision QIAA-D0 |

### nRF52840 Chip Revision

The Twiddler's E73 module contains an nRF52840 with markings:
```
N52840
Q1AAD0
2412BR
```

**QIAA-D0** = Revision D (pre-hardened APPROTECT, vulnerable to glitching)

Revision E and later have hardened APPROTECT that is significantly more difficult to bypass.

### MKS DLC32 Pin Mapping

The ESP32_nRF52_SWD firmware was adapted for the MKS DLC32 board:

| Signal | GPIO | MKS Header | Function |
|--------|------|------------|----------|
| GLITCHER | 32 | Spindle- | DEC1 crowbar MOSFET |
| NRF_POWER | 4 | I2C SCL | D4184 power control |
| swd_clock | 22 | Probe | SWCLK to target |
| swd_data | 0 | I2C SDA | SWDIO to target |
| OSCI_PIN | 34 | X limit | Oscilloscope trigger (optional) |
| LED | 2 | Internal | Status indicator |

### Wiring Diagram

```
MKS DLC32                          Twiddler 4 (J2 Header)
┌──────────┐                       ┌──────────┐
│ Spindle- ├──[Schottky]──────────►│ DEC1 pad │
│          │   (anode→DEC1)        │          │
│ I2C SCL  ├──────────┐            │          │
│          │          │            │          │
│ Probe    ├──────────┼───────────►│ SWCLK    │
│          │          │            │          │
│ I2C SDA  ├──────────┼───────────►│ SWDIO    │
│          │          │            │          │
│ GND      ├──────────┼─────┐      │ GND      │
└──────────┘          │     │      └──────────┘
                      │     │
              D4184 Module  │      Battery
              ┌───────┴───┐ │      ┌────┐
              │ TRIG  GND ├─┘  ┌───┤ +  │
              │           │    │   │    │
              │ VIN- VOUT-├────┼──►│ -  │──► Twiddler GND
              │           │    │   └────┘    (via MOSFET)
              │ VIN+ VOUT+├────┘
              └───────────┘
```

### Schottky Diode Orientation

Critical for protecting DEC1 from the floating spindle output:

```
DEC1 (0.9V) ────[A  K]──── Spindle- (floats to 12V when off)
              Schottky
           (stripe toward Spindle-)
```

- **MOSFET ON**: Forward biased, DEC1 drains to GND (glitch)
- **MOSFET OFF**: Reverse biased, 12V blocked from DEC1

## Firmware

### ESP32_nRF52_SWD

Using [atc1441's ESP32_nRF52_SWD](https://github.com/atc1441/ESP32_nRF52_SWD) firmware with MKS DLC32 adaptations.

**platformio.ini addition:**
```ini
[env:MKS_DLC32_Glitcher]
board = esp32dev
board_build.partitions = partition_noOTA.csv
build_flags =
    -D LED=2
    -D LED_STATE_ON=LOW
    -D GLITCHER=32
    -D OSCI_PIN=34
    -D NRF_POWER=4
    -D swd_clock_pin=22
    -D swd_data_pin=0
```

### Web Interface

The firmware provides a web UI at `http://<esp32-ip>/` with:
- Glitch delay/width configuration
- Enable/disable glitcher
- SWD init and register read
- Flash dump controls

Configure WiFi credentials in `src/web.cpp` before building.

## Observations

### SWD Communication Window

When the Twiddler powers on, there's a brief window where SWD is accessible before the firmware reconfigures the pins:

1. **Power on** → Boot ROM executes
2. **~1-5ms** → APPROTECT check occurs
3. **~5-10ms** → Application firmware loads
4. **~10ms+** → Firmware reconfigures P0.18/P0.20 as GPIO outputs

After firmware reconfiguration, SWDIO and SWDCLK are driven LOW, blocking all SWD communication. This is an additional anti-debug measure beyond APPROTECT.

### Logic Analyzer Captures

SWD traffic captured during glitch attempts shows:
- **Y:OK** responses indicating successful SWD transactions
- **ACK:0** when target not responding
- **CSW/TAR** register accesses working

This confirms SWD communication is possible during the boot window.

## Glitching Parameters

### Timing Window

Based on nRF52840 boot sequence analysis:

| Phase | Time (µs) | Notes |
|-------|-----------|-------|
| Power ramp | 0-500 | Voltage stabilizing |
| Boot ROM init | 500-1500 | Core initializing |
| APPROTECT check | 1500-5000 | **Target window** |
| App firmware | 5000-10000 | Firmware loading |
| GPIO reconfig | 10000+ | SWD pins driven low |

**Recommended sweep**: 1500-10000 µs

### Glitch Width

The firmware sweeps width values 0-30 at each delay step. Typical successful glitches occur with widths of 5-20.

### Success Criteria

The glitcher checks for success by reading FICR.INFO.VARIANT (0x10000100):
- **0x00052840** = nRF52840 identified, glitch successful
- **0x00000000** = APPROTECT still blocking reads

On success, the glitcher automatically stops and reports "We Have a good glitch".

## Current Status

**In Progress**: Glitcher running sweep of 1500-10000 µs delay range.

Hardware verified working:
- [x] Power control via D4184 MOSFET
- [x] SWD communication during boot window
- [x] Glitch pulses visible on oscilloscope
- [x] Schottky protection functioning
- [ ] Successful APPROTECT bypass (pending)

## References

- [ESP32_nRF52_SWD GitHub](https://github.com/atc1441/ESP32_nRF52_SWD)
- [nRF52840 APPROTECT bypass research](https://limitedresults.com/2020/06/nrf52-debug-resurrection-approtect-bypass/)
- [MKS DLC32 V2.1 schematic](https://github.com/makerbase-mks/MKS-DLC32)

## Photos

- `photos/e73-module-reference/15_e73_module_shield_crystal_antenna.jpg` - E73 module overview
- `photos/e73-module-reference/16_glitch_rig_mks_dlc32_twiddler_d4184.jpg` - Complete glitching setup
- `photos/e73-module-reference/17_oscilloscope_glitch_pulse_dec1.jpg` - Oscilloscope capture of glitch pulse
