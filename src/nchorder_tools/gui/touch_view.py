"""
Touch Visualizer Widget

Displays real-time touch sensor data:
- Square sensor: 2D touch position as colored circle
- Bar sensors: 3 vertical bars with touch positions
- Button state: Twiddler 4 button layout with labels
- Chord table: Scrollable list showing loaded chords and live match
"""

from kivy.uix.widget import Widget
from kivy.uix.boxlayout import BoxLayout
from kivy.uix.gridlayout import GridLayout
from kivy.uix.scrollview import ScrollView
from kivy.uix.label import Label
from kivy.graphics import Color, Ellipse, Rectangle, Line, RoundedRectangle
from kivy.properties import (
    NumericProperty, BooleanProperty, ListProperty, ObjectProperty, StringProperty
)
from kivy.clock import Clock
from kivy.core.text import Label as CoreLabel
from kivy.metrics import sp

from ..cdc_client import TouchFrame


# Button bit positions matching firmware
BTN_BITS = {
    'T1': 0, 'F1L': 1, 'F1M': 2, 'F1R': 3,
    'T2': 4, 'F2L': 5, 'F2M': 6, 'F2R': 7,
    'T3': 8, 'F3L': 9, 'F3M': 10, 'F3R': 11,
    'T4': 12, 'F4L': 13, 'F4M': 14, 'F4R': 15,
    'F0L': 16, 'F0M': 17, 'F0R': 18, 'T0': 19,
}

BTN_NAMES = {v: k for k, v in BTN_BITS.items()}


class TouchSquare(Widget):
    """Square sensor visualization (thumb area)"""

    touch_x = NumericProperty(0)  # 0-1792
    touch_y = NumericProperty(0)  # 0-1792
    touch_size = NumericProperty(0)
    max_coord = NumericProperty(1800)  # Trill Square with prescaler 3

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.bind(
            pos=self._update_canvas,
            size=self._update_canvas,
            touch_x=self._update_canvas,
            touch_y=self._update_canvas,
            touch_size=self._update_canvas
        )
        self._update_canvas()

    def _update_canvas(self, *args):
        self.canvas.clear()
        with self.canvas:
            # Background
            Color(0.15, 0.15, 0.15)
            Rectangle(pos=self.pos, size=self.size)

            # Border
            Color(0.4, 0.4, 0.4)
            Line(rectangle=(*self.pos, *self.size), width=1)

            # Grid lines
            Color(0.25, 0.25, 0.25)
            for i in range(1, 4):
                x = self.x + (self.width * i / 4)
                Line(points=[x, self.y, x, self.top], width=1)
                y = self.y + (self.height * i / 4)
                Line(points=[self.x, y, self.right, y], width=1)

            # Touch point
            if self.touch_size > 0:
                # Map coordinates to widget space
                px = self.x + (self.touch_x / self.max_coord) * self.width
                py = self.y + (self.touch_y / self.max_coord) * self.height

                # Size based on touch pressure
                radius = 10 + (self.touch_size / 100)
                radius = min(radius, 40)

                # Color based on position
                Color(0.2, 0.8, 0.3)  # Green
                Ellipse(
                    pos=(px - radius, py - radius),
                    size=(radius * 2, radius * 2)
                )

                # Center dot
                Color(1, 1, 1)
                Ellipse(pos=(px - 3, py - 3), size=(6, 6))


class TouchBar(Widget):
    """Single bar sensor visualization with multitouch support"""

    touches = ListProperty([])  # List of (pos, size) tuples
    bar_index = NumericProperty(0)  # 0, 1, or 2
    max_pos = NumericProperty(3200)  # Trill Bar full range

    # Colors for each bar (left, middle, right)
    BAR_COLORS = [
        (0.8, 0.3, 0.3),  # Red
        (0.3, 0.8, 0.3),  # Green
        (0.3, 0.3, 0.8),  # Blue
    ]

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.bind(
            pos=self._update_canvas,
            size=self._update_canvas,
            touches=self._update_canvas
        )
        self._update_canvas()

    def _update_canvas(self, *args):
        self.canvas.clear()
        with self.canvas:
            # Background
            Color(0.15, 0.15, 0.15)
            Rectangle(pos=self.pos, size=self.size)

            # Border
            Color(0.4, 0.4, 0.4)
            Line(rectangle=(*self.pos, *self.size), width=1)

            # Draw all touch indicators
            color = self.BAR_COLORS[self.bar_index % 3]
            for touch in self.touches:
                touch_pos, touch_size = touch
                if touch_size > 0:
                    # Map position to widget (vertical bar)
                    py = self.y + (touch_pos / self.max_pos) * self.height

                    # Bar width based on touch size
                    bar_width = self.width * 0.8
                    bar_height = 4 + (touch_size / 50)
                    bar_height = min(bar_height, 20)

                    Color(*color)
                    Rectangle(
                        pos=(self.x + (self.width - bar_width) / 2, py - bar_height / 2),
                        size=(bar_width, bar_height)
                    )


