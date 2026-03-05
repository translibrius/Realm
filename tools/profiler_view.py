#!/usr/bin/env python3
"""
Realm Profiler Viewer — DearPyGui-based real-time visualizer.

Reads profiler data from shared memory written by the engine
(engine/src/profiler/profiler.c) and displays live graphs + table.

Usage:
    python3 tools/profiler_view.py

Requires: pip install dearpygui
"""

import mmap
import struct
import sys
import time
import platform as plat
from collections import deque

import dearpygui.dearpygui as dpg

# --- Shared memory protocol (must match profiler.c) ---

SHM_NAME = "/realm_profiler"
SHM_MAGIC = 0x524C5046  # "RLPF"
ZONE_NAME_LEN = 32
MAX_BROADCAST_ZONES = 64

# shm_header: magic(u32) sequence(u32) zone_count(u32) _pad(u32) frame_time_ns(i64)
HEADER_FMT = "<IIIIq"
HEADER_SIZE = struct.calcsize(HEADER_FMT)

# shm_zone: name(32s) call_count(u32) [4 pad] total_ns(i64) max_ns(i64) avg_ns(i64)
# C struct has 4 bytes padding after call_count for i64 alignment → 64 bytes total
ZONE_FMT = "<32sI4xqqq"
ZONE_SIZE = struct.calcsize(ZONE_FMT)
assert ZONE_SIZE == 64, f"Zone size mismatch: {ZONE_SIZE} != 64"

SHM_TOTAL_SIZE = HEADER_SIZE + MAX_BROADCAST_ZONES * ZONE_SIZE

# --- Shared memory reader ---

class ShmReader:
    def __init__(self):
        self.mm = None
        self.fd = None
        self.last_seq = 0

    def open(self):
        """Open the shared memory region created by the engine. Returns True on success."""
        if self.mm is not None:
            return True

        try:
            if plat.system() in ("Darwin", "Linux"):
                return self._open_posix()
            elif plat.system() == "Windows":
                return self._open_windows()
        except Exception:
            self.mm = None
            self.fd = None
        return False

    def _open_posix(self):
        import ctypes
        import os

        if plat.system() == "Darwin":
            libc = ctypes.CDLL("libSystem.B.dylib", use_errno=True)
        else:
            libc = ctypes.CDLL("librt.so.1", use_errno=True)

        shm_open_fn = libc.shm_open
        shm_open_fn.restype = ctypes.c_int
        shm_open_fn.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_uint]

        fd = shm_open_fn(SHM_NAME.encode(), os.O_RDONLY, 0o666)
        if fd < 0:
            return False

        self.fd = fd
        self.mm = mmap.mmap(fd, SHM_TOTAL_SIZE, access=mmap.ACCESS_READ)
        return True

    def _open_windows(self):
        self.mm = mmap.mmap(-1, SHM_TOTAL_SIZE, tagname="realm_profiler", access=mmap.ACCESS_READ)
        return self.mm is not None

    def read(self):
        """Read the current frame from shared memory. Returns (frame_data, zones) or None."""
        if self.mm is None:
            if not self.open():
                return None

        try:
            self.mm.seek(0)
            header_data = self.mm.read(HEADER_SIZE)
        except (ValueError, OSError):
            # Shared memory was closed/invalidated
            self.close()
            return None

        magic, seq, zone_count, _pad, frame_time_ns = struct.unpack(HEADER_FMT, header_data)

        if magic != SHM_MAGIC:
            # Engine not running or shutting down
            self.close()
            return None

        if seq == self.last_seq:
            return None  # No new data

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

        # Re-read sequence to check for torn read
        self.mm.seek(4)
        seq2_data = self.mm.read(4)
        seq2 = struct.unpack("<I", seq2_data)[0]
        if seq2 != seq:
            return None  # Torn read, skip this frame

        self.last_seq = seq
        return {
            'frame_time_ns': frame_time_ns,
            'zones': zones,
        }

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
STALE_LIMIT = 30  # Prune entries not seen for this many updates

ema_state = {}       # name → {total_ns, avg_ns, max_ns, call_count} (floats)
avg_history = {}     # name → {key: deque} for running average
stale_counter = {}   # name → int, incremented when zone absent


def smooth_zones(raw_zones, mode, strength):
    """Apply smoothing to raw zone data. Returns a new list of smoothed zone dicts."""
    if mode == SMOOTHING_RAW:
        return list(raw_zones)

    present = set()

    if mode == SMOOTHING_EMA:
        alpha = 1.0 - strength  # strength 0.7 → alpha 0.3 (30% new, 70% old)
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

        # Build output from smoothed state, only for zones in current snapshot
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
        # Map strength to window size: 0.0 → 3, 1.0 → 30
        window = int(3 + strength * 27)
        for z in raw_zones:
            name = z['name']
            present.add(name)
            stale_counter[name] = 0
            if name not in avg_history:
                avg_history[name] = {k: deque(maxlen=window) for k in NUMERIC_KEYS}
            else:
                # Resize deques if window changed
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

    # Prune stale entries
    for name in list(stale_counter.keys()):
        if name not in present:
            stale_counter[name] += 1
            if stale_counter[name] > STALE_LIMIT:
                ema_state.pop(name, None)
                avg_history.pop(name, None)
                del stale_counter[name]

    return result


