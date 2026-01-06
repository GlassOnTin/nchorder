"""
Northern Chorder Configuration App

Main Kivy application with:
- Touch visualizer
- Chord layout editor
- Device connection management
- Configuration controls

Cross-platform: Windows, Linux, Mac, Android (via Kivy/Buildozer)
"""

from kivy.app import App
from kivy.uix.boxlayout import BoxLayout
from kivy.uix.gridlayout import GridLayout
from kivy.uix.tabbedpanel import TabbedPanel, TabbedPanelItem
from kivy.uix.label import Label
from kivy.uix.button import Button
from kivy.uix.slider import Slider
from kivy.uix.spinner import Spinner
from kivy.uix.popup import Popup
from kivy.clock import Clock, mainthread
from kivy.properties import ObjectProperty, BooleanProperty, StringProperty

from pathlib import Path

from .touch_view import TouchVisualizer, GPIODiagnostics
from .chord_view import ChordMapView, ChordConfig
from .cheatsheet_view import CheatSheetView
from .exercise_view import ExerciseView
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

        # Tabbed panel for different views
        self.tabs = TabbedPanel(do_default_tab=False, tab_pos='top_left')

        # Tab 1: Touch visualizer
        touch_tab = TabbedPanelItem(text='Touch')
        touch_content = BoxLayout(orientation='vertical', spacing=5, padding=5)
        self.touch_vis = TouchVisualizer()
        touch_content.add_widget(self.touch_vis)
        touch_tab.add_widget(touch_content)
        self.tabs.add_widget(touch_tab)

        # Tab 2: Device Config (sliders)
        config_tab = TabbedPanelItem(text='Config')
        config_content = BoxLayout(orientation='vertical', spacing=5, padding=5)

        # Load config button at top
        config_bar = BoxLayout(orientation='horizontal', size_hint_y=None, height=40, spacing=10)
        self.config_label = Label(text='No config loaded', size_hint_x=0.7, halign='left')
        self.config_label.bind(size=self.config_label.setter('text_size'))
        load_cfg_btn = Button(text='Load Config', size_hint_x=0.3)
        load_cfg_btn.bind(on_press=self._on_load_config)
        config_bar.add_widget(self.config_label)
        config_bar.add_widget(load_cfg_btn)
        config_content.add_widget(config_bar)

        # Config sliders
        self.config_panel = ConfigPanel()
        config_content.add_widget(self.config_panel)

        config_tab.add_widget(config_content)
        self.tabs.add_widget(config_tab)

        # Tab 3: Chord layout editor
        chord_tab = TabbedPanelItem(text='Chord Editor')
        self.chord_map = ChordMapView()
        chord_tab.add_widget(self.chord_map)
        self.tabs.add_widget(chord_tab)

        # Tab 4: Cheat Sheet
        cheatsheet_tab = TabbedPanelItem(text='Cheat Sheet')
        self.cheatsheet = CheatSheetView()
        cheatsheet_tab.add_widget(self.cheatsheet)
        self.tabs.add_widget(cheatsheet_tab)

        # Tab 5: Exercise mode
        exercise_tab = TabbedPanelItem(text='Exercise')
        self.exercise = ExerciseView()
        exercise_tab.add_widget(self.exercise)
        self.tabs.add_widget(exercise_tab)

        # Tab 6: Debug (GPIO diagnostics)
        debug_tab = TabbedPanelItem(text='Debug')
        debug_content = BoxLayout(orientation='vertical', padding=10, spacing=10)
        self.gpio_diag = GPIODiagnostics()
        debug_content.add_widget(self.gpio_diag)
        debug_content.add_widget(Label(text='GPIO diagnostics for Twiddler 4 button debugging'))
        debug_tab.add_widget(debug_content)
        self.tabs.add_widget(debug_tab)

        # Set default tab
        self.tabs.default_tab = touch_tab

        # Loaded chord config (shared between views)
        self._chord_config = None

        # Add tabs to layout
        self.add_widget(self.tabs)

        # Auto-connect to first device found
        Clock.schedule_once(lambda dt: self._auto_connect(), 0.5)
        # Periodically check for device reconnection
        Clock.schedule_interval(lambda dt: self._check_connection(), 2.0)

        # Try to auto-load default config
        Clock.schedule_once(lambda dt: self._try_load_default_config(), 0.2)

    def _try_load_default_config(self):
        """Try to load a default config file on startup"""
        # Look for config in common locations
        search_paths = [
            Path.cwd() / 'configs' / 'mirrorwalk_nomcc_fixed.cfg',
            Path.cwd() / 'configs',
            Path.home() / '.config' / 'nchorder',
        ]
        for path in search_paths:
            if path.is_file():
                self._load_config_file(str(path))
                break
            elif path.is_dir():
                # Look for any .cfg file
                cfgs = list(path.glob('*.cfg'))
                if cfgs:
                    self._load_config_file(str(cfgs[0]))
                    break

    def _on_load_config(self, instance):
        """Show file chooser to load config"""
        from kivy.uix.filechooser import FileChooserListView

        content = BoxLayout(orientation='vertical')
        chooser = FileChooserListView(
            path=str(Path.cwd() / 'configs') if (Path.cwd() / 'configs').exists() else str(Path.home()),
            filters=['*.cfg']
        )
        content.add_widget(chooser)

        btn_row = BoxLayout(size_hint_y=None, height=50, spacing=10)

        popup = Popup(
            title='Load Chord Config',
            content=content,
            size_hint=(0.9, 0.9)
        )

        def do_load(btn):
            if chooser.selection:
                self._load_config_file(chooser.selection[0])
            popup.dismiss()

        def do_cancel(btn):
            popup.dismiss()

        from kivy.uix.button import Button as KButton
        load_btn = KButton(text='Load')
        load_btn.bind(on_press=do_load)
        cancel_btn = KButton(text='Cancel')
        cancel_btn.bind(on_press=do_cancel)

        btn_row.add_widget(load_btn)
        btn_row.add_widget(cancel_btn)
        content.add_widget(btn_row)

        popup.open()

    def _load_config_file(self, filepath: str):
        """Load chord config file and update views"""
        config = ChordConfig()
        if config.load(filepath):
            self._chord_config = config
            self.config_label.text = f'Config: {Path(filepath).name} ({len(config.entries)} chords)'

            # Update touch visualizer with config for live chord lookup
            self.touch_vis.load_config(config)

            # Also load into chord editor
            self.chord_map._config = config
            self.chord_map.chord_tree.config = config
            self.chord_map.chord_tree.refresh()
            self.chord_map.status_label.text = f'{Path(filepath).name} ({len(config.entries)} chords)'

            # Load into cheat sheet
            self.cheatsheet.load_config(config)

            # Load into exercise mode
            self.exercise.load_config(config)
        else:
            self.config_label.text = 'Failed to load config'

    def _auto_connect(self):
        """Auto-connect to first available device"""
        if self.device and self.device.is_connected():
            return  # Already connected

        devices = NChorderDevice.find_devices()
        if devices:
            self.device = NChorderDevice(devices[0])
            if self.device.connect():
                # Load device config
                self.config_panel.device = self.device
                self.config_panel.load_from_device()

                # Set device for chord editor
                self.chord_map.device = self.device

                # Auto-start streaming
                self._start_stream()

    def _check_connection(self):
        """Periodically check connection and reconnect if needed"""
        if self.device and self.device.is_connected():
            return  # Still connected

        # Lost connection or not connected - try to reconnect
        if self._streaming:
            self._streaming = False
        self._auto_connect()

    def _start_stream(self):
        """Start touch data streaming"""
        if not self.device or not self.device.is_connected():
            return

        if self.device.start_stream(callback=self._on_touch_frame, rate_hz=60):
            self._streaming = True

    def _stop_stream(self):
        """Stop touch data streaming"""
        if self.device:
            self.device.stop_stream()
        self._streaming = False

    @mainthread
    def _on_touch_frame(self, frame: TouchFrame):
        """Handle incoming touch frame (called from background thread)"""
        self.touch_vis.update(frame)
        # Update GPIO debug panel if in GPIO mode
        if frame.is_gpio_driver():
            self.gpio_diag.update(frame.get_gpio_diagnostics())
        # Route chord events to exercise view
        self.exercise.on_chord_event(frame.buttons)


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
