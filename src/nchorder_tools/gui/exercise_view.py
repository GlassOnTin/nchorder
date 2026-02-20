"""
Exercise Mode View

Typing tutor for chord keyboards that:
- Groups chords by thumb button prefix
- Generates combinatorial practice sequences
- Detects chord input from physical Twiddler or QWERTY keyboard
- Tracks timing, WPM, and accuracy
"""

import random
import time

from nchorder_tools.wordlist import WORD_LIST

from kivy.uix.boxlayout import BoxLayout
from kivy.uix.label import Label
from kivy.uix.button import Button
from kivy.uix.spinner import Spinner
from kivy.uix.widget import Widget
from kivy.uix.popup import Popup
from kivy.graphics import Color, Rectangle, Line, RoundedRectangle
from kivy.properties import ObjectProperty, StringProperty, NumericProperty, BooleanProperty
from kivy.clock import Clock
from kivy.core.text import Label as CoreLabel
from kivy.core.window import Window
from kivy.metrics import dp


# Thumb button bit positions
THUMB_BITS = {'T1': 0, 'T2': 4, 'T3': 8, 'T4': 12, 'T0': 19}

# Finger row bit ranges (for grouping chords by row combination)
ROW_BITS = {
    'F1': [1, 2, 3],      # F1L, F1M, F1R
    'F2': [5, 6, 7],      # F2L, F2M, F2R
    'F3': [9, 10, 11],    # F3L, F3M, F3R
    'F4': [13, 14, 15],   # F4L, F4M, F4R
    'F0': [16, 17, 18],   # F0L, F0M, F0R
}

# Button bit positions for chord hint rendering
BTN_BITS = {
    'T1': 0, 'F1L': 1, 'F1M': 2, 'F1R': 3,
    'T2': 4, 'F2L': 5, 'F2M': 6, 'F2R': 7,
    'T3': 8, 'F3L': 9, 'F3M': 10, 'F3R': 11,
    'T4': 12, 'F4L': 13, 'F4M': 14, 'F4R': 15,
    'F0L': 16, 'F0M': 17, 'F0R': 18,
    # T0 (bit 19) intentionally excluded - only used for mouse click
}

# QWERTY keyboard -> chord button mapping for practice without hardware
# Layout mirrors the Twiddler grid onto a standard keyboard:
#   Number row:  1=T1  2=T2  3=T3  4=T4  5=T0
#   Left hand:   Q/W/E = F1   A/S/D = F2   Z/X/C = F3
#   Right hand:  U/I/O = F4   J/K/L = F0
QWERTY_TO_BTN = {
    # Thumbs (number row)
    49: 0,    # '1' -> T1 (bit 0)
    50: 4,    # '2' -> T2 (bit 4)
    51: 8,    # '3' -> T3 (bit 8)
    52: 12,   # '4' -> T4 (bit 12)
    53: 19,   # '5' -> T0 (bit 19)
    # F1 row - index finger (left hand top)
    113: 1,   # 'q' -> F1L (bit 1)
    119: 2,   # 'w' -> F1M (bit 2)
    101: 3,   # 'e' -> F1R (bit 3)
    # F2 row - middle finger (left hand home)
    97: 5,    # 'a' -> F2L (bit 5)
    115: 6,   # 's' -> F2M (bit 6)
    100: 7,   # 'd' -> F2R (bit 7)
    # F3 row - ring finger (left hand bottom)
    122: 9,   # 'z' -> F3L (bit 9)
    120: 10,  # 'x' -> F3M (bit 10)
    99: 11,   # 'c' -> F3R (bit 11)
    # F4 row - pinky (right hand)
    117: 13,  # 'u' -> F4L (bit 13)
    105: 14,  # 'i' -> F4M (bit 14)
    111: 15,  # 'o' -> F4R (bit 15)
    # F0 row (right hand home)
    106: 16,  # 'j' -> F0L (bit 16)
    107: 17,  # 'k' -> F0M (bit 17)
    108: 18,  # 'l' -> F0R (bit 18)
}

# Numpad -> chord button mapping (physical grid matches Twiddler layout)
# Requires NumLock ON. Both QWERTY and numpad mappings are active simultaneously.
#   Operator row:  / = T1   * = T2   - = T3   + = T4
#   Numpad grid:   7/8/9 = F1   4/5/6 = F2   1/2/3 = F3   0/./Enter = F4
NUMPAD_TO_BTN = {
    # Thumbs (operator row)
    267: 0,   # numpad / -> T1 (bit 0)
    268: 4,   # numpad * -> T2 (bit 4)
    269: 8,   # numpad - -> T3 (bit 8)
    270: 12,  # numpad + -> T4 (bit 12)
    300: 19,  # numlock  -> T0 (bit 19)
    # F1 row (top numpad row)
    263: 1,   # numpad 7 -> F1L (bit 1)
    264: 2,   # numpad 8 -> F1M (bit 2)
    265: 3,   # numpad 9 -> F1R (bit 3)
    # F2 row (middle numpad row)
    260: 5,   # numpad 4 -> F2L (bit 5)
    261: 6,   # numpad 5 -> F2M (bit 6)
    262: 7,   # numpad 6 -> F2R (bit 7)
    # F3 row (lower numpad row)
    257: 9,   # numpad 1 -> F3L (bit 9)
    258: 10,  # numpad 2 -> F3M (bit 10)
    259: 11,  # numpad 3 -> F3R (bit 11)
    # F4 row (bottom numpad row)
    256: 13,  # numpad 0 -> F4L (bit 13)
    266: 14,  # numpad . -> F4M (bit 14)
    271: 15,  # numpad enter -> F4R (bit 15)
}

# Combined mapping: both QWERTY and numpad active
KB_TO_BTN = {**QWERTY_TO_BTN, **NUMPAD_TO_BTN}

# Reverse map for display: bit -> key label
BTN_TO_KEY_LABEL = {v: k for k, v in {
    'T1': 0, 'T2': 4, 'T3': 8, 'T4': 12, 'T0': 19,
    'F1L': 1, 'F1M': 2, 'F1R': 3,
    'F2L': 5, 'F2M': 6, 'F2R': 7,
    'F3L': 9, 'F3M': 10, 'F3R': 11,
    'F4L': 13, 'F4M': 14, 'F4R': 15,
    'F0L': 16, 'F0M': 17, 'F0R': 18,
}.items()}

