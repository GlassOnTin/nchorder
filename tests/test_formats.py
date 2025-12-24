"""Tests for config format readers and writers."""

import pytest
import struct
from pathlib import Path
from io import BytesIO

from nchorder_tools.formats import (
    detect_version,
    read_config,
    ConfigV7,
    ConfigV4,
    ConfigV5,
)
from nchorder_tools.config import TwiddlerConfig, ChordEntry


class TestDetectVersion:
    """Tests for version detection."""

    def test_detect_v7_new_tuner(self):
        """Detect v7 format with 0x0907 version."""
        data = b'\x00\x00\x00\x00\x07\x09' + b'\x00' * 122
        assert detect_version(data) == 7

    def test_detect_v7_old_tuner(self):
        """Detect v7 format with 0x0107 version."""
        data = b'\x00\x00\x00\x00\x07\x01' + b'\x00' * 122
        assert detect_version(data) == 7

    def test_detect_v5(self):
        """Detect v5 format."""
        data = b'\x05' + b'\x00' * 17
        assert detect_version(data) == 5

    def test_detect_v4(self):
        """Detect v4 format."""
        data = b'\x04' + b'\x00' * 13
        assert detect_version(data) == 4

    def test_detect_unknown(self):
        """Unknown format raises ValueError."""
        data = b'\x08' + b'\x00' * 20
        with pytest.raises(ValueError, match="Unknown config format"):
            detect_version(data)

    def test_detect_too_small(self):
        """File too small raises ValueError."""
        data = b'\x00\x00'
        with pytest.raises(ValueError, match="File too small"):
            detect_version(data)


class TestReadConfig:
    """Tests for auto-detecting reader."""

    def test_read_v7_file(self, sample_v7_config):
        """Read v7 config file successfully."""
        config = read_config(sample_v7_config)
        assert config.version == 7
        assert len(config.chords) > 0

    def test_read_from_path(self, sample_v7_config):
        """Read using Path object."""
        config = read_config(sample_v7_config)
        assert config.version == 7

    def test_read_from_string(self, sample_v7_config):
        """Read using string path."""
        config = read_config(str(sample_v7_config))
        assert config.version == 7

    def test_read_from_file_object(self, sample_v7_config):
        """Read using file object."""
        with open(sample_v7_config, 'rb') as f:
            config = read_config(f)
        assert config.version == 7


