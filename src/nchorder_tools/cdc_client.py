"""
Northern Chorder - USB CDC Client

Provides communication with the nChorder firmware via USB CDC serial.
Supports configuration, touch streaming, and chord upload.

Platform Support:
- Desktop (Windows/Linux/macOS): Uses pyserial
- Android: Uses usb4a/usbserial4a (requires USB host mode)
"""

import os
import struct
import threading
import time
from dataclasses import dataclass
from enum import IntEnum
from typing import Callable, Optional, List

# Platform detection - try multiple methods
_ANDROID = False
try:
    from kivy.utils import platform
    _ANDROID = platform == 'android'
except ImportError:
    _ANDROID = os.environ.get('NCHORDER_SERIAL_BACKEND') == 'android' or \
               'ANDROID_STORAGE' in os.environ

serial = None
usb_serial = None
_usb4a_error = None  # Track import errors for debugging

if _ANDROID:
    # Android: use usb4a/usbserial4a
    try:
        from usb4a import usb
        from usbserial4a import serial4a
        usb_serial = serial4a
    except ImportError as e:
        _usb4a_error = str(e)
else:
    # Desktop: use pyserial
    try:
        import serial
        import serial.tools.list_ports
    except ImportError:
        pass

# Serial error base class — pyserial's SerialException on desktop,
# OSError on Android where pyserial isn't available
_SerialError = getattr(serial, 'SerialException', OSError)


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
    # Config upload commands (chunked transfer)
    UPLOAD_START = 0x12
    UPLOAD_DATA = 0x13
    UPLOAD_COMMIT = 0x14
    UPLOAD_ABORT = 0x15


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

# Touch frame size (71 bytes) - multitouch support
# 1 sync + 6 thumb + 60 bars (3 bars * 5 touches * 4 bytes) + 4 buttons
TOUCH_FRAME_SIZE = 71
MAX_BAR_TOUCHES = 5


@dataclass
class BarTouch:
    """Single touch on a bar sensor"""
    pos: int    # Position (0xFFFF = no touch)
    size: int   # Touch size/pressure


