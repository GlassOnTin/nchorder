"""Config format readers and writers for v4, v5, and v7."""

import struct
from pathlib import Path
from typing import BinaryIO, Union

from .config import TwiddlerConfig, ChordEntry


class ConfigV4:
    """Twiddler config format v4 (Twiddler 2.1, early T3)."""

    HEADER_SIZE = 14  # v4 has smaller header than v5

    @classmethod
    def read(cls, source: Union[str, Path, BinaryIO]) -> TwiddlerConfig:
        """Read v4 config file."""
        if isinstance(source, (str, Path)):
            with open(source, 'rb') as f:
                return cls._parse(f.read())
        return cls._parse(source.read())

    @classmethod
    def _parse(cls, data: bytes) -> TwiddlerConfig:
        config = TwiddlerConfig(version=4)

        # v4 header structure (14 bytes based on analysis)
        version = data[0]
        if version != 4:
            raise ValueError(f"Expected v4, got v{version}")

        options = data[1]
        chord_count = struct.unpack('<H', data[2:4])[0]

        # Parse remaining header fields
        config.sleep_timeout = struct.unpack('<H', data[4:6])[0]
        config.mouse_left_action = struct.unpack('<H', data[6:8])[0]
        config.mouse_middle_action = struct.unpack('<H', data[8:10])[0]
        config.mouse_right_action = struct.unpack('<H', data[10:12])[0]
        config.mouse_accel = data[12]
        config.key_repeat_delay = data[13]

        # Parse flags from options byte
        config.key_repeat = bool(options & 0x01)
        config.direct_key = bool(options & 0x02)
        config.joystick_left_click = bool(options & 0x04)
        config.disable_bluetooth = bool(options & 0x08)
        config.sticky_num = bool(options & 0x10)
        config.sticky_shift = bool(options & 0x80)

        # Parse chords (4 bytes each: 2-byte chord + 1-byte mod + 1-byte key)
        offset = cls.HEADER_SIZE
        for _ in range(chord_count):
            if offset + 4 > len(data):
                break
            chord_bits = struct.unpack('<H', data[offset:offset+2])[0]
            modifier = data[offset+2]
            hid_key = data[offset+3]
            offset += 4

            # Skip empty/invalid entries
            if modifier == 0xFF:
                continue

            is_shifted = bool(modifier & 0x02)  # Shift modifier bit
            config.chords.append(ChordEntry(
                chord=chord_bits,
                hid_key=hid_key,
                modifier=modifier,
                is_shifted=is_shifted
            ))

        return config


