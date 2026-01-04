#!/usr/bin/env python3
"""Command-line interface for Twiddler tools."""

import argparse
import json
import sys
from pathlib import Path

from .formats import read_config, detect_version, ConfigV7
from .csv_parser import read_csv
from .hid import chord_to_buttons, hid_to_char, chord_to_tutor_notation, hid_to_tutor_key, BUTTON_BITS
from .cdc_client import NChorderDevice


def check_firmware_quirks(config):
    """Check for known firmware quirks that may cause issues.

    Returns a list of warning messages.
    """
    warnings = []

    # Quirk: N+4L mapped to '0' (HID 0x27) breaks N+S+4R Bluetooth erase
    # The firmware fails to recognize the N+S+4R system chord when N+4L
    # is mapped to HID code 0x27.
    n_bit = 1 << BUTTON_BITS['N']
    l4_bit = 1 << BUTTON_BITS['4L']
    n_4l_chord = n_bit | l4_bit

    for chord in config.chords:
        if (chord.chord & 0xFFFF) == n_4l_chord and chord.hid_key == 0x27:
            warnings.append(
                "WARNING: N+4L is mapped to '0' (HID 0x27). This breaks the "
                "N+S+4R Bluetooth erase chord due to a firmware quirk. "
                "Workaround: switch to default config (0.CFG) to erase Bluetooth pairings."
            )
            break

    # Check for layout chords that shadow system chords
    # System chords have modifier low byte 0x01 (mouse) or 0x07 (system function)
    system_chords = {c.chord: c for c in config.chords if (c.modifier & 0xFF) in (0x01, 0x07)}
    layout_chords = [c for c in config.chords if (c.modifier & 0xFF) not in (0x01, 0x07)]

    for lc in layout_chords:
        if lc.chord in system_chords:
            sc = system_chords[lc.chord]
            buttons = '+'.join(chord_to_buttons(lc.chord)) or 'NONE'
            layout_out = hid_to_char(lc.hid_key, lc.is_shifted)
            warnings.append(
                f"WARNING: {buttons} is mapped to '{layout_out}' but also has a system chord. "
                f"The layout chord takes precedence, disabling the system function."
            )

    return warnings


def read_input(path: Path):
    """Read config from file, auto-detecting format including CSV."""
    if path.suffix.lower() == '.csv':
        return read_csv(path)
    return read_config(path)


def cmd_info(args):
    """Show config file information."""
    with open(args.file, 'rb') as f:
        data = f.read()

    version = detect_version(data)
    config = read_config(args.file)

    print(f"File: {args.file}")
    print(f"Size: {len(data)} bytes")
    print(f"Format: v{version}")
    print(f"Chords: {len(config.chords)}")
    print()

    print("Settings:")
    print(f"  Sleep timeout: {config.sleep_timeout}")
    print(f"  Key repeat delay: {config.key_repeat_delay}")
    print(f"  Mouse accel: {config.mouse_accel}")
    print()

    # Check for known firmware quirks
    quirk_warnings = check_firmware_quirks(config)
    if quirk_warnings:
        print("Firmware Quirks Detected:")
        for warning in quirk_warnings:
            print(f"  {warning}")
        print()

    # Check for chord conflicts (same chord mapped twice)
    conflicts = config.find_conflicts()
    if conflicts:
        print("Chord Conflicts Detected:")
        for orig, dup in conflicts:
            buttons = '+'.join(chord_to_buttons(orig.chord)) or 'NONE'
            out1 = hid_to_char(orig.hid_key, orig.is_shifted)
            out2 = hid_to_char(dup.hid_key, dup.is_shifted)
            print(f"  {buttons}: '{out1}' conflicts with '{out2}'")
        print()

    if args.verbose:
        print("Chord mappings:")
        for i, chord in enumerate(config.chords):
            buttons = '+'.join(chord_to_buttons(chord.chord)) or 'NONE'
            if chord.is_multi:
                output = f"[multi:{len(chord.multi_chars)} chars]"
            else:
                output = hid_to_char(chord.hid_key, chord.is_shifted)
            shift_mark = " (shift)" if chord.is_shifted else ""
            print(f"  {i:3d}: {buttons:20s} -> {output}{shift_mark}")


