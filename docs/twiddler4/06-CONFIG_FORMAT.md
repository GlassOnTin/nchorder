# Configuration File Format

[← Back to Index](README.md) | [Previous: I2C Analysis](04-I2C_ANALYSIS.md)

This section documents the binary `.cfg` file format used by the Twiddler 4, determined through config file examination and community documentation.

## Overview

Twiddler configuration files define:
- Chord-to-key mappings
- System function chords (config switching, toggles)
- Mouse mode chords
- Multi-character macros (optional)

Files are stored on the device's FAT12 filesystem as `0.CFG`, `1.CFG`, `2.CFG`, etc.

## File Structure

```
Offset  Size    Description
------  ----    -----------
0x00    128     Header (settings, index table)
0x80    8*N     Chord entries (N = chord count from header)
EOF     varies  String table (if multi-char macros present)
```

## Header Format (128 bytes)

```
Offset  Size  Type    Description
------  ----  ----    -----------
0x00    4     u32     Version (always 0 for v7 format)
0x04    4     u32     Flags/settings bitfield
0x08    2     u16     Chord count
0x0A    2     u16     String table offset (CRITICAL - see below)
0x0C    4     u32     Sleep timeout (ms)
0x10    44    -       Reserved/unknown
0x3C    4     u32     Additional settings
0x40    32    -       Reserved
0x60    32    u8[32]  Index table (for fast chord lookup)
```

### Flags Field (offset 0x04)

```
Bit     Description
---     -----------
0       Key repeat enabled (nav keys only: arrows, Home, End, PgUp, PgDn, Delete, Backspace)
1       Direct key mode
2       Joystick left-click enabled
3       Bluetooth disabled
4       Sticky Num Lock
5       Sticky Shift
6       Haptic feedback
7-31    Reserved/unknown
```

**Note on Key Repeat (bit 0)**: When enabled, the firmware sends repeated key events for navigation HID codes (0x4A-0x52: Home, PgUp, Delete, End, PgDn, Right, Left, Down, Up) and Backspace (0x2A). Letter keys (0x04-0x1D) do NOT repeat regardless of this setting.

### String Table Offset (offset 0x0A) - CRITICAL

The u16 at offset 0x0A specifies where the string table begins in the file. This is used for multi-character chords (macros that output words like "the", "and", "ing").

**CRITICAL**: This offset MUST point to valid string table data or the end of file. If it points into the chord data area, the firmware will interpret chord entries as string table entries and **crash when certain chords are pressed**.

```
File layout:
  0x00-0x7F:           Header (128 bytes)
  0x80 to string_off:  Chord entries (8 bytes each)
  string_off to EOF:   String table (if any)
```

**Safe values**:
- If config has no multi-char chords: set to `128 + (chord_count * 8)` (end of chords)
- If config has multi-char chords: set to actual string table offset

### Index Table (offset 0x60) - CRITICAL

32-byte lookup table for fast chord matching. Each entry (0-31) corresponds to the low 5 bits of a chord's bitmask (the prefix). The value at each entry is the index of the FIRST chord in the sorted chord array that has that prefix. Entries of 0x80 indicate "no chords with this prefix".

**CRITICAL**: The index table MUST be computed for YOUR specific chord set. Do NOT copy the index table from the default config.

**Algorithm to build index table**:
```python
index_table = [0x80] * 32  # Default: no chords
for prefix in range(32):
    for idx, chord in enumerate(sorted_chords):
        if (chord.bitmask & 0x1f) == prefix:
            index_table[prefix] = idx
            break  # First match only
```

**Requirements**:
1. Chords MUST be sorted by bitmask (ascending) for the index table to work
2. Index table MUST be recomputed whenever chords are added/removed/reordered

## Chord Entry Format (8 bytes)

```
Offset  Size  Type    Description
------  ----  ----    -----------
0x00    4     u32     Button bitmask + flags
0x04    2     u16     Modifier/type field
0x06    2     u16     HID keycode or function code
```

### Button Bitmask (bytes 0-3)

Low 16 bits encode button positions:

