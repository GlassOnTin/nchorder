"""Tests for TwiddlerConfig and ChordEntry."""

import pytest
from nchorder_tools.config import TwiddlerConfig, ChordEntry


class TestChordEntry:
    """Tests for ChordEntry dataclass."""

    def test_repr_simple(self):
        """Test repr for simple chord."""
        entry = ChordEntry(chord=0x0002, hid_key=0x04, modifier=0x0002)
        r = repr(entry)
        assert "1L" in r
        assert "a" in r

    def test_repr_multi(self):
        """Test repr for multi-char chord."""
        entry = ChordEntry(
            chord=0x0002,
            hid_key=0x04,
            modifier=0x02FF,
            is_multi=True,
            multi_chars=[(0x0002, 0x04), (0x0002, 0x05)]
        )
        r = repr(entry)
        assert "multi" in r
        assert "2 chars" in r


class TestTwiddlerConfigConflicts:
    """Tests for find_conflicts() method."""

    def test_find_conflicts_empty(self):
        """Empty config has no conflicts."""
        config = TwiddlerConfig()
        assert config.find_conflicts() == []

    def test_find_conflicts_none(self):
        """Config with unique chords has no conflicts."""
        config = TwiddlerConfig()
        config.chords = [
            ChordEntry(chord=0x0002, hid_key=0x04, modifier=0x0002),  # 1L -> a
            ChordEntry(chord=0x0004, hid_key=0x05, modifier=0x0002),  # 1M -> b
            ChordEntry(chord=0x0008, hid_key=0x06, modifier=0x0002),  # 1R -> c
        ]
        assert config.find_conflicts() == []

    def test_find_conflicts_duplicate(self):
        """Config with duplicate chord is detected."""
        config = TwiddlerConfig()
        e1 = ChordEntry(chord=0x0002, hid_key=0x04, modifier=0x0002)  # 1L -> a
        e2 = ChordEntry(chord=0x0002, hid_key=0x05, modifier=0x0002)  # 1L -> b (conflict!)
        config.chords = [e1, e2]

        conflicts = config.find_conflicts()
        assert len(conflicts) == 1
        assert conflicts[0] == (e1, e2)

    def test_find_conflicts_multiple(self):
        """Config with multiple conflicts."""
        config = TwiddlerConfig()
        e1 = ChordEntry(chord=0x0002, hid_key=0x04, modifier=0x0002)  # 1L -> a
        e2 = ChordEntry(chord=0x0002, hid_key=0x05, modifier=0x0002)  # 1L -> b (conflict!)
        e3 = ChordEntry(chord=0x0004, hid_key=0x06, modifier=0x0002)  # 1M -> c
        e4 = ChordEntry(chord=0x0004, hid_key=0x07, modifier=0x0002)  # 1M -> d (conflict!)
        config.chords = [e1, e2, e3, e4]

        conflicts = config.find_conflicts()
        assert len(conflicts) == 2
        assert (e1, e2) in conflicts
        assert (e3, e4) in conflicts

    def test_find_conflicts_normalizes_bitmask(self):
        """Conflict detection normalizes to 16-bit."""
        config = TwiddlerConfig()
        e1 = ChordEntry(chord=0x0002, hid_key=0x04, modifier=0x0002)
        e2 = ChordEntry(chord=0x10002, hid_key=0x05, modifier=0x0002)  # High bits set
        config.chords = [e1, e2]

        conflicts = config.find_conflicts()
        assert len(conflicts) == 1


class TestTwiddlerConfigHelpers:
    """Tests for TwiddlerConfig helper methods."""

    def test_get_chord_found(self):
        """get_chord returns matching entry."""
        config = TwiddlerConfig()
        entry = ChordEntry(chord=0x0002, hid_key=0x04, modifier=0x0002)
        config.chords = [entry]

        assert config.get_chord(0x0002) == entry

    def test_get_chord_not_found(self):
        """get_chord returns None for missing chord."""
        config = TwiddlerConfig()
        config.chords = [ChordEntry(chord=0x0002, hid_key=0x04, modifier=0x0002)]

        assert config.get_chord(0x0004) is None

    def test_add_chord_unshifted(self):
        """add_chord adds unshifted chord."""
        config = TwiddlerConfig()
        config.add_chord(0x0002, 0x04)

        assert len(config.chords) == 1
        assert config.chords[0].chord == 0x0002
        assert config.chords[0].hid_key == 0x04
        assert config.chords[0].modifier == 0x0002
        assert config.chords[0].is_shifted is False

    def test_add_chord_shifted(self):
        """add_chord adds shifted chord."""
        config = TwiddlerConfig()
        config.add_chord(0x0002, 0x04, shifted=True)

        assert len(config.chords) == 1
        assert config.chords[0].modifier == 0x0220
        assert config.chords[0].is_shifted is True

    def test_repr(self):
        """Test repr of TwiddlerConfig."""
        config = TwiddlerConfig()
        config.chords = [ChordEntry(chord=0x0002, hid_key=0x04, modifier=0x0002)]

        r = repr(config)
        assert "v7" in r
        assert "1 chords" in r