QWERTY_KEY_LABELS = {
    0: '1', 4: '2', 8: '3', 12: '4', 19: '5',
    1: 'Q', 2: 'W', 3: 'E',
    5: 'A', 6: 'S', 7: 'D',
    9: 'Z', 10: 'X', 11: 'C',
    13: 'U', 14: 'I', 15: 'O',
    16: 'J', 17: 'K', 18: 'L',
}

NUMPAD_KEY_LABELS = {
    0: '/', 4: '*', 8: '-', 12: '+', 19: 'NmLk',
    1: 'Nm7', 2: 'Nm8', 3: 'Nm9',
    5: 'Nm4', 6: 'Nm5', 7: 'Nm6',
    9: 'Nm1', 10: 'Nm2', 11: 'Nm3',
    13: 'Nm0', 14: 'Nm.', 15: 'NmEnt',
    16: 'J', 17: 'K', 18: 'L',  # F0 only on QWERTY
}

# Layout for chord hint widget (excluding T0)
# Thumb row uses 4 columns, finger rows use 3 columns centered
HINT_LAYOUT = [
    ['T1', 'T2', 'T3', 'T4'],      # Thumb row (4 buttons)
    ['F0L', 'F0M', 'F0R'],         # F0 row (3 buttons, centered)
    ['F1L', 'F1M', 'F1R'],         # F1 row
    ['F2L', 'F2M', 'F2R'],         # F2 row
    ['F3L', 'F3M', 'F3R'],         # F3 row
    ['F4L', 'F4M', 'F4R'],         # F4 row
]

class ChordHintWidget(Widget):
    """Small widget showing which buttons to press for a chord."""

    chord_mask = NumericProperty(0)
    pressed_mask = NumericProperty(0)  # For showing wrong buttons in red

    # Colors
    COLOR_BG = (0.12, 0.12, 0.14, 1)
    COLOR_BTN_OFF = (0.28, 0.28, 0.30, 1)
    COLOR_BTN_ON = (0.2, 0.7, 0.3, 1)  # Green for correct buttons
    COLOR_BTN_WRONG = (0.8, 0.2, 0.2, 1)  # Red for wrong buttons
    COLOR_SHADOW = (0.08, 0.08, 0.10, 1)
    COLOR_HIGHLIGHT = (0.45, 0.45, 0.48, 1)

    def __init__(self, chord_mask: int = 0, pressed_mask: int = 0, **kwargs):
        super().__init__(**kwargs)
        self.chord_mask = chord_mask
        self.pressed_mask = pressed_mask
        self.bind(pos=self._update, size=self._update, chord_mask=self._update, pressed_mask=self._update)
        self._update()

    def _draw_hexagon(self, cx, cy, radius, color, is_active=False, is_wrong=False):
        """Draw a hexagon centered at (cx, cy)."""
        import math
        points = []
        for i in range(6):
            angle = math.pi / 6 + i * math.pi / 3  # Start rotated for flat top
            px = cx + radius * math.cos(angle)
            py = cy + radius * math.sin(angle)
            points.extend([px, py])

        # Shadow (offset down-right)
        if is_active:
            Color(0.05, 0.05, 0.05, 0.8)
        else:
            Color(*self.COLOR_SHADOW)
        shadow_points = [p + 2 if i % 2 == 0 else p - 2 for i, p in enumerate(points)]
        Line(points=shadow_points + shadow_points[:2], width=1.5, close=True)

        # Fill
        Color(*color)
        from kivy.graphics import Mesh
        # Use triangle fan for filled hexagon
        vertices = []
        indices = []
        # Center vertex
        vertices.extend([cx, cy, 0, 0])
        for i in range(6):
            vertices.extend([points[i*2], points[i*2+1], 0, 0])
        for i in range(6):
            indices.extend([0, i+1, ((i+1) % 6) + 1])
        Mesh(vertices=vertices, indices=indices, mode='triangles')

        # Highlight (top edge)
        if is_active:
            if is_wrong:
                Color(1.0, 0.5, 0.5, 0.9)  # Light red highlight
            else:
                Color(0.5, 1.0, 0.6, 0.9)  # Green highlight
        else:
            Color(*self.COLOR_HIGHLIGHT)
        Line(points=points[:4], width=1.2)  # Top two edges

        # Border
        if is_active:
            if is_wrong:
                Color(0.6, 0.1, 0.1, 1)  # Dark red border
            else:
                Color(0.4, 1.0, 0.5, 1)  # Green border
            Line(points=points + points[:2], width=1.5, close=True)

    def _draw_circle(self, cx, cy, radius, color, is_active=False, is_wrong=False):
        """Draw a circle with shadow/highlight."""
        from kivy.graphics import Ellipse

        # Shadow
        if is_active:
            Color(0.05, 0.05, 0.05, 0.8)
        else:
            Color(*self.COLOR_SHADOW)
        Ellipse(pos=(cx - radius + 2, cy - radius - 2), size=(radius * 2, radius * 2))

        # Fill
        Color(*color)
        Ellipse(pos=(cx - radius, cy - radius), size=(radius * 2, radius * 2))

        # Highlight arc (top)
        if is_active:
            if is_wrong:
                Color(1.0, 0.5, 0.5, 0.9)  # Light red highlight
            else:
                Color(0.5, 1.0, 0.6, 0.9)  # Green highlight
        else:
            Color(*self.COLOR_HIGHLIGHT)
        Line(circle=(cx, cy, radius, 45, 135), width=1.2)

        # Border for active
        if is_active:
            if is_wrong:
                Color(0.6, 0.1, 0.1, 1)  # Dark red border
            else:
                Color(0.4, 1.0, 0.5, 1)  # Green border
            Line(circle=(cx, cy, radius), width=1.5)

    def _draw_rounded_rect(self, x, y, w, h, color, is_active=False, is_wrong=False):
        """Draw a rounded rectangle with shadow/highlight."""
        radius = 4

        # Shadow
        if is_active:
            Color(0.05, 0.05, 0.05, 0.8)
        else:
            Color(*self.COLOR_SHADOW)
        RoundedRectangle(pos=(x + 2, y - 2), size=(w, h), radius=[radius])

        # Fill
        Color(*color)
        RoundedRectangle(pos=(x, y), size=(w, h), radius=[radius])

        # Highlight (top edge)
        if is_active:
            if is_wrong:
                Color(1.0, 0.5, 0.5, 0.9)  # Light red highlight
            else:
                Color(0.5, 1.0, 0.6, 0.9)  # Green highlight
        else:
            Color(*self.COLOR_HIGHLIGHT)
        Line(points=[x + radius, y + h, x + w - radius, y + h], width=1.2)

        # Border for active
        if is_active:
            if is_wrong:
                Color(0.6, 0.1, 0.1, 1)  # Dark red border
            else:
                Color(0.4, 1.0, 0.5, 1)  # Green border
            Line(rounded_rectangle=(x, y, w, h, radius), width=1.5)

    def _update(self, *args):
        self.canvas.clear()

        rows = len(HINT_LAYOUT)
        cell_h = self.height / rows
        margin = 3

        with self.canvas:
            # Background
            Color(*self.COLOR_BG)
            Rectangle(pos=self.pos, size=self.size)

            # Draw each button
            for row_idx, row in enumerate(HINT_LAYOUT):
                num_cols = len(row)
                cell_w = self.width / num_cols

                for col_idx, btn_name in enumerate(row):
                    if not btn_name:
                        continue

                    # All rows centered on widget center
                    cx = self.x + (col_idx + 0.5) * cell_w
                    cy = self.top - (row_idx + 0.5) * cell_h

                    # Check if this button is in the expected chord or was wrongly pressed
                    bit = BTN_BITS.get(btn_name, -1)
                    is_expected = bit >= 0 and (self.chord_mask & (1 << bit))
                    is_pressed = bit >= 0 and (self.pressed_mask & (1 << bit))
                    is_wrong = is_pressed and not is_expected

                    # Color: green for correct, red for wrong, gray for inactive
                    if is_expected:
                        color = self.COLOR_BTN_ON
                        is_active = True
                    elif is_wrong:
                        color = self.COLOR_BTN_WRONG
                        is_active = True
                    else:
                        color = self.COLOR_BTN_OFF
                        is_active = False

                    # Thumb buttons (T1-T4) = hexagons
                    if btn_name.startswith('T'):
                        radius = min(cell_w, cell_h) * 0.38
                        self._draw_hexagon(cx, cy, radius, color, is_active, is_wrong)
                    # F0 row = circles
                    elif btn_name.startswith('F0'):
                        radius = min(cell_w, cell_h) * 0.28
                        self._draw_circle(cx, cy, radius, color, is_active, is_wrong)
                    # Other finger buttons = square rounded rectangles
                    else:
                        size = min(cell_w, cell_h) - margin * 2
                        x = cx - size / 2
                        y = cy - size / 2
                        self._draw_rounded_rect(x, y, size, size, color, is_active, is_wrong)