@dataclass
class TouchFrame:
    """Touch sensor data frame from streaming"""
    thumb_x: int        # 0-1800 (or 0x1234 = GPIO driver marker)
    thumb_y: int        # 0-1800 (or callback_count for GPIO)
    thumb_size: int     # Touch pressure/size (or raw_buttons low 16)
    bar0: list          # List of BarTouch (up to 5)
    bar1: list          # List of BarTouch (up to 5)
    bar2: list          # List of BarTouch (up to 5)
    buttons: int        # 32-bit button bitmask (20 buttons used)
    # Raw values for diagnostics (GPIO mode)
    _raw_values: tuple = None

    @classmethod
    def from_bytes(cls, data: bytes) -> 'TouchFrame':
        """Parse touch frame from 71-byte packet"""
        if len(data) != TOUCH_FRAME_SIZE:
            raise ValueError(f"Expected {TOUCH_FRAME_SIZE} bytes, got {len(data)}")
        if data[0] != STREAM_SYNC:
            raise ValueError(f"Invalid sync byte: 0x{data[0]:02X}")

        # Unpack all values: 3 thumb + 30 bar touches + 1 buttons (uint32)
        values = struct.unpack('<HHH' + 'HH' * 15 + 'I', data[1:])

        # Parse bar touches (5 per bar, pos+size each)
        def parse_bar(start_idx):
            touches = []
            for i in range(MAX_BAR_TOUCHES):
                pos = values[start_idx + i * 2]
                size = values[start_idx + i * 2 + 1]
                if pos != 0xFFFF:  # Valid touch
                    touches.append(BarTouch(pos=pos, size=size))
            return touches

        frame = cls(
            thumb_x=values[0],
            thumb_y=values[1],
            thumb_size=values[2],
            bar0=parse_bar(3),       # Starts at index 3
            bar1=parse_bar(13),      # 3 + 10 (5 touches * 2 values)
            bar2=parse_bar(23),      # 13 + 10
            buttons=values[33],      # Last value (uint32)
            _raw_values=values
        )
        return frame

    def is_gpio_driver(self) -> bool:
        """Check if this frame is from GPIO driver (marker 0x1234)"""
        return self.thumb_x == 0x1234

    def get_gpio_diagnostics(self) -> dict:
        """
        Extract GPIO diagnostics from frame (only valid for GPIO driver).

        Returns dict with:
        - callback_count: Number of button callbacks
        - raw_buttons: Current raw button state (before debounce)
        - raw_p0: Raw NRF_P0->IN register value
        - raw_p1: Raw NRF_P1->IN register value
        - prev_raw_state: Previous raw button state
        - debounce_count: Current debounce counter
        """
        if not self._raw_values or not self.is_gpio_driver():
            return {}

        v = self._raw_values
        # Bar0[0]: P0 state, Bar0[1]: raw buttons high + debounce
        # Bar1[0]: P1 state, Bar1[1]: prev raw state
        p0_low = v[3]    # bar0[0].pos
        p0_high = v[4]   # bar0[0].size
        raw_btn_high = v[5]  # bar0[1].pos
        debounce = v[6]  # bar0[1].size
        p1_low = v[13]   # bar1[0].pos
        p1_high = v[14]  # bar1[0].size
        prev_low = v[15] # bar1[1].pos
        prev_high = v[16] # bar1[1].size

        return {
            'callback_count': self.thumb_y,
            'raw_buttons': (raw_btn_high << 16) | self.thumb_size,
            'raw_p0': (p0_high << 16) | p0_low,
            'raw_p1': (p1_high << 16) | p1_low,
            'prev_raw_state': (prev_high << 16) | prev_low,
            'debounce_count': debounce,
        }


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
        if not _ANDROID and serial is None:
            raise ImportError("pyserial is required: pip install pyserial")
        if _ANDROID and usb_serial is None:
            raise ImportError("usbserial4a is required for Android")

        self._port_path = port
        self._serial = None
        self._streaming = False
        self._stream_thread: Optional[threading.Thread] = None
        self._stream_callback: Optional[Callable[[TouchFrame], None]] = None
        self._stop_event = threading.Event()
        self._is_android = _ANDROID

    @classmethod
    def get_usb_status(cls) -> str:
        """Get USB subsystem status for debugging."""
        if _ANDROID:
            if usb_serial is None:
                return f"Android USB: usb4a not available ({_usb4a_error or 'not imported'})"
            try:
                from usb4a import usb
                device_list = usb.get_usb_device_list()
                if not device_list:
                    return "Android USB: No devices found"
                info = []
                for d in device_list:
                    info.append(f"{d.getDeviceName()}: VID={hex(d.getVendorId())} PID={hex(d.getProductId())}")
                return f"Android USB: {len(device_list)} device(s): " + ", ".join(info)
            except Exception as e:
                return f"Android USB error: {e}"
        else:
            if serial is None:
                return "Desktop: pyserial not available"
            ports = list(serial.tools.list_ports.comports())
            if not ports:
                return "Desktop: No serial ports"
            return f"Desktop: {len(ports)} port(s)"

    @classmethod
    def find_devices(cls) -> List[str]:
        """Find all connected nChorder devices."""
        if _ANDROID:
            # Android: use usb4a to enumerate devices
            if usb_serial is None:
                return []
            try:
                from usb4a import usb
                device_list = usb.get_usb_device_list()
                devices = []
                for device in device_list:
                    # Match by VID/PID - Nordic nRF52840 USB
                    if device.getVendorId() == cls.VID and device.getProductId() == cls.PID:
                        devices.append(device.getDeviceName())
                return devices
            except Exception:
                return []
        else:
            # Desktop: use pyserial
            if serial is None:
                return []
            candidates = []
            for port in serial.tools.list_ports.comports():
                if port.vid == cls.VID and port.pid == cls.PID:
                    candidates.append(port.device)
            if len(candidates) <= 1:
                return candidates
            # Multiple ports with same VID/PID (can happen after reflash).
            # Probe each with GET_VERSION to find the live one.
            devices = []
            for port_path in candidates:
                try:
                    s = serial.Serial(port_path, 115200, timeout=0.3,
                                      write_timeout=0.3)
                    s.reset_input_buffer()
                    s.write(bytes([CDCCommand.GET_VERSION]))
                    resp = s.read(3)
                    s.close()
                    if resp and len(resp) == 3:
                        devices.append(port_path)
                except Exception:
                    pass
            return devices if devices else candidates

    @classmethod
    def has_usb_permission(cls, device_name: str) -> bool:
        """Check if we have USB permission for the device (Android only)."""
        if not _ANDROID:
            return True  # Desktop doesn't need explicit permission
        try:
            from usb4a import usb
            device = usb.get_usb_device(device_name)
            if device is None:
                return False
            return usb.has_usb_device_permission(device)
        except Exception:
            return False

    @classmethod
    def request_usb_permission(cls, device_name: str) -> bool:
        """
        Request USB permission for device (Android only).

        Bypasses usb4a's request_usb_device_permission() which uses
        FLAG_IMMUTABLE — broken on Android 12+ because the USB system
        needs FLAG_MUTABLE to write EXTRA_PERMISSION_GRANTED into the
        PendingIntent result.

        Returns True if permission request was initiated (not if granted).
        Call has_usb_permission() after user responds to check result.
        """
        if not _ANDROID:
            return True  # Desktop doesn't need explicit permission
        try:
            from jnius import autoclass
            from usb4a import usb

            device = usb.get_usb_device(device_name)
            if device is None:
                return False

            PythonActivity = autoclass('org.kivy.android.PythonActivity')
            Context = autoclass('android.content.Context')
            Intent = autoclass('android.content.Intent')
            PendingIntent = autoclass('android.app.PendingIntent')

            context = PythonActivity.mActivity
            usb_manager = context.getSystemService(Context.USB_SERVICE)

            intent = Intent('org.nchorder.USB_PERMISSION')
            intent.setPackage(context.getPackageName())

            # FLAG_MUTABLE (1 << 25) so system can add EXTRA_PERMISSION_GRANTED
            pintent = PendingIntent.getBroadcast(
                context, 0, intent, 1 << 25
            )
            usb_manager.requestPermission(device, pintent)
            return True
        except Exception:
            return False

    def connect(self, timeout: float = 1.0) -> bool:
        """
        Connect to the device.

        Args:
            timeout: Read timeout in seconds

        Returns:
            True if connection successful
        """
        if self._serial and (not hasattr(self._serial, 'is_open') or self._serial.is_open):
            return True

        port = self._port_path
        if port is None:
            devices = self.find_devices()
            if not devices:
                return False
            port = devices[0]

        try:
            if self._is_android:
                # Android USB serial - check permission first
                from usb4a import usb
                device = usb.get_usb_device(port)
                if device is None:
                    return False
                # Check if we have permission
                if not usb.has_usb_device_permission(device):
                    # Request permission - this shows a dialog to the user
                    usb.request_usb_device_permission(device)
                    return False  # Can't connect yet, waiting for permission
                self._serial = usb_serial.get_serial_port(
                    port,
                    baudrate=115200,
                    timeout=timeout
                )
                if self._serial is None:
                    return False
                self._serial.open()
            else:
                # Desktop pyserial
                self._serial = serial.Serial(
                    port=port,
                    baudrate=115200,  # Ignored for USB CDC
                    timeout=timeout,
                    write_timeout=timeout
                )
            self._port_path = port
            return True
        except Exception:
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
        except _SerialError:
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
            True if acknowledged (or True if sent while streaming)
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

            except _SerialError:
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
        """Save configuration to flash (may take up to 5s for FDS)."""
        if not self.is_connected():
            return False
        try:
            old_timeout = self._serial.timeout
            self._serial.timeout = 6.0  # FDS write can take up to 5s
            response = self._send_command(CDCCommand.SAVE_FLASH)
            self._serial.timeout = old_timeout
            print(f"[SAVE_FLASH] response={response.hex() if response else None} len={len(response) if response else 0}", flush=True)
            return response and len(response) >= 1 and response[0] == CDCResponse.ACK
        except Exception as e:
            print(f"[SAVE_FLASH] exception: {e}", flush=True)
            return False

    def load_from_flash(self) -> bool:
        """Load configuration from flash."""
        response = self._send_command(CDCCommand.LOAD_FLASH)
        return response and len(response) >= 1 and response[0] == CDCResponse.ACK

    # Config file upload

    def upload_config(self, config_data: bytes, chunk_size: int = 60,
                       save_to_flash: bool = True) -> bool:
        """
        Upload complete config file to device.

        Uses chunked transfer protocol:
        1. UPLOAD_START with total size
        2. Multiple UPLOAD_DATA with chunks
        3. UPLOAD_COMMIT to parse and activate
        4. SAVE_FLASH to persist (optional, default True)

        Args:
            config_data: Raw binary config file (.cfg)
            chunk_size: Bytes per chunk (max 63, default 60)
            save_to_flash: If True, persist to flash after commit (default True)

        Returns:
            True if upload successful
        """
        if not self.is_connected():
            return False

        total_size = len(config_data)
        print(f"[UPLOAD] total_size={total_size}, connected={self.is_connected()}")
        if total_size == 0 or total_size > 4096:
            print(f"[UPLOAD] FAIL: size out of range")
            return False

        # Start upload
        start_data = struct.pack('<H', total_size)
        response = self._send_command(CDCCommand.UPLOAD_START, start_data)
        print(f"[UPLOAD] START response={response.hex() if response else None}")
        if not response or response[0] != CDCResponse.ACK:
            print(f"[UPLOAD] FAIL at START")
            return False

        # Send chunks
        offset = 0
        chunk_num = 0
        while offset < total_size:
            chunk = config_data[offset:offset + chunk_size]
            response = self._send_command(CDCCommand.UPLOAD_DATA, chunk)
            if not response or response[0] != CDCResponse.ACK:
                print(f"[UPLOAD] FAIL at chunk {chunk_num}, offset={offset}, response={response.hex() if response else None}")
                self._send_command(CDCCommand.UPLOAD_ABORT)
                return False
            offset += len(chunk)
            chunk_num += 1
        print(f"[UPLOAD] sent {chunk_num} chunks, {offset} bytes")

        # Commit
        response = self._send_command(CDCCommand.UPLOAD_COMMIT)
        print(f"[UPLOAD] COMMIT response={response.hex() if response else None}")
        if not response or response[0] != CDCResponse.ACK:
            print(f"[UPLOAD] FAIL at COMMIT")
            return False

        # Save to flash for persistence across power cycles
        if save_to_flash:
            result = self.save_to_flash()
            print(f"[UPLOAD] SAVE_FLASH result={result}")
            return result

        return True

    def upload_config_file(self, filepath: str) -> bool:
        """
        Upload config file from disk.

        Args:
            filepath: Path to .cfg file

        Returns:
            True if upload successful
        """
        try:
            with open(filepath, 'rb') as f:
                config_data = f.read()
            return self.upload_config(config_data)
        except IOError:
            return False

    def abort_upload(self) -> bool:
        """Abort any in-progress config upload."""
        response = self._send_command(CDCCommand.UPLOAD_ABORT)
        return response and len(response) >= 1 and response[0] == CDCResponse.ACK


def main():
    """CLI test interface."""
    import sys

    device = NChorderDevice()

    print("Looking for nChorder devices...")
    devices = NChorderDevice.find_devices()
    if not devices:
        print("No devices found. Check USB connection.")
        return

    print(f"Found: {devices}")
    if not device.connect():
        print("Connection failed.")
        return

    print("Connected!")

    version = device.get_version()
    if version:
        print(f"Firmware: {version}")

    config = device.get_config()
    if config:
        print(f"Config: {config}")

    # Check for config upload argument
    if len(sys.argv) > 1:
        config_path = sys.argv[1]
        print(f"\nUploading config: {config_path}")
        if device.upload_config_file(config_path):
            print("Config uploaded successfully!")
        else:
            print("Config upload failed.")
    else:
        # Test single touch read
        touch = device.get_touches()
        if touch:
            print(f"Touch: {touch}")

    device.disconnect()


if __name__ == '__main__':
    main()
