"""
Chord Layout Viewer and Editor

Displays chord mappings in a tree view organized by button hierarchy.
"""

import struct
from pathlib import Path
from typing import Dict, List, Optional, Tuple

from kivy.uix.widget import Widget
from kivy.uix.boxlayout import BoxLayout
from kivy.uix.gridlayout import GridLayout
from kivy.uix.scrollview import ScrollView
from kivy.uix.label import Label
from kivy.uix.button import Button
from kivy.uix.popup import Popup
from kivy.uix.filechooser import FileChooserListView
from kivy.uix.textinput import TextInput
from kivy.uix.togglebutton import ToggleButton
from kivy.uix.treeview import TreeView, TreeViewLabel, TreeViewNode
from kivy.graphics import Color, Rectangle, Line
from kivy.properties import (
    ObjectProperty, StringProperty, NumericProperty, ListProperty, DictProperty
)
from kivy.clock import Clock


# Button bit positions
BTN_BITS = {
    'T1': 0, 'F1L': 1, 'F1M': 2, 'F1R': 3,
    'T2': 4, 'F2L': 5, 'F2M': 6, 'F2R': 7,
    'T3': 8, 'F3L': 9, 'F3M': 10, 'F3R': 11,
    'T4': 12, 'F4L': 13, 'F4M': 14, 'F4R': 15,
    'F0L': 16, 'F0M': 17, 'F0R': 18, 'T0': 19,
}

BTN_NAMES = {v: k for k, v in BTN_BITS.items()}

# HID keycodes
HID_NAMES = {
    0x04: 'a', 0x05: 'b', 0x06: 'c', 0x07: 'd', 0x08: 'e', 0x09: 'f',
    0x0A: 'g', 0x0B: 'h', 0x0C: 'i', 0x0D: 'j', 0x0E: 'k', 0x0F: 'l',
    0x10: 'm', 0x11: 'n', 0x12: 'o', 0x13: 'p', 0x14: 'q', 0x15: 'r',
    0x16: 's', 0x17: 't', 0x18: 'u', 0x19: 'v', 0x1A: 'w', 0x1B: 'x',
    0x1C: 'y', 0x1D: 'z',
    0x1E: '1', 0x1F: '2', 0x20: '3', 0x21: '4', 0x22: '5',
    0x23: '6', 0x24: '7', 0x25: '8', 0x26: '9', 0x27: '0',
    0x28: 'Enter', 0x29: 'Esc', 0x2A: 'Bksp', 0x2B: 'Tab', 0x2C: 'Space',
    0x2D: '-', 0x2E: '=', 0x2F: '[', 0x30: ']', 0x31: '\\',
    0x33: ';', 0x34: "'", 0x35: '`', 0x36: ',', 0x37: '.', 0x38: '/',
    0x39: 'Caps', 0x3A: 'F1', 0x3B: 'F2', 0x3C: 'F3', 0x3D: 'F4',
    0x3E: 'F5', 0x3F: 'F6', 0x40: 'F7', 0x41: 'F8', 0x42: 'F9',
    0x43: 'F10', 0x44: 'F11', 0x45: 'F12',
    0x49: 'Ins', 0x4A: 'Home', 0x4B: 'PgUp', 0x4C: 'Del',
    0x4D: 'End', 0x4E: 'PgDn', 0x4F: 'Right', 0x50: 'Left',
    0x51: 'Down', 0x52: 'Up', 0x53: 'NumLk',
}

HID_CODES = {v.lower(): k for k, v in HID_NAMES.items()}

# Shifted characters
SHIFTED = {
    '!': ('1', True), '@': ('2', True), '#': ('3', True), '$': ('4', True),
    '%': ('5', True), '^': ('6', True), '&': ('7', True), '*': ('8', True),
    '(': ('9', True), ')': ('0', True), '_': ('-', True), '+': ('=', True),
    '{': ('[', True), '}': (']', True), '|': ('\\', True), ':': (';', True),
    '"': ("'", True), '~': ('`', True), '<': (',', True), '>': ('.', True),
    '?': ('/', True),
}


