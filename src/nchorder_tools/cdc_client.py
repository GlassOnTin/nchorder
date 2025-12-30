"""
Northern Chorder - USB CDC Client

Provides communication with the nChorder firmware via USB CDC serial.
Supports configuration, touch streaming, and chord upload.
"""

import struct
import threading
import time
from dataclasses import dataclass
from enum import IntEnum
from typing import Callable, Optional, List

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    serial = None


class CDCCommand(IntEnum):
    """CDC protocol command codes"""
    GET_VERSION = 0x01
    GET_TOUCHES = 0x02
    GET_CONFIG = 0x03
    SET_CONFIG = 0x04
    GET_CHORDS = 0x05
    SET_CHORDS = 0x06
    SAVE_FLASH = 0x07
    LOAD_FLASH = 0x08
    RESET_DEFAULT = 0x09
    STREAM_START = 0x10
    STREAM_STOP = 0x11


class CDCResponse(IntEnum):
    """CDC protocol response codes"""
    ACK = 0x06
    NAK = 0x15
    ERROR = 0xFF


class ConfigID(IntEnum):
    """Configuration parameter IDs for SET_CONFIG"""
    THRESHOLD_PRESS = 0x01
    THRESHOLD_RELEASE = 0x02
    DEBOUNCE_MS = 0x03
    POLL_RATE_MS = 0x04
    MOUSE_SPEED = 0x05
    MOUSE_ACCEL = 0x06
    VOLUME_SENSITIVITY = 0x07


# Stream sync byte
STREAM_SYNC = 0xAA

# Touch frame size (21 bytes)
TOUCH_FRAME_SIZE = 21


@dataclass
class TouchFrame:
    """Touch sensor data frame from streaming"""
    thumb_x: int        # 0-3200
    thumb_y: int        # 0-3200
    thumb_size: int     # Touch pressure/size
    bar0_pos: int       # Left column position
    bar0_size: int
    bar1_pos: int       # Middle column position
    bar1_size: int
    bar2_pos: int       # Right column position
    bar2_size: int
    buttons: int        # 16-bit button bitmask

    @classmethod
    def from_bytes(cls, data: bytes) -> 'TouchFrame':
        """Parse touch frame from 21-byte packet"""
        if len(data) != TOUCH_FRAME_SIZE:
            raise ValueError(f"Expected {TOUCH_FRAME_SIZE} bytes, got {len(data)}")
        if data[0] != STREAM_SYNC:
            raise ValueError(f"Invalid sync byte: 0x{data[0]:02X}")

        values = struct.unpack('<HHHHHHHHHH', data[1:])
        return cls(
            thumb_x=values[0],
            thumb_y=values[1],
            thumb_size=values[2],
            bar0_pos=values[3],
            bar0_size=values[4],
            bar1_pos=values[5],
            bar1_size=values[6],
            bar2_pos=values[7],
            bar2_size=values[8],
            buttons=values[9]
        )


@dataclass
class DeviceConfig:
    """Runtime configuration from device"""
    threshold_press: int = 500
    threshold_release: int = 250
    debounce_ms: int = 30
    poll_rate_ms: int = 15
    mouse_speed: int = 10
    mouse_accel: int = 3
    volume_sensitivity: int = 5

    @classmethod
    def from_bytes(cls, data: bytes) -> 'DeviceConfig':
        """Parse config from 22-byte response (7 fields + 4 reserved)"""
        if len(data) < 14:  # Minimum: 7 fields * 2 bytes
            raise ValueError(f"Config response too short: {len(data)} bytes")

        values = struct.unpack('<HHHHHHH', data[:14])
        return cls(
            threshold_press=values[0],
            threshold_release=values[1],
            debounce_ms=values[2],
            poll_rate_ms=values[3],
            mouse_speed=values[4],
            mouse_accel=values[5],
            volume_sensitivity=values[6]
        )


@dataclass
class DeviceVersion:
    """Firmware version information"""
    major: int
    minor: int
    hw_rev: int

    @classmethod
    def from_bytes(cls, data: bytes) -> 'DeviceVersion':
        """Parse version from 3-byte response"""
        if len(data) != 3:
            raise ValueError(f"Expected 3 bytes, got {len(data)}")
        return cls(major=data[0], minor=data[1], hw_rev=data[2])

    def __str__(self) -> str:
        return f"v{self.major}.{self.minor} (hw rev {self.hw_rev})"


