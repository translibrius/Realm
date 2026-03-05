#!/usr/bin/env python3
"""
Realm Profiler Viewer — DearPyGui-based real-time visualizer.

Reads profiler data from shared memory written by the engine
(engine/src/profiler/profiler.c) and displays live graphs + table.

Three view modes:
  - Flat: classic bar chart + sortable table (original view)
  - Call Tree: hierarchical expandable tree with self/inclusive time
  - Flame Chart: timeline-like stacked rectangles

Usage:
    python3 tools/profiler_view.py

Requires: pip install dearpygui
"""

import colorsys
import mmap
import struct
import time
import platform as plat
from collections import defaultdict, deque

import dearpygui.dearpygui as dpg

# --- Shared memory protocol — flat zones (must match profiler.c) ---

SHM_NAME = "/realm_profiler"
SHM_MAGIC = 0x524C5046  # "RLPF"
ZONE_NAME_LEN = 32
MAX_BROADCAST_ZONES = 64

HEADER_FMT = "<IIIIq"
HEADER_SIZE = struct.calcsize(HEADER_FMT)

ZONE_FMT = "<32sI4xqqq"
ZONE_SIZE = struct.calcsize(ZONE_FMT)
assert ZONE_SIZE == 64, f"Zone size mismatch: {ZONE_SIZE} != 64"

SHM_TOTAL_SIZE = HEADER_SIZE + MAX_BROADCAST_ZONES * ZONE_SIZE

# --- Shared memory protocol — call tree edges (must match profiler.c) ---

SHM_TREE_NAME = "/realm_profiler_tree"
SHM_TREE_MAGIC = 0x524C5054  # "RLPT"
MAX_BROADCAST_EDGES = 256

TREE_HEADER_FMT = "<IIIIq"
TREE_HEADER_SIZE = struct.calcsize(TREE_HEADER_FMT)

# shm_edge: parent_name(32s) child_name(32s) call_count(u32) pad(4) total_ns(i64) max_ns(i64) avg_ns(i64)
EDGE_FMT = "<32s32sI4xqqq"
EDGE_SIZE = struct.calcsize(EDGE_FMT)
assert EDGE_SIZE == 96, f"Edge size mismatch: {EDGE_SIZE} != 96"

SHM_TREE_TOTAL_SIZE = TREE_HEADER_SIZE + MAX_BROADCAST_EDGES * EDGE_SIZE


# --- Shared memory readers ---

def _shm_open_posix(name, size):
    """Open a POSIX shared memory region. Returns (mm, fd) or (None, None)."""
    import ctypes
    import os

    if plat.system() == "Darwin":
        libc = ctypes.CDLL("libSystem.B.dylib", use_errno=True)
    else:
        libc = ctypes.CDLL("librt.so.1", use_errno=True)

    shm_open_fn = libc.shm_open
    shm_open_fn.restype = ctypes.c_int
    shm_open_fn.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_uint]

    fd = shm_open_fn(name.encode(), os.O_RDONLY, 0o666)
    if fd < 0:
        return None, None

    mm = mmap.mmap(fd, size, access=mmap.ACCESS_READ)
    return mm, fd


class ShmReader:
    """Reads the flat zone data from /realm_profiler."""

    def __init__(self):
        self.mm = None
        self.fd = None
        self.last_seq = 0

    def open(self):
        if self.mm is not None:
            return True
        try:
            if plat.system() in ("Darwin", "Linux"):
                self.mm, self.fd = _shm_open_posix(SHM_NAME, SHM_TOTAL_SIZE)
                return self.mm is not None
            elif plat.system() == "Windows":
                self.mm = mmap.mmap(-1, SHM_TOTAL_SIZE, tagname="realm_profiler",
                                    access=mmap.ACCESS_READ)
                return self.mm is not None
        except Exception:
            self.mm = None
            self.fd = None
        return False

    def read(self):
        if self.mm is None:
            if not self.open():
                return None
        try:
            self.mm.seek(0)
            header_data = self.mm.read(HEADER_SIZE)
        except (ValueError, OSError):
            self.close()
            return None

        magic, seq, zone_count, _pad, frame_time_ns = struct.unpack(HEADER_FMT, header_data)
        if magic != SHM_MAGIC:
            self.close()
            return None
        if seq == self.last_seq:
            return None

        zone_count = min(zone_count, MAX_BROADCAST_ZONES)
        zones = []
        for _ in range(zone_count):
            zone_data = self.mm.read(ZONE_SIZE)
            name_raw, call_count, total_ns, max_ns, avg_ns = struct.unpack(ZONE_FMT, zone_data)
            name = name_raw.split(b'\x00', 1)[0].decode('utf-8', errors='replace')
            zones.append({
                'name': name,
                'call_count': call_count,
                'total_ns': total_ns,
                'max_ns': max_ns,
                'avg_ns': avg_ns,
            })

        # Torn read check
        self.mm.seek(4)
        seq2 = struct.unpack("<I", self.mm.read(4))[0]
        if seq2 != seq:
            return None

        self.last_seq = seq
        return {'frame_time_ns': frame_time_ns, 'zones': zones}

    def close(self):
        if self.mm is not None:
            try:
                self.mm.close()
            except Exception:
                pass
            self.mm = None
        if self.fd is not None:
            import os
            try:
                os.close(self.fd)
            except Exception:
                pass
            self.fd = None


