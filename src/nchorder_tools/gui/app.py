"""
Northern Chorder Configuration App

Main Kivy application with:
- Touch visualizer
- Chord layout editor
- Device connection management
- Configuration controls

Cross-platform: Windows, Linux, Mac, Android (via Kivy/Buildozer)
"""

import os

from kivy.app import App
from kivy.uix.boxlayout import BoxLayout
from kivy.uix.gridlayout import GridLayout
from kivy.uix.scrollview import ScrollView
from kivy.uix.tabbedpanel import TabbedPanel, TabbedPanelItem
from kivy.uix.label import Label
from kivy.uix.button import Button
from kivy.uix.slider import Slider
from kivy.uix.spinner import Spinner
from kivy.uix.popup import Popup
from kivy.clock import Clock, mainthread
from kivy.properties import ObjectProperty, BooleanProperty, StringProperty
from kivy.metrics import dp

from pathlib import Path
from kivy.utils import platform

# Platform detection
_ANDROID = platform == 'android'

from .touch_view import TouchVisualizer
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


class ConfigPanel(ScrollView):
    """Configuration sliders and controls - scrollable for mobile"""

    device = ObjectProperty(None, allownone=True)

    _on_save_callback = None

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.do_scroll_x = False

        # Inner container with fixed height content
        container = BoxLayout(
            orientation='vertical',
            padding=dp(10),
            spacing=dp(8),
            size_hint_y=None
        )
        container.bind(minimum_height=container.setter('height'))

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
            row = BoxLayout(orientation='horizontal', size_hint_y=None, height=dp(50))

            lbl = Label(text=label, size_hint_x=0.4, halign='left', font_size='14sp')
            lbl.bind(size=lbl.setter('text_size'))

            slider = Slider(
                min=min_val,
                max=max_val,
                value=default,
                size_hint_x=0.45
            )
            slider.config_id = config_id

            value_lbl = Label(text=str(default), size_hint_x=0.15, font_size='14sp')

            def on_value(instance, value, vlbl=value_lbl, cid=config_id):
                vlbl.text = str(int(value))
                self._debounce_config(cid, int(value))

            slider.bind(value=on_value)

            row.add_widget(lbl)
            row.add_widget(slider)
            row.add_widget(value_lbl)
            container.add_widget(row)
            self.sliders[config_id] = slider

        # Buttons
        btn_row = BoxLayout(orientation='horizontal', size_hint_y=None, height=dp(50), spacing=dp(10))
        btn_row.padding = [0, dp(15), 0, 0]

        reset_btn = Button(text='Reset Defaults', font_size='14sp')
        reset_btn.bind(on_press=self._on_reset)

        save_btn = Button(text='Save to Flash', font_size='14sp')
        save_btn.bind(on_press=self._on_save)

        btn_row.add_widget(reset_btn)
        btn_row.add_widget(save_btn)
        container.add_widget(btn_row)

        self.add_widget(container)

    def _debounce_config(self, config_id: str, value: int):
        """Debounce slider changes — send after 0.2s of no movement."""
        from kivy.clock import Clock
        if not hasattr(self, '_config_events'):
            self._config_events = {}
        if config_id in self._config_events:
            self._config_events[config_id].cancel()
        self._config_events[config_id] = Clock.schedule_once(
            lambda dt, cid=config_id, v=value: self._apply_config(cid, v), 0.2)

    _ID_MAP = {
        'threshold_press': ConfigID.THRESHOLD_PRESS,
        'threshold_release': ConfigID.THRESHOLD_RELEASE,
        'debounce_ms': ConfigID.DEBOUNCE_MS,
        'poll_rate_ms': ConfigID.POLL_RATE_MS,
        'mouse_speed': ConfigID.MOUSE_SPEED,
        'mouse_accel': ConfigID.MOUSE_ACCEL,
        'volume_sensitivity': ConfigID.VOLUME_SENSITIVITY,
    }

    def _apply_config(self, config_id: str, value: int):
        """Queue a configuration change (applied on Save)."""
        if not hasattr(self, '_pending_config'):
            self._pending_config = {}
        if config_id in self._ID_MAP:
            self._pending_config[config_id] = value

    def _flush_pending_config(self):
        """Send all pending config changes to device (call when stream is stopped)."""
        if not hasattr(self, '_pending_config') or not self._pending_config:
            return
        if not self.device or not self.device.is_connected():
            return
        for config_id, value in self._pending_config.items():
            if config_id in self._ID_MAP:
                self.device.set_config(self._ID_MAP[config_id], value)
        self._pending_config.clear()

    def _on_reset(self, instance):
        """Reset configuration to defaults"""
        if self.device and self.device.is_connected():
            if self._on_save_callback:
                self._on_save_callback(reset=True)
            else:
                if self.device.reset_defaults():
                    self.load_from_device()

    def _on_save(self, instance):
        """Save configuration to flash"""
        if self.device and self.device.is_connected():
            if self._on_save_callback:
                self._on_save_callback()
            else:
                self._flush_pending_config()
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


