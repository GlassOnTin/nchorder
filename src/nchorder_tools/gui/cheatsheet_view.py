"""
Chord Cheat Sheet View

Visual reference cards showing button layouts with output labels.
Each card shows one "base" button and all chords that start with it.
"""

from pathlib import Path
from typing import Dict, List, Optional, Tuple

from kivy.uix.widget import Widget
from kivy.uix.boxlayout import BoxLayout
from kivy.uix.gridlayout import GridLayout
from kivy.uix.scrollview import ScrollView
from kivy.uix.label import Label
from kivy.uix.button import Button
from kivy.graphics import Color, Rectangle, RoundedRectangle, Line
from kivy.properties import ObjectProperty, NumericProperty, ListProperty, StringProperty
from kivy.core.text import Label as CoreLabel

from .chord_view import ChordConfig, ChordEntry, BTN_BITS, BTN_NAMES


# Unicode symbol replacements for common keys
# Using widely-supported symbols only
KEY_SYMBOLS = {
    'space': '␣',
    'enter': '↵',
    'tab': '→|',
    'bksp': '←x',
    'del': 'x→',
    'esc': 'Esc',
    'up': '↑',
    'down': '↓',
    'left': '←',
    'right': '→',
    'home': '|←',
    'end': '→|',
    'pgup': '↑↑',
    'pgdn': '↓↓',
    'caps': 'Cap',
    'ins': 'Ins',
    'numlk': 'Num',
}

# Font with good Unicode symbol support
SYMBOL_FONT = 'DejaVuSans'  # Common on Linux, good symbol coverage


def parse_key_output(key_str: str) -> tuple[str, bool, bool, bool]:
    """
    Parse a key string into (base_key, has_shift, has_alt, has_ctrl).

    Format from chord_view.py key_str():
        "a" -> plain lowercase
        "A" -> shifted (uppercase = shift)
        "C-x" -> Ctrl+x
        "A-Tab" -> Alt+Tab
        "S-F1" -> Shift+F1
        "C-A-x" -> Ctrl+Alt+x

    Examples:
        "a" -> ("a", False, False, False)
        "A" -> ("A", True, False, False)
        "C-x" -> ("x", False, False, True)
        "A-Tab" -> ("Tb", False, True, False)
    """
    has_shift = False
    has_alt = False
    has_ctrl = False

    # Parse C-, A-, S- prefixes
    remaining = key_str
    while True:
        if remaining.startswith('C-'):
            has_ctrl = True
            remaining = remaining[2:]
        elif remaining.startswith('A-'):
            has_alt = True
            remaining = remaining[2:]
        elif remaining.startswith('S-'):
            has_shift = True
            remaining = remaining[2:]
        else:
            break

    base = remaining

    # Detect shift from uppercase single letter
    if len(base) == 1 and base.isupper():
        has_shift = True

    # Convert to symbol if available
    base_lower = base.lower()
    if base_lower in KEY_SYMBOLS:
        base = KEY_SYMBOLS[base_lower]

    return base, has_shift, has_alt, has_ctrl


# Button layout matching physical Twiddler
# T0 moved to first row between T1 and T4
CHEATSHEET_LAYOUT = {
    # Thumb row (row 0) - T1 left, T0 center-left, T4 right
    'T1': (0, 0),
    'T0': (0, 1),  # Between T1 and T2/T3
    'T4': (0, 3),
    # Second row for T2/T3
    'T2': (1, 1), 'T3': (1, 2),
    # Finger buttons (rows 2-6)
    'F0L': (2, 0), 'F0M': (2, 1), 'F0R': (2, 2),
    'F1L': (3, 0), 'F1M': (3, 1), 'F1R': (3, 2),
    'F2L': (4, 0), 'F2M': (4, 1), 'F2R': (4, 2),
    'F3L': (5, 0), 'F3M': (5, 1), 'F3R': (5, 2),
    'F4L': (6, 0), 'F4M': (6, 1), 'F4R': (6, 2),
}

# Grid dimensions
GRID_ROWS = 7
GRID_COLS = 4


def get_chord_buttons(mask: int) -> List[Tuple[int, str]]:
    """Get list of (bit, name) for buttons in a chord, sorted by bit order."""
    buttons = []
    for btn_name, bit in BTN_BITS.items():
        if mask & (1 << bit):
            buttons.append((bit, btn_name))
    buttons.sort()  # Sort by bit = physical order
    return buttons


