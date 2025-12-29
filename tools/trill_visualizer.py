#!/usr/bin/env python3
"""
Trill Sensor Visualizer

Reads sensor data from nChorder firmware via SEGGER RTT and displays
a live visualization of all Trill sensors.

Usage:
    1. Connect J-Link to the device
    2. Run: python trill_visualizer.py

Requires:
    - pylink (pip install pylink-square)
    - or: JLinkRTTClient running in background

Data format from firmware:
    TRILL:ch,type,init,n[,data...]
    - 2D (Square): TRILL:0,2D,1,2,x0,y0,s0,x1,y1,s1
    - 1D (Bar):    TRILL:1,1D,1,3,p0,s0,p1,s1,p2,s2
"""

import sys
import re
import time
import subprocess
import threading
from collections import defaultdict

# Try to import curses for terminal UI
try:
    import curses
    HAS_CURSES = True
except ImportError:
    HAS_CURSES = False


class TrillVisualizer:
    def __init__(self):
        self.sensors = {
            0: {'type': '2D', 'name': 'Square (Thumbs)', 'init': False, 'touches': []},
            1: {'type': '1D', 'name': 'Bar L (Left col)', 'init': False, 'touches': []},
            2: {'type': '1D', 'name': 'Bar M (Mid col)', 'init': False, 'touches': []},
            3: {'type': '1D', 'name': 'Bar R (Right col)', 'init': False, 'touches': []},
        }
        self.running = True
        self.last_update = time.time()

    def parse_line(self, line):
        """Parse a TRILL: line from RTT output."""
        line = line.strip()
        if not line.startswith('TRILL:'):
            return False

        try:
            parts = line[6:].split(',')
            ch = int(parts[0])
            sensor_type = parts[1]
            init = int(parts[2]) == 1
            num_touches = int(parts[3])

            if ch not in self.sensors:
                return False

            self.sensors[ch]['init'] = init
            self.sensors[ch]['type'] = sensor_type
            self.sensors[ch]['touches'] = []

            idx = 4
            if sensor_type == '2D':
                # x, y, size triplets
                for _ in range(num_touches):
                    if idx + 2 < len(parts):
                        x = int(parts[idx])
                        y = int(parts[idx + 1])
                        size = int(parts[idx + 2])
                        self.sensors[ch]['touches'].append({'x': x, 'y': y, 'size': size})
                        idx += 3
            else:
                # position, size pairs
                for _ in range(num_touches):
                    if idx + 1 < len(parts):
                        pos = int(parts[idx])
                        size = int(parts[idx + 1])
                        self.sensors[ch]['touches'].append({'pos': pos, 'size': size})
                        idx += 2

            self.last_update = time.time()
            return True
        except (ValueError, IndexError) as e:
            return False

    def render_square(self, sensor, width=20, height=10):
        """Render 2D square sensor as ASCII grid."""
        lines = []
        lines.append(f"  {sensor['name']} {'[INIT]' if sensor['init'] else '[----]'}")

        # Create grid
        grid = [[' ' for _ in range(width)] for _ in range(height)]

        # Draw quadrant lines
        mid_x = width // 2
        mid_y = height // 2
        for y in range(height):
            grid[y][mid_x] = '|'
        for x in range(width):
            grid[mid_y][x] = '-'
        grid[mid_y][mid_x] = '+'

        # Plot touches
        for touch in sensor['touches']:
            # Trill Square range is 0-1792 for both X and Y
            tx = int(touch['x'] * (width - 1) / 1792) if touch['x'] < 1792 else width - 1
            ty = int(touch['y'] * (height - 1) / 1792) if touch['y'] < 1792 else height - 1
            tx = max(0, min(width - 1, tx))
            ty = max(0, min(height - 1, ty))

            # Size determines character
            if touch['size'] > 500:
                grid[ty][tx] = '#'
            elif touch['size'] > 300:
                grid[ty][tx] = 'O'
            else:
                grid[ty][tx] = 'o'

        # Add labels
        lines.append("  T1    |    T2  ")
        for row in grid[:mid_y]:
            lines.append("  " + ''.join(row))
        lines.append("  " + ''.join(grid[mid_y]))
        for row in grid[mid_y+1:]:
            lines.append("  " + ''.join(row))
        lines.append("  T3    |    T4  ")

        # Show touch details
        if sensor['touches']:
            for i, t in enumerate(sensor['touches'][:2]):
                lines.append(f"  Touch{i}: x={t['x']:4d} y={t['y']:4d} size={t['size']:4d}")
        else:
            lines.append("  No touches")
            lines.append("")

        return lines

    def render_bar(self, sensor, width=40):
        """Render 1D bar sensor as ASCII line."""
        lines = []
        lines.append(f"  {sensor['name']} {'[INIT]' if sensor['init'] else '[----]'}")

        # Create bar with zone markers
        bar = ['-' for _ in range(width)]
        zone_width = width // 4
        for z in range(1, 4):
            bar[z * zone_width] = '|'

        # Plot touches
        for touch in sensor['touches']:
            # Trill Bar range is 0-3200
            pos = int(touch['pos'] * (width - 1) / 3200) if touch['pos'] < 3200 else width - 1
            pos = max(0, min(width - 1, pos))

            if touch['size'] > 500:
                bar[pos] = '#'
            elif touch['size'] > 300:
                bar[pos] = 'O'
            else:
                bar[pos] = 'o'

        lines.append("  F1   |  F2   |  F3   |  F4   ")
        lines.append("  " + ''.join(bar))

        # Show touch details
        if sensor['touches']:
            details = []
            for i, t in enumerate(sensor['touches'][:3]):
                details.append(f"p={t['pos']:4d} s={t['size']:4d}")
            lines.append("  " + "  ".join(details))
        else:
            lines.append("  No touches")

        return lines

    def render(self):
        """Render all sensors to terminal."""
        output = []
        output.append("=" * 50)
        output.append("  nChorder Trill Sensor Visualizer")
        output.append("  Press Ctrl+C to exit")
        output.append("=" * 50)
        output.append("")

        # Render each sensor based on its actual type
        for ch in range(4):
            sensor = self.sensors[ch]
            if sensor['type'] == '2D':
                output.extend(self.render_square(sensor))
            else:
                output.extend(self.render_bar(sensor))
            output.append("")

        # Status
        age = time.time() - self.last_update
        if age > 2.0:
            output.append(f"  [!] No data for {age:.1f}s - check RTT connection")
        else:
            output.append(f"  Last update: {age:.2f}s ago")

        return output

    def run_simple(self):
        """Simple mode - just print updates."""
        print("Waiting for TRILL data from RTT...")
        print("Run JLinkRTTClient in another terminal and connect to the device")
        print("")

        while self.running:
            try:
                line = input()
                if self.parse_line(line):
                    # Clear screen and redraw
                    print("\033[2J\033[H", end='')
                    for l in self.render():
                        print(l)
            except EOFError:
                break
            except KeyboardInterrupt:
                break


def main():
    print("nChorder Trill Visualizer")
    print("")
    print("To use this tool:")
    print("  1. Start JLinkExe and connect to the device:")
    print("     JLinkExe -device nRF52840_xxAA -if SWD -speed 4000 -autoconnect 1")
    print("")
    print("  2. In JLink prompt, type: r (reset) then g (go)")
    print("")
    print("  3. In another terminal, run:")
    print("     JLinkRTTClient | python tools/trill_visualizer.py")
    print("")
    print("Or pipe RTT output directly:")
    print("  JLinkRTTClient 2>/dev/null | python tools/trill_visualizer.py")
    print("")

    # Check if we have input
    import select
    if select.select([sys.stdin], [], [], 0.0)[0]:
        # Data available on stdin, run in pipe mode
        viz = TrillVisualizer()
        viz.run_simple()
    else:
        print("No input detected. Pipe RTT output to this script.")
        print("")
        print("Example:")
        print("  JLinkRTTClient 2>/dev/null | python tools/trill_visualizer.py")


if __name__ == '__main__':
    main()
