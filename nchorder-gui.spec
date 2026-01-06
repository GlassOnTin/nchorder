# -*- mode: python ; coding: utf-8 -*-
"""
PyInstaller spec for Northern Chorder GUI

Usage:
    pyinstaller nchorder-gui.spec

This creates a single-file executable with all Kivy dependencies bundled.
"""

import sys
import os
from pathlib import Path
from PyInstaller.utils.hooks import collect_submodules, collect_data_files

block_cipher = None

# Project paths
project_root = Path(SPECPATH)
src_path = project_root / 'src'

# Collect Kivy data and hidden imports
kivy_deps_all = collect_submodules('kivy')
kivy_data = collect_data_files('kivy')

# Hidden imports for Kivy backends
hidden_imports = [
    'kivy.core.window.window_sdl2',
    'kivy.core.text.text_sdl2',
    'kivy.core.image.img_sdl2',
    'kivy.core.audio.audio_sdl2',
    'kivy.core.clipboard.clipboard_sdl2',
    'kivy.graphics.cgl',
    'kivy.graphics.opengl',
    'kivy.uix.filechooser',
    'kivy.uix.treeview',
    'kivy.uix.spinner',
    'kivy.uix.popup',
    'kivy.uix.slider',
    'serial',
    'serial.tools',
    'serial.tools.list_ports',
] + kivy_deps_all

# Data files to include
datas = [
    (str(project_root / 'configs'), 'configs'),
] + kivy_data

# Add icon if it exists
icon_path = project_root / 'firmware' / 'include' / 'icon.png'
icon_file = str(icon_path) if icon_path.exists() else None

a = Analysis(
    [str(project_root / 'main.py')],
    pathex=[str(src_path)],
    binaries=[],
    datas=datas,
    hiddenimports=hidden_imports,
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=[
        'tkinter',
        'matplotlib',
        'numpy.distutils',
        'PIL.ImageTk',
    ],
    win_no_prefer_redirects=False,
    win_private_assemblies=False,
    cipher=block_cipher,
    noarchive=False,
)

pyz = PYZ(a.pure, a.zipped_data, cipher=block_cipher)

exe = EXE(
    pyz,
    a.scripts,
    a.binaries,
    a.zipfiles,
    a.datas,
    [],
    name='nchorder-gui',
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    upx_exclude=[],
    runtime_tmpdir=None,
    console=False,  # Windowed mode
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
    icon=icon_file,
)

# macOS app bundle (only on macOS)
if sys.platform == 'darwin':
    app = BUNDLE(
        exe,
        name='Northern Chorder.app',
        icon=icon_file,
        bundle_identifier='org.nchorder.gui',
        info_plist={
            'CFBundleShortVersionString': '0.1.0',
            'CFBundleVersion': '0.1.0',
            'NSHighResolutionCapable': 'True',
            'NSPrincipalClass': 'NSApplication',
            'NSAppleScriptEnabled': False,
        },
    )
