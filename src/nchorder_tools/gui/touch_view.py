"""
Touch Visualizer Widget

Displays real-time touch sensor data:
- Square sensor: 2D touch position as colored circle
- Bar sensors: 3 vertical bars with touch positions
- Button state: 16 LEDs showing current chord bitmask
"""

from kivy.uix.widget import Widget
from kivy.graphics import Color, Ellipse, Rectangle, Line
from kivy.properties import (
    NumericProperty, BooleanProperty, ListProperty, ObjectProperty
)
from kivy.clock import Clock

from ..cdc_client import TouchFrame


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
    """Single bar sensor visualization"""

    touch_pos = NumericProperty(0)  # Position along bar
    touch_size = NumericProperty(0)  # Touch pressure
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
            touch_pos=self._update_canvas,
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

            # Touch indicator
            if self.touch_size > 0:
                # Map position to widget (vertical bar)
                py = self.y + (self.touch_pos / self.max_pos) * self.height

                # Bar width based on touch size
                bar_width = self.width * 0.8
                bar_height = 4 + (self.touch_size / 50)
                bar_height = min(bar_height, 20)

                # Color for this bar
                color = self.BAR_COLORS[self.bar_index % 3]
                Color(*color)

                Rectangle(
                    pos=(self.x + (self.width - bar_width) / 2, py - bar_height / 2),
                    size=(bar_width, bar_height)
                )


class ButtonIndicator(Widget):
    """Button state LED indicators"""

    buttons = NumericProperty(0)  # 16-bit bitmask

    # Button layout: 4 rows x 4 columns
    ROWS = 4
    COLS = 4

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.bind(
            pos=self._update_canvas,
            size=self._update_canvas,
            buttons=self._update_canvas
        )
        self._update_canvas()

    def _update_canvas(self, *args):
        self.canvas.clear()

        led_w = self.width / self.COLS
        led_h = self.height / self.ROWS

        with self.canvas:
            for i in range(16):
                row = i // self.COLS
                col = i % self.COLS

                x = self.x + col * led_w
                y = self.top - (row + 1) * led_h

                # LED state
                is_on = (self.buttons >> i) & 1

                if is_on:
                    Color(0.2, 1.0, 0.2)  # Bright green
                else:
                    Color(0.15, 0.25, 0.15)  # Dim green

                # LED circle
                margin = 4
                Ellipse(
                    pos=(x + margin, y + margin),
                    size=(led_w - margin * 2, led_h - margin * 2)
                )


class TouchVisualizer(Widget):
    """Combined touch visualization panel"""

    touch_frame = ObjectProperty(None, allownone=True)

    def __init__(self, **kwargs):
        super().__init__(**kwargs)

        # Create child widgets (will be positioned in layout)
        self.square = TouchSquare()
        self.bar0 = TouchBar(bar_index=0)
        self.bar1 = TouchBar(bar_index=1)
        self.bar2 = TouchBar(bar_index=2)
        self.buttons = ButtonIndicator()

        self.add_widget(self.square)
        self.add_widget(self.bar0)
        self.add_widget(self.bar1)
        self.add_widget(self.bar2)
        self.add_widget(self.buttons)

        self.bind(pos=self._layout, size=self._layout)
        self._layout()

    def _layout(self, *args):
        """Position child widgets"""
        if self.width < 100 or self.height < 100:
            return

        # Layout: [bars] [square] [buttons]
        # Bars take 30% width, square 50%, buttons 20%

        bar_width = self.width * 0.08
        bar_spacing = self.width * 0.02
        square_size = min(self.width * 0.45, self.height * 0.8)
        button_width = self.width * 0.18

        # Center vertically
        cy = self.y + (self.height - square_size) / 2

        # Bars on left
        x = self.x + 10
        bar_height = square_size
        for bar in [self.bar0, self.bar1, self.bar2]:
            bar.pos = (x, cy)
            bar.size = (bar_width, bar_height)
            x += bar_width + bar_spacing

        # Square in middle
        x += bar_spacing
        self.square.pos = (x, cy)
        self.square.size = (square_size, square_size)

        # Buttons on right
        x += square_size + bar_spacing * 2
        self.buttons.pos = (x, cy)
        self.buttons.size = (button_width, square_size)

    def update(self, frame: TouchFrame):
        """Update display with new touch data"""
        self.touch_frame = frame

        # Square sensor (firmware now maps Trill V→X, H→Y correctly)
        self.square.touch_x = frame.thumb_x
        self.square.touch_y = frame.thumb_y
        self.square.touch_size = frame.thumb_size

        # Bar sensors
        self.bar0.touch_pos = frame.bar0_pos
        self.bar0.touch_size = frame.bar0_size
        self.bar1.touch_pos = frame.bar1_pos
        self.bar1.touch_size = frame.bar1_size
        self.bar2.touch_pos = frame.bar2_pos
        self.bar2.touch_size = frame.bar2_size

        # Buttons
        self.buttons.buttons = frame.buttons
