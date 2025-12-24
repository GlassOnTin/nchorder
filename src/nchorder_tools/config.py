"""Core Twiddler configuration data structures."""

from dataclasses import dataclass, field
from typing import Optional


@dataclass
class ChordEntry:
    """A single chord mapping."""
    chord: int  # Button bitmask
    hid_key: int  # HID keycode
    modifier: int  # Modifier flags
    is_shifted: bool = False
    is_multi: bool = False  # Multi-character chord
    multi_chars: list[tuple[int, int]] = field(default_factory=list)  # [(mod, hid), ...]

    def __repr__(self):
        from .hid import chord_to_buttons, hid_to_char
        buttons = '+'.join(chord_to_buttons(self.chord)) or 'NONE'
        if self.is_multi:
            return f"ChordEntry({buttons} -> [multi:{len(self.multi_chars)} chars])"
        char = hid_to_char(self.hid_key, self.is_shifted)
        return f"ChordEntry({buttons} -> {char!r})"


@dataclass
class TwiddlerConfig:
    """Complete Twiddler configuration."""
    version: int = 7

    # Device settings
    sleep_timeout: int = 3720
    key_repeat_delay: int = 100
    mouse_accel: int = 10

    # Flags
    key_repeat: bool = False
    direct_key: bool = False
    joystick_left_click: bool = True
    disable_bluetooth: bool = False
    sticky_num: bool = False
    sticky_shift: bool = False
    haptic_feedback: bool = False

    # Mouse button actions
    mouse_left_action: int = 0
    mouse_middle_action: int = 3
    mouse_right_action: int = 1

    # Chord mappings
    chords: list[ChordEntry] = field(default_factory=list)

    # String table for multi-char chords
    strings: list[list[tuple[int, int]]] = field(default_factory=list)

    def get_chord(self, buttons: int) -> Optional[ChordEntry]:
        """Find chord entry by button combination."""
        for chord in self.chords:
            if chord.chord == buttons:
                return chord
        return None

    def add_chord(self, buttons: int, hid_key: int, shifted: bool = False):
        """Add a simple single-key chord."""
        modifier = 0x0220 if shifted else 0x0002
        self.chords.append(ChordEntry(
            chord=buttons,
            hid_key=hid_key,
            modifier=modifier,
            is_shifted=shifted
        ))

    def find_unmapped(self, reference_chords: list[int] = None) -> list[int]:
        """Find common chord combinations without mappings.

        Args:
            reference_chords: List of chord bitmasks to check.
                             If None, uses generate_common_chords().

        Returns list of unmapped chord bitmasks.
        """
        if reference_chords is None:
            from .hid import generate_common_chords
            reference_chords = generate_common_chords()

        mapped = {entry.chord & 0xFFFF for entry in self.chords}
        return [c for c in reference_chords if c not in mapped]

    def find_conflicts(self) -> list[tuple["ChordEntry", "ChordEntry"]]:
        """Find chord entries with identical button combinations.

        Returns list of (first_entry, duplicate_entry) pairs where both entries
        map the same chord bitmask to different outputs.
        """
        seen: dict[int, "ChordEntry"] = {}
        conflicts = []
        for entry in self.chords:
            # Normalize to 16-bit chord bitmask
            bits = entry.chord & 0xFFFF
            if bits in seen:
                conflicts.append((seen[bits], entry))
            else:
                seen[bits] = entry
        return conflicts

    def diff(self, other: "TwiddlerConfig") -> dict:
        """Compare two configs, return differences.

        Returns dict with keys:
        - 'added': chords in other but not self
        - 'removed': chords in self but not other
        - 'changed': list of (chord_bits, self_entry, other_entry) for different mappings
        - 'settings': dict of changed settings {name: (self_val, other_val)}
        """
        self_map = {e.chord & 0xFFFF: e for e in self.chords}
        other_map = {e.chord & 0xFFFF: e for e in other.chords}

        self_keys = set(self_map.keys())
        other_keys = set(other_map.keys())

        added = [other_map[k] for k in sorted(other_keys - self_keys)]
        removed = [self_map[k] for k in sorted(self_keys - other_keys)]

        changed = []
        for k in sorted(self_keys & other_keys):
            s, o = self_map[k], other_map[k]
            if s.hid_key != o.hid_key or s.modifier != o.modifier:
                changed.append((k, s, o))

        settings = {}
        for attr in ['sleep_timeout', 'key_repeat_delay', 'mouse_accel']:
            sv, ov = getattr(self, attr), getattr(other, attr)
            if sv != ov:
                settings[attr] = (sv, ov)

        return {'added': added, 'removed': removed, 'changed': changed, 'settings': settings}

    def __repr__(self):
        return f"TwiddlerConfig(v{self.version}, {len(self.chords)} chords)"
