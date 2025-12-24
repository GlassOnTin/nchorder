"""USB HID keycode mappings."""

from typing import Optional, Tuple

# HID Usage Table for Keyboard/Keypad Page (0x07)
# Format: hid_code: (unshifted, shifted)
HID_MAP = {
    # Letters a-z
    0x04: ('a', 'A'), 0x05: ('b', 'B'), 0x06: ('c', 'C'), 0x07: ('d', 'D'),
    0x08: ('e', 'E'), 0x09: ('f', 'F'), 0x0A: ('g', 'G'), 0x0B: ('h', 'H'),
    0x0C: ('i', 'I'), 0x0D: ('j', 'J'), 0x0E: ('k', 'K'), 0x0F: ('l', 'L'),
    0x10: ('m', 'M'), 0x11: ('n', 'N'), 0x12: ('o', 'O'), 0x13: ('p', 'P'),
    0x14: ('q', 'Q'), 0x15: ('r', 'R'), 0x16: ('s', 'S'), 0x17: ('t', 'T'),
    0x18: ('u', 'U'), 0x19: ('v', 'V'), 0x1A: ('w', 'W'), 0x1B: ('x', 'X'),
    0x1C: ('y', 'Y'), 0x1D: ('z', 'Z'),

    # Numbers 1-0
    0x1E: ('1', '!'), 0x1F: ('2', '@'), 0x20: ('3', '#'), 0x21: ('4', '$'),
    0x22: ('5', '%'), 0x23: ('6', '^'), 0x24: ('7', '&'), 0x25: ('8', '*'),
    0x26: ('9', '('), 0x27: ('0', ')'),

    # Special keys
    0x28: ('<Return>', '<Return>'),
    0x29: ('<Escape>', '<Escape>'),
    0x2A: ('<Backspace>', '<Backspace>'),
    0x2B: ('<Tab>', '<Tab>'),
    0x2C: ('<Space>', '<Space>'),

    # Punctuation
    0x2D: ('-', '_'), 0x2E: ('=', '+'), 0x2F: ('[', '{'), 0x30: (']', '}'),
    0x31: ('\\', '|'), 0x32: ('#', '~'),  # Non-US
    0x33: (';', ':'), 0x34: ("'", '"'), 0x35: ('`', '~'),
    0x36: (',', '<'), 0x37: ('.', '>'), 0x38: ('/', '?'),

    # Caps Lock
    0x39: ('<CapsLock>', '<CapsLock>'),

    # Function keys F1-F12
    0x3A: ('<F1>', '<F1>'), 0x3B: ('<F2>', '<F2>'), 0x3C: ('<F3>', '<F3>'),
    0x3D: ('<F4>', '<F4>'), 0x3E: ('<F5>', '<F5>'), 0x3F: ('<F6>', '<F6>'),
    0x40: ('<F7>', '<F7>'), 0x41: ('<F8>', '<F8>'), 0x42: ('<F9>', '<F9>'),
    0x43: ('<F10>', '<F10>'), 0x44: ('<F11>', '<F11>'), 0x45: ('<F12>', '<F12>'),

    # Navigation
    0x46: ('<PrintScreen>', '<PrintScreen>'),
    0x47: ('<ScrollLock>', '<ScrollLock>'),
    0x48: ('<Pause>', '<Pause>'),
    0x49: ('<Insert>', '<Insert>'),
    0x4A: ('<Home>', '<Home>'),
    0x4B: ('<PageUp>', '<PageUp>'),
    0x4C: ('<Delete>', '<Delete>'),
    0x4D: ('<End>', '<End>'),
    0x4E: ('<PageDown>', '<PageDown>'),
    0x4F: ('<Right>', '<Right>'),
    0x50: ('<Left>', '<Left>'),
    0x51: ('<Down>', '<Down>'),
    0x52: ('<Up>', '<Up>'),
    0x53: ('<NumLock>', '<NumLock>'),

    # Numpad
    0x54: ('/', '/'),   # Numpad /
    0x55: ('*', '*'),   # Numpad *
    0x56: ('-', '-'),   # Numpad -
    0x57: ('+', '+'),   # Numpad +
    0x58: ('<Enter>', '<Enter>'),  # Numpad Enter
    0x59: ('1', '1'), 0x5A: ('2', '2'), 0x5B: ('3', '3'),
    0x5C: ('4', '4'), 0x5D: ('5', '5'), 0x5E: ('6', '6'),
    0x5F: ('7', '7'), 0x60: ('8', '8'), 0x61: ('9', '9'),
    0x62: ('0', '0'), 0x63: ('.', '.'),
}

# Modifier bit flags (in v7 modifier field)
MOD_NONE = 0x0002
MOD_SHIFT = 0x0220  # Actually 0x2002 in little-endian storage
MOD_MULTI = 0x02FF  # Multi-character chord


def hid_to_char(hid_code: int, shifted: bool = False) -> str:
    """Convert HID keycode to character string."""
    if hid_code in HID_MAP:
        return HID_MAP[hid_code][1 if shifted else 0]
    return f'<0x{hid_code:02X}>'


def char_to_hid(char: str) -> Optional[Tuple[int, bool]]:
    """Convert character to (HID code, shifted) tuple."""
    for hid_code, (unshifted, shifted) in HID_MAP.items():
        if char == unshifted:
            return (hid_code, False)
        if char == shifted:
            return (hid_code, True)
    return None