class TreeShmReader:
    """Reads call tree edge data from /realm_profiler_tree."""

    def __init__(self):
        self.mm = None
        self.fd = None
        self.last_seq = 0

    def open(self):
        if self.mm is not None:
            return True
        try:
            if plat.system() in ("Darwin", "Linux"):
                self.mm, self.fd = _shm_open_posix(SHM_TREE_NAME, SHM_TREE_TOTAL_SIZE)
                return self.mm is not None
            elif plat.system() == "Windows":
                self.mm = mmap.mmap(-1, SHM_TREE_TOTAL_SIZE, tagname="realm_profiler_tree",
                                    access=mmap.ACCESS_READ)
                return self.mm is not None
        except Exception:
            self.mm = None
            self.fd = None
        return False

    def read(self):
        if self.mm is None:
            if not self.open():
                return None
        try:
            self.mm.seek(0)
            header_data = self.mm.read(TREE_HEADER_SIZE)
        except (ValueError, OSError):
            self.close()
            return None

        magic, seq, edge_count, _pad, frame_time_ns = struct.unpack(TREE_HEADER_FMT, header_data)
        if magic != SHM_TREE_MAGIC:
            self.close()
            return None
        if seq == self.last_seq:
            return None

        edge_count = min(edge_count, MAX_BROADCAST_EDGES)
        edges = []
        for _ in range(edge_count):
            edge_data = self.mm.read(EDGE_SIZE)
            parent_raw, child_raw, call_count, total_ns, max_ns, avg_ns = struct.unpack(
                EDGE_FMT, edge_data)
            parent = parent_raw.split(b'\x00', 1)[0].decode('utf-8', errors='replace')
            child = child_raw.split(b'\x00', 1)[0].decode('utf-8', errors='replace')
            edges.append({
                'parent': parent,
                'child': child,
                'call_count': call_count,
                'total_ns': total_ns,
                'max_ns': max_ns,
                'avg_ns': avg_ns,
            })

        # Torn read check
        self.mm.seek(4)
        seq2 = struct.unpack("<I", self.mm.read(4))[0]
        if seq2 != seq:
            return None

        self.last_seq = seq
        return {'frame_time_ns': frame_time_ns, 'edges': edges}

    def close(self):
        if self.mm is not None:
            try:
                self.mm.close()
            except Exception:
                pass
            self.mm = None
        if self.fd is not None:
            import os
            try:
                os.close(self.fd)
            except Exception:
                pass
            self.fd = None


# --- Smoothing engine ---

SMOOTHING_RAW = "Raw"
SMOOTHING_EMA = "EMA"
SMOOTHING_RUNNING_AVG = "Running Average"

NUMERIC_KEYS = ('total_ns', 'avg_ns', 'max_ns', 'call_count')
STALE_LIMIT = 30

ema_state = {}
avg_history = {}
stale_counter = {}


def smooth_zones(raw_zones, mode, strength):
    """Apply smoothing to raw zone data. Returns a new list of smoothed zone dicts."""
    if mode == SMOOTHING_RAW:
        return list(raw_zones)

    present = set()

    if mode == SMOOTHING_EMA:
        alpha = 1.0 - strength
        for z in raw_zones:
            name = z['name']
            present.add(name)
            stale_counter[name] = 0
            if name in ema_state:
                s = ema_state[name]
                for k in NUMERIC_KEYS:
                    s[k] = s[k] * (1.0 - alpha) + float(z[k]) * alpha
            else:
                ema_state[name] = {k: float(z[k]) for k in NUMERIC_KEYS}

        result = []
        for z in raw_zones:
            s = ema_state[z['name']]
            result.append({
                'name': z['name'],
                'call_count': s['call_count'],
                'total_ns': s['total_ns'],
                'avg_ns': s['avg_ns'],
                'max_ns': s['max_ns'],
            })

    elif mode == SMOOTHING_RUNNING_AVG:
        window = int(3 + strength * 27)
        for z in raw_zones:
            name = z['name']
            present.add(name)
            stale_counter[name] = 0
            if name not in avg_history:
                avg_history[name] = {k: deque(maxlen=window) for k in NUMERIC_KEYS}
            else:
                for k in NUMERIC_KEYS:
                    if avg_history[name][k].maxlen != window:
                        old = list(avg_history[name][k])
                        avg_history[name][k] = deque(old[-window:], maxlen=window)
            for k in NUMERIC_KEYS:
                avg_history[name][k].append(float(z[k]))

        result = []
        for z in raw_zones:
            name = z['name']
            h = avg_history[name]
            result.append({
                'name': name,
                'call_count': sum(h['call_count']) / len(h['call_count']),
                'total_ns': sum(h['total_ns']) / len(h['total_ns']),
                'avg_ns': sum(h['avg_ns']) / len(h['avg_ns']),
                'max_ns': sum(h['max_ns']) / len(h['max_ns']),
            })
    else:
        return list(raw_zones)

    for name in list(stale_counter.keys()):
        if name not in present:
            stale_counter[name] += 1
            if stale_counter[name] > STALE_LIMIT:
                ema_state.pop(name, None)
                avg_history.pop(name, None)
                del stale_counter[name]

    return result


# --- Edge smoothing ---

edge_ema_state = {}
edge_stale_counter = {}

EDGE_NUMERIC_KEYS = ('total_ns', 'max_ns', 'avg_ns', 'call_count')