class TwiddlerButtonGrid(Widget):
    """Twiddler 4 button layout with dynamic chord hints"""

    buttons = NumericProperty(0)  # Button bitmask
    config = ObjectProperty(None, allownone=True)  # ChordConfig for hints

    # Physical Twiddler layout:
    # Row 0: Thumb buttons (T1, T0, T2, T3, T4)
    # Row 1: F0 (index finger top row)
    # Row 2-5: F1-F4 finger rows
    # Empty string = no button at that position

    LAYOUT = [
        ['T1', 'T0', 'T2', 'T3', 'T4'],  # Thumb row
        ['', 'F0L', 'F0M', 'F0R', ''],   # F0 row (centered)
        ['', 'F1L', 'F1M', 'F1R', ''],   # F1 row
        ['', 'F2L', 'F2M', 'F2R', ''],   # F2 row
        ['', 'F3L', 'F3M', 'F3R', ''],   # F3 row
        ['', 'F4L', 'F4M', 'F4R', ''],   # F4 row
    ]

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self._chord_hints = {}  # button_name -> output_string
        self.bind(
            pos=self._update_canvas,
            size=self._update_canvas,
            buttons=self._update_canvas
        )
        self._update_canvas()

    def _get_pressed_buttons(self) -> list:
        """Get list of currently pressed button names, sorted by bit order."""
        pressed = []
        for btn_name, bit in BTN_BITS.items():
            if self.buttons & (1 << bit):
                pressed.append((bit, btn_name))
        pressed.sort()
        return [name for bit, name in pressed]

    def _update_chord_hints(self):
        """Update chord hints based on currently pressed buttons."""
        self._chord_hints = {}

        if not self.config:
            return

        pressed = self._get_pressed_buttons()
        if not pressed:
            return

        # Get the current button mask
        current_mask = self.buttons

        # Look up all chords that START with the currently pressed buttons
        for entry in self.config.entries:
            if not entry.is_keyboard:
                continue

            chord_mask = entry.chord_mask

            # Check if current buttons are a prefix of this chord
            if (chord_mask & current_mask) == current_mask and chord_mask != current_mask:
                # Find the "next" button(s) in this chord
                remaining = chord_mask & ~current_mask
                # Get the lowest bit (next button to press)
                for bit in range(20):
                    if remaining & (1 << bit):
                        btn_name = BTN_NAMES.get(bit)
                        if btn_name and btn_name not in self._chord_hints:
                            self._chord_hints[btn_name] = entry.key_str()
                        break  # Only show hint for next button in sequence

    def _update_canvas(self, *args):
        self.canvas.clear()

        # Update chord hints based on current buttons
        self._update_chord_hints()

        rows = len(self.LAYOUT)
        cols = len(self.LAYOUT[0])
        cell_w = self.width / cols
        cell_h = self.height / rows
        margin = 3

        with self.canvas:
            # Background
            Color(0.12, 0.12, 0.12)
            Rectangle(pos=self.pos, size=self.size)

            for row_idx, row in enumerate(self.LAYOUT):
                for col_idx, btn_name in enumerate(row):
                    # Skip empty cells
                    if not btn_name:
                        continue

                    x = self.x + col_idx * cell_w
                    y = self.top - (row_idx + 1) * cell_h

                    # Get button state
                    bit = BTN_BITS.get(btn_name, -1)
                    is_pressed = bit >= 0 and (self.buttons >> bit) & 1
                    has_hint = btn_name in self._chord_hints

                    # Button background
                    if is_pressed:
                        Color(0.2, 0.7, 0.3)  # Bright green when pressed
                    elif has_hint:
                        Color(0.2, 0.2, 0.3)  # Slightly highlighted for hint
                    else:
                        Color(0.25, 0.25, 0.25)  # Dark gray

                    RoundedRectangle(
                        pos=(x + margin, y + margin),
                        size=(cell_w - margin * 2, cell_h - margin * 2),
                        radius=[5]
                    )

                    # Border
                    if is_pressed:
                        Color(0.4, 1.0, 0.5)
                    elif has_hint:
                        Color(0.4, 0.5, 0.7)  # Blue border for hints
                    else:
                        Color(0.4, 0.4, 0.4)
                    Line(
                        rounded_rectangle=(
                            x + margin, y + margin,
                            cell_w - margin * 2, cell_h - margin * 2, 5
                        ),
                        width=1.2
                    )

        # Draw labels on top
        self.canvas.after.clear()
        with self.canvas.after:
            for row_idx, row in enumerate(self.LAYOUT):
                for col_idx, btn_name in enumerate(row):
                    # Skip empty cells
                    if not btn_name:
                        continue

                    cx = self.x + col_idx * cell_w + cell_w / 2
                    cy = self.top - (row_idx + 1) * cell_h + cell_h / 2

                    bit = BTN_BITS.get(btn_name, -1)
                    is_pressed = bit >= 0 and (self.buttons >> bit) & 1
                    hint = self._chord_hints.get(btn_name)

                    # Determine what text to show
                    if hint and not is_pressed:
                        # Show chord hint
                        text = hint
                        if len(text) > 3:
                            text = text[:3]
                        color = (0.4, 0.9, 0.4, 1)  # Green for hints
                        font_size = sp(16)
                    else:
                        # Show button name
                        text = btn_name
                        color = (1, 1, 1, 1) if is_pressed else (0.5, 0.5, 0.5, 1)
                        font_size = sp(14)

                    # Render text
                    label = CoreLabel(text=text, font_size=font_size, bold=is_pressed)
                    label.refresh()
                    texture = label.texture

                    Color(*color)
                    Rectangle(
                        texture=texture,
                        pos=(cx - texture.width / 2, cy - texture.height / 2),
                        size=texture.size
                    )