class ChordEntry:
    """Single chord mapping entry"""

    def __init__(self, chord_mask: int, modifier: int, keycode: int):
        self.chord_mask = chord_mask
        self.modifier = modifier  # Full modifier field (type in low byte, mods in high byte)
        self.keycode = keycode

    @property
    def event_type(self) -> int:
        return self.modifier & 0xFF

    @property
    def mod_flags(self) -> int:
        return (self.modifier >> 8) & 0xFF

    @property
    def is_keyboard(self) -> bool:
        return self.event_type == 0x02

    @property
    def is_mouse(self) -> bool:
        return self.event_type == 0x01

    @property
    def has_shift(self) -> bool:
        return bool(self.mod_flags & 0x20)

    @property
    def has_alt(self) -> bool:
        return bool(self.mod_flags & 0x04)

    @property
    def has_ctrl(self) -> bool:
        return bool(self.mod_flags & 0x02)

    def chord_str(self) -> str:
        """Human-readable chord buttons"""
        btns = []
        for i in range(20):
            if self.chord_mask & (1 << i):
                btns.append(BTN_NAMES.get(i, f'B{i}'))
        return '+'.join(btns) if btns else 'NONE'

    def key_str(self) -> str:
        """Human-readable key output"""
        if not self.is_keyboard:
            if self.is_mouse:
                return f'Mouse({self.mod_flags:#x})'
            return f'Event({self.event_type:#x})'

        key_name = HID_NAMES.get(self.keycode, f'0x{self.keycode:02X}')

        # Apply shift to get actual character
        if self.has_shift and len(key_name) == 1:
            for shifted, (base, _) in SHIFTED.items():
                if base == key_name:
                    key_name = shifted
                    break
            else:
                key_name = key_name.upper()

        prefix = ''
        if self.has_ctrl:
            prefix += 'C-'
        if self.has_alt:
            prefix += 'A-'
        if self.has_shift and len(key_name) > 1:
            prefix += 'S-'

        return prefix + key_name

    def to_bytes(self) -> bytes:
        """Serialize to config format"""
        return struct.pack('<IHH', self.chord_mask, self.modifier, self.keycode)


class ChordConfig:
    """Chord configuration from .cfg file"""

    def __init__(self):
        self.entries: List[ChordEntry] = []
        self.header: bytes = bytes(128)
        self.filepath: Optional[Path] = None

    def load(self, filepath: str) -> bool:
        """Load config from file"""
        try:
            path = Path(filepath)
            data = path.read_bytes()

            if len(data) < 128:
                return False

            self.header = bytearray(data[:128])
            chord_count = struct.unpack_from('<H', data, 8)[0]

            self.entries = []
            for i in range(chord_count):
                off = 128 + i * 8
                if off + 8 > len(data):
                    break
                mask = struct.unpack_from('<I', data, off)[0]
                modifier = struct.unpack_from('<H', data, off + 4)[0]
                keycode = struct.unpack_from('<H', data, off + 6)[0]
                self.entries.append(ChordEntry(mask, modifier, keycode))

            self.filepath = path
            return True
        except Exception as e:
            print(f"Load error: {e}")
            return False

    def save(self, filepath: Optional[str] = None) -> bool:
        """Save config to file"""
        try:
            path = Path(filepath) if filepath else self.filepath
            if not path:
                return False

            # Update header
            struct.pack_into('<H', self.header, 8, len(self.entries))
            struct.pack_into('<H', self.header, 10, 128 + len(self.entries) * 8)

            # Build file
            data = bytes(self.header)
            for entry in self.entries:
                data += entry.to_bytes()

            path.write_bytes(data)
            self.filepath = path
            return True
        except Exception as e:
            print(f"Save error: {e}")
            return False

    def find_chord(self, chord_mask: int) -> Optional[ChordEntry]:
        """Find entry by chord mask"""
        for entry in self.entries:
            if entry.chord_mask == chord_mask:
                return entry
        return None

    def add_or_update(self, entry: ChordEntry):
        """Add new chord or update existing"""
        for i, e in enumerate(self.entries):
            if e.chord_mask == entry.chord_mask:
                self.entries[i] = entry
                return
        self.entries.append(entry)

    def remove(self, chord_mask: int) -> bool:
        """Remove chord by mask"""
        for i, e in enumerate(self.entries):
            if e.chord_mask == chord_mask:
                del self.entries[i]
                return True
        return False