def smooth_edges(raw_edges, mode, strength):
    """Apply smoothing to raw edge data. Returns a new list of smoothed edge dicts."""
    if mode == SMOOTHING_RAW:
        return list(raw_edges)

    present = set()

    if mode == SMOOTHING_EMA:
        alpha = 1.0 - strength
        for e in raw_edges:
            key = (e['parent'], e['child'])
            present.add(key)
            edge_stale_counter[key] = 0
            if key in edge_ema_state:
                s = edge_ema_state[key]
                for k in EDGE_NUMERIC_KEYS:
                    s[k] = s[k] * (1.0 - alpha) + float(e[k]) * alpha
            else:
                edge_ema_state[key] = {k: float(e[k]) for k in EDGE_NUMERIC_KEYS}

        result = []
        for e in raw_edges:
            key = (e['parent'], e['child'])
            s = edge_ema_state[key]
            result.append({
                'parent': e['parent'],
                'child': e['child'],
                'call_count': s['call_count'],
                'total_ns': s['total_ns'],
                'max_ns': s['max_ns'],
                'avg_ns': s['avg_ns'],
            })
    else:
        return list(raw_edges)

    for key in list(edge_stale_counter.keys()):
        if key not in present:
            edge_stale_counter[key] += 1
            if edge_stale_counter[key] > STALE_LIMIT:
                edge_ema_state.pop(key, None)
                del edge_stale_counter[key]

    return result


# --- Rank-stable sort ---

prev_rank_order = []


def rank_stable_sort(zones, key='total_ns', threshold_pct=0.10):
    global prev_rank_order

    by_name = {z['name']: z for z in zones}

    if not prev_rank_order:
        zones.sort(key=lambda z: z[key], reverse=True)
        prev_rank_order = [z['name'] for z in zones]
        return zones

    ordered = []
    for name in prev_rank_order:
        if name in by_name:
            ordered.append(by_name.pop(name))
    newcomers = sorted(by_name.values(), key=lambda z: z[key], reverse=True)
    ordered.extend(newcomers)

    changed = True
    while changed:
        changed = False
        for i in range(len(ordered) - 1):
            upper = ordered[i][key]
            lower = ordered[i + 1][key]
            if lower > upper and (upper == 0 or (lower - upper) / upper > threshold_pct):
                ordered[i], ordered[i + 1] = ordered[i + 1], ordered[i]
                changed = True

    prev_rank_order = [z['name'] for z in ordered]
    return ordered


# --- Call tree helpers ---

def build_call_tree(flat_zones, edges, frame_time_ns):
    """Build tree structure from flat zones + edge data.

    Returns (roots, children_of, node_info) where:
      roots: list of root child names
      children_of: dict parent_name -> [edge_dicts sorted by total_ns desc]
      node_info: dict name -> {total_ns, self_ns, call_count, pct}
    """
    children_of = defaultdict(list)
    for e in edges:
        children_of[e['parent']].append(e)

    # Sort children by total_ns descending
    for parent in children_of:
        children_of[parent].sort(key=lambda e: e['total_ns'], reverse=True)

    # Build node info from flat zones
    flat_by_name = {z['name']: z for z in flat_zones}
    node_info = {}
    for name, z in flat_by_name.items():
        child_time = sum(e['total_ns'] for e in children_of.get(name, []))
        self_ns = max(0, z['total_ns'] - child_time)
        pct = (z['total_ns'] / frame_time_ns * 100.0) if frame_time_ns > 0 else 0.0
        node_info[name] = {
            'total_ns': z['total_ns'],
            'self_ns': self_ns,
            'call_count': z['call_count'],
            'pct': pct,
        }

    roots = [e['child'] for e in children_of.get('', [])]
    return roots, children_of, node_info


# --- Flame chart helpers ---

def name_to_color(name):
    """Deterministic color from function name."""
    h = hash(name) % 360
    r, g, b = colorsys.hls_to_rgb(h / 360.0, 0.55, 0.7)
    return (int(r * 255), int(g * 255), int(b * 255), 220)


def layout_flame_rects(roots, children_of, node_info, frame_time_ns):
    """Produce a list of flame chart rectangles.

    Returns list of {name, x_start_ns, x_end_ns, depth, total_ns, self_ns, call_count}.
    """
    rects = []

    def recurse(parent_name, x_offset, depth):
        children = children_of.get(parent_name, [])
        x = x_offset
        for edge in children:
            child = edge['child']
            width = edge['total_ns']
            if width <= 0:
                continue
            info = node_info.get(child, {})
            rects.append({
                'name': child,
                'x_start_ns': x,
                'x_end_ns': x + width,
                'depth': depth,
                'total_ns': width,
                'self_ns': info.get('self_ns', 0),
                'call_count': edge['call_count'],
            })
            recurse(child, x, depth + 1)
            x += width

    recurse('', 0, 0)
    return rects


# --- Application state ---

FRAME_HISTORY_LEN = 300
DEFAULT_TOP_N = 15
DEFAULT_REFRESH_HZ = 2.0

frame_times_ms = deque([0.0] * FRAME_HISTORY_LEN, maxlen=FRAME_HISTORY_LEN)
frame_indices = list(range(FRAME_HISTORY_LEN))
current_zones = []
current_edges = []
current_frame_time_ns = 0

reader = ShmReader()
tree_reader = TreeShmReader()
connected = False
tree_connected = False
last_display_time = 0.0
pending_data = None
pending_tree_data = None

# Flame chart view state
flame_zoom = 1.0
flame_pan_ns = 0
flame_dragging = False
flame_drag_start_x = 0
flame_drag_start_pan = 0
flame_view_dirty = False
flame_selected_idx = None  # index into flame_rects_cache, or None