# Lesson definitions: (name, description, filter_func)
# Ordered from easiest to hardest
LESSONS = [
    ('F1+F2', 'Row 1 + Row 2 (9 letters)', lambda rows: rows == {'F1', 'F2'}),
    ('F1+F3', 'Row 1 + Row 3', lambda rows: rows == {'F1', 'F3'}),
    ('F2+F3', 'Row 2 + Row 3', lambda rows: rows == {'F2', 'F3'}),
    ('F1+F4', 'Row 1 + Row 4', lambda rows: rows == {'F1', 'F4'}),
    ('F2+F4', 'Row 2 + Row 4', lambda rows: rows == {'F2', 'F4'}),
    ('F3+F4', 'Row 3 + Row 4', lambda rows: rows == {'F3', 'F4'}),
    ('3-finger', 'Three finger chords', lambda rows: len(rows) == 3 and 'T1' not in rows),
    ('Num+*', 'Number row (T1 thumb)', lambda rows: 'T1' in rows and len(rows) == 2),
    ('All 2-btn', 'All two-button chords', lambda rows: True),  # filtered by button count
]


class ExerciseDisplay(Widget):
    """Shows scrolling target text with typed progress - cursor stays centered"""

    target_text = StringProperty('')
    typed_text = StringProperty('')
    cursor_pos = NumericProperty(0)
    visible_chars = NumericProperty(30)  # How many chars to show in the window

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.bind(
            pos=self._update_canvas,
            size=self._update_canvas,
            target_text=self._update_canvas,
            typed_text=self._update_canvas,
            cursor_pos=self._update_canvas
        )
        self._update_canvas()

    def _update_canvas(self, *args):
        self.canvas.clear()
        with self.canvas:
            # Background
            Color(0.12, 0.12, 0.12)
            Rectangle(pos=self.pos, size=self.size)

            # Border
            Color(0.3, 0.3, 0.3)
            Line(rectangle=(*self.pos, *self.size), width=1)

        # Clear after canvas for text
        self.canvas.after.clear()

        if not self.target_text:
            # Show placeholder - use dp() for proper scaling
            with self.canvas.after:
                label = CoreLabel(text='Press Start to begin', font_size=dp(28))
                label.refresh()
                texture = label.texture
                Color(0.5, 0.5, 0.5, 1)
                Rectangle(
                    texture=texture,
                    pos=(self.center_x - texture.width / 2, self.center_y - texture.height / 2),
                    size=texture.size
                )
            return

        # Draw scrolling text display - use dp() for scaling
        with self.canvas.after:
            font_size = dp(28)
            char_width = font_size * 0.65  # Monospace width

            # Calculate visible window - cursor at 1/4 from left for look-ahead
            cursor_screen_pos = 0.25  # Cursor position as fraction of width
            chars_before = int(self.visible_chars * cursor_screen_pos)
            chars_after = self.visible_chars - chars_before

            # Window start/end in the full text
            window_start = max(0, self.cursor_pos - chars_before)
            window_end = min(len(self.target_text), self.cursor_pos + chars_after)

            # Calculate starting X position
            # The cursor char should be at cursor_screen_pos of the widget width
            cursor_x = self.x + self.width * cursor_screen_pos
            start_x = cursor_x - (self.cursor_pos - window_start) * char_width

            # Draw characters in the visible window
            for i in range(window_start, window_end):
                char = self.target_text[i]
                x = start_x + (i - window_start) * char_width

                # Skip if outside widget bounds
                if x < self.x - char_width or x > self.right + char_width:
                    continue

                # Color based on position relative to cursor
                if i < self.cursor_pos:
                    # Already typed - check if correct
                    if i < len(self.typed_text) and self.typed_text[i] == char:
                        Color(0.3, 0.7, 0.3, 0.6)  # Faded green - correct
                    else:
                        Color(0.7, 0.3, 0.3, 0.6)  # Faded red - incorrect
                elif i == self.cursor_pos:
                    Color(1.0, 1.0, 0.3, 1)  # Bright yellow - current
                else:
                    # Upcoming - fade based on distance
                    distance = i - self.cursor_pos
                    fade = max(0.4, 1.0 - distance * 0.03)
                    Color(0.8, 0.8, 0.8, fade)  # White/gray - upcoming

                is_current = (i == self.cursor_pos)
                label = CoreLabel(text=char, font_size=font_size if not is_current else font_size + 4, bold=is_current)
                label.refresh()

                # Center vertically, with current char slightly raised
                y_offset = 4 if is_current else 0
                Rectangle(
                    texture=label.texture,
                    pos=(x - label.texture.width / 2, self.center_y - label.texture.height / 2 + y_offset),
                    size=label.texture.size
                )

            # Draw cursor underline
            Color(1.0, 1.0, 0.3, 1)
            cursor_char_x = cursor_x - char_width / 2
            Line(points=[cursor_char_x, self.center_y - font_size / 2 - 5,
                        cursor_char_x + char_width, self.center_y - font_size / 2 - 5], width=2)