class ChordTreeNode(BoxLayout, TreeViewNode):
    """Tree node for a chord or button in the hierarchy"""

    # Callback for edit request (set by parent)
    on_edit_request = ObjectProperty(None)

    def __init__(self, text='', output='', entry=None, **kwargs):
        super().__init__(**kwargs)
        self.orientation = 'horizontal'
        self.size_hint_y = None
        self.height = 28
        self.entry = entry  # ChordEntry if this is a leaf with output
        self._long_press_event = None
        self._touch_start = None

        # Button name label
        self.btn_label = Label(
            text=text,
            size_hint_x=0.4,
            halign='left',
            valign='middle',
            font_size='14sp'
        )
        self.btn_label.bind(size=self.btn_label.setter('text_size'))

        # Output label (shows key mapping)
        self.output_label = Label(
            text=output,
            size_hint_x=0.6,
            halign='left',
            valign='middle',
            font_size='14sp',
            bold=bool(output),
            color=(0.4, 0.9, 0.4, 1) if output else (0.5, 0.5, 0.5, 1)
        )
        self.output_label.bind(size=self.output_label.setter('text_size'))

        self.add_widget(self.btn_label)
        self.add_widget(self.output_label)

    def on_touch_down(self, touch):
        if self.collide_point(*touch.pos):
            # Right-click = immediate edit
            if touch.button == 'right':
                self._request_edit()
                return True
            # Long press detection
            touch.grab(self)
            self._touch_start = touch.pos
            self._long_press_event = Clock.schedule_once(
                lambda dt: self._on_long_press(), 0.5
            )
        return super().on_touch_down(touch)

    def on_touch_up(self, touch):
        if touch.grab_current is self:
            touch.ungrab(self)
            if self._long_press_event:
                self._long_press_event.cancel()
                self._long_press_event = None
        return super().on_touch_up(touch)

    def on_touch_move(self, touch):
        if touch.grab_current is self and self._touch_start:
            # Cancel long press if moved too far
            dx = abs(touch.pos[0] - self._touch_start[0])
            dy = abs(touch.pos[1] - self._touch_start[1])
            if dx > 10 or dy > 10:
                if self._long_press_event:
                    self._long_press_event.cancel()
                    self._long_press_event = None
        return super().on_touch_move(touch)

    def _on_long_press(self):
        """Handle long press - request edit"""
        self._long_press_event = None
        self._request_edit()

    def _request_edit(self):
        """Request edit for this node"""
        if self.on_edit_request and self.entry:
            self.on_edit_request(self.entry)