# --- DearPyGui setup ---

dpg.create_context()

# Theme
with dpg.theme() as global_theme:
    with dpg.theme_component(dpg.mvAll):
        dpg.add_theme_color(dpg.mvThemeCol_WindowBg, (20, 20, 22, 255))
        dpg.add_theme_color(dpg.mvThemeCol_TitleBg, (35, 35, 40, 255))
        dpg.add_theme_color(dpg.mvThemeCol_TitleBgActive, (45, 45, 55, 255))
        dpg.add_theme_color(dpg.mvThemeCol_FrameBg, (30, 30, 35, 255))
        dpg.add_theme_color(dpg.mvThemeCol_TableHeaderBg, (35, 35, 40, 255))
        dpg.add_theme_color(dpg.mvThemeCol_TableRowBg, (25, 25, 30, 255))
        dpg.add_theme_color(dpg.mvThemeCol_TableRowBgAlt, (30, 30, 38, 255))
        dpg.add_theme_color(dpg.mvThemeCol_Tab, (35, 35, 45, 255))
        dpg.add_theme_color(dpg.mvThemeCol_TabActive, (55, 55, 75, 255))
        dpg.add_theme_color(dpg.mvThemeCol_TabHovered, (65, 65, 85, 255))
        dpg.add_theme_style(dpg.mvStyleVar_WindowRounding, 4)
        dpg.add_theme_style(dpg.mvStyleVar_FrameRounding, 3)

dpg.bind_theme(global_theme)

with dpg.theme() as plot_theme:
    with dpg.theme_component(dpg.mvAll):
        dpg.add_theme_color(dpg.mvPlotCol_PlotBg, (25, 25, 30, 255), category=dpg.mvThemeCat_Plots)

with dpg.theme() as bar_theme:
    with dpg.theme_component(dpg.mvBarSeries):
        dpg.add_theme_color(dpg.mvPlotCol_Fill, (100, 200, 100, 200), category=dpg.mvThemeCat_Plots)
        dpg.add_theme_color(dpg.mvPlotCol_Line, (100, 200, 100, 255), category=dpg.mvThemeCat_Plots)

with dpg.theme() as line_theme:
    with dpg.theme_component(dpg.mvLineSeries):
        dpg.add_theme_color(dpg.mvPlotCol_Line, (100, 180, 255, 255), category=dpg.mvThemeCat_Plots)

with dpg.theme() as hint_theme:
    with dpg.theme_component(dpg.mvAll):
        dpg.add_theme_color(dpg.mvThemeCol_Text, (120, 120, 140, 255))

with dpg.theme() as self_time_theme:
    with dpg.theme_component(dpg.mvAll):
        dpg.add_theme_color(dpg.mvThemeCol_Text, (255, 180, 100, 255))

with dpg.theme() as name_theme:
    with dpg.theme_component(dpg.mvAll):
        dpg.add_theme_color(dpg.mvThemeCol_Text, (220, 220, 230, 255))

with dpg.theme() as dim_theme:
    with dpg.theme_component(dpg.mvAll):
        dpg.add_theme_color(dpg.mvThemeCol_Text, (120, 120, 140, 255))

# Heat palette: percentage brackets → color temperature
HEAT_COLORS = [
    (100, 100, 120, 255),   # 0: < 1%   — dim gray
    (80, 180, 80, 255),     # 1: 1-5%   — green
    (120, 200, 80, 255),    # 2: 5-10%  — yellow-green
    (200, 200, 60, 255),    # 3: 10-20% — yellow
    (240, 180, 50, 255),    # 4: 20-30% — gold
    (255, 140, 40, 255),    # 5: 30-45% — orange
    (255, 80, 40, 255),     # 6: 45-65% — red-orange
    (255, 50, 50, 255),     # 7: 65%+   — red
]

heat_themes = []
for _hc in HEAT_COLORS:
    with dpg.theme() as _ht:
        with dpg.theme_component(dpg.mvAll):
            dpg.add_theme_color(dpg.mvThemeCol_Text, _hc)
    heat_themes.append(_ht)


def heat_bucket(pct):
    """Return index 0-7 for the heat palette based on frame percentage."""
    if pct < 1.0:  return 0
    if pct < 5.0:  return 1
    if pct < 10.0: return 2
    if pct < 20.0: return 3
    if pct < 30.0: return 4
    if pct < 45.0: return 5
    if pct < 65.0: return 6
    return 7

# --- Main window ---

dpg.create_viewport(title="Realm Profiler", width=1100, height=800, min_width=600, min_height=400)


def on_smoothing_mode_changed(sender, value):
    if value == SMOOTHING_RAW:
        dpg.configure_item("strength_slider", enabled=False)
        dpg.configure_item("strength_hint", show=False)
    else:
        dpg.configure_item("strength_slider", enabled=True)
        dpg.configure_item("strength_hint", show=True)
        _update_strength_hint(value, dpg.get_value("strength_slider"))


def on_strength_changed(sender, value):
    mode = dpg.get_value("smoothing_mode")
    _update_strength_hint(mode, value)


def _update_strength_hint(mode, strength):
    if mode == SMOOTHING_EMA:
        alpha = 1.0 - strength
        dpg.set_value("strength_hint", f"  alpha={alpha:.2f}  ({strength:.0%} old + {alpha:.0%} new)")
    elif mode == SMOOTHING_RUNNING_AVG:
        window = int(3 + strength * 27)
        dpg.set_value("strength_hint", f"  window={window} samples (~{window * 0.1:.1f}s)")


