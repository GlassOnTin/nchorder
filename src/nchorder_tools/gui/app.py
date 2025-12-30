"""
Northern Chorder Configuration App

Main Kivy application with:
- Touch visualizer
- Device connection management
- Configuration controls
"""

from kivy.app import App
from kivy.uix.boxlayout import BoxLayout
from kivy.uix.gridlayout import GridLayout
from kivy.uix.label import Label
from kivy.uix.button import Button
from kivy.uix.slider import Slider
from kivy.uix.spinner import Spinner
from kivy.uix.popup import Popup
from kivy.clock import Clock, mainthread
from kivy.properties import ObjectProperty, BooleanProperty, StringProperty

from .touch_view import TouchVisualizer
from ..cdc_client import NChorderDevice, TouchFrame, ConfigID


class StatusBar(BoxLayout):
    """Connection status and device info"""

    status_text = StringProperty("Disconnected")
    version_text = StringProperty("")

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.orientation = 'horizontal'
        self.size_hint_y = None
        self.height = 40
        self.padding = 10
        self.spacing = 10

        self.status_label = Label(
            text=self.status_text,
            size_hint_x=0.3,
            halign='left'
        )
        self.version_label = Label(
            text=self.version_text,
            size_hint_x=0.7,
            halign='right'
        )

        self.add_widget(self.status_label)
        self.add_widget(self.version_label)

        self.bind(status_text=lambda *a: setattr(self.status_label, 'text', self.status_text))
        self.bind(version_text=lambda *a: setattr(self.version_label, 'text', self.version_text))


class ConfigPanel(BoxLayout):
    """Configuration sliders and controls"""

    device = ObjectProperty(None, allownone=True)

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.orientation = 'vertical'
        self.padding = 10
        self.spacing = 5

        # Configuration sliders
        self.sliders = {}
        configs = [
            ('threshold_press', 'Press Threshold', 100, 1000, 500),
            ('threshold_release', 'Release Threshold', 50, 500, 250),
            ('debounce_ms', 'Debounce (ms)', 10, 100, 30),
            ('poll_rate_ms', 'Poll Rate (ms)', 5, 50, 15),
            ('mouse_speed', 'Mouse Speed', 1, 20, 10),
            ('mouse_accel', 'Mouse Accel', 0, 10, 3),
            ('volume_sensitivity', 'Volume Sens', 1, 10, 5),
        ]

        for config_id, label, min_val, max_val, default in configs:
            row = BoxLayout(orientation='horizontal', size_hint_y=None, height=40)

            lbl = Label(text=label, size_hint_x=0.4, halign='left')
            lbl.bind(size=lbl.setter('text_size'))

            slider = Slider(
                min=min_val,
                max=max_val,
                value=default,
                size_hint_x=0.5
            )
            slider.config_id = config_id

            value_lbl = Label(text=str(default), size_hint_x=0.1)

            def on_value(instance, value, vlbl=value_lbl, cid=config_id):
                vlbl.text = str(int(value))
                self._apply_config(cid, int(value))

            slider.bind(value=on_value)

            row.add_widget(lbl)
            row.add_widget(slider)
            row.add_widget(value_lbl)
            self.add_widget(row)
            self.sliders[config_id] = slider

        # Buttons
        btn_row = BoxLayout(orientation='horizontal', size_hint_y=None, height=50, spacing=10)
        btn_row.padding = [0, 10, 0, 0]

        reset_btn = Button(text='Reset Defaults')
        reset_btn.bind(on_press=self._on_reset)

        save_btn = Button(text='Save to Flash')
        save_btn.bind(on_press=self._on_save)

        btn_row.add_widget(reset_btn)
        btn_row.add_widget(save_btn)
        self.add_widget(btn_row)

    def _apply_config(self, config_id: str, value: int):
        """Apply a configuration change to device"""
        if not self.device or not self.device.is_connected():
            return

        # Map string to ConfigID enum
        id_map = {
            'threshold_press': ConfigID.THRESHOLD_PRESS,
            'threshold_release': ConfigID.THRESHOLD_RELEASE,
            'debounce_ms': ConfigID.DEBOUNCE_MS,
            'poll_rate_ms': ConfigID.POLL_RATE_MS,
            'mouse_speed': ConfigID.MOUSE_SPEED,
            'mouse_accel': ConfigID.MOUSE_ACCEL,
            'volume_sensitivity': ConfigID.VOLUME_SENSITIVITY,
        }

        if config_id in id_map:
            self.device.set_config(id_map[config_id], value)

    def _on_reset(self, instance):
        """Reset configuration to defaults"""
        if self.device and self.device.is_connected():
            if self.device.reset_defaults():
                self.load_from_device()

    def _on_save(self, instance):
        """Save configuration to flash"""
        if self.device and self.device.is_connected():
            self.device.save_to_flash()

    def load_from_device(self):
        """Load current configuration from device"""
        if not self.device or not self.device.is_connected():
            return

        config = self.device.get_config()
        if config:
            self.sliders['threshold_press'].value = config.threshold_press
            self.sliders['threshold_release'].value = config.threshold_release
            self.sliders['debounce_ms'].value = config.debounce_ms
            self.sliders['poll_rate_ms'].value = config.poll_rate_ms
            self.sliders['mouse_speed'].value = config.mouse_speed
            self.sliders['mouse_accel'].value = config.mouse_accel
            self.sliders['volume_sensitivity'].value = config.volume_sensitivity