class ExerciseStats(BoxLayout):
    """Timer, WPM, accuracy display"""

    elapsed_time = NumericProperty(0)
    wpm = NumericProperty(0)
    accuracy = NumericProperty(100)
    progress_current = NumericProperty(0)
    progress_total = NumericProperty(0)

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.orientation = 'horizontal'
        self.size_hint_y = None
        self.height = dp(48)
        self.padding = dp(8)
        self.spacing = dp(10)

        # Time display
        self.time_label = Label(text='Time: 00:00', size_hint_x=0.25, halign='left', font_size='16sp')
        self.time_label.bind(size=self.time_label.setter('text_size'))

        # WPM display
        self.wpm_label = Label(text='WPM: 0', size_hint_x=0.25, halign='center', font_size='16sp')

        # Accuracy display
        self.accuracy_label = Label(text='Accuracy: 100%', size_hint_x=0.25, halign='center', font_size='16sp')

        # Progress display
        self.progress_label = Label(text='Progress: 0/0', size_hint_x=0.25, halign='right', font_size='16sp')
        self.progress_label.bind(size=self.progress_label.setter('text_size'))

        self.add_widget(self.time_label)
        self.add_widget(self.wpm_label)
        self.add_widget(self.accuracy_label)
        self.add_widget(self.progress_label)

        self.bind(
            elapsed_time=self._update_labels,
            wpm=self._update_labels,
            accuracy=self._update_labels,
            progress_current=self._update_labels,
            progress_total=self._update_labels
        )

    def _update_labels(self, *args):
        mins = int(self.elapsed_time) // 60
        secs = int(self.elapsed_time) % 60
        self.time_label.text = f'Time: {mins:02d}:{secs:02d}'
        self.wpm_label.text = f'WPM: {self.wpm:.0f}'
        self.accuracy_label.text = f'Accuracy: {self.accuracy:.0f}%'
        self.progress_label.text = f'Progress: {self.progress_current}/{self.progress_total}'


