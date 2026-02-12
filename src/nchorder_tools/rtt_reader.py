"""
RTT Reader - Read debug output from nChorder via JLink RTT

Parses TRILL sensor data and NRF_LOG messages from the firmware's RTT output.
Works independently of USB CDC - only requires JLink connection.
"""

import re
import subprocess
import threading
import time
from dataclasses import dataclass, field
from typing import Callable, Optional, List
from queue import Queue, Empty


@dataclass
class TrillTouch2D:
    """2D touch data (for Square sensor)"""
    x: int = 0
    y: int = 0
    size: int = 0


@dataclass
class TrillTouch1D:
    """1D touch data (for Bar sensors)"""
    position: int = 0
    size: int = 0


@dataclass
class TrillSensorData:
    """Parsed sensor data from RTT TRILL: output"""
    channel: int = 0
    is_2d: bool = False
    initialized: bool = False
    num_touches: int = 0
    touches_2d: List[TrillTouch2D] = field(default_factory=list)
    touches_1d: List[TrillTouch1D] = field(default_factory=list)


@dataclass
class GestureEvent:
    """Parsed gesture event from NRF_LOG"""
    event_type: str = ""  # "touch_start", "mouse_mode", "tap", "mouse_end"
    quadrant: int = -1
    x: int = 0
    y: int = 0
    distance: int = 0
    frames: int = 0


@dataclass
class ButtonEvent:
    """Parsed button state from NRF_LOG"""
    raw_mask: int = 0
    button_names: str = ""


@dataclass
class Raw2DEvent:
    """Raw 2D sensor bytes for debugging"""
    header: str = ""
    y_bytes: str = ""
    x_bytes: str = ""
    size_bytes: str = ""