# Button bit positions in chord representation
# Twiddler layout: 4 thumb buttons + 4 rows of 3 finger buttons
BUTTON_BITS = {
    # Thumb buttons (bits 0, 4, 8, 12)
    'N': 0,   # Num
    'A': 4,   # Alt
    'C': 8,   # Ctrl
    'S': 12,  # Shift

    # Row 1 (bits 1, 2, 3)
    '1L': 1, '1M': 2, '1R': 3,

    # Row 2 (bits 5, 6, 7)
    '2L': 5, '2M': 6, '2R': 7,

    # Row 3 (bits 9, 10, 11)
    '3L': 9, '3M': 10, '3R': 11,

    # Row 4 (bits 13, 14, 15)
    '4L': 13, '4M': 14, '4R': 15,
}


def chord_to_buttons(chord: int) -> list[str]:
    """Convert chord bitmask to list of button names."""
    buttons = []
    for name, bit in BUTTON_BITS.items():
        if chord & (1 << bit):
            buttons.append(name)
    return buttons


def buttons_to_chord(buttons: list[str]) -> int:
    """Convert list of button names to chord bitmask."""
    chord = 0
    for name in buttons:
        if name in BUTTON_BITS:
            chord |= (1 << BUTTON_BITS[name])
    return chord


def chord_to_tutor_notation(chord: int, include_thumbs: bool = True) -> str:
    """Convert chord bitmask to Tutor 4-char notation (e.g., 'RROL').

    Format: 4 characters for rows 1-4, each is L/M/R/O.
    Thumb buttons are prepended as prefix if include_thumbs=True.

    Args:
        chord: The chord bitmask
        include_thumbs: If True, prefix thumb buttons (e.g., 'N LOOO').
                       If False, just return 4-char finger notation.
    """
    # Build finger row notation
    rows = []
    for row in range(1, 5):
        if chord & (1 << BUTTON_BITS[f'{row}L']):
            rows.append('L')
        elif chord & (1 << BUTTON_BITS[f'{row}M']):
            rows.append('M')
        elif chord & (1 << BUTTON_BITS[f'{row}R']):
            rows.append('R')
        else:
            rows.append('O')

    finger_str = ''.join(rows)

    if not include_thumbs:
        return finger_str

    # Build thumb prefix
    thumb = ''
    if chord & (1 << BUTTON_BITS['N']):
        thumb += 'N'
    if chord & (1 << BUTTON_BITS['A']):
        thumb += 'A'
    if chord & (1 << BUTTON_BITS['C']):
        thumb += 'C'
    if chord & (1 << BUTTON_BITS['S']):
        thumb += 'S'

    if thumb:
        return thumb + ' ' + finger_str
    return finger_str


def generate_common_chords() -> list[int]:
    """Generate list of commonly-used chord bitmasks.

    Returns chords in approximate order of ergonomic preference:
    1. Single finger buttons (12 chords)
    2. Two-finger adjacent rows (27 chords)
    3. Single finger + one thumb (48 chords)
    4. Two-finger + thumb combinations (108 chords)
    Total: 195 chords
    """
    chords = []

    # Single finger (rows 1-4, positions L/M/R) - 12 chords
    for row in range(1, 5):
        for col in 'LMR':
            chords.append(1 << BUTTON_BITS[f'{row}{col}'])

    # Two-finger, adjacent rows - 36 chords
    for row1 in range(1, 4):
        row2 = row1 + 1
        for col1 in 'LMR':
            for col2 in 'LMR':
                bits = (1 << BUTTON_BITS[f'{row1}{col1}']) | (1 << BUTTON_BITS[f'{row2}{col2}'])
                chords.append(bits)

    # Single finger + thumb modifiers - 48 chords
    for thumb in 'NACS':
        for row in range(1, 5):
            for col in 'LMR':
                bits = (1 << BUTTON_BITS[thumb]) | (1 << BUTTON_BITS[f'{row}{col}'])
                chords.append(bits)

    # Two-finger + single thumb - 144 chords (most common patterns)
    for thumb in 'NACS':
        for row1 in range(1, 4):
            row2 = row1 + 1
            for col1 in 'LMR':
                for col2 in 'LMR':
                    bits = (1 << BUTTON_BITS[thumb]) | \
                           (1 << BUTTON_BITS[f'{row1}{col1}']) | \
                           (1 << BUTTON_BITS[f'{row2}{col2}'])
                    chords.append(bits)

    return chords


def hid_to_tutor_key(hid_code: int, modifier: int) -> Optional[str]:
    """Convert HID keycode and modifier to Tutor key string.

    Returns None for system/mouse chords that shouldn't be exported.
    """
    # Skip system chords (modifier low byte 0x01 or 0x07)
    if (modifier & 0xFF) in (0x01, 0x07):
        return None

    # Multi-character chords
    if modifier == 0xFF02 or modifier == 0x02FF:
        return None  # Can't represent multi-char in simple JSON

    shifted = (modifier == 0x2002 or modifier == 0x0220)

    if hid_code in HID_MAP:
        key = HID_MAP[hid_code][1 if shifted else 0]
        # Clean up special key names for Tutor format
        if key.startswith('<') and key.endswith('>'):
            inner = key[1:-1].lower()
            # Map to Tutor-expected names
            tutor_names = {
                'space': ' ',  # Literal space character
                'return': 'enter',
                'backspace': 'backspace',
                'tab': 'tab',
                'escape': 'esc',
            }
            key = tutor_names.get(inner, inner)
        return key

    return None