class TestTwiddlerConfigUnmapped:
    """Tests for find_unmapped() method."""

    def test_find_unmapped_empty_config(self):
        """Empty config has all common chords unmapped."""
        config = TwiddlerConfig()
        unmapped = config.find_unmapped()
        assert len(unmapped) == 195  # All common chords

    def test_find_unmapped_with_mappings(self):
        """Config with some mappings excludes them from unmapped."""
        config = TwiddlerConfig()
        config.chords = [
            ChordEntry(chord=0x0002, hid_key=0x04, modifier=0x0002),  # 1L
            ChordEntry(chord=0x0004, hid_key=0x05, modifier=0x0002),  # 1M
        ]
        unmapped = config.find_unmapped()
        assert 0x0002 not in unmapped
        assert 0x0004 not in unmapped
        assert len(unmapped) == 193

    def test_find_unmapped_custom_reference(self):
        """Can use custom reference chord list."""
        config = TwiddlerConfig()
        config.chords = [ChordEntry(chord=0x0002, hid_key=0x04, modifier=0x0002)]

        # Custom reference with only 3 chords
        reference = [0x0002, 0x0004, 0x0008]
        unmapped = config.find_unmapped(reference)
        assert unmapped == [0x0004, 0x0008]


class TestTwiddlerConfigDiff:
    """Tests for diff() method."""

    def test_diff_identical(self):
        """Identical configs have no differences."""
        config1 = TwiddlerConfig()
        config1.add_chord(0x0002, 0x04)
        config2 = TwiddlerConfig()
        config2.add_chord(0x0002, 0x04)

        d = config1.diff(config2)
        assert d['added'] == []
        assert d['removed'] == []
        assert d['changed'] == []
        assert d['settings'] == {}

    def test_diff_added(self):
        """Detect chords added in second config."""
        config1 = TwiddlerConfig()
        config1.add_chord(0x0002, 0x04)

        config2 = TwiddlerConfig()
        config2.add_chord(0x0002, 0x04)
        config2.add_chord(0x0004, 0x05)  # Added

        d = config1.diff(config2)
        assert len(d['added']) == 1
        assert d['added'][0].chord == 0x0004

    def test_diff_removed(self):
        """Detect chords removed from first config."""
        config1 = TwiddlerConfig()
        config1.add_chord(0x0002, 0x04)
        config1.add_chord(0x0004, 0x05)

        config2 = TwiddlerConfig()
        config2.add_chord(0x0002, 0x04)

        d = config1.diff(config2)
        assert len(d['removed']) == 1
        assert d['removed'][0].chord == 0x0004

    def test_diff_changed(self):
        """Detect changed mappings."""
        config1 = TwiddlerConfig()
        config1.add_chord(0x0002, 0x04)  # 1L -> a

        config2 = TwiddlerConfig()
        config2.add_chord(0x0002, 0x05)  # 1L -> b (different key)

        d = config1.diff(config2)
        assert len(d['changed']) == 1
        chord, old, new = d['changed'][0]
        assert chord == 0x0002
        assert old.hid_key == 0x04
        assert new.hid_key == 0x05

    def test_diff_settings(self):
        """Detect changed settings."""
        config1 = TwiddlerConfig()
        config1.sleep_timeout = 1000

        config2 = TwiddlerConfig()
        config2.sleep_timeout = 2000

        d = config1.diff(config2)
        assert 'sleep_timeout' in d['settings']
        assert d['settings']['sleep_timeout'] == (1000, 2000)