def cmd_convert(args):
    """Convert config between formats."""
    config = read_input(args.input)

    fmt = "CSV" if args.input.suffix.lower() == '.csv' else f"v{config.version}"
    print(f"Read: {args.input} ({fmt}, {len(config.chords)} chords)")

    # Warn about conflicts in input
    conflicts = config.find_conflicts()
    if conflicts:
        print(f"WARNING: {len(conflicts)} chord conflicts detected in input", file=sys.stderr)

    # Add system chords from reference config if requested
    if args.system_chords:
        ref_config = read_config(args.system_chords)
        # System chords have modifier low byte 0x01 or 0x07
        system_chords = [c for c in ref_config.chords if (c.modifier & 0xFF) in (0x01, 0x07)]

        # Only add system chords that don't conflict with layout chords
        layout_bits = {c.chord for c in config.chords}
        non_conflicting = [sc for sc in system_chords if sc.chord not in layout_bits]
        conflicting = [sc for sc in system_chords if sc.chord in layout_bits]

        config.chords.extend(non_conflicting)
        print(f"Added {len(non_conflicting)} system chords from {args.system_chords}")
        if conflicting:
            print(f"Skipped {len(conflicting)} system chords that conflict with layout")

    # Determine output format
    out_version = args.format
    if out_version is None:
        # Default to v7 for conversion
        out_version = 7

    if out_version == 7:
        ConfigV7.write(config, args.output)
        print(f"Wrote: {args.output} (v7, {len(config.chords)} chords)")

        # Fix header if reference config provided
        if args.system_chords:
            ref_data = open(args.system_chords, 'rb').read()
            out_data = bytearray(open(args.output, 'rb').read())
            # Copy index table from reference
            out_data[0x60:0x80] = ref_data[0x60:0x80]
            open(args.output, 'wb').write(out_data)
            print(f"Copied index table from {args.system_chords}")
    else:
        print(f"Error: Output format v{out_version} not yet supported", file=sys.stderr)
        return 1

    # Check for known firmware quirks in the output config
    quirk_warnings = check_firmware_quirks(config)
    if quirk_warnings:
        print()
        for warning in quirk_warnings:
            print(warning, file=sys.stderr)

    return 0


def cmd_json(args):
    """Export config as Tutor-compatible JSON."""
    config = read_input(args.file)

    chords = []
    macros = []
    skipped = 0
    include_thumbs = not args.no_thumbs
    include_macros = args.include_macros

    for chord_entry in config.chords:
        chord_notation = chord_to_tutor_notation(chord_entry.chord, include_thumbs)

        # Handle multi-character macros
        if chord_entry.is_multi:
            if include_macros and chord_entry.multi_chars:
                # Convert multi-char sequence to string
                sequence = []
                for mod, hid in chord_entry.multi_chars:
                    shifted = (mod & 0x22) != 0  # Check shift bit
                    char = hid_to_char(hid, shifted)
                    if not char.startswith('<'):  # Skip special keys in macros
                        sequence.append(char)
                if sequence:
                    macros.append({
                        "chord": chord_notation,
                        "sequence": sequence,
                        "text": ''.join(sequence)
                    })
            else:
                skipped += 1
            continue

        key = hid_to_tutor_key(chord_entry.hid_key, chord_entry.modifier)

        if key is None:
            skipped += 1
            continue

        chords.append({
            "chord": chord_notation,
            "key": key
        })

    output = {"chords": chords}
    if macros:
        output["macros"] = macros

    if args.output:
        with open(args.output, 'w') as f:
            json.dump(output, f, indent=2)
        print(f"Wrote {len(chords)} chords", file=sys.stderr, end='')
        if macros:
            print(f", {len(macros)} macros", file=sys.stderr, end='')
        print(f" to {args.output}", file=sys.stderr)
        if skipped:
            print(f"Skipped {skipped} system chords", file=sys.stderr)
    else:
        print(json.dumps(output, indent=2))
        if skipped:
            print(f"# Skipped {skipped} system chords", file=sys.stderr)

    return 0