class GPIODiagnostics(BoxLayout):
    """Debug display for GPIO button driver diagnostics"""

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.orientation = 'vertical'
        self.size_hint_y = None
        self.height = 160
        self.padding = 5
        self.spacing = 2

        # Title
        title = Label(
            text='[GPIO Debug]',
            size_hint_y=None,
            height=28,
            font_size='16sp',
            color=(0.7, 0.7, 0.2, 1)
        )
        self.add_widget(title)

        # Info labels
        self.info_labels = {}
        for key in ['raw_p0', 'raw_p1', 'raw_btns', 'prev_raw', 'callbacks', 'debounce']:
            lbl = Label(
                text=f'{key}: ---',
                size_hint_y=None,
                height=22,
                font_size='14sp',
                halign='left',
                color=(0.6, 0.6, 0.6, 1)
            )
            lbl.bind(size=lbl.setter('text_size'))
            self.info_labels[key] = lbl
            self.add_widget(lbl)

    def update(self, diag: dict):
        """Update from GPIO diagnostics dict"""
        if not diag:
            for lbl in self.info_labels.values():
                lbl.text = '---'
            return

        self.info_labels['raw_p0'].text = f'P0:  0x{diag.get("raw_p0", 0):08X}'
        self.info_labels['raw_p1'].text = f'P1:  0x{diag.get("raw_p1", 0):08X}'
        self.info_labels['raw_btns'].text = f'Raw: 0x{diag.get("raw_buttons", 0):06X}'
        self.info_labels['prev_raw'].text = f'Prv: 0x{diag.get("prev_raw_state", 0):06X}'
        self.info_labels['callbacks'].text = f'Callbacks: {diag.get("callback_count", 0)}'
        self.info_labels['debounce'].text = f'Debounce: {diag.get("debounce_count", 0)}'

        # Highlight if raw buttons != 0 (buttons being pressed)
        raw_btns = diag.get("raw_buttons", 0)
        if raw_btns:
            self.info_labels['raw_btns'].color = (0.3, 1.0, 0.3, 1)  # Green
        else:
            self.info_labels['raw_btns'].color = (0.6, 0.6, 0.6, 1)  # Gray


