# Twiddler 4 Hardware & Memory Map

## Hardware

### Radio Module: EByte E73-2G4M08S1C

The Twiddler 4 uses the EByte E73-2G4M08S1C BLE module.

| Specification | Value |
|---------------|-------|
| MCU | Nordic nRF52840 (ARM Cortex-M4F @ 64MHz) |
| Flash | 1 MB |
| RAM | 256 KB |
| BLE | 4.2 / 5.0 |
| TX Power | 8 dBm |
| Frequency | 2.360 - 2.500 GHz |
| Module Size | 18mm × 13mm |
| Antenna | Ceramic (built-in) |
| Supply Voltage | 1.7 - 5.5V |
| Temperature | -40°C to +85°C |

The E73-2G4M08S1C is a cost-effective (~$3-5) SMD module commonly used in
commercial BLE products. It exposes most nRF52840 GPIO pins for custom designs.

### References
- [EByte Product Page](https://www.cdebyte.com/products/E73-2G4M08S1C)
- [EByte User Manual](https://manuals.plus/ebyte/e73-2g4m08s1c-2-4ghz-smd-wireless-module-manual)

---

## Flash Memory Layout (1MB total)

```
┌────────────────────────────────────────────────────────────┐
│  Address Range      │  Size   │  Component                │
├────────────────────────────────────────────────────────────┤
│  0x00000 - 0x00FFF  │   4 KB  │  MBR (Master Boot Record) │
├────────────────────────────────────────────────────────────┤
│  0x01000 - 0x26FFF  │ 152 KB  │  SoftDevice S140 v7.x     │
│                     │         │  (BLE stack)              │
├────────────────────────────────────────────────────────────┤
│  0x27000 - 0xEFFFF  │ 804 KB  │  APPLICATION              │
│                     │         │  (Custom Firmware)        │
├────────────────────────────────────────────────────────────┤
│  0xF0000 - 0xFFFFF  │  64 KB  │  BOOTLOADER               │
│                     │         │  (DFU capable)            │
└────────────────────────────────────────────────────────────┘
```

## Key Addresses

| Component | Start | End | Size |
|-----------|-------|-----|------|
| MBR | 0x00000 | 0x00FFF | 4 KB |
| SoftDevice S140 | 0x01000 | 0x26FFF | 152 KB |
| Application | 0x27000 | 0xEFFFF | 804 KB |
| Bootloader | 0xF0000 | 0xFFFFF | 64 KB |
| UICR (config) | 0x10001000 | 0x100010FF | 256 B |

## RAM Memory Layout (256KB total)

```
┌────────────────────────────────────────────────────────────┐
│  Address Range          │  Size   │  Component            │
├────────────────────────────────────────────────────────────┤
│  0x20000000 - 0x2000225F │  8.6 KB │  SoftDevice RAM       │
├────────────────────────────────────────────────────────────┤
│  0x20002260 - 0x2003FFFF │ 247 KB  │  Application RAM      │
└────────────────────────────────────────────────────────────┘
```

## Linker Script Settings

Our `ble_app_hids_keyboard_gcc_nrf52.ld` uses:

```
FLASH (rx) : ORIGIN = 0x27000, LENGTH = 0xc9000
RAM (rwx)  : ORIGIN = 0x20002260, LENGTH = 0x3dda0
```

Calculation:
- Flash: 0x27000 + 0xc9000 = 0xF0000 (ends at bootloader boundary)
- RAM: 0x20002260 + 0x3dda0 = 0x20040000 (end of RAM)

## Original Twiddler Firmware

- File: `firmware/TWIDDLER.3.08.0.bin`
- Size: 102,724 bytes (0x19144)
- If loaded at 0x27000, ends at 0x40144
- Leaves ample room for bootloader at 0xF0000

## Safety Considerations

### make flash (SAFE)
Uses `--sectorerase` to only erase sectors being programmed.
Bootloader at 0xF0000-0xFFFFF is preserved.

### make erase (DESTRUCTIVE)
Uses `--eraseall` which wipes entire flash including bootloader.
Only use on development kits, NEVER on actual Twiddler!

## DFU Update Process

The Twiddler 4 supports firmware updates via:

1. **USB Mass Storage** (SD card mode)
   - Device appears as USB drive
   - Copy firmware file to device
   - Device auto-updates on disconnect

2. **BLE DFU** (if bootloader supports)
   - Use nRF Connect app
   - Enter DFU mode on device
   - Transfer firmware package

## Bootloader Variants (Nordic SDK)

| Type | Address | Size | Use Case |
|------|---------|------|----------|
| Secure BLE | 0xF8000 | 24 KB | Production BLE DFU |
| Secure BLE Debug | 0xF1000 | 52 KB | Development with debug |
| USB | 0xE0000 | 122 KB | USB-only DFU |
| Open | 0xE0000 | 122 KB | Development without security |

Our linker script uses 0xF0000 as the boundary, which is compatible with all
bootloader variants except the larger debug/USB bootloaders.

## References

- Nordic nRF52840 Product Specification
- nRF5 SDK 17.1.0 Documentation
- S140 SoftDevice Specification v7.x
