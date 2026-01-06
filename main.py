#!/usr/bin/env python3
"""
Northern Chorder - Main entry point for Buildozer/Android builds

This file is required by Buildozer for Android APK packaging.
For desktop usage, use 'nchorder-gui' command after pip install.
"""

import os
import sys

# Add src directory to path for package imports
src_path = os.path.join(os.path.dirname(__file__), 'src')
if src_path not in sys.path:
    sys.path.insert(0, src_path)

# Detect Android and configure serial backend
def is_android():
    """Check if running on Android"""
    return 'ANDROID_STORAGE' in os.environ or hasattr(sys, 'getandroidapilevel')

if is_android():
    # On Android, set environment to use usb4a/usbserial4a
    os.environ['NCHORDER_SERIAL_BACKEND'] = 'android'
else:
    os.environ['NCHORDER_SERIAL_BACKEND'] = 'pyserial'

# Launch the GUI app
from nchorder_tools.gui.app import main

if __name__ == '__main__':
    main()