with dpg.window(tag="main_window"):
    # Status header
    dpg.add_text("Waiting for Realm...", tag="status_text", color=(200, 200, 100, 255))
    dpg.add_separator()

    # Settings panel
    with dpg.collapsing_header(label="Settings", default_open=False):
        with dpg.group(horizontal=True):
            dpg.add_text("Smoothing")
            dpg.add_combo((SMOOTHING_RAW, SMOOTHING_EMA, SMOOTHING_RUNNING_AVG),
                          default_value=SMOOTHING_EMA, tag="smoothing_mode", width=160,
                          callback=on_smoothing_mode_changed)

        with dpg.group(horizontal=True):
            dpg.add_text("Strength ")
            dpg.add_slider_float(default_value=0.7, min_value=0.0, max_value=1.0,
                                 tag="strength_slider", width=200, format="%.2f",
                                 callback=on_strength_changed)
            dpg.add_text("  alpha=0.30  (70% old + 30% new)", tag="strength_hint")
            dpg.bind_item_theme("strength_hint", hint_theme)

        dpg.add_spacer(height=4)

        with dpg.group(horizontal=True):
            dpg.add_text("Refresh  ")
            dpg.add_slider_float(default_value=DEFAULT_REFRESH_HZ, min_value=0.5, max_value=10.0,
                                 tag="refresh_hz_slider", width=200, format="%.1f Hz")

        with dpg.group(horizontal=True):
            dpg.add_text("Top N    ")
            dpg.add_slider_int(default_value=DEFAULT_TOP_N, min_value=5, max_value=64,
                               tag="top_n_slider", width=200, format="%d")

        dpg.add_spacer(height=4)

        with dpg.group(horizontal=True):
            dpg.add_checkbox(label="Pause Updates", default_value=False, tag="pause_checkbox")

    # Frame time plot (shared across all tabs)
    with dpg.collapsing_header(label="Frame Time History", default_open=True):
        with dpg.plot(label="##frame_time", height=150, width=-1, tag="frame_plot"):
            dpg.bind_item_theme("frame_plot", plot_theme)
            dpg.add_plot_axis(dpg.mvXAxis, label="Frame", tag="ft_x_axis", no_tick_labels=True)
            with dpg.plot_axis(dpg.mvYAxis, label="ms", tag="ft_y_axis"):
                dpg.set_axis_limits("ft_y_axis", 0, 33)
                dpg.add_line_series(frame_indices, list(frame_times_ms), label="Frame Time",
                                    tag="ft_line", parent="ft_y_axis")
                dpg.bind_item_theme("ft_line", line_theme)

    # --- Tab bar with three views ---
    with dpg.tab_bar(tag="view_tabs"):

        # === Tab 1: Flat View ===
        with dpg.tab(label="Flat"):
            with dpg.collapsing_header(label="Top Functions", default_open=True):
                with dpg.plot(label="##top_funcs", height=250, width=-1, tag="bar_plot"):
                    dpg.bind_item_theme("bar_plot", plot_theme)
                    dpg.add_plot_axis(dpg.mvXAxis, label="ms", tag="bar_x_axis")
                    with dpg.plot_axis(dpg.mvYAxis, label="", tag="bar_y_axis",
                                       no_tick_labels=True):
                        dpg.set_axis_ticks("bar_y_axis", tuple())
                        dpg.add_bar_series([], [], label="Total", horizontal=True,
                                           tag="bar_series", parent="bar_y_axis")
                        dpg.bind_item_theme("bar_series", bar_theme)

            with dpg.collapsing_header(label="Function Detail", default_open=True):
                with dpg.table(tag="zone_table", header_row=True, resizable=True,
                               borders_innerH=True, borders_outerH=True,
                               borders_innerV=True, borders_outerV=True,
                               row_background=True, sortable=True,
                               callback=lambda s, d: sort_table(d)):
                    dpg.add_table_column(label="Function", width_fixed=True,
                                         init_width_or_weight=280)
                    dpg.add_table_column(label="Calls", width_fixed=True,
                                         init_width_or_weight=70)
                    dpg.add_table_column(label="Total (ms)", width_fixed=True,
                                         init_width_or_weight=100, default_sort=True)
                    dpg.add_table_column(label="Avg (us)", width_fixed=True,
                                         init_width_or_weight=100)
                    dpg.add_table_column(label="Max (us)", width_fixed=True,
                                         init_width_or_weight=100)

        # === Tab 2: Call Tree ===
        with dpg.tab(label="Call Tree"):
            dpg.add_text("Waiting for tree data...", tag="tree_status",
                         color=(200, 200, 100, 255))
            dpg.add_child_window(tag="tree_container", height=-1, border=False)

        # === Tab 3: Flame Chart ===
        with dpg.tab(label="Flame Chart"):
            dpg.add_text("Waiting for tree data...", tag="flame_status",
                         color=(200, 200, 100, 255))
            dpg.add_text("Scroll to zoom, drag to pan", tag="flame_hint")
            dpg.bind_item_theme("flame_hint", hint_theme)
            dpg.add_drawlist(tag="flame_canvas", width=1060, height=500)


# Flame chart tooltip window
with dpg.window(tag="flame_tooltip", show=False, no_title_bar=True, no_resize=True,
                no_move=True, no_scrollbar=True, no_collapse=True, no_focus_on_appearing=True,
                no_bring_to_front_on_focus=True, popup=True):
    dpg.add_text("", tag="flame_tooltip_text")