def get_first_button(mask: int) -> Optional[str]:
    """Get the first (base) button name for a chord."""
    buttons = get_chord_buttons(mask)
    return buttons[0][1] if buttons else None


def get_last_button(mask: int) -> Optional[str]:
    """Get the last button name for a chord."""
    buttons = get_chord_buttons(mask)
    return buttons[-1][1] if buttons else None


class CompressedChordCard(Widget):
    """
    Single card showing one base button and all chords starting with it.

    The base button is highlighted, and other button positions show
    the output character for pressing base + that button.

    Text colors indicate modifiers:
    - Green: No modifiers
    - Yellow: Shift
    - Cyan: Alt
    - Magenta: Ctrl
    - Orange: Shift+Alt
    - Red: Ctrl+anything
    """

    base_button = StringProperty('')

    # Colors
    COLOR_BG = (0.12, 0.12, 0.12, 1)
    COLOR_BTN_OFF = (0.25, 0.25, 0.25, 1)
    COLOR_BTN_BASE = (0.2, 0.5, 0.7, 1)  # Blue for base button
    COLOR_BTN_CHORD = (0.15, 0.15, 0.15, 1)  # Darker for chord buttons
    COLOR_TEXT = (0.9, 0.9, 0.9, 1)

    # Modifier-based text colors
    COLOR_PLAIN = (0.4, 0.9, 0.4, 1)      # Green: no modifiers
    COLOR_SHIFT = (0.9, 0.9, 0.3, 1)      # Yellow: Shift
    COLOR_ALT = (0.3, 0.9, 0.9, 1)        # Cyan: Alt
    COLOR_CTRL = (0.9, 0.3, 0.9, 1)       # Magenta: Ctrl
    COLOR_SHIFT_ALT = (0.9, 0.6, 0.2, 1)  # Orange: Shift+Alt
    COLOR_CTRL_ANY = (0.9, 0.3, 0.3, 1)   # Red: Ctrl+anything

    def __init__(self, base_button: str, chord_map: Dict[str, str], **kwargs):
        """
        Args:
            base_button: Name of the base button (e.g., 'T1')
            chord_map: Dict mapping button names to output strings
                       e.g., {'F0L': 'a', 'F0M': 'b', ...}
        """
        super().__init__(**kwargs)
        self.base_button = base_button
        self.chord_map = chord_map  # button_name -> output_string
        self.bind(pos=self._update, size=self._update)
        self._update()

    def _update(self, *args):
        self.canvas.clear()

        # Calculate cell size
        cell_w = self.width / GRID_COLS
        cell_h = self.height / GRID_ROWS
        margin = 2

        with self.canvas:
            # Background
            Color(*self.COLOR_BG)
            Rectangle(pos=self.pos, size=self.size)

            # Draw each button position
            for btn_name, (row, col) in CHEATSHEET_LAYOUT.items():
                x = self.x + col * cell_w + margin
                y = self.top - (row + 1) * cell_h + margin
                w = cell_w - margin * 2
                h = cell_h - margin * 2

                # Determine button state
                is_base = (btn_name == self.base_button)
                has_chord = btn_name in self.chord_map

                # Button color
                if is_base:
                    Color(*self.COLOR_BTN_BASE)
                elif has_chord:
                    Color(*self.COLOR_BTN_CHORD)
                else:
                    Color(*self.COLOR_BTN_OFF)

                RoundedRectangle(pos=(x, y), size=(w, h), radius=[3])

        # Draw labels after canvas (using CoreLabel for text on canvas)
        self._draw_labels()

    def _get_modifier_color(self, has_shift: bool, has_alt: bool, has_ctrl: bool):
        """Get text color based on modifiers."""
        if has_ctrl:
            return self.COLOR_CTRL_ANY
        elif has_shift and has_alt:
            return self.COLOR_SHIFT_ALT
        elif has_alt:
            return self.COLOR_ALT
        elif has_shift:
            return self.COLOR_SHIFT
        else:
            return self.COLOR_PLAIN

    def _draw_labels(self):
        """Draw output labels on buttons."""
        cell_w = self.width / GRID_COLS
        cell_h = self.height / GRID_ROWS

        with self.canvas:
            for btn_name, (row, col) in CHEATSHEET_LAYOUT.items():
                cx = self.x + (col + 0.5) * cell_w
                cy = self.top - (row + 0.5) * cell_h

                is_base = (btn_name == self.base_button)

                if is_base:
                    # Show base button name
                    text = self.base_button
                    color = self.COLOR_TEXT
                    font_size = 10
                elif btn_name in self.chord_map:
                    # Parse the key output for symbol and modifiers
                    raw_key = self.chord_map[btn_name]
                    text, has_shift, has_alt, has_ctrl = parse_key_output(raw_key)
                    color = self._get_modifier_color(has_shift, has_alt, has_ctrl)
                    font_size = 14 if len(text) <= 2 else 10
                else:
                    continue

                # Truncate long text
                if len(text) > 3:
                    text = text[:2] + '..'

                # Render white text with symbol font, then tint with Color
                label = CoreLabel(
                    text=text,
                    font_size=font_size,
                    bold=True,
                    font_name=SYMBOL_FONT
                )
                label.refresh()
                texture = label.texture

                # Tint the white text texture with our color
                Color(color[0], color[1], color[2], color[3])
                Rectangle(
                    texture=texture,
                    pos=(cx - texture.width / 2, cy - texture.height / 2),
                    size=texture.size
                )