class MainLayout(BoxLayout):
    """Main application layout"""

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.orientation = 'vertical'
        self.padding = 10
        self.spacing = 10

        # Device
        self.device = None
        self._streaming = False

        # Status bar
        self.status_bar = StatusBar()

        # Main content
        content = BoxLayout(orientation='horizontal', spacing=10)

        # Touch visualizer (left side)
        self.touch_vis = TouchVisualizer(size_hint_x=0.65)

        # Config panel (right side)
        self.config_panel = ConfigPanel(size_hint_x=0.35)

        content.add_widget(self.touch_vis)
        content.add_widget(self.config_panel)

        # Connection controls
        conn_bar = BoxLayout(orientation='horizontal', size_hint_y=None, height=50, spacing=10)

        self.port_spinner = Spinner(
            text='Select Device',
            values=['Scanning...'],
            size_hint_x=0.5
        )

        self.connect_btn = Button(text='Connect', size_hint_x=0.25)
        self.connect_btn.bind(on_press=self._on_connect)

        self.stream_btn = Button(text='Start Stream', size_hint_x=0.25)
        self.stream_btn.bind(on_press=self._on_stream)
        self.stream_btn.disabled = True

        conn_bar.add_widget(self.port_spinner)
        conn_bar.add_widget(self.connect_btn)
        conn_bar.add_widget(self.stream_btn)

        # Add all to main layout
        self.add_widget(self.status_bar)
        self.add_widget(content)
        self.add_widget(conn_bar)

        # Scan for devices
        Clock.schedule_once(lambda dt: self._scan_devices(), 0.5)

    def _scan_devices(self):
        """Scan for connected devices"""
        devices = NChorderDevice.find_devices()
        if devices:
            self.port_spinner.values = devices
            self.port_spinner.text = devices[0]
        else:
            self.port_spinner.values = ['No devices found']
            self.port_spinner.text = 'No devices found'

    def _on_connect(self, instance):
        """Handle connect/disconnect button"""
        if self.device and self.device.is_connected():
            self._disconnect()
        else:
            self._connect()

    def _connect(self):
        """Connect to selected device"""
        port = self.port_spinner.text
        if port in ['No devices found', 'Select Device', 'Scanning...']:
            return

        self.device = NChorderDevice(port)
        if self.device.connect():
            self.connect_btn.text = 'Disconnect'
            self.stream_btn.disabled = False
            self.status_bar.status_text = f"Connected: {port}"

            # Get version
            version = self.device.get_version()
            if version:
                self.status_bar.version_text = f"Firmware: {version}"

            # Load config
            self.config_panel.device = self.device
            self.config_panel.load_from_device()
        else:
            self.status_bar.status_text = "Connection failed"

    def _disconnect(self):
        """Disconnect from device"""
        if self._streaming:
            self._stop_stream()

        if self.device:
            self.device.disconnect()
            self.device = None

        self.connect_btn.text = 'Connect'
        self.stream_btn.disabled = True
        self.stream_btn.text = 'Start Stream'
        self.status_bar.status_text = "Disconnected"
        self.status_bar.version_text = ""
        self.config_panel.device = None

    def _on_stream(self, instance):
        """Handle stream start/stop button"""
        if self._streaming:
            self._stop_stream()
        else:
            self._start_stream()

    def _start_stream(self):
        """Start touch data streaming"""
        if not self.device or not self.device.is_connected():
            return

        if self.device.start_stream(callback=self._on_touch_frame, rate_hz=60):
            self._streaming = True
            self.stream_btn.text = 'Stop Stream'

    def _stop_stream(self):
        """Stop touch data streaming"""
        if self.device:
            self.device.stop_stream()
        self._streaming = False
        self.stream_btn.text = 'Start Stream'

    @mainthread
    def _on_touch_frame(self, frame: TouchFrame):
        """Handle incoming touch frame (called from background thread)"""
        self.touch_vis.update(frame)


class NChorderApp(App):
    """Main application class"""

    def build(self):
        self.title = 'Northern Chorder'
        return MainLayout()

    def on_stop(self):
        """Clean up on app exit"""
        if hasattr(self.root, 'device') and self.root.device:
            self.root.device.disconnect()


def main():
    NChorderApp().run()


if __name__ == '__main__':
    main()