```
Bit   Button    Description
---   ------    -----------
0     T1 (N)    Thumb 1 - labeled "N" (Num)
1     F1L       Row 1 - Left (index finger)
2     F1M       Row 1 - Middle
3     F1R       Row 1 - Right
4     T2 (A)    Thumb 2 - labeled "A" (Alt)
5     F2L       Row 2 - Left (middle finger)
6     F2M       Row 2 - Middle
7     F2R       Row 2 - Right
8     T3 (E)    Thumb 3 - labeled "E" (Ctrl on left-hand)
9     F3L       Row 3 - Left (ring finger)
10    F3M       Row 3 - Middle
11    F3R       Row 3 - Right
12    T4 (SP)   Thumb 4 - labeled "SP" (Shift/Space)
13    F4L       Row 4 - Left (pinky)
14    F4M       Row 4 - Middle
15    F4R       Row 4 - Right
```

> **Note**: Thumb button labels (N, A, E, SP) vary by left/right hand model. The T1-T4 notation is model-independent.

High 16 bits are flags:

```
Bit   Flag              Description
---   ----              -----------
19    MOUSE_MODE_ONLY   Chord only active in mouse mode (0x00080000)
```

### Modifier/Type Field (bytes 4-5)

Stored as little-endian u16:
- Low byte (offset+4): Event type
- High byte (offset+5): Modifier flags

#### Event Types (low byte)

```
Value   Type              Description
-----   ----              -----------
0x01    Mouse             Mouse action
0x02    Keyboard          Standard keyboard event
0x07    System            System function (config switch, toggles)
0xFF    Multi-char        Multi-character string (references string table)
```

#### Keyboard Modifier Flags (high byte, when low byte = 0x02)

```
Bit   Flag    USB HID Modifier
---   ----    ----------------
0     Shift   Left Shift (0x02 in USB HID)
1     Ctrl    Left Ctrl (0x01 in USB HID)
2     Alt     Left Alt (0x04 in USB HID)
5     GUI     Left GUI/Windows (0x08 in USB HID)
```

Common modifier combinations:

```
Value   Meaning
-----   -------
0x0002  Normal keypress (no modifiers)
0x0102  Shift + key
0x0202  Ctrl + key
0x0402  Alt + key
0x2002  GUI + key (Windows key combinations)
0x0302  Ctrl + Shift + key
```

#### Mouse Function Codes (high byte, when low byte = 0x01)

```
Value   Function
-----   --------
0x01    Mouse mode toggle
0x02    Left click
0x04    Scroll mode toggle
0x05    Speed decrease
0x06    Speed cycle (toggle between speeds)
0x0A    Middle click
0x0B    Speed increase
0x0C    Right click
```

#### System Function Codes (high byte, when low byte = 0x07)

```
Value   Function                Typical Chord
-----   --------                -------------
0x00    Sleep/Wake              2L+3L
0x14    Load config slot 0      2L+3M
0x24    Load config slot 1      2L+4L
0x34    Load config slot 2      2M+4L
0x44    Load config slot 3      1L+3L+4L
0x58    Load config slot 4      1L+4M
0x6C    Toggle key repeat       2L+4M
0x78    Toggle direct key mode  2R+4M
0x8C    Toggle sticky Num       3M+4M
0xA0    Toggle sticky Shift     1M+3M+4M
```

### HID Keycodes (bytes 6-7)

Standard USB HID keycodes:

```
Range       Keys
-----       ----
0x04-0x1D   a-z
0x1E-0x27   1-0 (number row, NOT numpad)
0x28        Enter
0x29        Escape
0x2A        Backspace
0x2B        Tab
0x2C        Space
0x2D-0x38   Punctuation (-, =, [, ], \, ;, ', `, ,, ., /)
0x39        Caps Lock
0x3A-0x45   F1-F12
0x49        Insert
0x4A        Home
0x4B        Page Up
0x4C        Delete
0x4D        End
0x4E        Page Down
0x4F        Right Arrow
0x50        Left Arrow
0x51        Down Arrow
0x52        Up Arrow
0x53        Num Lock
0x54-0x63   Numpad keys (different from number row!)
```

## String Table Format

For multi-character chords, each string entry is a sequence of (modifier, HID code) pairs:

```
[mod1:u16][hid1:u16][mod2:u16][hid2:u16]...[0x0000][0x0000]
```

Example:
```
String "the " = (0x0002,0x17)(0x0002,0x0B)(0x0002,0x08)(0x0002,0x2C)(0x0000,0x0000)
               't'          'h'          'e'          ' '          (terminator)
