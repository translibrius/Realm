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


# --- Application state ---

FRAME_HISTORY_LEN = 300
TOP_N_BARS = 15

frame_times_ms = deque([0.0] * FRAME_HISTORY_LEN, maxlen=FRAME_HISTORY_LEN)
frame_indices = list(range(FRAME_HISTORY_LEN))
current_zones = []
reader = ShmReader()
connected = False


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

# --- Main window ---

dpg.create_viewport(title="Realm Profiler", width=1000, height=750, min_width=600, min_height=400)

with dpg.window(tag="main_window"):
    # Status header
    dpg.add_text("Waiting for Realm...", tag="status_text", color=(200, 200, 100, 255))
    dpg.add_separator()

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
            dpg.add_text(str(z['call_count']))
            dpg.add_text(f"{total_ms:.3f}")
            dpg.add_text(f"{avg_us:.1f}")
            dpg.add_text(f"{max_us:.1f}")


def update():
    """Called every frame by DearPyGui."""
    global connected, current_zones

    data = reader.read()

    if data is None:
        # Try to reconnect periodically
        if not connected:
            reader.open()
        return

    connected = True
    frame_ms = data['frame_time_ns'] / 1e6
    fps = 1000.0 / frame_ms if frame_ms > 0 else 0

    # Update status
    dpg.set_value("status_text", f"Frame: {frame_ms:.2f} ms  |  {fps:.0f} fps  |  {len(data['zones'])} zones")
    dpg.configure_item("status_text", color=(100, 200, 100, 255))

    # Update frame time history
    frame_times_ms.append(frame_ms)
    dpg.set_value("ft_line", [frame_indices, list(frame_times_ms)])

    # Auto-scale Y axis
    max_ft = max(frame_times_ms) if frame_times_ms else 16.0
    dpg.set_axis_limits("ft_y_axis", 0, max(max_ft * 1.2, 1.0))

    # Update bar chart (top N functions by total_ns)
    zones = data['zones']
    current_zones = zones

    top = zones[:TOP_N_BARS]
    top.reverse()  # Horizontal bars: bottom = biggest

    if top:
        values = [z['total_ns'] / 1e6 for z in top]
        positions = list(range(len(top)))
        labels = [(z['name'][:28] if len(z['name']) > 28 else z['name']) for z in top]

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