class TestConfigV7:
    """Tests for v7 format reader and writer."""

    def test_read_header_settings(self, sample_v7_config):
        """Read header settings from v7 file."""
        config = ConfigV7.read(sample_v7_config)
        # MirrorWalk defaults
        assert config.sleep_timeout == 3720
        assert config.key_repeat_delay == 100

    def test_read_chord_count(self, sample_v7_config):
        """Read chord count from v7 file."""
        config = ConfigV7.read(sample_v7_config)
        # MirrorWalk has many chords
        assert len(config.chords) > 50

    def test_read_chord_structure(self, sample_v7_config):
        """Read chord structure correctly."""
        config = ConfigV7.read(sample_v7_config)
        # Find a chord and verify structure
        for chord in config.chords:
            assert isinstance(chord.chord, int)
            assert isinstance(chord.hid_key, int)
            assert isinstance(chord.modifier, int)
            assert isinstance(chord.is_shifted, bool)

    def test_write_creates_valid_header(self, tmp_path):
        """Write creates valid v7 header."""
        config = TwiddlerConfig()
        config.add_chord(0x0002, 0x04)  # 1L -> a

        out_path = tmp_path / "test.cfg"
        ConfigV7.write(config, out_path)

        with open(out_path, 'rb') as f:
            data = f.read()

        # Check header structure
        assert len(data) >= 128 + 8  # Header + 1 chord
        assert data[0:4] == b'\x00\x00\x00\x00'  # Reserved
        version = struct.unpack('<H', data[4:6])[0]
        assert version == 0x0907

    def test_write_chord_data(self, tmp_path):
        """Write chord data correctly."""
        config = TwiddlerConfig()
        config.add_chord(0x0002, 0x04, shifted=False)  # 1L -> a
        config.add_chord(0x0004, 0x05, shifted=True)   # 1M -> B

        out_path = tmp_path / "test.cfg"
        ConfigV7.write(config, out_path)

        with open(out_path, 'rb') as f:
            data = f.read()

        # Read back chord count
        chord_count = struct.unpack('<H', data[8:10])[0]
        assert chord_count == 2

        # First chord at offset 0x80
        chord1_bits = struct.unpack('<I', data[0x80:0x84])[0]
        chord1_mod = struct.unpack('<H', data[0x84:0x86])[0]
        chord1_key = struct.unpack('<H', data[0x86:0x88])[0]

        assert chord1_bits == 0x0002
        assert chord1_mod == 0x0002  # Unshifted
        assert chord1_key == 0x04

        # Second chord at offset 0x88
        chord2_bits = struct.unpack('<I', data[0x88:0x8C])[0]
        chord2_mod = struct.unpack('<H', data[0x8C:0x8E])[0]
        chord2_key = struct.unpack('<H', data[0x8E:0x90])[0]

        assert chord2_bits == 0x0004
        assert chord2_mod == 0x0220  # Shifted
        assert chord2_key == 0x05

    def test_roundtrip_simple(self, tmp_path):
        """Roundtrip simple config through write/read."""
        original = TwiddlerConfig()
        original.sleep_timeout = 5000
        original.key_repeat_delay = 200
        original.add_chord(0x0002, 0x04)  # 1L -> a
        original.add_chord(0x0006, 0x08)  # 1L+1M -> e

        out_path = tmp_path / "roundtrip.cfg"
        ConfigV7.write(original, out_path)

        loaded = ConfigV7.read(out_path)

        assert loaded.version == 7
        assert loaded.sleep_timeout == 5000
        assert loaded.key_repeat_delay == 200
        assert len(loaded.chords) == 2

    def test_roundtrip_preserves_chords(self, tmp_path):
        """Roundtrip preserves chord data."""
        original = TwiddlerConfig()
        original.add_chord(0x0002, 0x04, shifted=False)
        original.add_chord(0x0004, 0x05, shifted=True)

        out_path = tmp_path / "roundtrip.cfg"
        ConfigV7.write(original, out_path)
        loaded = ConfigV7.read(out_path)

        assert len(loaded.chords) == len(original.chords)
        for orig, load in zip(original.chords, loaded.chords):
            assert orig.chord == load.chord
            assert orig.hid_key == load.hid_key
            assert orig.is_shifted == load.is_shifted

    def test_roundtrip_real_config(self, sample_v7_config, tmp_path):
        """Roundtrip real MirrorWalk config."""
        original = ConfigV7.read(sample_v7_config)
        orig_chord_count = len(original.chords)

        out_path = tmp_path / "mirror_roundtrip.cfg"
        ConfigV7.write(original, out_path)
        loaded = ConfigV7.read(out_path)

        # Chord count preserved
        assert len(loaded.chords) == orig_chord_count

        # Settings preserved
        assert loaded.sleep_timeout == original.sleep_timeout
        assert loaded.key_repeat_delay == original.key_repeat_delay

    def test_write_to_file_object(self, tmp_path):
        """Write to file-like object."""
        config = TwiddlerConfig()
        config.add_chord(0x0002, 0x04)

        out_path = tmp_path / "test.cfg"
        with open(out_path, 'wb') as f:
            ConfigV7.write(config, f)

        # Verify file was written
        assert out_path.exists()
        loaded = ConfigV7.read(out_path)
        assert len(loaded.chords) == 1

    def test_preserves_system_chords(self, tmp_path):
        """System chord modifiers are preserved."""
        config = TwiddlerConfig()
        # System chord with modifier 0x0701
        system_chord = ChordEntry(
            chord=0x1002,
            hid_key=0x00,
            modifier=0x0701,
            is_shifted=False
        )
        config.chords.append(system_chord)

        out_path = tmp_path / "system.cfg"
        ConfigV7.write(config, out_path)
        loaded = ConfigV7.read(out_path)

        assert len(loaded.chords) == 1
        assert loaded.chords[0].modifier == 0x0701


class TestConfigV4:
    """Basic tests for v4 format (limited without sample files)."""

    def test_parse_minimal_v4(self):
        """Parse minimal v4 data."""
        # Build minimal v4 header (14 bytes)
        data = bytearray(14 + 4)  # Header + 1 chord
        data[0] = 4  # Version
        data[1] = 0  # Options
        struct.pack_into('<H', data, 2, 1)  # 1 chord
        struct.pack_into('<H', data, 4, 3720)  # Sleep timeout

        # Chord entry (4 bytes)
        struct.pack_into('<H', data, 14, 0x0002)  # Chord bits
        data[16] = 0x00  # Modifier
        data[17] = 0x04  # HID key (a)

        config = ConfigV4._parse(bytes(data))
        assert config.version == 4
        assert len(config.chords) == 1


class TestConfigV5:
    """Basic tests for v5 format (limited without sample files)."""

    def test_parse_minimal_v5(self):
        """Parse minimal v5 data."""
        # Build minimal v5 header (18 bytes)
        data = bytearray(18 + 4)  # Header + 1 chord
        data[0] = 5  # Version
        data[1] = 0  # Options
        struct.pack_into('<H', data, 2, 1)  # 1 chord
        struct.pack_into('<H', data, 4, 3720)  # Sleep timeout

        # Chord entry (4 bytes)
        struct.pack_into('<H', data, 18, 0x0002)  # Chord bits
        data[20] = 0x00  # Modifier
        data[21] = 0x04  # HID key (a)

        config = ConfigV5._parse(bytes(data))
        assert config.version == 5
        assert len(config.chords) == 1