# --- Rank-stable sort ---
# Maintains the previous sort order and only swaps adjacent entries when the
# difference exceeds a percentage threshold.  This prevents functions with
# similar timings from flickering positions every update.

prev_rank_order = []  # list of function names in last displayed order


def rank_stable_sort(zones, key='total_ns', threshold_pct=0.10):
    """Sort zones but resist swapping neighbours closer than threshold_pct."""
    global prev_rank_order

    # Build lookup by name
    by_name = {z['name']: z for z in zones}

    if not prev_rank_order:
        # First frame — just do a plain descending sort
        zones.sort(key=lambda z: z[key], reverse=True)
        prev_rank_order = [z['name'] for z in zones]
        return zones

    # Start from previous order, appending any new names at the end
    ordered = []
    for name in prev_rank_order:
        if name in by_name:
            ordered.append(by_name.pop(name))
    # New functions that weren't in previous order
    newcomers = sorted(by_name.values(), key=lambda z: z[key], reverse=True)
    ordered.extend(newcomers)

    # Bubble pass: swap adjacent pairs only if the lower-ranked one is
    # significantly larger than the higher-ranked one
    changed = True
    while changed:
        changed = False
        for i in range(len(ordered) - 1):
            upper = ordered[i][key]
            lower = ordered[i + 1][key]
            # lower should swap up only if it exceeds upper by threshold
            if lower > upper and (upper == 0 or (lower - upper) / upper > threshold_pct):
                ordered[i], ordered[i + 1] = ordered[i + 1], ordered[i]
                changed = True

    prev_rank_order = [z['name'] for z in ordered]
    return ordered


# --- Application state ---

FRAME_HISTORY_LEN = 300
DEFAULT_TOP_N = 15
DEFAULT_REFRESH_HZ = 2.0

frame_times_ms = deque([0.0] * FRAME_HISTORY_LEN, maxlen=FRAME_HISTORY_LEN)
frame_indices = list(range(FRAME_HISTORY_LEN))
current_zones = []
reader = ShmReader()
connected = False
last_display_time = 0.0     # monotonic time of last table/bar refresh
pending_data = None          # latest data waiting for next display tick


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
        dpg.add_theme_style(dpg.mvStyleVar_WindowRounding, 4)
        dpg.add_theme_style(dpg.mvStyleVar_FrameRounding, 3)

dpg.bind_theme(global_theme)

# Plot background theme
with dpg.theme() as plot_theme:
    with dpg.theme_component(dpg.mvAll):
        dpg.add_theme_color(dpg.mvPlotCol_PlotBg, (25, 25, 30, 255), category=dpg.mvThemeCat_Plots)

# Bar chart color theme
with dpg.theme() as bar_theme:
    with dpg.theme_component(dpg.mvBarSeries):
        dpg.add_theme_color(dpg.mvPlotCol_Fill, (100, 200, 100, 200), category=dpg.mvThemeCat_Plots)
        dpg.add_theme_color(dpg.mvPlotCol_Line, (100, 200, 100, 255), category=dpg.mvThemeCat_Plots)

# Frame time line theme
with dpg.theme() as line_theme:
    with dpg.theme_component(dpg.mvLineSeries):
        dpg.add_theme_color(dpg.mvPlotCol_Line, (100, 180, 255, 255), category=dpg.mvThemeCat_Plots)

# --- Hint text theme (dimmed) ---
with dpg.theme() as hint_theme:
    with dpg.theme_component(dpg.mvAll):
        dpg.add_theme_color(dpg.mvThemeCol_Text, (120, 120, 140, 255))

# --- Main window ---

dpg.create_viewport(title="Realm Profiler", width=1000, height=750, min_width=600, min_height=400)