class NChorderDevice:
    """
    USB CDC communication with nChorder firmware.

    Example usage:
        device = NChorderDevice()
        if device.connect():
            print(device.get_version())
            config = device.get_config()
            device.set_mouse_speed(15)
            device.start_stream(callback=on_touch)
            ...
            device.disconnect()
    """

    # USB VID/PID for Nordic nRF52840
    VID = 0x1915
    PID = 0x520F

    def __init__(self, port: Optional[str] = None):
        """
        Initialize device connection.

        Args:
            port: Serial port path (e.g., '/dev/ttyACM2').
                  If None, will auto-detect nChorder device.
        """
        if serial is None:
            raise ImportError("pyserial is required: pip install pyserial")

        self._port_path = port
        self._serial: Optional[serial.Serial] = None
        self._streaming = False
        self._stream_thread: Optional[threading.Thread] = None
        self._stream_callback: Optional[Callable[[TouchFrame], None]] = None
        self._stop_event = threading.Event()

    @classmethod
    def find_devices(cls) -> List[str]:
        """Find all connected nChorder devices."""
        if serial is None:
            return []

        devices = []
        for port in serial.tools.list_ports.comports():
            if port.vid == cls.VID and port.pid == cls.PID:
                devices.append(port.device)
        return devices

    def connect(self, timeout: float = 1.0) -> bool:
        """
        Connect to the device.

        Args:
            timeout: Read timeout in seconds

        Returns:
            True if connection successful
        """
        if self._serial and self._serial.is_open:
            return True

        port = self._port_path
        if port is None:
            devices = self.find_devices()
            if not devices:
                return False
            port = devices[0]

        try:
            self._serial = serial.Serial(
                port=port,
                baudrate=115200,  # Ignored for USB CDC
                timeout=timeout,
                write_timeout=timeout
            )
            self._port_path = port
            return True
        except serial.SerialException:
            return False

    def disconnect(self) -> None:
        """Disconnect from the device."""
        self.stop_stream()
        if self._serial:
            self._serial.close()
            self._serial = None

    def is_connected(self) -> bool:
        """Check if device is connected."""
        return self._serial is not None and self._serial.is_open

    def _send_command(self, cmd: int, data: bytes = b'') -> Optional[bytes]:
        """
        Send command and read response.

        Args:
            cmd: Command byte
            data: Optional command payload

        Returns:
            Response bytes or None on error
        """
        if not self.is_connected():
            return None

        try:
            self._serial.reset_input_buffer()
            self._serial.write(bytes([cmd]) + data)
            return self._serial.read(64)
        except serial.SerialException:
            return None

    def get_version(self) -> Optional[DeviceVersion]:
        """Get firmware version."""
        response = self._send_command(CDCCommand.GET_VERSION)
        if response and len(response) == 3:
            return DeviceVersion.from_bytes(response)
        return None

    def get_config(self) -> Optional[DeviceConfig]:
        """Get current device configuration."""
        response = self._send_command(CDCCommand.GET_CONFIG)
        if response and len(response) >= 14:
            return DeviceConfig.from_bytes(response)
        return None

    def set_config(self, config_id: ConfigID, value: int) -> bool:
        """
        Set a configuration parameter.

        Args:
            config_id: Configuration parameter ID
            value: New value (0-65535)

        Returns:
            True if acknowledged
        """
        data = struct.pack('<BH', config_id, value)
        response = self._send_command(CDCCommand.SET_CONFIG, data)
        return response and len(response) >= 1 and response[0] == CDCResponse.ACK

    def set_threshold_press(self, value: int) -> bool:
        """Set touch press threshold (100-1000)."""
        return self.set_config(ConfigID.THRESHOLD_PRESS, value)

    def set_threshold_release(self, value: int) -> bool:
        """Set touch release threshold (50-500)."""
        return self.set_config(ConfigID.THRESHOLD_RELEASE, value)

    def set_debounce(self, value: int) -> bool:
        """Set debounce time in ms (10-100)."""
        return self.set_config(ConfigID.DEBOUNCE_MS, value)

    def set_poll_rate(self, value: int) -> bool:
        """Set sensor poll rate in ms (5-50)."""
        return self.set_config(ConfigID.POLL_RATE_MS, value)

    def set_mouse_speed(self, value: int) -> bool:
        """Set mouse movement speed (1-20)."""
        return self.set_config(ConfigID.MOUSE_SPEED, value)

    def set_mouse_accel(self, value: int) -> bool:
        """Set mouse acceleration (0-10)."""
        return self.set_config(ConfigID.MOUSE_ACCEL, value)

    def set_volume_sensitivity(self, value: int) -> bool:
        """Set volume gesture sensitivity (1-10)."""
        return self.set_config(ConfigID.VOLUME_SENSITIVITY, value)

    def reset_defaults(self) -> bool:
        """Reset configuration to defaults."""
        response = self._send_command(CDCCommand.RESET_DEFAULT)
        return response and len(response) >= 1 and response[0] == CDCResponse.ACK

    def get_touches(self) -> Optional[TouchFrame]:
        """Get single touch frame (non-streaming)."""
        response = self._send_command(CDCCommand.GET_TOUCHES)
        if response and len(response) == TOUCH_FRAME_SIZE:
            try:
                return TouchFrame.from_bytes(response)
            except ValueError:
                return None
        return None

    def start_stream(
        self,
        callback: Callable[[TouchFrame], None],
        rate_hz: int = 60
    ) -> bool:
        """
        Start touch data streaming.

        Args:
            callback: Function called with each TouchFrame
            rate_hz: Stream rate in Hz (1-100)

        Returns:
            True if streaming started
        """
        if self._streaming:
            return True

        rate = max(1, min(100, rate_hz))
        response = self._send_command(CDCCommand.STREAM_START, bytes([rate]))
        if not response or response[0] != CDCResponse.ACK:
            return False

        self._stream_callback = callback
        self._streaming = True
        self._stop_event.clear()

        self._stream_thread = threading.Thread(
            target=self._stream_loop,
            daemon=True
        )
        self._stream_thread.start()
        return True

    def stop_stream(self) -> None:
        """Stop touch data streaming."""
        if not self._streaming:
            return

        self._stop_event.set()
        self._streaming = False

        if self._stream_thread:
            self._stream_thread.join(timeout=1.0)
            self._stream_thread = None

        # Send stop command
        self._send_command(CDCCommand.STREAM_STOP)

    def _stream_loop(self) -> None:
        """Background thread for reading stream data."""
        buffer = bytearray()

        while not self._stop_event.is_set():
            try:
                data = self._serial.read(64)
                if not data:
                    continue

                buffer.extend(data)

                # Process complete frames
                while len(buffer) >= TOUCH_FRAME_SIZE:
                    # Find sync byte
                    sync_idx = buffer.find(STREAM_SYNC)
                    if sync_idx < 0:
                        buffer.clear()
                        break
                    if sync_idx > 0:
                        # Discard bytes before sync
                        del buffer[:sync_idx]
                        continue

                    if len(buffer) < TOUCH_FRAME_SIZE:
                        break

                    # Extract frame
                    frame_data = bytes(buffer[:TOUCH_FRAME_SIZE])
                    del buffer[:TOUCH_FRAME_SIZE]

                    try:
                        frame = TouchFrame.from_bytes(frame_data)
                        if self._stream_callback:
                            self._stream_callback(frame)
                    except ValueError:
                        pass

            except serial.SerialException:
                break

    # Chord management (TODO: implement in firmware)

    def get_chords(self, offset: int = 0, count: int = 100) -> Optional[bytes]:
        """Get chord data from device."""
        data = struct.pack('<HH', offset, count)
        response = self._send_command(CDCCommand.GET_CHORDS, data)
        if response and response[0] != CDCResponse.NAK:
            return response
        return None

    def set_chords(self, data: bytes, offset: int = 0) -> bool:
        """Upload chord data to device."""
        header = struct.pack('<HH', offset, len(data) // 8)  # Assume 8 bytes per chord
        response = self._send_command(CDCCommand.SET_CHORDS, header + data)
        return response and len(response) >= 1 and response[0] == CDCResponse.ACK

    def save_to_flash(self) -> bool:
        """Save configuration to flash."""
        response = self._send_command(CDCCommand.SAVE_FLASH)
        return response and len(response) >= 1 and response[0] == CDCResponse.ACK

    def load_from_flash(self) -> bool:
        """Load configuration from flash."""
        response = self._send_command(CDCCommand.LOAD_FLASH)
        return response and len(response) >= 1 and response[0] == CDCResponse.ACK


def main():
    """CLI test interface."""
    device = NChorderDevice()

    print("Looking for nChorder devices...")
    devices = NChorderDevice.find_devices()
    if not devices:
        print("No devices found. Check USB connection.")
        return

    print(f"Found: {devices}")
    if device.connect():
        print("Connected!")

        version = device.get_version()
        if version:
            print(f"Firmware: {version}")

        config = device.get_config()
        if config:
            print(f"Config: {config}")

        # Test single touch read
        touch = device.get_touches()
        if touch:
            print(f"Touch: {touch}")

        device.disconnect()
    else:
        print("Connection failed.")


if __name__ == '__main__':
    main()
