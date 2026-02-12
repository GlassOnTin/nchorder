#!/usr/bin/env python3
"""
Mobile Preview - Test GUI layout at mobile screen dimensions

Renders the nChorder GUI at Pixel 8 Pro screen dimensions and takes
screenshots of each panel. This allows verifying mobile layout without
deploying to a device.

Usage:
    python src/nchorder_tools/gui/mobile_preview.py [--portrait] [--landscape]

Screenshots are saved to: ./mobile_preview/
"""

import os
import sys
from pathlib import Path

# Must set before ANY kivy imports
os.environ['KIVY_NO_ARGS'] = '1'
os.environ['KIVY_METRICS_DENSITY'] = '3'  # Pixel 8 Pro is ~3x density

# Pixel 8 Pro dimensions
# Physical: 1344 x 2992 pixels
# At 3x density: 448dp x 997dp
PIXEL_8_PRO_WIDTH_PX = 1344
PIXEL_8_PRO_HEIGHT_PX = 2992
PIXEL_8_PRO_DENSITY = 3

# Parse orientation from sys.argv before Kivy can see it
PORTRAIT = '--landscape' not in sys.argv or '--portrait' in sys.argv
LANDSCAPE = '--landscape' in sys.argv

# Scale down for display (3x is too large for most monitors)
DISPLAY_SCALE = 0.5

if PORTRAIT:
    width_px = PIXEL_8_PRO_WIDTH_PX
    height_px = PIXEL_8_PRO_HEIGHT_PX
else:
    width_px = PIXEL_8_PRO_HEIGHT_PX
    height_px = PIXEL_8_PRO_WIDTH_PX

display_width = int(width_px * DISPLAY_SCALE)
display_height = int(height_px * DISPLAY_SCALE)

# Configure window size before importing Kivy
from kivy.config import Config
Config.set('graphics', 'width', str(display_width))
Config.set('graphics', 'height', str(display_height))
Config.set('graphics', 'resizable', '0')

# Now import Kivy and app components
from kivy.app import App
from kivy.core.window import Window
from kivy.clock import Clock
from kivy.metrics import Metrics

# Add src to path if not already there
src_dir = Path(__file__).parent.parent.parent
if str(src_dir) not in sys.path:
    sys.path.insert(0, str(src_dir))

# Import after environment setup
from nchorder_tools.gui.app import MainLayout


class MobilePreviewApp(App):
    """Preview app that takes screenshots of each panel"""

    def __init__(self, output_dir='mobile_preview', orientation='portrait', **kwargs):
        super().__init__(**kwargs)
        self.output_dir = Path(output_dir)
        self.orientation = orientation
        self.screenshot_count = 0
        self.tab_names = ['Touch', 'Tune', 'Chords', 'Cheat', 'Learn']
        self.current_tab_index = 0

    def build(self):
        self.title = f'Northern Chorder - Mobile Preview ({self.orientation})'
        self.root = MainLayout()

        # Create output directory
        self.output_dir.mkdir(exist_ok=True)

        # Print info
        print(f"\n=== Mobile Preview Mode ===")
        print(f"Simulating: Pixel 8 Pro ({self.orientation})")
        print(f"Window: {Window.width}x{Window.height} px (scaled 0.5x for display)")
        print(f"Density: {Metrics.density}")
        print(f"DPI: {Metrics.dpi}")
        print(f"Output: {self.output_dir.absolute()}")
        print()

        # Schedule screenshot capture for each tab
        Clock.schedule_once(self._start_screenshots, 1.0)

        return self.root

    def _start_screenshots(self, dt):
        """Start taking screenshots of each tab"""
        self._capture_current_tab()

    def _capture_current_tab(self):
        """Capture screenshot of current tab and move to next"""
        if self.current_tab_index >= len(self.tab_names):
            print(f"\nDone! {self.screenshot_count} screenshots saved to {self.output_dir}/")
            print("Review the images to verify mobile layout.\n")
            App.get_running_app().stop()
            return

        tab_name = self.tab_names[self.current_tab_index]

        # Switch to this tab
        tabs = self.root.tabs
        for tab in tabs.tab_list:
            if tab.text == tab_name:
                tabs.switch_to(tab)
                break

        # Wait for render, then capture
        Clock.schedule_once(lambda dt: self._do_capture(tab_name), 0.5)

    def _do_capture(self, tab_name):
        """Actually capture the screenshot"""
        filename = f"{self.orientation}_{tab_name.lower()}.png"
        filepath = self.output_dir / filename

        # Export screenshot
        Window.screenshot(name=str(filepath))
        self.screenshot_count += 1
        print(f"  [{self.screenshot_count}/5] Captured: {filename}")

        # Move to next tab
        self.current_tab_index += 1
        Clock.schedule_once(lambda dt: self._capture_current_tab(), 0.3)


def main():
    """Main entry point"""
    orientation = 'landscape' if LANDSCAPE and not PORTRAIT else 'portrait'

    # Reconfigure for orientation (if different from initial)
    if orientation == 'landscape':
        w = int(PIXEL_8_PRO_HEIGHT_PX * DISPLAY_SCALE)
        h = int(PIXEL_8_PRO_WIDTH_PX * DISPLAY_SCALE)
        Window.size = (w, h)

    app = MobilePreviewApp(orientation=orientation)
    app.run()


if __name__ == '__main__':
    main()
