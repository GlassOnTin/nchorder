"""Tests for HID keycode mappings and button utilities."""

import pytest
from nchorder_tools.hid import (
    hid_to_char,
    char_to_hid,
    chord_to_buttons,
    buttons_to_chord,
    chord_to_tutor_notation,
    hid_to_tutor_key,
    generate_common_chords,
    BUTTON_BITS,
    HID_MAP,
)


class TestHidToChar:
    """Tests for hid_to_char conversion."""

    def test_lowercase_letters(self):
        """Convert HID codes to lowercase letters."""
        assert hid_to_char(0x04, False) == 'a'
        assert hid_to_char(0x1D, False) == 'z'
        assert hid_to_char(0x10, False) == 'm'

    def test_uppercase_letters(self):
        """Convert HID codes to uppercase letters when shifted."""
        assert hid_to_char(0x04, True) == 'A'
        assert hid_to_char(0x1D, True) == 'Z'
        assert hid_to_char(0x10, True) == 'M'

    def test_numbers_unshifted(self):
        """Convert HID codes to numbers."""
        assert hid_to_char(0x1E, False) == '1'
        assert hid_to_char(0x27, False) == '0'
        assert hid_to_char(0x22, False) == '5'

    def test_numbers_shifted(self):
        """Convert HID codes to symbols when shifted."""
        assert hid_to_char(0x1E, True) == '!'
        assert hid_to_char(0x27, True) == ')'
        assert hid_to_char(0x1F, True) == '@'

    def test_special_keys(self):
        """Convert HID codes for special keys."""
        assert hid_to_char(0x28, False) == '<Return>'
        assert hid_to_char(0x29, False) == '<Escape>'
        assert hid_to_char(0x2A, False) == '<Backspace>'
        assert hid_to_char(0x2B, False) == '<Tab>'
        assert hid_to_char(0x2C, False) == '<Space>'

    def test_function_keys(self):
        """Convert HID codes for function keys."""
        assert hid_to_char(0x3A, False) == '<F1>'
        assert hid_to_char(0x45, False) == '<F12>'

    def test_unknown_code(self):
        """Unknown HID codes return hex representation."""
        assert hid_to_char(0xFF, False) == '<0xFF>'
        assert hid_to_char(0xAB, False) == '<0xAB>'


class TestCharToHid:
    """Tests for char_to_hid conversion."""

    def test_lowercase_letters(self):
        """Convert lowercase letters to HID codes."""
        assert char_to_hid('a') == (0x04, False)
        assert char_to_hid('z') == (0x1D, False)

    def test_uppercase_letters(self):
        """Convert uppercase letters to HID codes with shift."""
        assert char_to_hid('A') == (0x04, True)
        assert char_to_hid('Z') == (0x1D, True)

    def test_numbers(self):
        """Convert numbers to HID codes."""
        assert char_to_hid('1') == (0x1E, False)
        assert char_to_hid('0') == (0x27, False)

    def test_symbols(self):
        """Convert symbols to HID codes with shift."""
        assert char_to_hid('!') == (0x1E, True)
        assert char_to_hid('@') == (0x1F, True)

    def test_punctuation(self):
        """Convert punctuation to HID codes."""
        assert char_to_hid('-') == (0x2D, False)
        assert char_to_hid('_') == (0x2D, True)

    def test_unknown_char(self):
        """Unknown characters return None."""
        assert char_to_hid('€') is None
        assert char_to_hid('π') is None

    def test_roundtrip_letters(self):
        """Verify roundtrip for all letters."""
        for char in 'abcdefghijklmnopqrstuvwxyz':
            hid, shifted = char_to_hid(char)
            assert hid_to_char(hid, shifted) == char

        for char in 'ABCDEFGHIJKLMNOPQRSTUVWXYZ':
            hid, shifted = char_to_hid(char)
            assert hid_to_char(hid, shifted) == char


class TestButtonConversions:
    """Tests for chord bitmask and button list conversions."""

    def test_chord_to_buttons_single(self):
        """Convert single button chord to list."""
        assert chord_to_buttons(0x0001) == ['N']
        assert chord_to_buttons(0x0002) == ['1L']
        assert chord_to_buttons(0x0010) == ['A']

    def test_chord_to_buttons_multiple(self):
        """Convert multi-button chord to list."""
        # N + 1L = bit 0 + bit 1 = 0x0003
        buttons = chord_to_buttons(0x0003)
        assert set(buttons) == {'N', '1L'}

    def test_chord_to_buttons_all_thumbs(self):
        """Convert all thumb buttons."""
        # N=0, A=4, C=8, S=12 => 0x1111
        buttons = chord_to_buttons(0x1111)
        assert set(buttons) == {'N', 'A', 'C', 'S'}

    def test_chord_to_buttons_finger_row(self):
        """Convert finger row."""
        # 1L=1, 2L=5, 3L=9, 4L=13 => 0x2222
        buttons = chord_to_buttons(0x2222)
        assert set(buttons) == {'1L', '2L', '3L', '4L'}

    def test_buttons_to_chord_single(self):
        """Convert single button to chord."""
        assert buttons_to_chord(['N']) == 0x0001
        assert buttons_to_chord(['1L']) == 0x0002
        assert buttons_to_chord(['A']) == 0x0010

    def test_buttons_to_chord_multiple(self):
        """Convert multiple buttons to chord."""
        chord = buttons_to_chord(['N', '1L'])
        assert chord == 0x0003

    def test_buttons_to_chord_roundtrip(self):
        """Verify roundtrip conversion."""
        test_buttons = ['N', '1L', '2M', '3R']
        chord = buttons_to_chord(test_buttons)
        result = chord_to_buttons(chord)
        assert set(result) == set(test_buttons)

    def test_buttons_to_chord_invalid(self):
        """Invalid button names are ignored."""
        chord = buttons_to_chord(['N', 'INVALID', '1L'])
        assert chord == buttons_to_chord(['N', '1L'])