class ChordTreeView(BoxLayout):
    """Tree view of chord mappings organized by button hierarchy"""

    config = ObjectProperty(None, allownone=True)
    on_select = ObjectProperty(None)  # Callback when entry selected
    on_edit = ObjectProperty(None)    # Callback when edit requested (long-press/right-click)

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.orientation = 'vertical'

        # Scrollable tree
        self.scroll = ScrollView()
        self.tree = TreeView(
            root_options={'text': 'Chords'},
            hide_root=True,
            indent_level=20
        )
        self.tree.size_hint_y = None
        self.tree.bind(minimum_height=self.tree.setter('height'))
        self.tree.bind(selected_node=self._on_node_select)
        self.scroll.add_widget(self.tree)
        self.add_widget(self.scroll)

    def _on_node_select(self, tree, node):
        """Handle node selection"""
        if node and hasattr(node, 'entry') and node.entry and self.on_select:
            self.on_select(node.entry)

    def _on_node_edit(self, entry):
        """Handle edit request from node"""
        if self.on_edit:
            self.on_edit(entry)

    def refresh(self):
        """Rebuild tree from config"""
        # Clear existing nodes
        for node in list(self.tree.iterate_all_nodes()):
            self.tree.remove_node(node)

        if not self.config or not self.config.entries:
            return

        # Build tree structure: button_name -> {sub_buttons -> {...}, entries: [...]}
        # We organize by the buttons in the chord, sorted by bit position
        tree_data = {}

        for entry in self.config.entries:
            if not entry.is_keyboard:
                continue

            # Get buttons in this chord, sorted by bit position
            buttons = []
            for i in range(20):
                if entry.chord_mask & (1 << i):
                    buttons.append(BTN_NAMES.get(i, f'B{i}'))

            if not buttons:
                continue

            # Navigate/create tree path
            current = tree_data
            for btn in buttons[:-1]:  # All but last button
                if btn not in current:
                    current[btn] = {'_children': {}, '_entries': []}
                current = current[btn]['_children']

            # Last button holds the entry
            last_btn = buttons[-1]
            if last_btn not in current:
                current[last_btn] = {'_children': {}, '_entries': []}
            current[last_btn]['_entries'].append(entry)

        # Recursively add nodes to tree
        self._add_nodes(tree_data, None)

    def _add_nodes(self, data: dict, parent):
        """Recursively add nodes to tree"""
        # Sort buttons for consistent display
        btn_order = list(BTN_BITS.keys())

        for btn_name in btn_order:
            if btn_name not in data:
                continue

            node_data = data[btn_name]
            entries = node_data.get('_entries', [])
            children = node_data.get('_children', {})

            # Determine output text
            if entries:
                # Show first entry's output (usually just one per chord)
                output_text = entries[0].key_str()
                entry = entries[0]
            else:
                output_text = ''
                entry = None

            # Create node
            node = ChordTreeNode(
                text=btn_name,
                output=output_text,
                entry=entry,
                is_open=False
            )
            node.on_edit_request = self._on_node_edit

            if parent:
                self.tree.add_node(node, parent)
            else:
                self.tree.add_node(node)

            # Add children recursively
            if children:
                self._add_nodes(children, node)


class ChordListItem(BoxLayout):
    """Single chord entry in the list view"""

    entry = ObjectProperty(None)
    selected = ObjectProperty(False)

    def __init__(self, entry: ChordEntry, **kwargs):
        super().__init__(**kwargs)
        self.entry = entry
        self.orientation = 'horizontal'
        self.size_hint_y = None
        self.height = 40
        self.padding = [5, 2]
        self.spacing = 10

        # Chord buttons
        self.chord_label = Label(
            text=entry.chord_str(),
            size_hint_x=0.5,
            halign='left',
            valign='middle'
        )
        self.chord_label.bind(size=self.chord_label.setter('text_size'))

        # Output key
        self.key_label = Label(
            text=entry.key_str(),
            size_hint_x=0.3,
            halign='center',
            valign='middle',
            bold=True
        )

        # Type indicator
        type_text = 'KB' if entry.is_keyboard else ('M' if entry.is_mouse else '?')
        self.type_label = Label(
            text=type_text,
            size_hint_x=0.1,
            color=(0.5, 0.8, 0.5, 1) if entry.is_keyboard else (0.8, 0.5, 0.5, 1)
        )

        self.add_widget(self.chord_label)
        self.add_widget(self.key_label)
        self.add_widget(self.type_label)

        self.bind(pos=self._update_bg, size=self._update_bg, selected=self._update_bg)
        self._update_bg()

    def _update_bg(self, *args):
        self.canvas.before.clear()
        with self.canvas.before:
            if self.selected:
                Color(0.3, 0.4, 0.5, 1)
            else:
                Color(0.18, 0.18, 0.18, 1)
            Rectangle(pos=self.pos, size=self.size)