def cmd_dump(args):
    """Dump config as text format."""
    config = read_config(args.file)

    print(f"# Twiddler config v{config.version}")
    print(f"# {len(config.chords)} chords")
    print()

    # Settings
    print(f"sleep_timeout {config.sleep_timeout}")
    print(f"key_repeat_delay {config.key_repeat_delay}")
    print(f"mouse_accel {config.mouse_accel}")
    print()

    # Chords in twidlk-compatible format
    for chord in config.chords:
        buttons = chord_to_buttons(chord.chord)
        if not buttons:
            continue

        # Format chord notation
        # Thumb buttons as prefix, finger buttons as OOOO pattern
        thumb = ''.join(b for b in buttons if b in 'NACS')
        fingers = ['O', 'O', 'O', 'O']  # 4 rows
        for b in buttons:
            if b not in 'NACS':
                row = int(b[0]) - 1
                col = 'LMR'.index(b[1])
                fingers[row] = b[1]

        chord_str = thumb + ('+' if thumb else '') + ''.join(fingers)

        if chord.is_multi:
            output = "[multi]"
        else:
            char = hid_to_char(chord.hid_key, chord.is_shifted)
            output = char

        print(f"{chord_str} {output}")


def cmd_analyze(args):
    """Analyze config for coverage and issues."""
    config = read_input(args.file)

    print(f"Config: {args.file}")
    print(f"Format: v{config.version}")
    print(f"Total chords: {len(config.chords)}")
    print()

    # Count chord types
    keyboard_chords = [c for c in config.chords if (c.modifier & 0xFF) not in (0x01, 0x07)]
    system_chords = [c for c in config.chords if (c.modifier & 0xFF) == 0x07]
    mouse_chords = [c for c in config.chords if (c.modifier & 0xFF) == 0x01]
    multi_chords = [c for c in config.chords if c.is_multi]

    print("Chord breakdown:")
    print(f"  Keyboard: {len(keyboard_chords)}")
    print(f"  System:   {len(system_chords)}")
    print(f"  Mouse:    {len(mouse_chords)}")
    print(f"  Multi-char: {len(multi_chords)}")
    print()

    # Find unmapped common chords
    unmapped = config.find_unmapped()
    # Only count non-system unmapped (common chords are keyboard mappings)
    print(f"Unmapped common chords: {len(unmapped)} of 195")

    if args.verbose and unmapped:
        print("\nFirst 20 unmapped chords:")
        for chord in unmapped[:20]:
            buttons = '+'.join(chord_to_buttons(chord))
            print(f"  {buttons}")
        if len(unmapped) > 20:
            print(f"  ... and {len(unmapped) - 20} more")

    # Conflicts
    conflicts = config.find_conflicts()
    if conflicts:
        print(f"\nConflicts detected: {len(conflicts)}")
        for orig, dup in conflicts[:5]:
            buttons = '+'.join(chord_to_buttons(orig.chord)) or 'NONE'
            out1 = hid_to_char(orig.hid_key, orig.is_shifted)
            out2 = hid_to_char(dup.hid_key, dup.is_shifted)
            print(f"  {buttons}: '{out1}' vs '{out2}'")

    # Quirks
    quirks = check_firmware_quirks(config)
    if quirks:
        print(f"\nFirmware quirks: {len(quirks)}")
        for q in quirks:
            print(f"  {q[:80]}...")

    return 0