class TestTutorNotation:
    """Tests for Tutor notation conversion."""

    def test_fingers_only(self):
        """Convert finger-only chord."""
        # 1L = bit 1 = 0x0002
        assert chord_to_tutor_notation(0x0002, include_thumbs=True) == 'LOOO'
        assert chord_to_tutor_notation(0x0002, include_thumbs=False) == 'LOOO'

    def test_with_thumb(self):
        """Convert chord with thumb modifier."""
        # N + 1L = 0x0003
        assert chord_to_tutor_notation(0x0003, include_thumbs=True) == 'N LOOO'
        assert chord_to_tutor_notation(0x0003, include_thumbs=False) == 'LOOO'

    def test_multiple_thumbs(self):
        """Convert chord with multiple thumb modifiers."""
        # N + A + 1L = bit 0 + bit 4 + bit 1 = 0x0013
        assert chord_to_tutor_notation(0x0013, include_thumbs=True) == 'NA LOOO'

    def test_multiple_fingers(self):
        """Convert chord with multiple fingers."""
        # 1L + 2M + 3R = bit 1 + bit 6 + bit 11 = 0x0842
        notation = chord_to_tutor_notation(0x0842, include_thumbs=False)
        assert notation == 'LMRO'


class TestHidToTutorKey:
    """Tests for hid_to_tutor_key conversion."""

    def test_letter(self):
        """Convert letter HID code."""
        assert hid_to_tutor_key(0x04, 0x0002) == 'a'

    def test_shifted_letter(self):
        """Convert shifted letter HID code."""
        assert hid_to_tutor_key(0x04, 0x0220) == 'A'

    def test_special_keys(self):
        """Convert special keys to Tutor names."""
        assert hid_to_tutor_key(0x2C, 0x0002) == ' '  # Space becomes literal space
        assert hid_to_tutor_key(0x28, 0x0002) == 'enter'  # Return -> enter
        assert hid_to_tutor_key(0x2A, 0x0002) == 'backspace'
        assert hid_to_tutor_key(0x29, 0x0002) == 'esc'

    def test_system_chord_skipped(self):
        """System chords return None."""
        # Modifier low byte 0x07 = system function
        assert hid_to_tutor_key(0x04, 0x0007) is None

    def test_mouse_chord(self):
        """Mouse chords return descriptive strings."""
        # 0x0201 = Mouse Left (0x02 << 8 | 0x01)
        assert hid_to_tutor_key(0, 0x0201) == '<MouseLeft>'
        # 0x0C01 = Mouse Right
        assert hid_to_tutor_key(0, 0x0C01) == '<MouseRight>'
        # Generic/unknown mouse function
        assert hid_to_tutor_key(0, 0x0001) == '<Mouse:00>'

    def test_multi_char_skipped(self):
        """Multi-char chords return None."""
        assert hid_to_tutor_key(0x04, 0x02FF) is None
        assert hid_to_tutor_key(0x04, 0xFF02) is None


class TestGenerateCommonChords:
    """Tests for generate_common_chords function."""

    def test_returns_list(self):
        """Returns a list of integers."""
        chords = generate_common_chords()
        assert isinstance(chords, list)
        assert all(isinstance(c, int) for c in chords)

    def test_expected_count(self):
        """Returns expected number of chords (12 + 27 + 48 + 108 = 195)."""
        chords = generate_common_chords()
        # 12 single finger + 27 two-finger adjacent + 48 thumb+finger + 108 thumb+two-finger
        assert len(chords) == 195

    def test_single_finger_included(self):
        """Single finger chords are included."""
        chords = generate_common_chords()
        # 1L = bit 1 = 0x0002
        assert 0x0002 in chords
        # 4R = bit 15 = 0x8000
        assert 0x8000 in chords

    def test_two_finger_included(self):
        """Two-finger adjacent row chords are included."""
        chords = generate_common_chords()
        # 1L + 2L = bit 1 + bit 5 = 0x0022
        assert 0x0022 in chords

    def test_thumb_combos_included(self):
        """Thumb + finger combinations are included."""
        chords = generate_common_chords()
        # N + 1L = bit 0 + bit 1 = 0x0003
        assert 0x0003 in chords
        # S + 2M = bit 12 + bit 6 = 0x1040
        assert 0x1040 in chords

    def test_no_duplicates(self):
        """No duplicate chords in list."""
        chords = generate_common_chords()
        assert len(chords) == len(set(chords))