class ChordListView(ScrollView):
    """Scrollable list of all chord mappings"""

    config = ObjectProperty(None, allownone=True)
    selected_entry = ObjectProperty(None, allownone=True)

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.do_scroll_x = False

        self.container = BoxLayout(
            orientation='vertical',
            size_hint_y=None,
            spacing=2,
            padding=5
        )
        self.container.bind(minimum_height=self.container.setter('height'))
        self.add_widget(self.container)

        self._items: List[ChordListItem] = []

    def refresh(self):
        """Rebuild list from config"""
        self.container.clear_widgets()
        self._items = []

        if not self.config:
            return

        # Sort entries by chord mask for consistent display
        sorted_entries = sorted(self.config.entries, key=lambda e: e.chord_mask)

        for entry in sorted_entries:
            item = ChordListItem(entry)
            item.bind(on_touch_down=self._on_item_touch)
            self.container.add_widget(item)
            self._items.append(item)

    def _on_item_touch(self, item, touch):
        if item.collide_point(*touch.pos):
            self.select(item.entry)
            return True
        return False

    def select(self, entry: Optional[ChordEntry]):
        """Select an entry"""
        self.selected_entry = entry
        for item in self._items:
            item.selected = (item.entry == entry)


class ButtonSelector(GridLayout):
    """Grid of toggle buttons for selecting chord buttons"""

    selected_mask = NumericProperty(0)

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.cols = 4
        self.spacing = 5
        self.padding = 10
        self.size_hint_y = None
        self.height = 200

        self._buttons: Dict[str, ToggleButton] = {}

        # Create button grid in layout order
        button_layout = [
            ['T1', 'F1L', 'F1M', 'F1R'],
            ['T2', 'F2L', 'F2M', 'F2R'],
            ['T3', 'F3L', 'F3M', 'F3R'],
            ['T4', 'F4L', 'F4M', 'F4R'],
            ['T0', 'F0L', 'F0M', 'F0R'],
        ]

        for row in button_layout:
            for btn_name in row:
                btn = ToggleButton(
                    text=btn_name,
                    size_hint_y=None,
                    height=35
                )
                btn.btn_name = btn_name
                btn.bind(state=self._on_button_toggle)
                self.add_widget(btn)
                self._buttons[btn_name] = btn

    def _on_button_toggle(self, instance, state):
        # Recalculate mask
        mask = 0
        for name, btn in self._buttons.items():
            if btn.state == 'down':
                mask |= (1 << BTN_BITS[name])
        self.selected_mask = mask

    def set_mask(self, mask: int):
        """Set buttons from mask"""
        self.selected_mask = mask
        for name, btn in self._buttons.items():
            bit = BTN_BITS[name]
            btn.state = 'down' if (mask & (1 << bit)) else 'normal'