```

## Default System Chords

These chords should be included in every config for proper device function:

| Chord        | Bitmask      | Modifier | Function              |
|--------------|--------------|----------|-----------------------|
| F2L+F3L      | 0x00000220   | 0x0007   | Sleep/Wake            |
| F2L+F3M      | 0x00000420   | 0x1407   | Config Slot 0         |
| F2L+F4L      | 0x00002020   | 0x2407   | Config Slot 1         |
| F2M+F4L      | 0x00002040   | 0x3407   | Config Slot 2         |
| F1L+F3L+F4L  | 0x00002202   | 0x4407   | Config Slot 3         |
| F1L+F4M      | 0x00004002   | 0x5807   | Config Slot 4         |
| F2L+F4M      | 0x00004020   | 0x6C07   | Toggle Key Repeat     |
| F2R+F4M      | 0x00004080   | 0x7807   | Toggle Direct Key     |
| F3M+F4M      | 0x00004400   | 0x8C07   | Toggle Sticky Num     |
| F1M+F3M+F4M  | 0x00004404   | 0xA007   | Toggle Sticky Shift   |

## Default Mouse Chords

| Chord        | Bitmask      | Modifier | Function              |
|--------------|--------------|----------|-----------------------|
| T1+T4+F4L    | 0x00003001   | 0x0601   | Speed Toggle          |
| F1L*         | 0x00080002   | 0x0201   | Left Click            |
| F1M*         | 0x00080004   | 0x0A01   | Middle Click          |
| F1R*         | 0x00080008   | 0x0C01   | Right Click           |
| T2+T3*       | 0x00080110   | 0x0101   | Mouse Mode Toggle     |
| F4L*         | 0x00082000   | 0x0501   | Speed Decrease        |
| F4M*         | 0x00084000   | 0x0401   | Scroll Toggle         |
| F4R*         | 0x00088000   | 0x0B01   | Speed Increase        |

\* = MOUSE_MODE_ONLY flag set (bit 19 in bitmask)

## Building a Valid Config

1. **Add layout chords**: Use modifier `0x0002` for normal keys, `0x0102` for shifted
2. **Add system chords**: Copy from default config to preserve device functions
3. **Add mouse chords**: Copy from default config for mouse mode functionality
4. **Sort all chords**: By bitmask (ascending) - REQUIRED for index table
5. **Update header values**:
   - Chord count at offset 0x08
   - String table offset at offset 0x0A (set to `128 + chord_count * 8` if no macros)
6. **Rebuild index table**: Compute fresh index table for YOUR chord set

## Common Pitfalls

1. **Wrong modifier byte**: Using `0x0220` instead of `0x0002` causes unexpected behavior

2. **Unsorted chords**: The index table assumes sorted chords; unsorted configs will fail to match most chords

3. **Missing system chords**: Device may not respond to config switching or power management

4. **Numpad vs number row**: HID codes 0x59-0x62 are NUMPAD keys, not number row. Use 0x1E-0x27 for numbers above QWERTY

5. **Invalid string table offset**: If pointing into chord data, firmware crashes when chords are pressed

6. **Duplicate chord bitmasks**: Two entries with same bitmask cause unpredictable behavior

7. **Wrong index table**: Copying from default config causes crashes - always recompute

8. **Shadowed system chords**: Layout chord using same buttons as system chord blocks system function

## Validation Checks

When creating or modifying configs, verify:
- File size matches header chord count
- String table offset points to valid location
- Index table correctly computed for your chord set
- Chords are sorted by bitmask
- No duplicate bitmasks

---

[← Back to Index](README.md)