# --- Flat view functions ---

def sort_table(sort_specs):
    if sort_specs is None or len(sort_specs) == 0:
        return
    col_idx = sort_specs[0][0]
    reverse = sort_specs[0][1] == -1
    keys = [
        lambda z: z['name'].lower(),
        lambda z: z['call_count'],
        lambda z: z['total_ns'],
        lambda z: z['avg_ns'],
        lambda z: z['max_ns'],
    ]
    if col_idx < len(keys):
        current_zones.sort(key=keys[col_idx], reverse=reverse)
        rebuild_table()


def rebuild_table():
    children = dpg.get_item_children("zone_table", 1)
    if children:
        for row in children:
            dpg.delete_item(row)

    for z in current_zones:
        total_ms = z['total_ns'] / 1e6
        avg_us = z['avg_ns'] / 1e3
        max_us = z['max_ns'] / 1e3
        with dpg.table_row(parent="zone_table"):
            dpg.add_text(z['name'])
            dpg.add_text(f"{z['call_count']:.0f}" if isinstance(z['call_count'], float)
                         else str(z['call_count']))
            dpg.add_text(f"{total_ms:.3f}")
            dpg.add_text(f"{avg_us:.1f}")
            dpg.add_text(f"{max_us:.1f}")


def update_flat_view(zones, top_n):
    global current_zones

    zones = rank_stable_sort(zones)
    current_zones = zones

    top = zones[:top_n]
    top_reversed = list(reversed(top))

    if top_reversed:
        values = [z['total_ns'] / 1e6 for z in top_reversed]
        positions = list(range(len(top_reversed)))
        labels = [(z['name'][:28] if len(z['name']) > 28 else z['name']) for z in top_reversed]

        dpg.set_value("bar_series", [values, positions])
        dpg.set_axis_ticks("bar_y_axis", tuple(zip(labels, positions)))
        max_val = max(values) if values else 1.0
        dpg.set_axis_limits("bar_x_axis", 0, max_val * 1.15)
    else:
        dpg.set_value("bar_series", [[], []])
        dpg.set_axis_ticks("bar_y_axis", tuple())

    rebuild_table()


# --- Call tree view functions ---

def rebuild_tree_view(flat_zones, edges, frame_time_ns):
    """Rebuild the call tree tab content."""
    # Clear existing tree content
    children = dpg.get_item_children("tree_container", 1)
    if children:
        for child in children:
            dpg.delete_item(child)

    if not edges:
        dpg.add_text("No call tree data available", parent="tree_container",
                      color=(200, 200, 100, 255))
        return

    roots, children_of, node_info = build_call_tree(flat_zones, edges, frame_time_ns)

    edge_count = sum(len(v) for v in children_of.values())
    dpg.set_value("tree_status", f"{len(roots)} roots  |  {edge_count} edges  |  "
                  f"{len(node_info)} functions")
    dpg.configure_item("tree_status", color=(100, 200, 100, 255))

    def add_tree_node(name, edge_total_ns, edge_call_count, depth, parent_tag):
        info = node_info.get(name, {})
        total_ns = edge_total_ns
        self_ns = info.get('self_ns', total_ns)
        pct = (total_ns / frame_time_ns * 100.0) if frame_time_ns > 0 else 0.0
        self_pct = (self_ns / frame_time_ns * 100.0) if frame_time_ns > 0 else 0.0

        total_ms = total_ns / 1e6
        self_ms = self_ns / 1e6

        sub_children = children_of.get(name, [])
        if sub_children:
            label = f"{name}  {total_ms:.3f}ms ({pct:.1f}%)  self: {self_ms:.3f}ms ({self_pct:.1f}%)  x{edge_call_count}"
            default_open = depth < 2
            with dpg.tree_node(label=label, parent=parent_tag, default_open=default_open):
                tag = dpg.last_item()
                dpg.bind_item_theme(tag, heat_themes[heat_bucket(pct)])
                for child_edge in sub_children:
                    add_tree_node(child_edge['child'], child_edge['total_ns'],
                                  child_edge['call_count'], depth + 1, tag)
        else:
            with dpg.group(horizontal=True, parent=parent_tag):
                t = dpg.add_text("  \u25b8 ")
                dpg.bind_item_theme(t, dim_theme)
                t = dpg.add_text(name)
                dpg.bind_item_theme(t, name_theme)
                t = dpg.add_text(f"  {total_ms:.3f}ms ({pct:.1f}%)")
                dpg.bind_item_theme(t, heat_themes[heat_bucket(pct)])
                t = dpg.add_text(f"  self: {self_ms:.3f}ms ({self_pct:.1f}%)")
                dpg.bind_item_theme(t, self_time_theme)
                t = dpg.add_text(f"  x{edge_call_count}")
                dpg.bind_item_theme(t, dim_theme)

    # Add root entries
    root_edges = children_of.get('', [])
    for edge in root_edges:
        add_tree_node(edge['child'], edge['total_ns'], edge['call_count'], 0, "tree_container")


# --- Flame chart view functions ---

flame_rects_cache = []


def rebuild_flame_chart(flat_zones, edges, frame_time_ns):
    """Rebuild flame chart data cache from tree data, then redraw."""
    global flame_rects_cache, flame_selected_idx

    flame_selected_idx = None  # cache invalidated, clear selection

    if not edges or frame_time_ns <= 0:
        flame_rects_cache = []
        redraw_flame_from_cache()
        return

    roots, children_of, node_info = build_call_tree(flat_zones, edges, frame_time_ns)
    flame_rects_cache = layout_flame_rects(roots, children_of, node_info, frame_time_ns)
    redraw_flame_from_cache()