class ChordEditor(BoxLayout):
    """Editor for a single chord mapping"""

    entry = ObjectProperty(None, allownone=True)
    on_save = ObjectProperty(None)  # Callback

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.orientation = 'vertical'
        self.padding = 10
        self.spacing = 10

        # Title
        self.title = Label(
            text='Edit Chord',
            size_hint_y=None,
            height=30,
            bold=True
        )
        self.add_widget(self.title)

        # Button selector
        self.btn_selector = ButtonSelector()
        self.add_widget(self.btn_selector)

        # Key input
        key_row = BoxLayout(size_hint_y=None, height=40, spacing=10)
        key_row.add_widget(Label(text='Key:', size_hint_x=0.3))
        self.key_input = TextInput(
            text='',
            multiline=False,
            size_hint_x=0.7
        )
        key_row.add_widget(self.key_input)
        self.add_widget(key_row)

        # Modifier toggles
        mod_row = BoxLayout(size_hint_y=None, height=40, spacing=10)
        self.shift_btn = ToggleButton(text='Shift')
        self.alt_btn = ToggleButton(text='Alt')
        self.ctrl_btn = ToggleButton(text='Ctrl')
        mod_row.add_widget(self.shift_btn)
        mod_row.add_widget(self.alt_btn)
        mod_row.add_widget(self.ctrl_btn)
        self.add_widget(mod_row)

        # Preview
        self.preview = Label(
            text='',
            size_hint_y=None,
            height=40
        )
        self.add_widget(self.preview)

        # Buttons
        btn_row = BoxLayout(size_hint_y=None, height=50, spacing=10)
        save_btn = Button(text='Save')
        save_btn.bind(on_press=self._on_save)
        delete_btn = Button(text='Delete')
        delete_btn.bind(on_press=self._on_delete)
        btn_row.add_widget(save_btn)
        btn_row.add_widget(delete_btn)
        self.add_widget(btn_row)

        # Update preview on changes
        self.btn_selector.bind(selected_mask=self._update_preview)
        self.key_input.bind(text=self._update_preview)
        self.shift_btn.bind(state=self._update_preview)
        self.alt_btn.bind(state=self._update_preview)
        self.ctrl_btn.bind(state=self._update_preview)

    def load_entry(self, entry: Optional[ChordEntry]):
        """Load entry for editing"""
        self.entry = entry
        if entry:
            self.btn_selector.set_mask(entry.chord_mask)
            self.key_input.text = entry.key_str().lstrip('C-A-S-')
            self.shift_btn.state = 'down' if entry.has_shift else 'normal'
            self.alt_btn.state = 'down' if entry.has_alt else 'normal'
            self.ctrl_btn.state = 'down' if entry.has_ctrl else 'normal'
            self.title.text = 'Edit Chord'
        else:
            self.btn_selector.set_mask(0)
            self.key_input.text = ''
            self.shift_btn.state = 'normal'
            self.alt_btn.state = 'normal'
            self.ctrl_btn.state = 'normal'
            self.title.text = 'New Chord'
        self._update_preview()

    def _update_preview(self, *args):
        chord = self._chord_str()
        key = self.key_input.text
        mods = ''
        if self.ctrl_btn.state == 'down':
            mods += 'Ctrl+'
        if self.alt_btn.state == 'down':
            mods += 'Alt+'
        if self.shift_btn.state == 'down':
            mods += 'Shift+'
        self.preview.text = f'{chord} -> {mods}{key}'

    def _chord_str(self) -> str:
        mask = self.btn_selector.selected_mask
        btns = []
        for i in range(20):
            if mask & (1 << i):
                btns.append(BTN_NAMES.get(i, f'B{i}'))
        return '+'.join(btns) if btns else '(none)'

    def _build_entry(self) -> Optional[ChordEntry]:
        """Build entry from current UI state"""
        mask = self.btn_selector.selected_mask
        if mask == 0:
            return None

        key_text = self.key_input.text.strip().lower()
        if not key_text:
            return None

        # Look up keycode
        keycode = HID_CODES.get(key_text)
        if keycode is None:
            # Check shifted
            if key_text in SHIFTED:
                base, _ = SHIFTED[key_text]
                keycode = HID_CODES.get(base)
                # Shift will be forced on
            if keycode is None:
                return None

        # Build modifier field
        mod_flags = 0
        if self.shift_btn.state == 'down' or key_text in SHIFTED:
            mod_flags |= 0x20
        if self.alt_btn.state == 'down':
            mod_flags |= 0x04
        if self.ctrl_btn.state == 'down':
            mod_flags |= 0x02

        modifier = (mod_flags << 8) | 0x02  # 0x02 = keyboard event

        return ChordEntry(mask, modifier, keycode)

    def _on_save(self, instance):
        entry = self._build_entry()
        if entry and self.on_save:
            self.on_save(entry)

    def _on_delete(self, instance):
        if self.entry and self.on_save:
            # Pass None to indicate delete
            self.on_save(None)


