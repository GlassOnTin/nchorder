"""Twiddler Tools - Configuration utilities for the Twiddler keyboard."""

__version__ = "0.1.0"

from .config import TwiddlerConfig, ChordEntry
from .formats import ConfigV4, ConfigV5, ConfigV7, read_config, detect_version
from .csv_parser import read_csv
from .hid import HID_MAP, hid_to_char, char_to_hid, chord_to_buttons, buttons_to_chord
