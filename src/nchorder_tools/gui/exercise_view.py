"""
Exercise Mode View

Typing tutor for chord keyboards that:
- Groups chords by thumb button prefix
- Generates combinatorial practice sequences
- Detects chord input from physical Twiddler
- Tracks timing, WPM, and accuracy
"""

import random
import time

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
            # Show placeholder
            with self.canvas.after:
                label = CoreLabel(text='Press Start to begin', font_size=18)
                label.refresh()
                texture = label.texture
                Color(0.5, 0.5, 0.5, 1)
                Rectangle(
                    texture=texture,
                    pos=(self.center_x - texture.width / 2, self.center_y - texture.height / 2),
                    size=texture.size
                )
            return

        # Draw scrolling text display
        with self.canvas.after:
            font_size = 28
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
        self.height = 40
        self.padding = 10
        self.spacing = 20

        # Time display
        self.time_label = Label(text='Time: 00:00', size_hint_x=0.25, halign='left')
        self.time_label.bind(size=self.time_label.setter('text_size'))

        # WPM display
        self.wpm_label = Label(text='WPM: 0', size_hint_x=0.25, halign='center')

        # Accuracy display
        self.accuracy_label = Label(text='Accuracy: 100%', size_hint_x=0.25, halign='center')

        # Progress display
        self.progress_label = Label(text='Progress: 0/0', size_hint_x=0.25, halign='right')
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
        self.padding = 10
        self.spacing = 10

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
        self._permutation_queue = []  # Queue of 3-letter combinations to practice
        self._completed_count = 0  # Total combinations completed across all rounds
        self._missed_char = None  # Character that was missed, to reinforce in next target
        self._fixed_row = None  # Which row is fixed (e.g., 'F1')
        self._fixed_col = None  # Which column of fixed row (e.g., 'L', 'M', 'R')

        # Toolbar
        toolbar = BoxLayout(orientation='horizontal', size_hint_y=None, height=45, spacing=10)

        # Lesson selector
        lesson_names = [f'{name}' for name, desc, _ in LESSONS]
        self.group_spinner = Spinner(
            text=lesson_names[0] if lesson_names else 'F1+F2',
            values=lesson_names,
            size_hint_x=0.3
        )

        # Subset size selector (how many unique letters to practice)
        subset_box = BoxLayout(orientation='horizontal', size_hint_x=0.25, spacing=5)
        subset_box.add_widget(Label(text='Letters:', size_hint_x=0.4))
        self.subset_spinner = Spinner(
            text='3',
            values=['2', '3', '4', '5', '6'],
            size_hint_x=0.6
        )
        subset_box.add_widget(self.subset_spinner)

        # Start/Reset button (Start when stopped, Reset when running)
        self.start_btn = Button(text='Start', size_hint_x=0.15)
        self.start_btn.bind(on_press=self._on_start_reset)

        # Show Hints toggle
        from kivy.uix.togglebutton import ToggleButton
        self.hint_toggle = ToggleButton(text='Hints', size_hint_x=0.12)
        self.hint_toggle.bind(state=self._on_hint_toggle)

        toolbar.add_widget(self.group_spinner)
        toolbar.add_widget(subset_box)
        toolbar.add_widget(self.start_btn)
        toolbar.add_widget(self.hint_toggle)
        self.add_widget(toolbar)

        # Status label
        self.status_label = Label(
            text='Load a chord config to begin',
            size_hint_y=None,
            height=30,
            font_size='14sp',
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

        # Update status with lesson info
        lesson_name = self.group_spinner.text
        entries = self._get_lesson_entries(lesson_name)
        chars = self._get_printable_chars(entries)
        self.status_label.text = f'{len(config.entries)} chords. {lesson_name}: {len(chars)} chars'

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

    def _on_start_reset(self, instance):
        """Start exercise or reset if running"""
        if self._is_running:
            self._reset_exercise()
        else:
            self._start_exercise()

    def _start_exercise(self):
        """Start a new exercise"""
        if not self.config:
            self.status_label.text = 'Load a chord config first'
            return

        lesson_name = self.group_spinner.text
        subset_size = int(self.subset_spinner.text)

        # Get available characters for this lesson
        entries = self._get_lesson_entries(lesson_name)

        # For 2-row lessons, randomly choose which row is fixed
        rows_in_lesson = self._get_rows_in_lesson(lesson_name)
        if len(rows_in_lesson) == 2:
            # Randomly choose fixed vs varying row
            self._fixed_row = random.choice(rows_in_lesson)
            self._varying_row = [r for r in rows_in_lesson if r != self._fixed_row][0]
            # Randomly choose which column of the fixed row
            self._fixed_col = random.choice(['L', 'M', 'R'])

            # Filter entries to only those with fixed button + varying row
            entries = self._filter_entries_by_fixed_button(
                entries, self._fixed_row, self._fixed_col, self._varying_row
            )
        else:
            self._fixed_row = None
            self._fixed_col = None
            self._varying_row = None

        all_chars = self._get_printable_chars(entries)

        if not all_chars:
            self.status_label.text = f'No printable chars in {lesson_name} ({len(entries)} chords)'
            return

        if len(all_chars) < subset_size:
            # Use all available if fewer than requested
            subset_size = len(all_chars)

        # Select subset of characters (first N from available)
        self._current_chars = all_chars[:subset_size]

        # Generate all permutations, shuffle them, and concatenate into one long string
        perms = self._generate_permutations(self._current_chars)
        random.shuffle(perms)
        self._target_text = ''.join(perms)
        self._total_chars = len(self._target_text)
        self._completed_count = 0

        if not self._target_text:
            self.status_label.text = 'Failed to generate exercise'
            return

        # Reset state
        self._typed_text = ''
        self._cursor_pos = 0
        self._correct_count = 0
        self._total_typed = 0
        self._start_time = time.time()
        self._is_running = True
        self._prev_buttons = 0

        # Update UI
        self.start_btn.text = 'Reset'
        total_perms = len(self._current_chars) ** 3

        # Status message showing fixed button if applicable
        if self._fixed_row:
            fixed_btn = f'{self._fixed_row}{self._fixed_col}'
            self.status_label.text = f"Fixed: {fixed_btn}, vary {self._varying_row} | Chars: {''.join(self._current_chars)} ({self._total_chars} total)"
        else:
            self.status_label.text = f"Practicing: {''.join(self._current_chars)} ({self._total_chars} chars)"

        self.display.target_text = self._target_text
        self.display.typed_text = ''
        self.display.cursor_pos = 0

        self.stats.progress_total = self._total_chars
        self.stats.progress_current = 0
        self.stats.elapsed_time = 0
        self.stats.wpm = 0
        self.stats.accuracy = 100

        # Start timer
        self._timer_event = Clock.schedule_interval(self._update_timer, 0.1)

        # Show initial hint if always-show mode is on
        if self._always_show_hint and self._target_text:
            self._show_chord_hint(self._target_text[0])

    def _reset_exercise(self):
        """Stop and reset exercise state"""
        self._is_running = False
        self.start_btn.text = 'Start'

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

    def _on_hint_toggle(self, instance, state):
        """Toggle always-show-hint mode"""
        self._always_show_hint = (state == 'down')

        if self._always_show_hint and self._is_running:
            # Show hint for current target
            if self._cursor_pos < len(self._target_text):
                self._show_chord_hint(self._target_text[self._cursor_pos])
        elif not self._always_show_hint and self._hint_popup:
            # Hide hint when turning off
            self._hint_popup.dismiss()
            self._hint_popup = None

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

        if not self._is_running:
            self._prev_buttons = buttons
            return

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
        if not self._is_running:
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

        # Dismiss hint
        hint_label = Label(
            text='(tap to dismiss)',
            font_size='11sp',
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
        """Handle completion of full exercise - regenerate for next round"""
        self._completed_count += 1

        # Generate new shuffled set for next round
        perms = self._generate_permutations(self._current_chars)
        random.shuffle(perms)

        # If there was a missed char, put combos starting with it first
        if self._missed_char:
            # Move permutations starting with missed char to front
            missed_perms = [p for p in perms if p.startswith(self._missed_char)]
            other_perms = [p for p in perms if not p.startswith(self._missed_char)]
            perms = missed_perms + other_perms
            self._missed_char = None

        self._target_text = ''.join(perms)
        self._typed_text = ''
        self._cursor_pos = 0

        # Update display
        self.display.target_text = self._target_text
        self.display.typed_text = ''
        self.display.cursor_pos = 0

        # Update status
        if self._fixed_row:
            fixed_btn = f'{self._fixed_row}{self._fixed_col}'
            self.status_label.text = f"Round {self._completed_count + 1} | Fixed: {fixed_btn}, vary {self._varying_row}"
        else:
            self.status_label.text = f"Round {self._completed_count + 1} | Chars: {''.join(self._current_chars)}"

        # Reset progress for new round
        self.stats.progress_current = 0

        # Show hint for new target if in always-show mode
        if self._always_show_hint and self._target_text:
            self._show_chord_hint(self._target_text[0])