class ChordEditPopup(Popup):
    """Compact popup for editing a chord's output"""

    def __init__(self, entry: ChordEntry, on_save=None, on_delete=None, **kwargs):
        super().__init__(**kwargs)
        self.entry = entry
        self._on_save = on_save
        self._on_delete = on_delete

        self.title = f'Edit: {entry.chord_str()}'
        self.size_hint = (0.85, 0.5)

        content = BoxLayout(orientation='vertical', spacing=10, padding=10)

        # Current mapping display
        current_lbl = Label(
            text=f'Current: {entry.key_str()}',
            size_hint_y=None,
            height=30
        )
        content.add_widget(current_lbl)

        # Key input
        key_row = BoxLayout(size_hint_y=None, height=40, spacing=10)
        key_row.add_widget(Label(text='Key:', size_hint_x=0.3))
        self.key_input = TextInput(
            text=entry.key_str().lstrip('C-A-S-'),
            multiline=False,
            size_hint_x=0.7
        )
        key_row.add_widget(self.key_input)
        content.add_widget(key_row)

        # Modifier toggles
        mod_row = BoxLayout(size_hint_y=None, height=40, spacing=5)
        self.shift_btn = ToggleButton(text='Shift', state='down' if entry.has_shift else 'normal')
        self.alt_btn = ToggleButton(text='Alt', state='down' if entry.has_alt else 'normal')
        self.ctrl_btn = ToggleButton(text='Ctrl', state='down' if entry.has_ctrl else 'normal')
        mod_row.add_widget(self.shift_btn)
        mod_row.add_widget(self.alt_btn)
        mod_row.add_widget(self.ctrl_btn)
        content.add_widget(mod_row)

        # Buttons
        btn_row = BoxLayout(size_hint_y=None, height=50, spacing=10)
        save_btn = Button(text='Save')
        save_btn.bind(on_press=self._do_save)
        delete_btn = Button(text='Delete')
        delete_btn.bind(on_press=self._do_delete)
        cancel_btn = Button(text='Cancel')
        cancel_btn.bind(on_press=lambda x: self.dismiss())
        btn_row.add_widget(save_btn)
        btn_row.add_widget(delete_btn)
        btn_row.add_widget(cancel_btn)
        content.add_widget(btn_row)

        self.content = content

    def _do_save(self, instance):
        """Save changes"""
        key_text = self.key_input.text.strip().lower()
        if not key_text:
            return

        # Look up keycode
        keycode = HID_CODES.get(key_text)
        if keycode is None:
            # Check shifted
            if key_text in SHIFTED:
                base, _ = SHIFTED[key_text]
                keycode = HID_CODES.get(base)
            if keycode is None:
                return

        # Build modifier field
        mod_flags = 0
        if self.shift_btn.state == 'down' or key_text in SHIFTED:
            mod_flags |= 0x20
        if self.alt_btn.state == 'down':
            mod_flags |= 0x04
        if self.ctrl_btn.state == 'down':
            mod_flags |= 0x02

        modifier = (mod_flags << 8) | 0x02  # 0x02 = keyboard event

        new_entry = ChordEntry(self.entry.chord_mask, modifier, keycode)
        if self._on_save:
            self._on_save(new_entry)
        self.dismiss()

    def _do_delete(self, instance):
        """Delete this chord"""
        if self._on_delete:
            self._on_delete(self.entry)
        self.dismiss()