class ChordDisplay(BoxLayout):
    """Shows current chord and matched output"""

    buttons = NumericProperty(0)
    chord_text = StringProperty('')
    output_text = StringProperty('')

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.orientation = 'horizontal'
        self.size_hint_y = None
        self.height = 60
        self.padding = 10
        self.spacing = 10

        self.chord_label = Label(
            text='Chord: (none)',
            size_hint_x=0.5,
            halign='left',
            font_size='20sp'
        )
        self.chord_label.bind(size=self.chord_label.setter('text_size'))

        self.output_label = Label(
            text='Output: -',
            size_hint_x=0.5,
            halign='left',
            font_size='20sp',
            bold=True
        )
        self.output_label.bind(size=self.output_label.setter('text_size'))

        self.add_widget(self.chord_label)
        self.add_widget(self.output_label)

        self.bind(chord_text=self._update_labels, output_text=self._update_labels)

    def _update_labels(self, *args):
        self.chord_label.text = f'Chord: {self.chord_text}' if self.chord_text else 'Chord: (none)'
        self.output_label.text = f'Output: {self.output_text}' if self.output_text else 'Output: -'

    def update_from_buttons(self, buttons: int, config=None):
        """Update display from button state"""
        self.buttons = buttons

        if buttons == 0:
            self.chord_text = ''
            self.output_text = ''
            return

        # Build chord string
        btns = []
        for i in range(20):
            if buttons & (1 << i):
                btns.append(BTN_NAMES.get(i, f'B{i}'))
        self.chord_text = '+'.join(btns)

        # Look up in config if available
        if config:
            entry = config.find_chord(buttons)
            if entry:
                self.output_text = entry.key_str()
            else:
                self.output_text = '(unmapped)'
        else:
            self.output_text = ''


class ChordTableRow(BoxLayout):
    """Single row in chord table"""

    def __init__(self, chord_str: str, output_str: str, is_header=False, **kwargs):
        super().__init__(**kwargs)
        self.orientation = 'horizontal'
        self.size_hint_y = None
        self.height = 36 if not is_header else 40
        self.padding = [5, 2]

        font_size = '16sp' if not is_header else '18sp'
        bold = is_header

        chord_lbl = Label(
            text=chord_str,
            size_hint_x=0.6,
            halign='left',
            valign='middle',
            font_size=font_size,
            bold=bold
        )
        chord_lbl.bind(size=chord_lbl.setter('text_size'))

        output_lbl = Label(
            text=output_str,
            size_hint_x=0.4,
            halign='center',
            valign='middle',
            font_size=font_size,
            bold=bold
        )
        output_lbl.bind(size=output_lbl.setter('text_size'))

        self.add_widget(chord_lbl)
        self.add_widget(output_lbl)

        # Background
        with self.canvas.before:
            if is_header:
                Color(0.25, 0.25, 0.3)
            else:
                Color(0.15, 0.15, 0.15)
            self.bg_rect = Rectangle(pos=self.pos, size=self.size)
        self.bind(pos=self._update_bg, size=self._update_bg)

    def _update_bg(self, *args):
        self.bg_rect.pos = self.pos
        self.bg_rect.size = self.size


class ChordTable(BoxLayout):
    """Scrollable table of all chords"""

    config = ObjectProperty(None, allownone=True)

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.orientation = 'vertical'

        # Header
        self.header = ChordTableRow('Chord', 'Output', is_header=True)
        self.add_widget(self.header)

        # Scrollable content
        self.scroll = ScrollView(do_scroll_x=False)
        self.container = BoxLayout(
            orientation='vertical',
            size_hint_y=None,
            spacing=1,
            padding=[0, 5]
        )
        self.container.bind(minimum_height=self.container.setter('height'))
        self.scroll.add_widget(self.container)
        self.add_widget(self.scroll)

    def load_config(self, config):
        """Load chord config and populate table"""
        self.config = config
        self.container.clear_widgets()

        if not config:
            return

        # Sort by chord mask
        sorted_entries = sorted(config.entries, key=lambda e: e.chord_mask)

        for entry in sorted_entries:
            if entry.is_keyboard:
                row = ChordTableRow(entry.chord_str(), entry.key_str())
                self.container.add_widget(row)