class CheatSheetGrid(ScrollView):
    """Scrollable grid of compressed chord cards."""

    config = ObjectProperty(None, allownone=True)
    cards_per_row = NumericProperty(4)
    card_size = NumericProperty(120)

    # Min/max columns for responsive layout
    MIN_COLS = 2
    MAX_COLS = 6
    MIN_CARD_WIDTH = 100  # Minimum card width before reducing columns

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.do_scroll_x = False

        self.container = GridLayout(
            cols=self.cards_per_row,
            spacing=10,
            padding=10,
            size_hint_y=None
        )
        self.container.bind(minimum_height=self.container.setter('height'))
        self.add_widget(self.container)

        self._cards = []

        # Bind to size changes for responsive columns
        self.bind(size=self._on_size_change)

    def _on_size_change(self, instance, size):
        """Adjust column count based on available width."""
        if not self.config:
            return

        width = size[0]
        padding = 20  # Total horizontal padding
        spacing = 10  # Space between cards

        # Calculate how many cards fit at current card_size
        available = width - padding
        card_with_spacing = self.card_size + spacing

        if card_with_spacing > 0:
            cols = max(self.MIN_COLS, min(self.MAX_COLS, int(available / card_with_spacing)))

            if cols != self.cards_per_row:
                self.cards_per_row = cols
                self.container.cols = cols
                self.refresh()

    def refresh(self):
        """Rebuild grid from config with compressed cards."""
        self.container.clear_widgets()
        self._cards = []

        if not self.config:
            return

        # Group chords by first button
        chord_groups = self._group_by_first_button()

        # Create one card per base button that has chords
        for base_button in sorted(chord_groups.keys(), key=lambda b: BTN_BITS.get(b, 99)):
            chord_map = chord_groups[base_button]

            if not chord_map:
                continue

            # Card container (card + label below)
            card_box = BoxLayout(
                orientation='vertical',
                size_hint_y=None,
                height=self.card_size + 25
            )

            # The compressed chord card
            card = CompressedChordCard(
                base_button,
                chord_map,
                size_hint=(1, None),
                height=self.card_size
            )
            card_box.add_widget(card)
            self._cards.append(card)

            # Label showing base button name and chord count
            label = Label(
                text=f'{base_button} ({len(chord_map)} chords)',
                size_hint_y=None,
                height=25,
                font_size='12sp',
                color=(0.7, 0.7, 0.7, 1)
            )
            card_box.add_widget(label)

            self.container.add_widget(card_box)

    def _group_by_first_button(self) -> Dict[str, Dict[str, str]]:
        """
        Group chords by their first (base) button.

        Returns:
            Dict mapping base button name to dict of {last_button: output_string}
        """
        groups: Dict[str, Dict[str, str]] = {}

        for entry in self.config.entries:
            if not entry.is_keyboard:
                continue

            buttons = get_chord_buttons(entry.chord_mask)
            if not buttons:
                continue

            first_btn = buttons[0][1]
            last_btn = buttons[-1][1]
            output = entry.key_str()

            if first_btn not in groups:
                groups[first_btn] = {}

            # Map the last button to this chord's output
            # For single-button chords, first == last, so base shows its own output
            groups[first_btn][last_btn] = output

        return groups

    def export_image(self, filepath: str) -> bool:
        """Export grid to PNG image using widget screenshot."""
        try:
            # Use Kivy's built-in export_to_png on the container
            # First ensure all cards are visible
            self.container.export_to_png(filepath)
            return True
        except Exception as e:
            print(f"Export failed: {e}")
            return False