class ChordMapView(BoxLayout):
    """Main chord map view with tree and minimal controls"""

    config = ObjectProperty(None, allownone=True)
    device = ObjectProperty(None, allownone=True)

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.orientation = 'vertical'
        self.padding = 5
        self.spacing = 5

        # Compact toolbar
        toolbar = BoxLayout(size_hint_y=None, height=40, spacing=5)

        load_btn = Button(text='Load', size_hint_x=0.25)
        load_btn.bind(on_press=self._on_load)

        save_btn = Button(text='Save', size_hint_x=0.25)
        save_btn.bind(on_press=self._on_save)

        upload_btn = Button(text='Upload', size_hint_x=0.25)
        upload_btn.bind(on_press=self._on_upload)

        # Search/filter (future feature)
        self.search_input = TextInput(
            hint_text='Search...',
            size_hint_x=0.25,
            multiline=False
        )

        toolbar.add_widget(load_btn)
        toolbar.add_widget(save_btn)
        toolbar.add_widget(upload_btn)
        toolbar.add_widget(self.search_input)
        self.add_widget(toolbar)

        # Status/filename
        self.status_label = Label(
            text='No config loaded',
            size_hint_y=None,
            height=25,
            font_size='12sp'
        )
        self.add_widget(self.status_label)

        # Tree view (main content)
        self.chord_tree = ChordTreeView()
        self.chord_tree.on_select = self._on_chord_select
        self.chord_tree.on_edit = self._on_chord_edit
        self.add_widget(self.chord_tree)

        # Selected chord info (compact)
        self.info_label = Label(
            text='Long-press or right-click to edit',
            size_hint_y=None,
            height=30,
            font_size='13sp'
        )
        self.add_widget(self.info_label)

        self._config = ChordConfig()

    def _on_chord_select(self, entry: ChordEntry):
        """Handle chord selection from tree"""
        self.info_label.text = f'{entry.chord_str()}  →  {entry.key_str()}'

    def _on_chord_edit(self, entry: ChordEntry):
        """Handle edit request - show popup"""
        popup = ChordEditPopup(
            entry,
            on_save=self._on_edit_save,
            on_delete=self._on_edit_delete
        )
        popup.open()

    def _on_edit_save(self, entry: ChordEntry):
        """Save edited chord"""
        self._config.add_or_update(entry)
        self.chord_tree.refresh()
        self.status_label.text = 'Chord updated'

    def _on_edit_delete(self, entry: ChordEntry):
        """Delete chord"""
        self._config.remove(entry.chord_mask)
        self.chord_tree.refresh()
        self.status_label.text = 'Chord deleted'

    def _on_load(self, instance):
        """Show file chooser to load config"""
        content = BoxLayout(orientation='vertical')

        start_path = str(Path.cwd() / 'configs') if (Path.cwd() / 'configs').exists() else str(Path.home())
        chooser = FileChooserListView(
            path=start_path,
            filters=['*.cfg']
        )
        content.add_widget(chooser)

        btn_row = BoxLayout(size_hint_y=None, height=50, spacing=10)

        def do_load(btn):
            if chooser.selection:
                self.load_config(chooser.selection[0])
            popup.dismiss()

        def do_cancel(btn):
            popup.dismiss()

        load_btn = Button(text='Load')
        load_btn.bind(on_press=do_load)
        cancel_btn = Button(text='Cancel')
        cancel_btn.bind(on_press=do_cancel)

        btn_row.add_widget(load_btn)
        btn_row.add_widget(cancel_btn)
        content.add_widget(btn_row)

        popup = Popup(
            title='Load Config',
            content=content,
            size_hint=(0.9, 0.9)
        )
        popup.open()

    def load_config(self, filepath: str):
        """Load config file"""
        if self._config.load(filepath):
            self.chord_tree.config = self._config
            self.chord_tree.refresh()
            self.status_label.text = f'{Path(filepath).name} ({len(self._config.entries)} chords)'

    def _on_save(self, instance):
        """Save config to file"""
        if self._config.filepath:
            if self._config.save():
                self.status_label.text = f'Saved: {self._config.filepath.name}'
        else:
            self.status_label.text = 'No file path set'

    def _on_upload(self, instance):
        """Upload config to connected device"""
        if not self.device or not self.device.is_connected():
            self.status_label.text = 'No device connected'
            return

        # Build config bytes
        header = bytearray(self._config.header)
        struct.pack_into('<H', header, 8, len(self._config.entries))

        data = bytes(header)
        for entry in self._config.entries:
            data += entry.to_bytes()

        if self.device.upload_config(data):
            self.status_label.text = 'Config uploaded!'
        else:
            self.status_label.text = 'Upload failed'
