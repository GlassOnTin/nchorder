"""
Debug Visualizer - Real-time sensor and event debugging

Shows:
- Raw sensor data from all 4 Trill sensors
- Gesture detection events (tap vs slide)
- Button states (16-button bitmask)
- Raw RTT log output
"""

from kivy.app import App
from kivy.uix.boxlayout import BoxLayout
from kivy.uix.gridlayout import GridLayout
from kivy.uix.label import Label
from kivy.uix.button import Button
from kivy.uix.scrollview import ScrollView
from kivy.uix.widget import Widget
from kivy.graphics import Color, Ellipse, Rectangle, Line
from kivy.clock import Clock, mainthread
from kivy.properties import (
    NumericProperty, BooleanProperty, ListProperty,
    StringProperty, ObjectProperty
)

from ..rtt_reader import RTTReader, TrillSensorData, GestureEvent, ButtonEvent, Raw2DEvent


class SensorSquareWidget(Widget):
    """Visualize the Trill Square (2D) sensor"""

    touch_x = NumericProperty(0)
    touch_y = NumericProperty(0)
    touch_size = NumericProperty(0)
    max_coord = NumericProperty(3200)

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.bind(pos=self._redraw, size=self._redraw,
                  touch_x=self._redraw, touch_y=self._redraw,
                  touch_size=self._redraw)
        self._redraw()

    def _redraw(self, *args):
        self.canvas.clear()
        with self.canvas:
            # Background
            Color(0.12, 0.12, 0.15)
            Rectangle(pos=self.pos, size=self.size)

            # Grid
            Color(0.2, 0.2, 0.25)
            cx, cy = self.center
            # Vertical center line
            Line(points=[cx, self.y, cx, self.top], width=1)
            # Horizontal center line
            Line(points=[self.x, cy, self.right, cy], width=1)

            # Quadrant labels
            Color(0.3, 0.3, 0.35)
            q_size = 20
            for i, (qx, qy, label) in enumerate([
                (0.25, 0.75, "T1"),
                (0.75, 0.75, "T2"),
                (0.25, 0.25, "T3"),
                (0.75, 0.25, "T4"),
            ]):
                x = self.x + self.width * qx
                y = self.y + self.height * qy

            # Border
            Color(0.4, 0.4, 0.5)
            Line(rectangle=(*self.pos, *self.size), width=2)

            # Touch point
            if self.touch_size > 0:
                px = self.x + (self.touch_x / self.max_coord) * self.width
                py = self.y + (self.touch_y / self.max_coord) * self.height

                radius = 8 + (self.touch_size / 150)
                radius = min(radius, 30)

                # Glow
                Color(0.2, 0.8, 0.3, 0.3)
                Ellipse(pos=(px - radius*1.5, py - radius*1.5),
                        size=(radius*3, radius*3))

                # Main point
                Color(0.2, 0.9, 0.3)
                Ellipse(pos=(px - radius, py - radius),
                        size=(radius*2, radius*2))

                # Center dot
                Color(1, 1, 1)
                Ellipse(pos=(px - 3, py - 3), size=(6, 6))


class SensorBarWidget(Widget):
    """Visualize a Trill Bar (1D) sensor"""

    touch_pos = NumericProperty(0)
    touch_size = NumericProperty(0)
    bar_index = NumericProperty(0)
    max_pos = NumericProperty(3200)
    min_touch_size = NumericProperty(200)  # Minimum size to show (filter noise)

    BAR_COLORS = [
        (0.9, 0.3, 0.3),  # Red - Left
        (0.3, 0.9, 0.3),  # Green - Middle
        (0.3, 0.3, 0.9),  # Blue - Right
    ]

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.bind(pos=self._redraw, size=self._redraw,
                  touch_pos=self._redraw, touch_size=self._redraw)
        self._redraw()

    def _redraw(self, *args):
        self.canvas.clear()
        with self.canvas:
            # Background
            Color(0.12, 0.12, 0.15)
            Rectangle(pos=self.pos, size=self.size)

            # Zone dividers (4 zones)
            Color(0.25, 0.25, 0.3)
            for i in range(1, 4):
                y = self.y + (self.height * i / 4)
                Line(points=[self.x, y, self.right, y], width=1)

            # Border
            Color(0.4, 0.4, 0.5)
            Line(rectangle=(*self.pos, *self.size), width=2)

            # Touch indicator - only show if size exceeds threshold
            if self.touch_size >= self.min_touch_size:
                py = self.y + (self.touch_pos / self.max_pos) * self.height

                bar_h = 4 + (self.touch_size / 80)
                bar_h = min(bar_h, 15)

                # Brightness based on touch size (0.4 to 1.0)
                brightness = min(1.0, 0.4 + (self.touch_size / 2000))
                color = self.BAR_COLORS[self.bar_index % 3]
                Color(color[0] * brightness, color[1] * brightness, color[2] * brightness)
                Rectangle(
                    pos=(self.x + 4, py - bar_h/2),
                    size=(self.width - 8, bar_h)
                )