class ConfigV5:
    """Twiddler config format v5 (T3 firmware 12+)."""

    HEADER_SIZE = 18

    @classmethod
    def read(cls, source: Union[str, Path, BinaryIO]) -> TwiddlerConfig:
        """Read v5 config file."""
        if isinstance(source, (str, Path)):
            with open(source, 'rb') as f:
                return cls._parse(f.read())
        return cls._parse(source.read())

    @classmethod
    def _parse(cls, data: bytes) -> TwiddlerConfig:
        config = TwiddlerConfig(version=5)

        version = data[0]
        if version != 5:
            raise ValueError(f"Expected v5, got v{version}")

        options = data[1]
        chord_count = struct.unpack('<H', data[2:4])[0]
        config.sleep_timeout = struct.unpack('<H', data[4:6])[0]
        config.mouse_left_action = struct.unpack('<H', data[6:8])[0]
        config.mouse_middle_action = struct.unpack('<H', data[8:10])[0]
        config.mouse_right_action = struct.unpack('<H', data[10:12])[0]
        config.mouse_accel = data[12]
        config.key_repeat_delay = data[13]

        flags_b = data[14]
        flags_c = data[15]

        # Parse flags
        config.key_repeat = bool(options & 0x01)
        config.direct_key = bool(options & 0x02)
        config.joystick_left_click = bool(options & 0x04)
        config.disable_bluetooth = bool(options & 0x08)
        config.sticky_num = bool(options & 0x10)
        config.sticky_shift = bool(options & 0x80)
        config.haptic_feedback = bool(flags_c & 0x01)

        # Parse chords
        offset = cls.HEADER_SIZE
        multi_indices = []  # Track multi-char chord indices

        for _ in range(chord_count):
            if offset + 4 > len(data):
                break
            chord_bits = struct.unpack('<H', data[offset:offset+2])[0]
            modifier = data[offset+2]
            hid_key = data[offset+3]
            offset += 4

            is_multi = (modifier == 0xFF)
            is_shifted = bool(modifier & 0x02) if not is_multi else False

            entry = ChordEntry(
                chord=chord_bits,
                hid_key=hid_key,
                modifier=modifier,
                is_shifted=is_shifted,
                is_multi=is_multi
            )
            config.chords.append(entry)

            if is_multi:
                multi_indices.append((len(config.chords) - 1, hid_key))

        # Parse string table for multi-char chords if present
        if multi_indices:
            max_index = max(idx for _, idx in multi_indices)
            string_locs = []
            for _ in range(max_index + 1):
                if offset + 4 <= len(data):
                    loc = struct.unpack('<I', data[offset:offset+4])[0]
                    string_locs.append(loc)
                    offset += 4

            # Read string contents
            for chord_idx, str_idx in multi_indices:
                if str_idx < len(string_locs):
                    str_offset = string_locs[str_idx]
                    if str_offset < len(data):
                        str_len = struct.unpack('<H', data[str_offset:str_offset+2])[0]
                        chars = []
                        for i in range((str_len // 2) - 1):
                            char_off = str_offset + 2 + i * 2
                            if char_off + 2 <= len(data):
                                mod = data[char_off]
                                key = data[char_off + 1]
                                chars.append((mod, key))
                        config.chords[chord_idx].multi_chars = chars

        return config


class ConfigV7:
    """Twiddler config format v7 (T4 firmware 3.x)."""

    HEADER_SIZE = 128

    @classmethod
    def read(cls, source: Union[str, Path, BinaryIO]) -> TwiddlerConfig:
        """Read v7 config file."""
        if isinstance(source, (str, Path)):
            with open(source, 'rb') as f:
                return cls._parse(f.read())
        return cls._parse(source.read())

    @classmethod
    def _parse(cls, data: bytes) -> TwiddlerConfig:
        config = TwiddlerConfig(version=7)

        # Verify header - accept both 0x0107 (old Tuner) and 0x0907 (firmware 3.8+)
        version = struct.unpack('<H', data[4:6])[0]
        if version not in (0x0107, 0x0907):
            raise ValueError(f"Expected v7 (0x0107 or 0x0907), got 0x{version:04X}")

        chord_count = struct.unpack('<H', data[8:10])[0]
        config.sleep_timeout = struct.unpack('<H', data[10:12])[0]
        config.key_repeat_delay = struct.unpack('<H', data[12:14])[0]

        # Mouse actions (32-bit in v7)
        config.mouse_left_action = struct.unpack('<I', data[0x40:0x44])[0]
        config.mouse_middle_action = struct.unpack('<I', data[0x44:0x48])[0]
        config.mouse_right_action = struct.unpack('<I', data[0x48:0x4C])[0]

        # Parse chords (8 bytes each)
        offset = cls.HEADER_SIZE
        for _ in range(chord_count):
            if offset + 8 > len(data):
                break
            chord_bits = struct.unpack('<I', data[offset:offset+4])[0]
            modifier = struct.unpack('<H', data[offset+4:offset+6])[0]
            hid_key = struct.unpack('<H', data[offset+6:offset+8])[0]
            offset += 8

            is_multi = (modifier == 0x02FF)
            is_shifted = (modifier == 0x0220 or modifier == 0x2002)

            config.chords.append(ChordEntry(
                chord=chord_bits,
                hid_key=hid_key,
                modifier=modifier,
                is_shifted=is_shifted,
                is_multi=is_multi
            ))

        return config

    @classmethod
    def write(cls, config: TwiddlerConfig, dest: Union[str, Path, BinaryIO]) -> None:
        """Write config in v7 format."""
        data = cls._build(config)
        if isinstance(dest, (str, Path)):
            with open(dest, 'wb') as f:
                f.write(data)
        else:
            dest.write(data)

    @classmethod
    def _build(cls, config: TwiddlerConfig) -> bytes:
        """Build v7 binary from config."""
        # Header (128 bytes)
        header = bytearray(cls.HEADER_SIZE)

        # Reserved (0-3)
        # Version at 4-5 (0x0907 for firmware 3.8+, was 0x0107 in old Tuner)
        struct.pack_into('<H', header, 4, 0x0907)
        # Unknown at 6-7
        struct.pack_into('<H', header, 6, 0x0020)
        # Chord count at 8-9
        struct.pack_into('<H', header, 8, len(config.chords))
        # Sleep timeout at 10-11
        struct.pack_into('<H', header, 10, config.sleep_timeout)
        # Key repeat delay at 12-13
        struct.pack_into('<H', header, 12, config.key_repeat_delay)

        # Mouse actions at 0x40-0x4F
        struct.pack_into('<I', header, 0x40, config.mouse_left_action)
        struct.pack_into('<I', header, 0x44, config.mouse_middle_action)
        struct.pack_into('<I', header, 0x48, config.mouse_right_action)
        struct.pack_into('<I', header, 0x4C, 2)  # Unknown, default 2

        # Accel params at 0x50
        header[0x50] = config.mouse_accel
        header[0x51] = 0x0B
        header[0x52] = 0x09
        header[0x53] = 0x09

        # Chord index table at 0x60 (simplified - just sequential)
        for i in range(min(32, len(config.chords))):
            header[0x60 + i] = i if i < len(config.chords) else 0x80

        # Chord entries (8 bytes each)
        chords_data = bytearray()
        for chord in config.chords:
            entry = bytearray(8)
            struct.pack_into('<I', entry, 0, chord.chord)

            # Preserve original modifier if set, otherwise determine from flags
            if chord.modifier and chord.modifier not in (0, 0x0002, 0x0220, 0x02FF):
                # System/special chord - preserve original modifier
                mod = chord.modifier
            elif chord.is_multi:
                mod = 0x02FF
            elif chord.is_shifted:
                mod = 0x0220
            else:
                mod = 0x0002

            struct.pack_into('<H', entry, 4, mod)
            struct.pack_into('<H', entry, 6, chord.hid_key)
            chords_data.extend(entry)

        return bytes(header) + bytes(chords_data)


def detect_version(data: bytes) -> int:
    """Detect config format version from binary data."""
    if len(data) < 6:
        raise ValueError("File too small")

    # v7: starts with 4 null bytes, then version at offset 4
    if data[0:4] == b'\x00\x00\x00\x00':
        version = struct.unpack('<H', data[4:6])[0]
        if version in (0x0107, 0x0907):
            return 7

    # v4/v5: version byte at offset 0
    version = data[0]
    if version in (4, 5):
        return version

    raise ValueError(f"Unknown config format: first bytes = {data[:8].hex()}")


def read_config(source: Union[str, Path, BinaryIO]) -> TwiddlerConfig:
    """Auto-detect format and read config file."""
    if isinstance(source, (str, Path)):
        with open(source, 'rb') as f:
            data = f.read()
    else:
        data = source.read()

    version = detect_version(data)

    if version == 4:
        return ConfigV4._parse(data)
    elif version == 5:
        return ConfigV5._parse(data)
    elif version == 7:
        return ConfigV7._parse(data)
    else:
        raise ValueError(f"Unsupported version: {version}")