def cmd_diff(args):
    """Compare two config files."""
    config1 = read_input(args.file1)
    config2 = read_input(args.file2)

    d = config1.diff(config2)

    print(f"Comparing: {args.file1} -> {args.file2}")
    print()

    if d['settings']:
        print("Settings changed:")
        for k, (old, new) in d['settings'].items():
            print(f"  {k}: {old} -> {new}")
        print()

    if d['removed']:
        print(f"Removed ({len(d['removed'])} chords):")
        for e in d['removed'][:10]:
            buttons = '+'.join(chord_to_buttons(e.chord)) or 'NONE'
            out = hid_to_char(e.hid_key, e.is_shifted)
            print(f"  - {buttons}: {out}")
        if len(d['removed']) > 10:
            print(f"  ... and {len(d['removed']) - 10} more")
        print()

    if d['added']:
        print(f"Added ({len(d['added'])} chords):")
        for e in d['added'][:10]:
            buttons = '+'.join(chord_to_buttons(e.chord)) or 'NONE'
            out = hid_to_char(e.hid_key, e.is_shifted)
            print(f"  + {buttons}: {out}")
        if len(d['added']) > 10:
            print(f"  ... and {len(d['added']) - 10} more")
        print()

    if d['changed']:
        print(f"Changed ({len(d['changed'])} chords):")
        for chord, old, new in d['changed'][:10]:
            buttons = '+'.join(chord_to_buttons(chord)) or 'NONE'
            out1 = hid_to_char(old.hid_key, old.is_shifted)
            out2 = hid_to_char(new.hid_key, new.is_shifted)
            print(f"  {buttons}: {out1} -> {out2}")
        if len(d['changed']) > 10:
            print(f"  ... and {len(d['changed']) - 10} more")
        print()

    if not any([d['settings'], d['removed'], d['added'], d['changed']]):
        print("No differences found.")

    return 0


def get_layouts_dir() -> Path:
    """Get the configs directory containing layout files."""
    # Check relative to this module
    module_dir = Path(__file__).parent.parent.parent
    configs_dir = module_dir / 'configs'
    if configs_dir.exists():
        return configs_dir
    # Fallback to current directory
    return Path('configs')


def cmd_layouts(args):
    """List available chord layouts."""
    configs_dir = get_layouts_dir()

    if not configs_dir.exists():
        print(f"Configs directory not found: {configs_dir}", file=sys.stderr)
        return 1

    layouts = []
    for cfg_file in sorted(configs_dir.glob('*.cfg')):
        try:
            config = read_config(cfg_file)
            layouts.append((cfg_file.stem, len(config.chords), cfg_file))
        except Exception as e:
            layouts.append((cfg_file.stem, -1, cfg_file))

    if not layouts:
        print("No layout files found.")
        return 0

    print("Available layouts:")
    print()
    for name, chord_count, path in layouts:
        if chord_count >= 0:
            print(f"  {name:20s}  {chord_count:3d} chords  ({path})")
        else:
            print(f"  {name:20s}  (error reading file)")

    return 0


def cmd_upload(args):
    """Upload a chord layout to the connected device."""
    # Resolve layout name to file path
    config_path = args.file
    if not config_path.exists():
        # Try looking in configs directory
        configs_dir = get_layouts_dir()
        alt_path = configs_dir / f"{args.file}.cfg"
        if alt_path.exists():
            config_path = alt_path
        else:
            alt_path = configs_dir / args.file
            if alt_path.exists():
                config_path = alt_path
            else:
                print(f"Layout not found: {args.file}", file=sys.stderr)
                print(f"Use 'layouts' command to see available layouts.", file=sys.stderr)
                return 1

    # Read and validate config
    try:
        config = read_config(config_path)
        print(f"Layout: {config_path.stem} ({len(config.chords)} chords)")
    except Exception as e:
        print(f"Error reading config: {e}", file=sys.stderr)
        return 1

    # Check for quirks
    quirks = check_firmware_quirks(config)
    if quirks:
        print("Warnings:")
        for q in quirks:
            print(f"  {q[:70]}...")

    # Find device
    if args.port:
        port = args.port
    else:
        devices = NChorderDevice.find_devices()
        if not devices:
            print("No device found. Connect device or specify --port.", file=sys.stderr)
            return 1
        port = devices[0]
        print(f"Device: {port}")

    # Connect and upload
    device = NChorderDevice(port)
    if not device.connect():
        print(f"Failed to connect to {port}", file=sys.stderr)
        return 1

    try:
        print("Uploading...", end=' ', flush=True)
        if device.upload_config_file(str(config_path)):
            print("OK")
            print(f"Layout '{config_path.stem}' is now active.")
            return 0
        else:
            print("FAILED")
            return 1
    finally:
        device.disconnect()