class RTTReader:
    """
    Reads RTT output from nChorder firmware via JLink.

    Parses:
    - TRILL:ch,2D,init,n,x0,y0,s0,... (sensor data)
    - TRILL:ch,1D,init,n,p0,s0,... (sensor data)
    - Gesture: ... (gesture events)
    - Trill raw buttons: ... (button states)
    """

    # Regex patterns for parsing
    TRILL_2D_PATTERN = re.compile(r'TRILL:(\d+),2D,(\d+),(\d+)((?:,\d+,\d+,\d+)*)')
    TRILL_1D_PATTERN = re.compile(r'TRILL:(\d+),1D,(\d+),(\d+)((?:,\d+,\d+)*)')
    GESTURE_START_PATTERN = re.compile(r'Gesture: Touch start at \((\d+),(\d+)\)')
    GESTURE_MOUSE_PATTERN = re.compile(r'Gesture: Mouse mode \(dist=(\d+), frames=(\d+)\)')
    GESTURE_TAP_PATTERN = re.compile(r'Gesture: Tap Q(\d+) at \((\d+),(\d+)\) frames=(\d+)')
    GESTURE_END_PATTERN = re.compile(r'Gesture: Mouse ended \(dist=(\d+)\)')
    BUTTON_PATTERN = re.compile(r'Trill raw buttons: 0x([0-9A-Fa-f]+) \(([^)]+)\)')
    # RAW2D:[header]Y:bytes-X:bytes-S:bytes
    RAW2D_PATTERN = re.compile(r'RAW2D:\[([0-9A-Fa-f]+)\]Y:([0-9A-Fa-f]+)-X:([0-9A-Fa-f]+)-S:([0-9A-Fa-f]+)')

    def __init__(self, device: str = "NRF52840_XXAA"):
        self.device = device
        self._running = False
        self._thread: Optional[threading.Thread] = None
        self._process: Optional[subprocess.Popen] = None
        self._jlink_process: Optional[subprocess.Popen] = None

        # Callbacks
        self.on_sensor_data: Optional[Callable[[TrillSensorData], None]] = None
        self.on_gesture: Optional[Callable[[GestureEvent], None]] = None
        self.on_button: Optional[Callable[[ButtonEvent], None]] = None
        self.on_raw2d: Optional[Callable[[Raw2DEvent], None]] = None
        self.on_raw_line: Optional[Callable[[str], None]] = None

        # Latest state (for polling instead of callbacks)
        self.sensors: List[TrillSensorData] = [TrillSensorData(channel=i) for i in range(4)]
        self.last_gesture: Optional[GestureEvent] = None
        self.last_buttons: ButtonEvent = ButtonEvent()
        self.last_raw2d: Optional[Raw2DEvent] = None

        # Message queue for thread-safe access
        self._message_queue: Queue = Queue(maxsize=1000)

    def start(self) -> bool:
        """Start reading RTT output via JLinkExe + JLinkRTTClient."""
        if self._running:
            return True

        # Start JLinkExe first to provide RTT server
        try:
            # Start JLinkExe in background - it provides the RTT server
            self._jlink_process = subprocess.Popen(
                [
                    'JLinkExe',
                    '-Device', self.device,
                    '-If', 'SWD',
                    '-Speed', '4000',
                    '-AutoConnect', '1',
                ],
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                bufsize=1
            )

            # Give JLinkExe time to connect and start RTT server
            time.sleep(1.5)

            if self._jlink_process.poll() is not None:
                raise RuntimeError("JLinkExe exited unexpectedly")

            # Now connect with JLinkRTTClient
            self._process = subprocess.Popen(
                ['JLinkRTTClient'],
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                bufsize=1
            )

            self._running = True
            self._thread = threading.Thread(target=self._read_loop, daemon=True)
            self._thread.start()

            # Give RTTClient time to connect
            time.sleep(0.5)
            if self._process.poll() is not None:
                raise RuntimeError("JLinkRTTClient exited")

            return True

        except FileNotFoundError:
            print("JLinkExe or JLinkRTTClient not found.")
            print("Install SEGGER J-Link tools from: https://www.segger.com/downloads/jlink/")
            return False
        except Exception as e:
            print(f"Failed to start RTT reader: {e}")
            self.stop()
            return False

    def stop(self):
        """Stop reading RTT output."""
        self._running = False

        if self._process:
            self._process.terminate()
            try:
                self._process.wait(timeout=2)
            except subprocess.TimeoutExpired:
                self._process.kill()
            self._process = None

        if self._jlink_process:
            self._jlink_process.terminate()
            try:
                self._jlink_process.wait(timeout=2)
            except subprocess.TimeoutExpired:
                self._jlink_process.kill()
            self._jlink_process = None

        if self._thread:
            self._thread.join(timeout=1)
            self._thread = None

    def _read_loop(self):
        """Background thread reading RTT output."""
        if not self._process:
            return

        while self._running and self._process.poll() is None:
            try:
                line = self._process.stdout.readline()
                if not line:
                    continue

                line = line.strip()
                if not line:
                    continue

                # Add to message queue
                try:
                    self._message_queue.put_nowait(line)
                except:
                    pass  # Queue full, skip

                # Parse and dispatch
                self._parse_line(line)

            except Exception as e:
                if self._running:
                    print(f"RTT read error: {e}")

    def _parse_line(self, line: str):
        """Parse a single RTT line and dispatch to callbacks."""

        # Raw line callback
        if self.on_raw_line:
            self.on_raw_line(line)

        # Try TRILL 2D pattern
        match = self.TRILL_2D_PATTERN.search(line)
        if match:
            data = self._parse_trill_2d(match)
            self.sensors[data.channel] = data
            if self.on_sensor_data:
                self.on_sensor_data(data)
            return

        # Try TRILL 1D pattern
        match = self.TRILL_1D_PATTERN.search(line)
        if match:
            data = self._parse_trill_1d(match)
            self.sensors[data.channel] = data
            if self.on_sensor_data:
                self.on_sensor_data(data)
            return

        # Try gesture patterns
        match = self.GESTURE_TAP_PATTERN.search(line)
        if match:
            event = GestureEvent(
                event_type="tap",
                quadrant=int(match.group(1)),
                x=int(match.group(2)),
                y=int(match.group(3)),
                frames=int(match.group(4))
            )
            self.last_gesture = event
            if self.on_gesture:
                self.on_gesture(event)
            return

        match = self.GESTURE_MOUSE_PATTERN.search(line)
        if match:
            event = GestureEvent(
                event_type="mouse_mode",
                distance=int(match.group(1)),
                frames=int(match.group(2))
            )
            self.last_gesture = event
            if self.on_gesture:
                self.on_gesture(event)
            return

        match = self.GESTURE_START_PATTERN.search(line)
        if match:
            event = GestureEvent(
                event_type="touch_start",
                x=int(match.group(1)),
                y=int(match.group(2))
            )
            self.last_gesture = event
            if self.on_gesture:
                self.on_gesture(event)
            return

        match = self.GESTURE_END_PATTERN.search(line)
        if match:
            event = GestureEvent(
                event_type="mouse_end",
                distance=int(match.group(1))
            )
            self.last_gesture = event
            if self.on_gesture:
                self.on_gesture(event)
            return

        # Try button pattern
        match = self.BUTTON_PATTERN.search(line)
        if match:
            event = ButtonEvent(
                raw_mask=int(match.group(1), 16),
                button_names=match.group(2)
            )
            self.last_buttons = event
            if self.on_button:
                self.on_button(event)
            return

        # Try RAW2D pattern (debug output from Square sensor)
        match = self.RAW2D_PATTERN.search(line)
        if match:
            event = Raw2DEvent(
                header=match.group(1),
                y_bytes=match.group(2),
                x_bytes=match.group(3),
                size_bytes=match.group(4)
            )
            self.last_raw2d = event
            if self.on_raw2d:
                self.on_raw2d(event)
            return

    def _parse_trill_2d(self, match) -> TrillSensorData:
        """Parse TRILL 2D sensor data."""
        ch = int(match.group(1))
        init = int(match.group(2)) != 0
        num = int(match.group(3))

        touches = []
        if match.group(4):
            values = [int(v) for v in match.group(4).split(',') if v]
            for i in range(0, len(values), 3):
                if i + 2 < len(values):
                    touches.append(TrillTouch2D(
                        x=values[i],
                        y=values[i+1],
                        size=values[i+2]
                    ))

        return TrillSensorData(
            channel=ch,
            is_2d=True,
            initialized=init,
            num_touches=num,
            touches_2d=touches
        )

    def _parse_trill_1d(self, match) -> TrillSensorData:
        """Parse TRILL 1D sensor data."""
        ch = int(match.group(1))
        init = int(match.group(2)) != 0
        num = int(match.group(3))

        touches = []
        if match.group(4):
            values = [int(v) for v in match.group(4).split(',') if v]
            for i in range(0, len(values), 2):
                if i + 1 < len(values):
                    touches.append(TrillTouch1D(
                        position=values[i],
                        size=values[i+1]
                    ))

        return TrillSensorData(
            channel=ch,
            is_2d=False,
            initialized=init,
            num_touches=num,
            touches_1d=touches
        )

    def get_messages(self, max_count: int = 100) -> List[str]:
        """Get pending messages from the queue."""
        messages = []
        for _ in range(max_count):
            try:
                messages.append(self._message_queue.get_nowait())
            except Empty:
                break
        return messages


def main():
    """Test RTT reader."""
    reader = RTTReader()

    def on_sensor(data: TrillSensorData):
        if data.num_touches > 0:
            if data.is_2d:
                t = data.touches_2d[0]
                print(f"Ch{data.channel} 2D: ({t.x}, {t.y}) size={t.size}")
            else:
                t = data.touches_1d[0]
                print(f"Ch{data.channel} 1D: pos={t.position} size={t.size}")

    def on_gesture(event: GestureEvent):
        print(f"Gesture: {event}")

    def on_button(event: ButtonEvent):
        print(f"Buttons: 0x{event.raw_mask:04X} = {event.button_names}")

    reader.on_sensor_data = on_sensor
    reader.on_gesture = on_gesture
    reader.on_button = on_button

    if reader.start():
        print("RTT reader started. Press Ctrl+C to stop.")
        try:
            while True:
                time.sleep(1)
        except KeyboardInterrupt:
            pass
        reader.stop()
    else:
        print("Failed to start RTT reader")


if __name__ == '__main__':
    main()
