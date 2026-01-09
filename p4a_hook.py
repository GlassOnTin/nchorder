"""
python-for-android hook to configure the build environment.

This hook disables the _uuid C module which requires libuuid (not available on Android).
Python's uuid module falls back to pure Python implementation automatically.
"""
import os


def before_apk_build(toolchain):
    """Called before the APK build starts."""
    # Disable _uuid module - requires libuuid which isn't available on Android NDK
    # Python's uuid module will use pure Python fallback
    os.environ['py_cv_module__uuid'] = 'n/a'

    # Also disable some other modules that can cause issues
    os.environ['py_cv_module__tkinter'] = 'n/a'

    print("[p4a_hook] Disabled _uuid module (libuuid not available on Android)")