class TouchVisualizer(BoxLayout):
    """Combined touch visualization panel for Twiddler 4"""

    touch_frame = ObjectProperty(None, allownone=True)
    config = ObjectProperty(None, allownone=True)  # ChordConfig for lookup

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.orientation = 'vertical'
        self.spacing = 10
        self.padding = 5

        self._sensors_visible = False  # Track if sensors panel is shown
        self._has_trill_data = False  # Track if we've received Trill touch data

        # Top section: sensors and buttons
        self.top_section = BoxLayout(orientation='horizontal', spacing=10, size_hint_y=0.6)

        # Touch sensors (for nChorder - hidden by default, shown when Trill data received)
        self.sensor_panel = BoxLayout(orientation='horizontal', spacing=5, size_hint_x=0.5)
        self.square = TouchSquare(size_hint_x=0.6)
        bar_panel = BoxLayout(orientation='horizontal', spacing=2, size_hint_x=0.4)
        self.bar0 = TouchBar(bar_index=0)
        self.bar1 = TouchBar(bar_index=1)
        self.bar2 = TouchBar(bar_index=2)
        bar_panel.add_widget(self.bar0)
        bar_panel.add_widget(self.bar1)
        bar_panel.add_widget(self.bar2)
        self.sensor_panel.add_widget(bar_panel)
        self.sensor_panel.add_widget(self.square)

        # Button grid (Twiddler 4 layout) - full width by default
        self.button_grid = TwiddlerButtonGrid(size_hint_x=1.0)

        # Start with sensors hidden (button grid full width)
        self.top_section.add_widget(self.button_grid)

        # Chord display (current chord + output)
        self.chord_display = ChordDisplay()

        self.add_widget(self.top_section)
        self.add_widget(self.chord_display)

    def update(self, frame: TouchFrame):
        """Update display with new touch data"""
        self.touch_frame = frame

        # Check if GPIO driver (thumb_x == 0x1234) - no Trill sensors
        if frame.is_gpio_driver():
            # GPIO mode - hide touch sensors, keep button grid full width
            if self._sensors_visible:
                self._hide_sensors()

            # GPIO mode - use raw buttons for display
            diag = frame.get_gpio_diagnostics()
            raw_buttons = diag.get('raw_buttons', 0)
            self.button_grid.buttons = raw_buttons if raw_buttons else frame.buttons
            self.chord_display.update_from_buttons(frame.buttons, self.config)
        else:
            # Trill sensor mode - show sensors only if we have touch data
            has_touch = (frame.thumb_size > 0 or
                         any(t.size > 0 for t in frame.bar0) or
                         any(t.size > 0 for t in frame.bar1) or
                         any(t.size > 0 for t in frame.bar2))

            if has_touch:
                self._has_trill_data = True

            # Only show sensors if we've received Trill data at some point
            if self._has_trill_data and not self._sensors_visible:
                self._show_sensors()

            # Update sensor displays
            self.square.touch_x = frame.thumb_x
            self.square.touch_y = frame.thumb_y
            self.square.touch_size = frame.thumb_size

            self.bar0.touches = [(t.pos, t.size) for t in frame.bar0]
            self.bar1.touches = [(t.pos, t.size) for t in frame.bar1]
            self.bar2.touches = [(t.pos, t.size) for t in frame.bar2]

            # Buttons
            self.button_grid.buttons = frame.buttons
            self.chord_display.update_from_buttons(frame.buttons, self.config)

    def _hide_sensors(self):
        """Hide touch sensor panel"""
        if self._sensors_visible:
            self.top_section.remove_widget(self.sensor_panel)
            self.button_grid.size_hint_x = 1.0
            self._sensors_visible = False

    def _show_sensors(self):
        """Show touch sensor panel (nChorder with Trill sensors)"""
        if not self._sensors_visible:
            self.button_grid.size_hint_x = 0.5
            self.top_section.add_widget(self.sensor_panel, index=0)
            self._sensors_visible = True

    def load_config(self, config):
        """Load chord config for display and lookup"""
        self.config = config
        self.button_grid.config = config  # Enable chord hints on button grid