class ButtonGridWidget(Widget):
    """16-button state indicator grid"""

    buttons = NumericProperty(0)

    BUTTON_NAMES = [
        "T1", "F1L", "F1M", "F1R",
        "T2", "F2L", "F2M", "F2R",
        "T3", "F3L", "F3M", "F3R",
        "T4", "F4L", "F4M", "F4R"
    ]

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.bind(pos=self._redraw, size=self._redraw, buttons=self._redraw)
        self._redraw()

    def _redraw(self, *args):
        self.canvas.clear()

        cols = 4
        rows = 4
        cell_w = self.width / cols
        cell_h = self.height / rows

        with self.canvas:
            for i in range(16):
                row = i // cols
                col = i % cols

                x = self.x + col * cell_w
                y = self.top - (row + 1) * cell_h

                is_on = (self.buttons >> i) & 1

                # Background
                if is_on:
                    Color(0.2, 0.9, 0.2)
                else:
                    Color(0.15, 0.2, 0.15)

                margin = 3
                Rectangle(
                    pos=(x + margin, y + margin),
                    size=(cell_w - margin*2, cell_h - margin*2)
                )


class EventLogWidget(ScrollView):
    """Scrolling log of events"""

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.layout = BoxLayout(orientation='vertical', size_hint_y=None)
        self.layout.bind(minimum_height=self.layout.setter('height'))
        self.add_widget(self.layout)
        self._max_lines = 50

    def add_line(self, text: str, color=(0.8, 0.8, 0.8, 1)):
        label = Label(
            text=text,
            size_hint_y=None,
            height=22,
            halign='left',
            valign='middle',
            color=color,
            font_size='12sp'
        )
        label.bind(size=label.setter('text_size'))

        self.layout.add_widget(label)

        # Limit lines
        while len(self.layout.children) > self._max_lines:
            self.layout.remove_widget(self.layout.children[-1])

        # Scroll to bottom
        self.scroll_y = 0