class ExerciseView(BoxLayout):
    """Main exercise view with chord group selection and typing practice"""

    config = ObjectProperty(None, allownone=True)
    device = ObjectProperty(None, allownone=True)

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.orientation = 'vertical'
        self.padding = dp(8)
        self.spacing = dp(8)

        # State
        self._chord_groups = {}
        self._target_text = ''
        self._typed_text = ''
        self._cursor_pos = 0
        self._start_time = 0
        self._is_running = False
        self._correct_count = 0
        self._total_typed = 0
        self._prev_buttons = 0  # Track button state for chord detection
        self._timer_event = None
        self._hint_popup = None  # Track current hint popup
        self._hint_min_time = 0  # Minimum time hint should stay open
        self._always_show_hint = False  # Always show chord hint mode
        self._last_pressed_chord = 0  # Track last pressed chord for showing wrong buttons
        self._current_chars = []  # Current subset of characters being practiced
        self._all_lesson_chars = []  # All characters available in current lesson
        self._permutation_queue = []  # Queue of 3-letter combinations to practice
        self._completed_count = 0  # Total combinations completed across all rounds
        self._missed_char = None  # Character that was missed, to reinforce in next target
        self._fixed_row = None  # Which row is fixed (e.g., 'F1')
        self._fixed_col = None  # Which column of fixed row (e.g., 'L', 'M', 'R')
        self._varying_row = None  # Which row varies
        self._kb_mode = False  # QWERTY keyboard chord input mode
        self._kb_buttons = 0   # Current keyboard-simulated button bitmask
        self._all_learned_chars = set()  # Accumulated learned chars across combos
        self._word_round_pending = False  # Word round queued after combo mastery
        self._is_word_round = False  # Currently in a word practice round

        # Toolbar
        toolbar = BoxLayout(orientation='horizontal', size_hint_y=None, height=dp(48), spacing=dp(8))

        # Lesson selector
        lesson_names = [f'{name}' for name, desc, _ in LESSONS]
        self.group_spinner = Spinner(
            text=lesson_names[0] if lesson_names else 'F1+F2',
            values=lesson_names,
            size_hint_x=0.25
        )
        self.group_spinner.bind(text=self._on_lesson_changed)

        # Characters dropdown - shows available char subsets for this lesson
        self.chars_spinner = Spinner(
            text='(select lesson)',
            values=[],
            size_hint_x=0.35
        )
        self.chars_spinner.bind(text=self._on_chars_changed)

        # Hidden subset_spinner for compatibility with progression code
        self.subset_spinner = Spinner(text='2', values=['2', '3', '4', '5', '6'])

        # Show Hints toggle
        from kivy.uix.togglebutton import ToggleButton
        self.hint_toggle = ToggleButton(text='Hints', size_hint_x=0.12)
        self.hint_toggle.bind(state=self._on_hint_toggle)

        # QWERTY keyboard input toggle
        self.kb_toggle = ToggleButton(text='QWERTY', size_hint_x=0.15)
        self.kb_toggle.bind(state=self._on_kb_toggle)

        toolbar.add_widget(self.group_spinner)
        toolbar.add_widget(self.chars_spinner)
        toolbar.add_widget(self.hint_toggle)
        toolbar.add_widget(self.kb_toggle)
        self.add_widget(toolbar)

        # Status label
        self.status_label = Label(
            text='Load a chord config to begin',
            size_hint_y=None,
            height=dp(36),
            font_size='18sp',
            color=(0.7, 0.7, 0.7, 1)
        )
        self.add_widget(self.status_label)

        # Exercise display (main area)
        self.display = ExerciseDisplay()
        self.add_widget(self.display)

        # Stats bar
        self.stats = ExerciseStats()
        self.add_widget(self.stats)

    def load_config(self, config):
        """Load chord config and group by lesson categories"""
        self.config = config
        self._group_chords_by_lesson()
        self._update_chars_spinner()

        self.status_label.text = f'{len(config.entries)} chords loaded — select chars to begin'

    def _get_rows_for_chord(self, mask: int) -> set:
        """Get which rows/thumbs are used in a chord."""
        rows = set()
        for row_name, bits in ROW_BITS.items():
            if any(mask & (1 << bit) for bit in bits):
                rows.add(row_name)
        for thumb_name, bit in THUMB_BITS.items():
            if mask & (1 << bit):
                rows.add(thumb_name)
        return rows

    def _group_chords_by_lesson(self):
        """Pre-compute chord groups for each lesson."""
        self._lesson_entries = {}

        if not self.config:
            return

        for lesson_name, desc, filter_func in LESSONS:
            entries = []
            for entry in self.config.entries:
                if not entry.is_keyboard:
                    continue
                rows = self._get_rows_for_chord(entry.chord_mask)
                # Special handling for button count filters
                btn_count = bin(entry.chord_mask).count('1')
                if lesson_name == 'All 2-btn':
                    if btn_count == 2:
                        entries.append(entry)
                elif lesson_name == '3-finger':
                    if btn_count == 3 and filter_func(rows):
                        entries.append(entry)
                elif filter_func(rows):
                    entries.append(entry)
            self._lesson_entries[lesson_name] = entries

    def _get_lesson_entries(self, lesson_name: str) -> list:
        """Get chord entries for a lesson."""
        return self._lesson_entries.get(lesson_name, [])

    def _get_printable_chars(self, entries: list) -> list:
        """Get single-character outputs from entries."""
        chars = []
        for entry in entries:
            key = entry.key_str()
            if len(key) == 1:
                chars.append(key)
        return chars

    def _get_rows_in_lesson(self, lesson_name: str) -> list:
        """Get which finger rows are used in a lesson (e.g., ['F1', 'F3'] for 'F1+F3')."""
        rows = []
        for row in ['F1', 'F2', 'F3', 'F4']:
            if row in lesson_name:
                rows.append(row)
        return rows

    def _get_button_bit(self, row: str, col: str) -> int:
        """Get bit position for a specific button (e.g., 'F1', 'L' -> bit for F1L)."""
        btn_name = f'{row}{col}'
        return BTN_BITS.get(btn_name, -1)

    def _filter_entries_by_fixed_button(self, entries: list, fixed_row: str, fixed_col: str, varying_row: str) -> list:
        """Filter entries to only those using the fixed button + any button from varying row."""
        fixed_bit = self._get_button_bit(fixed_row, fixed_col)
        if fixed_bit < 0:
            return entries

        # Get all bits for the varying row
        varying_bits = ROW_BITS.get(varying_row, [])

        filtered = []
        for entry in entries:
            mask = entry.chord_mask
            # Must have the fixed button pressed
            if not (mask & (1 << fixed_bit)):
                continue
            # Must have exactly one button from varying row
            varying_pressed = [b for b in varying_bits if mask & (1 << b)]
            if len(varying_pressed) == 1:
                filtered.append(entry)
        return filtered

    def _generate_permutations(self, chars: list, combo_length: int = 3) -> list:
        """Generate all permutations with repetition of given length."""
        from itertools import product
        return [''.join(p) for p in product(chars, repeat=combo_length)]

    def _get_words_for_chars(self, chars: set) -> list:
        """Filter WORD_LIST to words using only the given character set."""
        return [w for w in WORD_LIST if set(w) <= chars]

    def _generate_word_round(self, chars: set) -> str:
        """Generate a word practice round from learned chars.

        Returns a string of 10-15 words joined by space (if space is learned)
        or concatenated. Returns empty string if fewer than 5 words available.
        """
        words = self._get_words_for_chars(chars)
        if len(words) < 5:
            return ''
        count = min(random.randint(10, 15), len(words))
        selected = random.sample(words, count)
        separator = ' ' if ' ' in chars else ''
        return separator.join(selected)

    def _get_next_target(self) -> str:
        """Get the next target string from the permutation queue."""
        if not self._current_chars:
            return ''

        if not self._permutation_queue:
            # Regenerate and shuffle
            perms = self._generate_permutations(self._current_chars)
            random.shuffle(perms)
            self._permutation_queue = perms

        if self._permutation_queue:
            return self._permutation_queue.pop(0)
        return ''

    def _on_lesson_changed(self, spinner, text):
        """Lesson spinner changed - rebuild chars dropdown"""
        self._all_learned_chars = set()
        self._update_chars_spinner()
        if self._is_running:
            self._reset_exercise()

    def _on_chars_changed(self, spinner, text):
        """Chars spinner changed - prepare exercise (auto-starts on first chord)"""
        if not text or text.startswith('('):
            return
        if self._is_running:
            self._reset_exercise()
        self._prepare_exercise_from_chars(text)

    def _update_chars_spinner(self):
        """Rebuild chars dropdown for current lesson."""
        if not self.config:
            return

        lesson_name = self.group_spinner.text
        entries = self._get_lesson_entries(lesson_name)
        rows_in_lesson = self._get_rows_in_lesson(lesson_name)

        options = []
        if len(rows_in_lesson) == 2:
            # Generate options for each fixed row+column combination
            for fixed_row in rows_in_lesson:
                varying_row = [r for r in rows_in_lesson if r != fixed_row][0]
                for col in ['L', 'M', 'R']:
                    filtered = self._filter_entries_by_fixed_button(
                        entries, fixed_row, col, varying_row
                    )
                    chars = self._get_printable_chars(filtered)
                    if chars:
                        chars_str = ''.join(chars)
                        label = f"{fixed_row}{col}+{varying_row}: {chars_str}"
                        options.append(label)
        else:
            # Non-2-row lessons: just show all chars
            chars = self._get_printable_chars(entries)
            if chars:
                options.append(f"All: {''.join(chars)}")

        self.chars_spinner.values = options
        if options:
            self.chars_spinner.text = options[0]
        else:
            self.chars_spinner.text = '(no chars)'

    def _parse_chars_selection(self, text: str):
        """Parse chars spinner text to extract fixed row/col/varying and chars.
        Format: 'F1L+F2: etl' or 'All: etlnrs'"""
        self._fixed_row = None
        self._fixed_col = None
        self._varying_row = None

        if text.startswith('All:'):
            chars_str = text.split(': ', 1)[1] if ': ' in text else ''
            return list(chars_str)

        # Parse 'F1L+F2: etl'
        if '+' in text and ': ' in text:
            parts = text.split(': ', 1)
            btn_spec = parts[0]  # 'F1L+F2'
            chars_str = parts[1]  # 'etl'

            fixed_part, varying_part = btn_spec.split('+', 1)
            self._fixed_row = fixed_part[:2]  # 'F1'
            self._fixed_col = fixed_part[2:]  # 'L'
            self._varying_row = varying_part  # 'F2'
            return list(chars_str)

        return []

    def _prepare_exercise_from_chars(self, chars_text: str):
        """Prepare exercise from chars spinner selection (doesn't start timer yet)."""
        all_chars = self._parse_chars_selection(chars_text)
        if not all_chars:
            return

        # Start with 2 chars, store full pool for progression
        subset_size = min(2, len(all_chars))
        self._all_lesson_chars = all_chars
        self._current_chars = all_chars[:subset_size]
        self.subset_spinner.text = str(subset_size)

        # Generate exercise text
        perms = self._generate_permutations(self._current_chars)
        random.shuffle(perms)
        self._target_text = ''.join(perms)
        self._total_chars = len(self._target_text)
        self._completed_count = 0

        # Reset state but don't start timer yet
        self._typed_text = ''
        self._cursor_pos = 0
        self._correct_count = 0
        self._total_typed = 0
        self._prev_buttons = 0
        self._is_running = False
        self._word_round_pending = False
        self._is_word_round = False

        # Show target text so user can see what to type
        self.display.target_text = self._target_text
        self.display.typed_text = ''
        self.display.cursor_pos = 0

        self.stats.progress_total = self._total_chars
        self.stats.progress_current = 0
        self.stats.elapsed_time = 0
        self.stats.wpm = 0
        self.stats.accuracy = 100

        chars_str = ''.join(self._current_chars)
        self.status_label.text = f"Ready: {chars_str} — chord any key to begin"

        if self._always_show_hint and self._target_text:
            self._show_chord_hint(self._target_text[0])

    def _auto_start(self):
        """Auto-start the exercise timer on first input."""
        if self._is_running:
            return
        if not self._target_text:
            return
        self._start_time = time.time()
        self._is_running = True
        self._timer_event = Clock.schedule_interval(self._update_timer, 0.1)
        chars_str = ''.join(self._current_chars)
        self.status_label.text = f"Practicing: {chars_str}"

    def _start_exercise(self):
        """Start a new exercise from current chars spinner selection."""
        if not self.config:
            self.status_label.text = 'Load a chord config first'
            return

        chars_text = self.chars_spinner.text
        if not chars_text or chars_text.startswith('('):
            self.status_label.text = 'Select a character set'
            return

        self._prepare_exercise_from_chars(chars_text)

    def _reset_exercise(self):
        """Stop and reset exercise state"""
        self._is_running = False

        if self._timer_event:
            self._timer_event.cancel()
            self._timer_event = None

        self._target_text = ''
        self._typed_text = ''
        self._cursor_pos = 0

        self.display.target_text = ''
        self.display.typed_text = ''
        self.display.cursor_pos = 0

        self.stats.elapsed_time = 0
        self.stats.wpm = 0
        self.stats.accuracy = 100
        self.stats.progress_current = 0
        self.stats.progress_total = 0

        self.status_label.text = 'Ready'

        # Close hint popup if open
        if self._hint_popup:
            self._hint_popup.dismiss()
            self._hint_popup = None

        # Reset keyboard chord state
        self._kb_buttons = 0

        # Reset word round state
        self._word_round_pending = False
        self._is_word_round = False

    def _on_hint_toggle(self, instance, state):
        """Toggle always-show-hint mode"""
        self._always_show_hint = (state == 'down')

        if self._always_show_hint and self._target_text:
            # Show hint for current target
            if self._cursor_pos < len(self._target_text):
                self._show_chord_hint(self._target_text[self._cursor_pos])
        elif not self._always_show_hint and self._hint_popup:
            # Hide hint when turning off
            self._hint_popup.dismiss()
            self._hint_popup = None

    def _on_kb_toggle(self, instance, state):
        """Toggle QWERTY keyboard chord input mode"""
        self._kb_mode = (state == 'down')
        self._kb_buttons = 0

        if self._kb_mode:
            Window.bind(on_key_down=self._on_kb_key_down)
            Window.bind(on_key_up=self._on_kb_key_up)
            self.status_label.text = (
                'Keyboard: QWE/ASD/ZXC=F1-F3  UIO=F4 | Numpad: 789/456/123=F1-F3  0.Enter=F4'
            )
        else:
            Window.unbind(on_key_down=self._on_kb_key_down)
            Window.unbind(on_key_up=self._on_kb_key_up)
            if self._target_text:
                chars_str = ''.join(self._current_chars)
                self.status_label.text = f"Practicing: {chars_str}"

    def _on_kb_key_down(self, window, key, scancode, codepoint, modifiers):
        """Handle QWERTY key press -> update chord button bitmask"""
        if key in KB_TO_BTN:
            bit = KB_TO_BTN[key]
            self._kb_buttons |= (1 << bit)
            self.on_chord_event(self._kb_buttons)
            return True  # Consume the event

    def _on_kb_key_up(self, window, key, scancode):
        """Handle QWERTY/numpad key release -> update chord button bitmask"""
        if key in KB_TO_BTN:
            bit = KB_TO_BTN[key]
            self._kb_buttons &= ~(1 << bit)
            self.on_chord_event(self._kb_buttons)
            return True  # Consume the event

    def _update_timer(self, dt):
        """Update timer and stats"""
        if not self._is_running:
            return

        elapsed = time.time() - self._start_time
        self.stats.elapsed_time = elapsed

        # Calculate WPM (5 chars per word)
        if elapsed > 0 and self._total_typed > 0:
            self.stats.wpm = (self._total_typed / 5) / (elapsed / 60)

        # Calculate accuracy
        if self._total_typed > 0:
            self.stats.accuracy = (self._correct_count / self._total_typed) * 100

    def on_chord_event(self, buttons: int):
        """Handle chord event from CDC stream.

        Called when button state changes. Detects chord release
        (any button released) to capture the typed character.
        MirrorWalk triggers on first release, not when all buttons released.
        """
        # Dismiss hint popup when all buttons released (unless always-show mode or min time not elapsed)
        if self._hint_popup and buttons == 0 and not self._always_show_hint:
            if time.time() >= self._hint_min_time:
                self._hint_popup.dismiss()
                self._hint_popup = None

        # Detect any button release (button count decreased)
        prev_count = bin(self._prev_buttons).count('1')
        curr_count = bin(buttons).count('1')

        if self._prev_buttons != 0 and curr_count < prev_count:
            # A button was released - use the previous state as the chord
            chord_mask = self._prev_buttons
            self._last_pressed_chord = chord_mask

            if self.config:
                entry = self.config.find_chord(chord_mask)
                if entry and entry.is_keyboard:
                    char = entry.key_str()
                    if len(char) == 1:
                        self._handle_typed_char(char, chord_mask)

        self._prev_buttons = buttons

    def _handle_typed_char(self, char: str, pressed_chord: int = 0):
        """Process a typed character.

        Args:
            char: The character that was typed
            pressed_chord: The chord mask that was pressed (for showing wrong buttons)
        """
        # Auto-start on first input if exercise is prepared but not running
        if not self._is_running:
            if self._target_text:
                self._auto_start()
            else:
                return

        if self._cursor_pos >= len(self._target_text):
            return

        # Check if correct
        expected = self._target_text[self._cursor_pos]
        is_correct = (char == expected)

        # Update stats
        self._total_typed += 1

        if is_correct:
            self._correct_count += 1

            # Clear any pending incorrect chars first (keep only green ones)
            self._typed_text = self._typed_text[:self._cursor_pos]

            # Add correct char and advance cursor
            self._typed_text += char
            self._cursor_pos += 1

            # Update display
            self.display.typed_text = self._typed_text
            self.display.cursor_pos = self._cursor_pos
            self.stats.progress_current = self._cursor_pos

            # Check if exercise complete
            if self._cursor_pos >= len(self._target_text):
                self._complete_exercise()
            elif self._always_show_hint:
                # Update hint for next target character
                self._show_chord_hint(self._target_text[self._cursor_pos])
        else:
            # Clear any previous incorrect chars, show only this one
            self._typed_text = self._typed_text[:self._cursor_pos] + char
            self.display.typed_text = self._typed_text
            self.display.cursor_pos = self._cursor_pos

            # Remember missed char to reinforce in next target
            self._missed_char = expected

            # Show chord hint popup for the expected character (with wrong buttons in red)
            self._show_chord_hint(expected, pressed_chord)

            # Schedule removal after 2 seconds
            Clock.schedule_once(self._remove_incorrect_char, 2.0)

    def _show_chord_hint(self, expected_char: str, pressed_mask: int = 0):
        """Show popup with the correct chord pattern for a character.

        Args:
            expected_char: The character the user should have typed
            pressed_mask: The chord mask that was actually pressed (for showing wrong buttons in red)
        """
        # Find the chord entry for this character
        chord_mask = 0
        if self.config:
            for entry in self.config.entries:
                if entry.is_keyboard and entry.key_str() == expected_char:
                    chord_mask = entry.chord_mask
                    break

        # Create popup content
        content = BoxLayout(orientation='vertical', padding=10, spacing=5)

        # Character label at top
        char_label = Label(
            text=f'[b]{expected_char}[/b]',
            markup=True,
            font_size='36sp',
            size_hint_y=0.25
        )
        content.add_widget(char_label)

        # Chord button diagram (shows expected in green, wrong presses in red)
        chord_widget = ChordHintWidget(chord_mask=chord_mask, pressed_mask=pressed_mask, size_hint_y=0.65)
        content.add_widget(chord_widget)

        # Show key labels when in keyboard mode
        if self._kb_mode and chord_mask:
            qwerty_keys = []
            numpad_keys = []
            for bit in range(20):
                if chord_mask & (1 << bit):
                    qk = QWERTY_KEY_LABELS.get(bit)
                    nk = NUMPAD_KEY_LABELS.get(bit)
                    if qk:
                        qwerty_keys.append(qk)
                    if nk:
                        numpad_keys.append(nk)
            hint_parts = []
            if qwerty_keys:
                hint_parts.append(' + '.join(qwerty_keys))
            if numpad_keys:
                hint_parts.append(' + '.join(numpad_keys))
            if hint_parts:
                keys_label = Label(
                    text='  or  '.join(hint_parts),
                    font_size='17sp',
                    color=(0.9, 0.9, 0.5, 1),
                    size_hint_y=0.1
                )
                content.add_widget(keys_label)

        # Dismiss hint
        hint_label = Label(
            text='(tap to dismiss)',
            font_size='14sp',
            color=(0.5, 0.5, 0.5, 1),
            size_hint_y=0.1
        )
        content.add_widget(hint_label)

        # Dismiss any existing popup
        if self._hint_popup:
            self._hint_popup.dismiss()

        self._hint_popup = Popup(
            title='',
            content=content,
            size_hint=(0.35, 0.45),
            auto_dismiss=True,
            separator_height=0
        )
        self._hint_popup.open()
        # Popup stays open for minimum 1 second, then until all buttons released
        self._hint_min_time = time.time() + 1.0

    def _remove_incorrect_char(self, dt):
        """Remove the last incorrect character after delay"""
        if not self._is_running:
            return

        if self._typed_text and len(self._typed_text) > self._cursor_pos:
            # Remove the incorrect character(s) beyond cursor
            self._typed_text = self._typed_text[:self._cursor_pos]
            self.display.typed_text = self._typed_text

    def _complete_exercise(self):
        """Handle completion of full exercise - advance or regenerate"""
        self._completed_count += 1
        accuracy = self.stats.accuracy

        # Finishing a word round — don't try to advance (already advanced before word round)
        was_word_round = self._is_word_round
        if self._is_word_round:
            self._is_word_round = False
            if accuracy < 80:
                # Repeat word round on low accuracy
                self._word_round_pending = True
            # Fall through to generate next permutation round (or another word round)

        advanced = False
        if not was_word_round and accuracy >= 90:
            advanced = self._try_advance()

        # Intercept: serve a word round if one is pending
        if self._word_round_pending:
            word_text = self._generate_word_round(self._all_learned_chars)
            if word_text:
                self._word_round_pending = False
                self._is_word_round = True
                self._target_text = word_text
                self._total_chars = len(self._target_text)
                self._typed_text = ''
                self._cursor_pos = 0
                self._correct_count = 0
                self._total_typed = 0

                self.display.target_text = self._target_text
                self.display.typed_text = ''
                self.display.cursor_pos = 0

                n_chars = len(self._all_learned_chars)
                self.status_label.text = f"Word practice! ({n_chars} chars learned)"

                self.stats.progress_current = 0
                self.stats.progress_total = self._total_chars
                self.stats.accuracy = 100

                if self._always_show_hint and self._target_text:
                    self._show_chord_hint(self._target_text[0])
                return

        # Generate new shuffled set for next round
        perms = self._generate_permutations(self._current_chars)
        random.shuffle(perms)

        # If there was a missed char, put combos starting with it first
        if self._missed_char:
            missed_perms = [p for p in perms if p.startswith(self._missed_char)]
            other_perms = [p for p in perms if not p.startswith(self._missed_char)]
            perms = missed_perms + other_perms
            self._missed_char = None

        self._target_text = ''.join(perms)
        self._total_chars = len(self._target_text)
        self._typed_text = ''
        self._cursor_pos = 0
        self._correct_count = 0
        self._total_typed = 0

        # Update display
        self.display.target_text = self._target_text
        self.display.typed_text = ''
        self.display.cursor_pos = 0

        # Update status
        chars_str = ''.join(self._current_chars)
        if advanced:
            status = f"Added '{self._current_chars[-1]}' | Chars: {chars_str}"
        elif accuracy < 90:
            status = f"Round {self._completed_count + 1} (repeat, {accuracy:.0f}% accuracy) | Chars: {chars_str}"
        else:
            status = f"Round {self._completed_count + 1} | Chars: {chars_str}"

        if self._fixed_row:
            fixed_btn = f'{self._fixed_row}{self._fixed_col}'
            status += f" | Fixed: {fixed_btn}"

        self.status_label.text = status

        # Reset progress for new round
        self.stats.progress_current = 0
        self.stats.progress_total = self._total_chars
        self.stats.accuracy = 100

        # Show hint for new target if in always-show mode
        if self._always_show_hint and self._target_text:
            self._show_chord_hint(self._target_text[0])

    def _try_advance(self) -> bool:
        """Try to add the next character or advance to next lesson.
        Returns True if advancement happened."""
        # Add next char from current lesson pool
        current_count = len(self._current_chars)
        if current_count < len(self._all_lesson_chars):
            self._current_chars = self._all_lesson_chars[:current_count + 1]
            self.subset_spinner.text = str(len(self._current_chars))
            return True

        # All chars in current combo mastered - accumulate and check for word round
        self._all_learned_chars.update(self._current_chars)
        word_text = self._generate_word_round(self._all_learned_chars)
        if word_text:
            self._word_round_pending = True

        # Move to next combo in dropdown
        values = self.chars_spinner.values
        if values:
            current_idx = values.index(self.chars_spinner.text) if self.chars_spinner.text in values else -1
            if current_idx + 1 < len(values):
                # Move to next char combo in same lesson
                self.chars_spinner.text = values[current_idx + 1]
                return True

        # All combos in lesson mastered - advance to next lesson
        current_lesson = self.group_spinner.text
        lesson_names = [name for name, _, _ in LESSONS]
        if current_lesson in lesson_names:
            idx = lesson_names.index(current_lesson)
            if idx + 1 < len(lesson_names):
                self.group_spinner.text = lesson_names[idx + 1]
                # _on_lesson_changed will update chars spinner
                # Start with first option
                return True

        return False