def on_smoothing_mode_changed(sender, value):
    """Show/hide the strength slider based on mode."""
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

    # Frame time plot
    with dpg.collapsing_header(label="Frame Time History", default_open=True):
        with dpg.plot(label="##frame_time", height=180, width=-1, tag="frame_plot"):
            dpg.bind_item_theme("frame_plot", plot_theme)
            dpg.add_plot_axis(dpg.mvXAxis, label="Frame", tag="ft_x_axis", no_tick_labels=True)
            with dpg.plot_axis(dpg.mvYAxis, label="ms", tag="ft_y_axis"):
                dpg.set_axis_limits("ft_y_axis", 0, 33)
                dpg.add_line_series(frame_indices, list(frame_times_ms), label="Frame Time",
                                    tag="ft_line", parent="ft_y_axis")
                dpg.bind_item_theme("ft_line", line_theme)

    # Top functions bar chart
    with dpg.collapsing_header(label="Top Functions", default_open=True):
        with dpg.plot(label="##top_funcs", height=250, width=-1, tag="bar_plot"):
            dpg.bind_item_theme("bar_plot", plot_theme)
            dpg.add_plot_axis(dpg.mvXAxis, label="ms", tag="bar_x_axis")
            with dpg.plot_axis(dpg.mvYAxis, label="", tag="bar_y_axis", no_tick_labels=True):
                dpg.set_axis_ticks("bar_y_axis", tuple())
                dpg.add_bar_series([], [], label="Total", horizontal=True,
                                   tag="bar_series", parent="bar_y_axis")
                dpg.bind_item_theme("bar_series", bar_theme)

    # Detail table
    with dpg.collapsing_header(label="Function Detail", default_open=True):
        with dpg.table(tag="zone_table", header_row=True, resizable=True,
                       borders_innerH=True, borders_outerH=True,
                       borders_innerV=True, borders_outerV=True,
                       row_background=True, sortable=True,
                       callback=lambda s, d: sort_table(d)):
            dpg.add_table_column(label="Function", width_fixed=True, init_width_or_weight=280)
            dpg.add_table_column(label="Calls", width_fixed=True, init_width_or_weight=70)
            dpg.add_table_column(label="Total (ms)", width_fixed=True, init_width_or_weight=100,
                                 default_sort=True)
            dpg.add_table_column(label="Avg (us)", width_fixed=True, init_width_or_weight=100)
            dpg.add_table_column(label="Max (us)", width_fixed=True, init_width_or_weight=100)


def sort_table(sort_specs):
    """Sort the zone table by the clicked column."""
    if sort_specs is None or len(sort_specs) == 0:
        return
    col_idx = sort_specs[0][0]
    reverse = sort_specs[0][1] == -1  # -1 = descending
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
    """Rebuild the table rows from current_zones."""
    # Delete existing rows
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


def update():
    """Called every frame by DearPyGui."""
    global connected, current_zones, last_display_time, pending_data

    data = reader.read()

    if data is None:
        # Try to reconnect periodically
        if not connected:
            reader.open()
        return

    connected = True

    # Read settings
    paused = dpg.get_value("pause_checkbox")

    if paused:
        dpg.set_value("status_text", "PAUSED")
        dpg.configure_item("status_text", color=(200, 100, 100, 255))
        return

    frame_ms = data['frame_time_ns'] / 1e6
    fps = 1000.0 / frame_ms if frame_ms > 0 else 0
    mode = dpg.get_value("smoothing_mode")
    strength = dpg.get_value("strength_slider")

    # Always update status line and frame time graph (lightweight)
    mode_tag = "" if mode == SMOOTHING_RAW else f"  |  {mode}"
    dpg.set_value("status_text",
                  f"Frame: {frame_ms:.2f} ms  |  {fps:.0f} fps  |  "
                  f"{len(data['zones'])} zones{mode_tag}")
    dpg.configure_item("status_text", color=(100, 200, 100, 255))

    frame_times_ms.append(frame_ms)
    dpg.set_value("ft_line", [frame_indices, list(frame_times_ms)])

    max_ft = max(frame_times_ms) if frame_times_ms else 16.0
    dpg.set_axis_limits("ft_y_axis", 0, max(max_ft * 1.2, 1.0))

    # Always feed data into smoothing (keeps EMA/running avg up to date)
    # but only refresh the table and bar chart at the configured rate
    smooth_zones(data['zones'], mode, strength)
    pending_data = data

    refresh_hz = dpg.get_value("refresh_hz_slider")
    now = time.monotonic()
    if now - last_display_time < 1.0 / refresh_hz:
        return
    last_display_time = now

    # --- Display refresh (throttled) ---
    top_n = dpg.get_value("top_n_slider")

    # Rebuild smoothed zones from current state for display
    zones = smooth_zones(pending_data['zones'], mode, strength)

    # Rank-stable sort: resists swapping neighbours with similar values
    zones = rank_stable_sort(zones)
    current_zones = zones

    # Update bar chart (top N functions)
    top = zones[:top_n]
    top_reversed = list(reversed(top))  # Horizontal bars: bottom = biggest

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

    # Rebuild detail table
    rebuild_table()


# --- Main loop ---

dpg.setup_dearpygui()
dpg.show_viewport()
dpg.set_primary_window("main_window", True)

while dpg.is_dearpygui_running():
    update()
    dpg.render_dearpygui_frame()

reader.close()
dpg.destroy_context()