class ConnectionOverlay(BoxLayout):
    """Overlay shown when no device is connected"""

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.orientation = 'vertical'
        self.padding = dp(20)
        self.spacing = dp(10)

        # Center content vertically
        self.add_widget(BoxLayout())  # Spacer

        # Use size_hint_y=None with explicit height to prevent overlap
        # Height needs to accommodate: title + instructions + status (with multi-line text)
        content = BoxLayout(orientation='vertical', size_hint_y=None, height=dp(300), spacing=dp(15))

        title = Label(
            text='No Keyboard Connected',
            font_size=dp(20),
            bold=True,
            size_hint_y=None,
            height=dp(40)
        )
        content.add_widget(title)

        # Platform-specific instructions
        if _ANDROID:
            instruction_text = (
                'Connect your nChorder or Twiddler via\n'
                'USB-C OTG adapter to use Touch features.\n\n'
                'Cheat Sheet and Exercise modes work\n'
                'without a keyboard connected.'
            )
        else:
            instruction_text = (
                'Connect your nChorder or Twiddler via USB\n'
                'to use Touch and Config features.\n\n'
                'Cheat Sheet and Exercise modes work\n'
                'without a keyboard connected.'
            )

        instructions = Label(
            text=instruction_text,
            font_size=dp(14),
            halign='center',
            valign='top',
            size_hint_y=None,
            height=dp(120)
        )
        instructions.bind(size=instructions.setter('text_size'))
        content.add_widget(instructions)

        # Status label - needs more height for multi-line status messages
        self.status_label = Label(
            text='Searching for devices...',
            font_size=dp(12),
            color=(0.6, 0.6, 0.6, 1),
            halign='center',
            valign='top',
            size_hint_y=None,
            height=dp(100)  # Taller to handle multi-line status text
        )
        self.status_label.bind(size=self.status_label.setter('text_size'))
        content.add_widget(self.status_label)

        self.add_widget(content)
        self.add_widget(BoxLayout())  # Spacer