def main():
    parser = argparse.ArgumentParser(
        description='Twiddler configuration tools',
        prog='twiddler-tools'
    )
    subparsers = parser.add_subparsers(dest='command', help='Commands')

    # info command
    info_parser = subparsers.add_parser('info', help='Show config file information')
    info_parser.add_argument('file', type=Path, help='Config file to analyze')
    info_parser.add_argument('-v', '--verbose', action='store_true',
                            help='Show all chord mappings')

    # convert command
    convert_parser = subparsers.add_parser('convert', help='Convert between formats')
    convert_parser.add_argument('input', type=Path, help='Input config file')
    convert_parser.add_argument('output', type=Path, help='Output config file')
    convert_parser.add_argument('-f', '--format', type=int, choices=[4, 5, 7],
                               help='Output format version (default: 7)')
    convert_parser.add_argument('-s', '--system-chords', type=Path,
                               help='Add system chords from reference config (e.g., 0.CFG)')

    # dump command
    dump_parser = subparsers.add_parser('dump', help='Dump config as text')
    dump_parser.add_argument('file', type=Path, help='Config file to dump')

    # json command
    json_parser = subparsers.add_parser('json', help='Export as Tutor-compatible JSON')
    json_parser.add_argument('file', type=Path, help='Config file to export')
    json_parser.add_argument('-o', '--output', type=Path,
                            help='Output file (default: stdout)')
    json_parser.add_argument('--no-thumbs', action='store_true',
                            help='Exclude thumb buttons from chord notation')
    json_parser.add_argument('--include-macros', action='store_true',
                            help='Include multi-character macros in output')

    # analyze command
    analyze_parser = subparsers.add_parser('analyze', help='Analyze config coverage and issues')
    analyze_parser.add_argument('file', type=Path, help='Config file to analyze')
    analyze_parser.add_argument('-v', '--verbose', action='store_true',
                               help='Show unmapped chord details')

    # diff command
    diff_parser = subparsers.add_parser('diff', help='Compare two config files')
    diff_parser.add_argument('file1', type=Path, help='First config file')
    diff_parser.add_argument('file2', type=Path, help='Second config file')

    # layouts command
    layouts_parser = subparsers.add_parser('layouts', help='List available chord layouts')

    # upload command
    upload_parser = subparsers.add_parser('upload', help='Upload a chord layout to device')
    upload_parser.add_argument('file', type=Path,
                              help='Layout file or name (e.g., mirrorwalk or mirrorwalk.cfg)')
    upload_parser.add_argument('-p', '--port', type=str,
                              help='Serial port (auto-detect if not specified)')

    args = parser.parse_args()

    if args.command is None:
        parser.print_help()
        return 1

    if args.command == 'info':
        return cmd_info(args)
    elif args.command == 'convert':
        return cmd_convert(args)
    elif args.command == 'dump':
        return cmd_dump(args)
    elif args.command == 'json':
        return cmd_json(args)
    elif args.command == 'analyze':
        return cmd_analyze(args)
    elif args.command == 'diff':
        return cmd_diff(args)
    elif args.command == 'layouts':
        return cmd_layouts(args)
    elif args.command == 'upload':
        return cmd_upload(args)

    return 0


if __name__ == '__main__':
    sys.exit(main() or 0)