def redraw_flame_from_cache():
    """Redraw flame chart from cached layout data. Fast — called on every zoom/pan."""
    dpg.delete_item("flame_canvas", children_only=True)

    rects = flame_rects_cache
    if not rects or current_frame_time_ns <= 0:
        dpg.draw_text((10, 20), "No tree data", parent="flame_canvas", size=14,
                       color=(200, 200, 100, 255))
        return

    vp_width = dpg.get_viewport_width() - 40
    canvas_width = max(vp_width, 400)
    max_depth = max((r['depth'] for r in rects), default=0) + 1
    row_height = 22
    canvas_height = max(max_depth * row_height + 40, 200)
    dpg.configure_item("flame_canvas", width=canvas_width, height=canvas_height)

    ns_per_px = current_frame_time_ns / (canvas_width - 20) / flame_zoom if flame_zoom > 0 else 1

    dpg.set_value("flame_status",
                  f"Flame Chart  |  {len(rects)} blocks  |  zoom: {flame_zoom:.1f}x")
    dpg.configure_item("flame_status", color=(100, 200, 100, 255))

    for i, rect in enumerate(rects):
        x0 = 10 + (rect['x_start_ns'] - flame_pan_ns) / ns_per_px
        x1 = 10 + (rect['x_end_ns'] - flame_pan_ns) / ns_per_px
        y0 = rect['depth'] * row_height + 5
        y1 = y0 + row_height - 2

        if x1 < 0 or x0 > canvas_width:
            continue
        x0 = max(0, x0)
        x1 = min(canvas_width, x1)
        if x1 - x0 < 1:
            continue

        color = name_to_color(rect['name'])
        selected = (i == flame_selected_idx)
        border = (255, 255, 255, 255) if selected else (40, 40, 50, 255)
        thickness = 2 if selected else 1
        dpg.draw_rectangle((x0, y0), (x1, y1), parent="flame_canvas",
                           fill=color, color=border, thickness=thickness)

        width_px = x1 - x0
        if width_px > 40:
            max_chars = int(width_px / 7)
            label = rect['name'][:max_chars]
            dpg.draw_text((x0 + 3, y0 + 3), label, parent="flame_canvas",
                          size=12, color=(255, 255, 255, 230))


def handle_flame_scroll(sender, value):
    """Handle mouse wheel for cursor-centered zoom on flame chart."""
    global flame_zoom, flame_pan_ns, flame_view_dirty
    if not dpg.is_item_hovered("flame_canvas"):
        return
    if current_frame_time_ns <= 0:
        return

    # Compute ns position under cursor before zoom
    mouse_pos = dpg.get_mouse_pos()
    canvas_pos = dpg.get_item_pos("flame_canvas")
    local_x = mouse_pos[0] - canvas_pos[0]

    canvas_width = max(dpg.get_viewport_width() - 40, 400)
    ns_per_px_old = current_frame_time_ns / (canvas_width - 20) / flame_zoom

    ns_at_cursor = flame_pan_ns + (local_x - 10) * ns_per_px_old

    # Apply zoom
    if value > 0:
        flame_zoom = min(flame_zoom * 1.3, 50.0)
    else:
        flame_zoom = max(flame_zoom / 1.3, 1.0)

    # Adjust pan so cursor position stays fixed
    ns_per_px_new = current_frame_time_ns / (canvas_width - 20) / flame_zoom
    flame_pan_ns = int(ns_at_cursor - (local_x - 10) * ns_per_px_new)
    flame_pan_ns = max(0, flame_pan_ns)

    flame_view_dirty = True


def _flame_hit_test(local_x, local_y):
    """Return index into flame_rects_cache under (local_x, local_y), or None."""
    if not flame_rects_cache or current_frame_time_ns <= 0:
        return None
    canvas_width = max(dpg.get_viewport_width() - 40, 400)
    ns_per_px = current_frame_time_ns / (canvas_width - 20) / flame_zoom if flame_zoom > 0 else 1
    row_height = 22
    for i, rect in enumerate(flame_rects_cache):
        x0 = 10 + (rect['x_start_ns'] - flame_pan_ns) / ns_per_px
        x1 = 10 + (rect['x_end_ns'] - flame_pan_ns) / ns_per_px
        y0 = rect['depth'] * row_height + 5
        y1 = y0 + row_height - 2
        if x0 <= local_x <= x1 and y0 <= local_y <= y1:
            return i
    return None