class MainLayout(BoxLayout):
    """Main application layout"""

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.orientation = 'vertical'
        self.padding = 5
        self.spacing = 5

        # Device
        self.device = None
        self._streaming = False

        # Tabbed panel for different views
        self.tabs = TabbedPanel(do_default_tab=False, tab_pos='top_left')

        # Tab 1: Touch visualizer (with connection overlay)
        touch_tab = TabbedPanelItem(text='Touch')
        self.touch_content = BoxLayout(orientation='vertical', spacing=5, padding=5)
        self.touch_vis = TouchVisualizer()
        self.connection_overlay = ConnectionOverlay()
        # Start with overlay visible
        self.touch_content.add_widget(self.connection_overlay)
        touch_tab.add_widget(self.touch_content)
        self.tabs.add_widget(touch_tab)
        self._touch_tab = touch_tab

        # Tab 2: Device Config (sliders) - shorter name for mobile
        config_tab = TabbedPanelItem(text='Tune')
        config_content = BoxLayout(orientation='vertical', spacing=5, padding=5)

        # Load config button at top
        config_bar = BoxLayout(orientation='horizontal', size_hint_y=None, height=40, spacing=10)
        self.config_label = Label(text='No config', size_hint_x=0.6, halign='left', font_size='13sp')
        self.config_label.bind(size=self.config_label.setter('text_size'))
        load_cfg_btn = Button(text='Load', size_hint_x=0.4)
        load_cfg_btn.bind(on_press=self._on_load_config)
        config_bar.add_widget(self.config_label)
        config_bar.add_widget(load_cfg_btn)
        config_content.add_widget(config_bar)

        # Config sliders
        self.config_panel = ConfigPanel()
        config_content.add_widget(self.config_panel)

        config_tab.add_widget(config_content)
        self.tabs.add_widget(config_tab)

        # Tab 3: Chord layout editor - shorter name
        chord_tab = TabbedPanelItem(text='Chords')
        self.chord_map = ChordMapView()
        chord_tab.add_widget(self.chord_map)
        self.tabs.add_widget(chord_tab)

        # Tab 4: Cheat Sheet - shorter name
        cheatsheet_tab = TabbedPanelItem(text='Cheat')
        self.cheatsheet = CheatSheetView()
        cheatsheet_tab.add_widget(self.cheatsheet)
        self.tabs.add_widget(cheatsheet_tab)

        # Tab 5: Exercise mode - shorter name
        exercise_tab = TabbedPanelItem(text='Learn')
        self.exercise = ExerciseView()
        exercise_tab.add_widget(self.exercise)
        self.tabs.add_widget(exercise_tab)
        self._exercise_tab = exercise_tab

        # Explicitly switch to Touch tab to avoid double-highlight
        def _fix_tabs(dt):
            for th in self.tabs.tab_list:
                th.state = 'normal'
            touch_tab.state = 'down'
            self.tabs.switch_to(touch_tab)
        from kivy.clock import Clock
        Clock.schedule_once(_fix_tabs, 0)

        # Debug tab removed from mobile - too technical for end users
        # Tab 6: Debug (GPIO diagnostics)
        # debug_tab = TabbedPanelItem(text='Debug')
        # debug_content = BoxLayout(orientation='vertical', padding=10, spacing=10)
        # self.gpio_diag = GPIODiagnostics()
        # debug_content.add_widget(self.gpio_diag)
        # debug_content.add_widget(Label(text='GPIO diagnostics'))
        # debug_tab.add_widget(debug_content)
        # self.tabs.add_widget(debug_tab)

        # Set default tab to Cheat Sheet (works without device)
        self.tabs.default_tab = cheatsheet_tab

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

    def _show_touch_visualizer(self):
        """Switch from connection overlay to touch visualizer"""
        if self.connection_overlay.parent:
            self.touch_content.remove_widget(self.connection_overlay)
            self.touch_content.add_widget(self.touch_vis)

    def _show_connection_overlay(self):
        """Switch from touch visualizer to connection overlay"""
        if self.touch_vis.parent:
            self.touch_content.remove_widget(self.touch_vis)
            self.touch_content.add_widget(self.connection_overlay)

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
            device_name = devices[0]

            # On Android, check USB permission first
            if _ANDROID and not NChorderDevice.has_usb_permission(device_name):
                self.connection_overlay.status_label.text = (
                    'Device found!\nPlease grant USB permission...'
                )
                # Request permission - this shows a system dialog
                NChorderDevice.request_usb_permission(device_name)
                return

            self.device = NChorderDevice(device_name)
            if self.device.connect():
                # Show touch visualizer (hide overlay)
                self._show_touch_visualizer()

                # Load device config
                self.config_panel.device = self.device
                self.config_panel._on_save_callback = self._save_with_stream_restart
                self.config_panel.load_from_device()

                # Set device for chord editor
                self.chord_map.device = self.device

                # Auto-start streaming
                print(f"[CONNECT] connected, starting stream", flush=True)
                self._start_stream()
                print(f"[CONNECT] streaming={self._streaming}", flush=True)
            elif _ANDROID:
                # On Android, connect() returning False might mean permission pending
                self.connection_overlay.status_label.text = (
                    'Waiting for USB permission...\nTap "Allow" in the dialog.'
                )
        else:
            # No devices found - show debug info
            usb_status = NChorderDevice.get_usb_status()
            if _ANDROID:
                self.connection_overlay.status_label.text = (
                    f'No nChorder devices found.\n\n'
                    f'Connect via USB-C OTG adapter.\n\n'
                    f'{usb_status}'
                )
            else:
                self.connection_overlay.status_label.text = f'No devices found. {usb_status}'

    def _check_connection(self):
        """Periodically check connection and reconnect if needed"""
        if self.device and self.device.is_connected():
            return  # Still connected

        # Lost connection - show overlay
        if self._streaming:
            self._streaming = False
            self._show_connection_overlay()
            self.connection_overlay.status_label.text = 'Connection lost. Reconnecting...'

        self._auto_connect()

    def _save_with_stream_restart(self, reset=False):
        """Save to flash (or reset), stopping/restarting stream around it."""
        import threading
        was_streaming = self._streaming
        print(f"[SAVE] was_streaming={was_streaming}, connected={self.device.is_connected() if self.device else False}", flush=True)
        self._stop_stream()
        def _do():
            import time
            time.sleep(0.1)
            print(f"[SAVE] after stop, connected={self.device.is_connected() if self.device else False}", flush=True)
            if reset:
                self.device.reset_defaults()
                from kivy.clock import Clock
                Clock.schedule_once(lambda dt: self.config_panel.load_from_device(), 0)
            else:
                self.config_panel._flush_pending_config()
                self.device.save_to_flash()
            if was_streaming:
                time.sleep(0.2)
                ok = self._start_stream()
                print(f"[SAVE] stream restarted={ok}", flush=True)
        threading.Thread(target=_do, daemon=True).start()

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
        if not hasattr(self, '_frame_debug_count'):
            self._frame_debug_count = 0
        self._frame_debug_count += 1
        if self._frame_debug_count <= 3 or (frame.buttons and self._frame_debug_count % 100 == 0):
            print(f"[FRAME] #{self._frame_debug_count} buttons=0x{frame.buttons:04x}", flush=True)
        self.touch_vis.update(frame)
        # Route chord events to exercise view only when Learn tab is active
        if self.tabs.current_tab == self._exercise_tab:
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