class CheatSheetView(BoxLayout):
    """Main cheat sheet view with controls."""

    config = ObjectProperty(None, allownone=True)

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.orientation = 'vertical'
        self.padding = 5
        self.spacing = 5

        # Toolbar with color legend
        toolbar = BoxLayout(size_hint_y=None, height=40, spacing=5)

        # Color legend
        legend_colors = [
            (CompressedChordCard.COLOR_PLAIN, ''),
            (CompressedChordCard.COLOR_SHIFT, 'Shift'),
            (CompressedChordCard.COLOR_ALT, 'Alt'),
            (CompressedChordCard.COLOR_CTRL, 'Ctrl'),
        ]
        for color, name in legend_colors:
            lbl = Label(
                text=name,
                size_hint_x=0.12,
                color=color,
                bold=True
            )
            toolbar.add_widget(lbl)

        # Spacer
        toolbar.add_widget(Label(text='', size_hint_x=0.04))

        # Card size control
        toolbar.add_widget(Label(text='Size:', size_hint_x=0.12))
        size_minus = Button(text='-', size_hint_x=0.08)
        size_minus.bind(on_press=lambda x: self._adjust_size(-20))
        size_plus = Button(text='+', size_hint_x=0.08)
        size_plus.bind(on_press=lambda x: self._adjust_size(20))
        toolbar.add_widget(size_minus)
        toolbar.add_widget(size_plus)

        # Export button
        export_btn = Button(text='Export PNG', size_hint_x=0.2)
        export_btn.bind(on_press=self._on_export)
        toolbar.add_widget(export_btn)

        self.add_widget(toolbar)

        # Status
        self.status_label = Label(
            text='Load a config to view cheat sheet',
            size_hint_y=None,
            height=25,
            font_size='12sp'
        )
        self.add_widget(self.status_label)

        # Grid
        self.grid = CheatSheetGrid()
        self.add_widget(self.grid)

    def load_config(self, config: ChordConfig):
        """Load config and refresh display."""
        self.config = config
        self.grid.config = config
        self.grid.refresh()

        # Count unique base buttons
        bases = set()
        for entry in config.entries:
            if entry.is_keyboard:
                first = get_first_button(entry.chord_mask)
                if first:
                    bases.add(first)

        keyboard_count = len([e for e in config.entries if e.is_keyboard])
        self.status_label.text = f'{keyboard_count} chords in {len(bases)} groups'

    def _set_cols(self, btn):
        """Set columns count."""
        self.grid.cards_per_row = btn.n_cols
        self.grid.container.cols = btn.n_cols
        self.grid.refresh()

    def _adjust_size(self, delta):
        """Adjust card size."""
        new_size = max(80, min(250, self.grid.card_size + delta))
        self.grid.card_size = new_size
        self.grid.refresh()

    def _on_export(self, instance):
        """Export to PNG."""
        from kivy.uix.popup import Popup
        from kivy.uix.filechooser import FileChooserListView

        content = BoxLayout(orientation='vertical')

        start_path = str(Path.home())
        chooser = FileChooserListView(path=start_path)
        content.add_widget(chooser)

        # Filename input
        from kivy.uix.textinput import TextInput
        name_row = BoxLayout(size_hint_y=None, height=40, spacing=5)
        name_row.add_widget(Label(text='Filename:', size_hint_x=0.3))
        name_input = TextInput(text='cheatsheet.png', multiline=False, size_hint_x=0.7)
        name_row.add_widget(name_input)
        content.add_widget(name_row)

        btn_row = BoxLayout(size_hint_y=None, height=50, spacing=10)

        def do_export(btn):
            path = Path(chooser.path) / name_input.text
            if self.grid.export_image(str(path)):
                self.status_label.text = f'Exported: {path}'
            else:
                self.status_label.text = 'Export failed'
            popup.dismiss()

        def do_cancel(btn):
            popup.dismiss()

        export_btn = Button(text='Export')
        export_btn.bind(on_press=do_export)
        cancel_btn = Button(text='Cancel')
        cancel_btn.bind(on_press=do_cancel)

        btn_row.add_widget(export_btn)
        btn_row.add_widget(cancel_btn)
        content.add_widget(btn_row)

        popup = Popup(
            title='Export Cheat Sheet',
            content=content,
            size_hint=(0.9, 0.9)
        )
        popup.open()