def handle_flame_drag():
    """Handle mouse drag for panning, click for selection, and hover tooltips."""
    global flame_dragging, flame_drag_start_x, flame_drag_start_pan, flame_pan_ns
    global flame_view_dirty, flame_selected_idx

    if not dpg.is_item_hovered("flame_canvas"):
        if flame_dragging:
            flame_dragging = False
        return

    mouse_pos = dpg.get_mouse_pos()
    canvas_pos = dpg.get_item_pos("flame_canvas")
    local_x = mouse_pos[0] - canvas_pos[0]
    local_y = mouse_pos[1] - canvas_pos[1]

    if dpg.is_mouse_button_down(dpg.mvMouseButton_Left):
        if not flame_dragging:
            flame_dragging = True
            flame_drag_start_x = mouse_pos[0]
            flame_drag_start_pan = flame_pan_ns
        else:
            dx = mouse_pos[0] - flame_drag_start_x
            if current_frame_time_ns > 0:
                canvas_width = max(dpg.get_viewport_width() - 40, 400)
                ns_per_px = current_frame_time_ns / (canvas_width - 20) / flame_zoom
                flame_pan_ns = flame_drag_start_pan - int(dx * ns_per_px)
                flame_pan_ns = max(0, flame_pan_ns)
                flame_view_dirty = True
    else:
        if flame_dragging:
            # Mouse released — check if it was a click (minimal drag)
            dx = abs(mouse_pos[0] - flame_drag_start_x)
            if dx < 4:
                hit = _flame_hit_test(local_x, local_y)
                old = flame_selected_idx
                flame_selected_idx = hit if hit != flame_selected_idx else None
                if flame_selected_idx != old:
                    flame_view_dirty = True
            flame_dragging = False

    # Tooltip on hover (show for hovered OR selected block)
    show_idx = None
    if not flame_dragging:
        show_idx = _flame_hit_test(local_x, local_y)
    if show_idx is None:
        show_idx = flame_selected_idx

    if show_idx is not None and 0 <= show_idx < len(flame_rects_cache):
        rect = flame_rects_cache[show_idx]
        total_ms = rect['total_ns'] / 1e6
        self_ms = rect['self_ns'] / 1e6
        pct = (rect['total_ns'] / current_frame_time_ns * 100.0
               if current_frame_time_ns > 0 else 0.0)
        dpg.set_value("flame_tooltip_text",
                      f"{rect['name']}\n"
                      f"Total: {total_ms:.3f} ms ({pct:.1f}%)\n"
                      f"Self:  {self_ms:.3f} ms\n"
                      f"Calls: {rect['call_count']}")
        dpg.configure_item("flame_tooltip", show=True,
                           pos=(mouse_pos[0] + 15, mouse_pos[1] + 10))
    else:
        dpg.configure_item("flame_tooltip", show=False)


# Register mouse wheel handler
with dpg.handler_registry():
    dpg.add_mouse_wheel_handler(callback=handle_flame_scroll)


# --- Main update loop ---

def update():
    global connected, tree_connected, last_display_time, pending_data, pending_tree_data
    global current_edges, current_frame_time_ns, flame_view_dirty

    # Flame chart interaction runs every frame regardless of data/pause state
    handle_flame_drag()
    if flame_view_dirty and flame_rects_cache:
        flame_view_dirty = False
        redraw_flame_from_cache()

    # Read flat data
    data = reader.read()
    if data is None:
        if not connected:
            reader.open()
    else:
        connected = True

    # Read tree data
    tree_data = tree_reader.read()
    if tree_data is None:
        if not tree_connected:
            tree_reader.open()
    else:
        tree_connected = True

    if data is None and tree_data is None:
        return

    paused = dpg.get_value("pause_checkbox")
    if paused:
        dpg.set_value("status_text", "PAUSED")
        dpg.configure_item("status_text", color=(200, 100, 100, 255))
        return

    mode = dpg.get_value("smoothing_mode")
    strength = dpg.get_value("strength_slider")

    # Update frame time graph (always, lightweight)
    if data is not None:
        frame_ms = data['frame_time_ns'] / 1e6
        fps = 1000.0 / frame_ms if frame_ms > 0 else 0
        current_frame_time_ns = data['frame_time_ns']

        tree_tag = "  |  tree" if tree_connected else ""
        mode_tag = "" if mode == SMOOTHING_RAW else f"  |  {mode}"
        dpg.set_value("status_text",
                      f"Frame: {frame_ms:.2f} ms  |  {fps:.0f} fps  |  "
                      f"{len(data['zones'])} zones{tree_tag}{mode_tag}")
        dpg.configure_item("status_text", color=(100, 200, 100, 255))

        frame_times_ms.append(frame_ms)
        dpg.set_value("ft_line", [frame_indices, list(frame_times_ms)])

        max_ft = max(frame_times_ms) if frame_times_ms else 16.0
        dpg.set_axis_limits("ft_y_axis", 0, max(max_ft * 1.2, 1.0))

        smooth_zones(data['zones'], mode, strength)
        pending_data = data

    if tree_data is not None:
        smooth_edges(tree_data['edges'], mode, strength)
        pending_tree_data = tree_data

    # Throttled display refresh
    refresh_hz = dpg.get_value("refresh_hz_slider")
    now = time.monotonic()
    if now - last_display_time < 1.0 / refresh_hz:
        return
    last_display_time = now

    top_n = dpg.get_value("top_n_slider")

    # Flat view
    if pending_data is not None:
        zones = smooth_zones(pending_data['zones'], mode, strength)
        update_flat_view(zones, top_n)

    # Tree + flame views
    if pending_data is not None and pending_tree_data is not None:
        flat_zones = smooth_zones(pending_data['zones'], mode, strength)
        edges = smooth_edges(pending_tree_data['edges'], mode, strength)
        ft_ns = pending_data['frame_time_ns']
        current_edges = edges

        rebuild_tree_view(flat_zones, edges, ft_ns)
        rebuild_flame_chart(flat_zones, edges, ft_ns)


# --- Main loop ---

dpg.setup_dearpygui()
dpg.show_viewport()
dpg.set_primary_window("main_window", True)

while dpg.is_dearpygui_running():
    update()
    dpg.render_dearpygui_frame()

reader.close()
tree_reader.close()
dpg.destroy_context()
