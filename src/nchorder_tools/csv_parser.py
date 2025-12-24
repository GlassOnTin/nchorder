"""Parse Twiddler CSV exports from the Tuner."""

import csv
import re
from pathlib import Path
from typing import Union, TextIO

from .config import TwiddlerConfig, ChordEntry
from .hid import buttons_to_chord, char_to_hid, BUTTON_BITS, HID_MAP

# Thumb button mapping from CSV notation
THUMB_MAP = {
    '1': 'N',   # Thumb 1 = Num
    '2': 'A',   # Thumb 2 = Alt
    '3': 'C',   # Thumb 3 = Ctrl
    '4': 'S',   # Thumb 4 = Shift
}

# Special key name to HID code mapping
SPECIAL_KEYS = {
    '<Enter>': 0x28, '<Return>': 0x28,
    '<Escape>': 0x29, '<Esc>': 0x29,
    '<Backspace>': 0x2A, '<BS>': 0x2A,
    '<Tab>': 0x2B,
    '<Space>': 0x2C, ' ': 0x2C,
    '<CapsLock>': 0x39,
    '<F1>': 0x3A, '<F2>': 0x3B, '<F3>': 0x3C, '<F4>': 0x3D,
    '<F5>': 0x3E, '<F6>': 0x3F, '<F7>': 0x40, '<F8>': 0x41,
    '<F9>': 0x42, '<F10>': 0x43, '<F11>': 0x44, '<F12>': 0x45,
    '<PrintScreen>': 0x46, '<ScrollLock>': 0x47, '<Pause>': 0x48,
    '<Insert>': 0x49, '<Home>': 0x4A, '<PageUp>': 0x4B,
    '<Delete>': 0x4C, '<End>': 0x4D, '<PageDown>': 0x4E,
    '<RightArrow>': 0x4F, '<Right>': 0x4F,
    '<LeftArrow>': 0x50, '<Left>': 0x50,
    '<DownArrow>': 0x51, '<Down>': 0x51,
    '<UpArrow>': 0x52, '<Up>': 0x52,
    '<NumLock>': 0x53,
}


def parse_thumbs(thumb_str: str) -> list[str]:
    """Parse thumb button string like '1', '12', '123' to button names."""
    buttons = []
    for char in thumb_str:
        if char in THUMB_MAP:
            buttons.append(THUMB_MAP[char])
    return buttons


def parse_fingers(finger_str: str) -> list[str]:
    """Parse finger button string like '1R 2M 3L' to button names."""
    buttons = []
    parts = finger_str.strip().split()
    for part in parts:
        if len(part) >= 2:
            row = part[0]
            col = part[1]
            if row in '1234' and col in 'LMR':
                buttons.append(f"{row}{col}")
    return buttons


def parse_action(action_str: str) -> tuple[int, bool, bool]:
    """
    Parse action string like '[KB]a' or '[KB]<RShift>=</RShift>'.
    Returns (hid_code, is_shifted, is_multi).
    """
    if not action_str.startswith('[KB]'):
        return (0, False, False)  # Non-keyboard action

    content = action_str[4:]  # Remove [KB] prefix

    if not content:
        return (0, False, False)

    # Check for shifted keys: <RShift>X</RShift>
    shift_match = re.match(r'<[RL]Shift>(.+)</[RL]Shift>', content)
    is_shifted = shift_match is not None

    if shift_match:
        content = shift_match.group(1)

    # Check for special keys
    if content in SPECIAL_KEYS:
        return (SPECIAL_KEYS[content], is_shifted, False)

    # Check for complex modifier combos (multi-char or special)
    if '<' in content and '>' in content:
        # This is a complex chord, might be multi-char
        # For now, try to extract the base key
        # e.g., <LCtrl><LAlt>...<RShift>a</RShift>...
        # Just find the actual character
        clean = re.sub(r'<[^>]+>', '', content)
        if len(clean) == 1:
            result = char_to_hid(clean)
            if result:
                return (result[0], result[1] or is_shifted, False)
        return (0, False, True)  # Mark as multi for complex

    # Single character
    if len(content) == 1:
        result = char_to_hid(content)
        if result:
            return (result[0], result[1] or is_shifted, False)

    return (0, False, False)


def read_csv(source: Union[str, Path, TextIO]) -> TwiddlerConfig:
    """Read Twiddler CSV export and convert to config."""
    config = TwiddlerConfig(version=7)

    if isinstance(source, (str, Path)):
        with open(source, 'r', encoding='utf-8-sig') as f:
            return _parse_csv(f, config)
    return _parse_csv(source, config)


def _parse_csv(f: TextIO, config: TwiddlerConfig) -> TwiddlerConfig:
    """Parse CSV file handle."""
    reader = csv.DictReader(f)

    for row in reader:
        thumbs = row.get('Thumbs', '')
        fingers = row.get('Fingers', '')
        action = row.get('Actions', '')

        # Parse button combination
        buttons = parse_thumbs(thumbs) + parse_fingers(fingers)
        if not buttons:
            continue

        chord_bits = buttons_to_chord(buttons)

        # Parse action
        hid_code, is_shifted, is_multi = parse_action(action)

        if hid_code == 0 and not is_multi:
            continue  # Skip unmapped or invalid

        # Determine modifier
        if is_multi:
            modifier = 0x02FF
        elif is_shifted:
            modifier = 0x0220
        else:
            modifier = 0x0002

        config.chords.append(ChordEntry(
            chord=chord_bits,
            hid_key=hid_code,
            modifier=modifier,
            is_shifted=is_shifted,
            is_multi=is_multi
        ))

    return config