class DebugPanel(BoxLayout):
    """Main debug visualization panel"""

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.orientation = 'horizontal'
        self.padding = 10
        self.spacing = 10

        # RTT Reader
        self.rtt = RTTReader()
        self.rtt.on_sensor_data = self._on_sensor
        self.rtt.on_gesture = self._on_gesture
        self.rtt.on_button = self._on_button
        self.rtt.on_raw2d = self._on_raw2d

        # Left panel: sensors
        left = BoxLayout(orientation='vertical', size_hint_x=0.5, spacing=5)

        # Square sensor
        left.add_widget(Label(text='Square (Thumb)', size_hint_y=None, height=25))
        self.square = SensorSquareWidget(size_hint_y=0.5)
        left.add_widget(self.square)

        # Bar sensors
        bars_row = BoxLayout(orientation='horizontal', size_hint_y=0.3, spacing=5)
        self.bars = []
        for i, name in enumerate(['Left', 'Middle', 'Right']):
            col = BoxLayout(orientation='vertical')
            col.add_widget(Label(text=name, size_hint_y=None, height=20))
            bar = SensorBarWidget(bar_index=i)
            col.add_widget(bar)
            self.bars.append(bar)
            bars_row.add_widget(col)
        left.add_widget(bars_row)

        # Buttons grid
        left.add_widget(Label(text='Buttons', size_hint_y=None, height=25))
        self.button_grid = ButtonGridWidget(size_hint_y=0.2)
        left.add_widget(self.button_grid)

        self.add_widget(left)

        # Right panel: events and raw data
        right = BoxLayout(orientation='vertical', size_hint_x=0.5, spacing=5)

        # Status
        self.status_label = Label(
            text='Disconnected',
            size_hint_y=None,
            height=30,
            color=(1, 0.5, 0.5, 1)
        )
        right.add_widget(self.status_label)

        # Gesture info
        right.add_widget(Label(text='Gesture State', size_hint_y=None, height=25))
        self.gesture_label = Label(
            text='No gesture',
            size_hint_y=None,
            height=60,
            halign='left',
            valign='top'
        )
        self.gesture_label.bind(size=self.gesture_label.setter('text_size'))
        right.add_widget(self.gesture_label)

        # Raw values
        right.add_widget(Label(text='Raw Values', size_hint_y=None, height=25))
        self.raw_label = Label(
            text='Waiting for data...',
            size_hint_y=None,
            height=80,
            halign='left',
            valign='top',
            font_size='11sp'
        )
        self.raw_label.bind(size=self.raw_label.setter('text_size'))
        right.add_widget(self.raw_label)

        # Event log
        right.add_widget(Label(text='Event Log', size_hint_y=None, height=25))
        self.event_log = EventLogWidget()
        right.add_widget(self.event_log)

        # Connect button
        self.connect_btn = Button(
            text='Connect RTT',
            size_hint_y=None,
            height=50
        )
        self.connect_btn.bind(on_press=self._toggle_connection)
        right.add_widget(self.connect_btn)

        self.add_widget(right)

        # Start update timer
        Clock.schedule_interval(self._update, 1/30)

    def _toggle_connection(self, *args):
        if self.rtt._running:
            self.rtt.stop()
            self.connect_btn.text = 'Connect RTT'
            self.status_label.text = 'Disconnected'
            self.status_label.color = (1, 0.5, 0.5, 1)
            self.event_log.add_line("Disconnected", (1, 0.5, 0.5, 1))
        else:
            if self.rtt.start():
                self.connect_btn.text = 'Disconnect'
                self.status_label.text = 'Connected via RTT'
                self.status_label.color = (0.5, 1, 0.5, 1)
                self.event_log.add_line("Connected to RTT", (0.5, 1, 0.5, 1))
            else:
                self.event_log.add_line("Connection failed - is JLink running?", (1, 0.3, 0.3, 1))

    def _on_sensor(self, data: TrillSensorData):
        """Handle sensor data update."""
        pass  # Handled in _update via polling

    @mainthread
    def _on_gesture(self, event: GestureEvent):
        """Handle gesture event (called from background thread, runs on main)."""
        if event.event_type == "tap":
            self.event_log.add_line(
                f"TAP Q{event.quadrant} @ ({event.x},{event.y}) {event.frames}f",
                (0.3, 1, 0.3, 1)
            )
        elif event.event_type == "mouse_mode":
            self.event_log.add_line(
                f"MOUSE MODE dist={event.distance} frames={event.frames}",
                (0.3, 0.7, 1, 1)
            )
        elif event.event_type == "touch_start":
            self.event_log.add_line(
                f"Touch start @ ({event.x},{event.y})",
                (0.7, 0.7, 0.7, 1)
            )
        elif event.event_type == "mouse_end":
            self.event_log.add_line(
                f"Mouse end dist={event.distance}",
                (0.7, 0.7, 0.7, 1)
            )

    @mainthread
    def _on_button(self, event: ButtonEvent):
        """Handle button state change (called from background thread, runs on main)."""
        self.event_log.add_line(
            f"BUTTONS 0x{event.raw_mask:04X} = {event.button_names}",
            (1, 1, 0.3, 1)
        )

    @mainthread
    def _on_raw2d(self, event: Raw2DEvent):
        """Handle RAW2D debug output (called from background thread, runs on main)."""
        # Parse hex bytes to readable values
        def parse_positions(hex_str):
            """Parse 10 hex bytes (5 x 16-bit BE values) to list of ints."""
            vals = []
            for i in range(0, len(hex_str), 4):
                if i + 4 <= len(hex_str):
                    val = int(hex_str[i:i+4], 16)
                    if val != 0xFFFF:
                        vals.append(val)
            return vals

        y_vals = parse_positions(event.y_bytes)
        x_vals = parse_positions(event.x_bytes)
        s_vals = parse_positions(event.size_bytes)

        self.event_log.add_line(
            f"RAW2D Y={y_vals} X={x_vals} S={s_vals}",
            (0.7, 0.7, 1, 1)
        )

    def _update(self, dt):
        """Update display from latest sensor data."""
        if not self.rtt._running:
            return

        # Update Square sensor
        sq = self.rtt.sensors[0]
        if sq.is_2d and sq.touches_2d:
            t = sq.touches_2d[0]
            self.square.touch_x = t.x
            self.square.touch_y = t.y
            self.square.touch_size = t.size
        else:
            self.square.touch_size = 0

        # Update Bar sensors
        for i, bar in enumerate(self.bars):
            s = self.rtt.sensors[i + 1]
            if s.touches_1d:
                t = s.touches_1d[0]
                bar.touch_pos = t.position
                bar.touch_size = t.size
            else:
                bar.touch_size = 0

        # Update button grid
        self.button_grid.buttons = self.rtt.last_buttons.raw_mask

        # Update gesture label
        g = self.rtt.last_gesture
        if g:
            self.gesture_label.text = (
                f"Type: {g.event_type}\n"
                f"Pos: ({g.x}, {g.y})\n"
                f"Q: {g.quadrant}  Dist: {g.distance}  Frames: {g.frames}"
            )

        # Update raw values
        sq = self.rtt.sensors[0]
        if sq.touches_2d:
            t = sq.touches_2d[0]
            sq_text = f"Square: ({t.x}, {t.y}) size={t.size}"
        else:
            sq_text = "Square: no touch"

        bar_texts = []
        for i in range(3):
            s = self.rtt.sensors[i + 1]
            if s.touches_1d:
                t = s.touches_1d[0]
                bar_texts.append(f"Bar{i}: pos={t.position} size={t.size}")
            else:
                bar_texts.append(f"Bar{i}: no touch")

        self.raw_label.text = sq_text + "\n" + "\n".join(bar_texts)


class DebugApp(App):
    """Debug visualizer application"""

    def build(self):
        self.title = 'nChorder Debug'
        return DebugPanel()


def main():
    DebugApp().run()


if __name__ == '__main__':
    main()
