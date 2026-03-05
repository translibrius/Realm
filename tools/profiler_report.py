#!/usr/bin/env python3
"""
Realm Profiler Report — generates LLM-friendly markdown reports.

Two modes:
  --snapshot           Read current shared memory, write profiler_snapshot_<ts>.md
  --session FILE       Read a session binary file, write profiler_session_<ts>.md

Optional:
  --source-root DIR    Root of the source tree (for finding function definitions)
  --output FILE        Override output path

Requires: stdlib only (no pip dependencies).

Usage:
    python3 profiler_report.py --snapshot --source-root /path/to/Realm
    python3 profiler_report.py --session profiler_session.bin --source-root /path/to/Realm
"""

import argparse
import mmap
import os
import struct
import subprocess
import sys
import platform as plat
from datetime import datetime

# --- Shared memory / binary protocol (must match profiler.c) ---

SHM_NAME = "/realm_profiler"
SHM_MAGIC = 0x524C5046  # "RLPF"
ZONE_NAME_LEN = 32
MAX_BROADCAST_ZONES = 64

# shm_header: magic(u32) sequence(u32) zone_count(u32) _pad(u32) frame_time_ns(i64)
HEADER_FMT = "<IIIIq"
HEADER_SIZE = struct.calcsize(HEADER_FMT)

# shm_zone: name(32s) call_count(u32) [4 pad] total_ns(i64) max_ns(i64) avg_ns(i64)
ZONE_FMT = "<32sI4xqqq"
ZONE_SIZE = struct.calcsize(ZONE_FMT)
assert ZONE_SIZE == 64, f"Zone size mismatch: {ZONE_SIZE} != 64"

SHM_TOTAL_SIZE = HEADER_SIZE + MAX_BROADCAST_ZONES * ZONE_SIZE

# Edge data (call tree) appended after flat data in session files
EDGE_MAGIC = 0x524C5054  # "RLPT"
MAX_BROADCAST_EDGES = 256

# shm_edge: parent_name(32s) child_name(32s) call_count(u32) pad(4) total_ns(i64) max_ns(i64) avg_ns(i64)
EDGE_FMT = "<32s32sI4xqqq"
EDGE_SIZE = struct.calcsize(EDGE_FMT)
assert EDGE_SIZE == 96, f"Edge size mismatch: {EDGE_SIZE} != 96"


def read_shared_memory():
    """Read profiler data from POSIX shared memory. Returns dict or None."""
    try:
        if plat.system() in ("Darwin", "Linux"):
            return _read_shm_posix()
        elif plat.system() == "Windows":
            return _read_shm_windows()
    except Exception as e:
        print(f"Failed to read shared memory: {e}", file=sys.stderr)
    return None


def _read_shm_posix():
    import ctypes

    if plat.system() == "Darwin":
        libc = ctypes.CDLL("libSystem.B.dylib", use_errno=True)
    else:
        libc = ctypes.CDLL("librt.so.1", use_errno=True)

    shm_open_fn = libc.shm_open
    shm_open_fn.restype = ctypes.c_int
    shm_open_fn.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_uint]

    fd = shm_open_fn(SHM_NAME.encode(), os.O_RDONLY, 0o666)
    if fd < 0:
        print("Shared memory not available (engine not running?)", file=sys.stderr)
        return None

    try:
        mm = mmap.mmap(fd, SHM_TOTAL_SIZE, access=mmap.ACCESS_READ)
        result = _parse_data(mm.read(SHM_TOTAL_SIZE))
        mm.close()
        return result
    finally:
        os.close(fd)


def _read_shm_windows():
    mm = mmap.mmap(-1, SHM_TOTAL_SIZE, tagname="realm_profiler", access=mmap.ACCESS_READ)
    if mm is None:
        return None
    result = _parse_data(mm.read(SHM_TOTAL_SIZE))
    mm.close()
    return result


def read_binary_file(path):
    """Read profiler data from a binary session file. Returns dict or None."""
    try:
        with open(path, "rb") as f:
            data = f.read()
        result = _parse_data(data)
        if result is not None:
            result['edges'] = _parse_edges(data)
        return result
    except Exception as e:
        print(f"Failed to read session file: {e}", file=sys.stderr)
        return None


def _parse_data(data):
    """Parse raw bytes (shared memory or file) into a dict."""
    if len(data) < HEADER_SIZE:
        return None

    magic, sequence, zone_count, _pad, frame_time_ns = struct.unpack_from(HEADER_FMT, data, 0)
    if magic != SHM_MAGIC:
        print(f"Bad magic: 0x{magic:08X} (expected 0x{SHM_MAGIC:08X})", file=sys.stderr)
        return None

    zone_count = min(zone_count, MAX_BROADCAST_ZONES)
    zones = []
    offset = HEADER_SIZE
    for _ in range(zone_count):
        if offset + ZONE_SIZE > len(data):
            break
        name_raw, call_count, total_ns, max_ns, avg_ns = struct.unpack_from(ZONE_FMT, data, offset)
        name = name_raw.split(b'\x00', 1)[0].decode('utf-8', errors='replace')
        zones.append({
            'name': name,
            'call_count': call_count,
            'total_ns': total_ns,
            'max_ns': max_ns,
            'avg_ns': avg_ns,
        })
        offset += ZONE_SIZE

    return {
        'frame_time_ns': frame_time_ns,
        'frame_count': sequence,  # repurposed in session files
        'zones': zones,
        'edges': [],
    }


def _parse_edges(data):
    """Parse edge data appended after the flat header in session files."""
    offset = SHM_TOTAL_SIZE
    if offset + 8 > len(data):
        return []

    magic, edge_count = struct.unpack_from("<II", data, offset)
    if magic != EDGE_MAGIC:
        return []

    offset += 8
    edge_count = min(edge_count, MAX_BROADCAST_EDGES)
    edges = []
    for _ in range(edge_count):
        if offset + EDGE_SIZE > len(data):
            break
        parent_raw, child_raw, call_count, total_ns, max_ns, avg_ns = struct.unpack_from(
            EDGE_FMT, data, offset)
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
        offset += EDGE_SIZE
    return edges


# --- Source location finder ---

def find_function_location(func_name, source_root):
    """Use grep to find where a function is defined. Returns (file, line, snippet) or None."""
    if not source_root or not os.path.isdir(source_root):
        return None

    # Search for function definition patterns in C files
    # Match: "type func_name(" at start of line (with optional qualifiers)
    patterns = [
        rf'^[a-zA-Z_].*\b{func_name}\s*\(',   # standard definition
        rf'^\w+\s+\**{func_name}\s*\(',         # return type + name
    ]

    for pattern in patterns:
        try:
            result = subprocess.run(
                ['grep', '-rnE', pattern, '--include=*.c', '--include=*.h', '--include=*.m',
                 source_root],
                capture_output=True, text=True, timeout=5
            )
            if result.returncode == 0 and result.stdout.strip():
                matches = result.stdout.strip().split('\n')
                # Prefer .c/.m files (implementations) over .h files (declarations)
                best = None
                for line in matches:
                    parts = line.split(':', 2)
                    if len(parts) < 3:
                        continue
                    filepath = parts[0]
                    lineno = int(parts[1])
                    ext = os.path.splitext(filepath)[1]
                    is_impl = ext in ('.c', '.m')
                    # Skip vendor files if we have a non-vendor match
                    is_vendor = '/vendor/' in filepath
                    if best is None:
                        best = (filepath, lineno, is_impl, is_vendor)
                    elif is_impl and not best[2]:
                        best = (filepath, lineno, is_impl, is_vendor)
                    elif is_impl == best[2] and not is_vendor and best[3]:
                        best = (filepath, lineno, is_impl, is_vendor)

                if best:
                    filepath, lineno = best[0], best[1]
                    rel_path = os.path.relpath(filepath, source_root)
                    snippet = _extract_snippet(filepath, lineno, context=4)
                    return rel_path, lineno, snippet
        except (subprocess.TimeoutExpired, Exception):
            continue

    return None


def _extract_snippet(filepath, lineno, context=4):
    """Extract a few lines of code around the given line number."""
    try:
        with open(filepath, 'r') as f:
            lines = f.readlines()
        start = max(0, lineno - 1)
        end = min(len(lines), lineno + context)
        return ''.join(lines[start:end]).rstrip()
    except Exception:
        return None


# --- Report generation ---

def generate_report(data, source_root=None, mode="snapshot"):
    """Generate a markdown report from profiler data."""
    now = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    frame_ms = data['frame_time_ns'] / 1e6
    fps = 1000.0 / frame_ms if frame_ms > 0 else 0
    zones = data['zones']
    total_zone_ns = sum(z['total_ns'] for z in zones)

    lines = []
    lines.append(f"# Realm Profiler Report — {now}")
    lines.append("")

    if mode == "session":
        lines.append(f"**Mode:** Session summary ({data.get('frame_count', '?')} frames)")
    else:
        lines.append("**Mode:** Live snapshot")
    lines.append("")

    lines.append("## Summary")
    lines.append(f"- **Frame time:** {frame_ms:.2f} ms ({fps:.0f} fps)")
    lines.append(f"- **Active zones:** {len(zones)}")

    if zones:
        top3_ns = sum(z['total_ns'] for z in zones[:3])
        top3_pct = (top3_ns / total_zone_ns * 100) if total_zone_ns > 0 else 0
        lines.append(f"- **Top 3 hotspots:** {top3_pct:.0f}% of measured time")
    lines.append("")

    lines.append("## Hotspots")
    lines.append("")

    for i, z in enumerate(zones):
        total_ms = z['total_ns'] / 1e6
        avg_us = z['avg_ns'] / 1e3
        max_us = z['max_ns'] / 1e3
        pct = (z['total_ns'] / data['frame_time_ns'] * 100) if data['frame_time_ns'] > 0 else 0

        lines.append(f"### {i+1}. `{z['name']}` — {total_ms:.3f} ms/frame ({pct:.1f}%)")
        lines.append(f"- Calls: {z['call_count']}/frame | Avg: {avg_us:.1f} us | Max: {max_us:.1f} us")

        # Try to find source location
        if source_root:
            loc = find_function_location(z['name'], source_root)
            if loc:
                rel_path, lineno, snippet = loc
                lines.append(f"- **Location:** `{rel_path}:{lineno}`")
                if snippet:
                    lines.append("```c")
                    lines.append(snippet)
                    lines.append("```")

        lines.append("")

    # Call tree section (if edge data available)
    edges = data.get('edges', [])
    if edges:
        lines.append("## Call Tree")
        lines.append("")

        # Build tree structure
        from collections import defaultdict
        children_of = defaultdict(list)
        for e in edges:
            children_of[e['parent']].append(e)
        for parent in children_of:
            children_of[parent].sort(key=lambda e: e['total_ns'], reverse=True)

        # Build node info for self-time
        flat_by_name = {z['name']: z for z in zones}
        node_self_ns = {}
        for name, z in flat_by_name.items():
            child_time = sum(e['total_ns'] for e in children_of.get(name, []))
            node_self_ns[name] = max(0, z['total_ns'] - child_time)

        def format_tree_node(name, edge_total_ns, edge_call_count, depth):
            total_ms = edge_total_ns / 1e6
            pct = (edge_total_ns / data['frame_time_ns'] * 100) if data['frame_time_ns'] > 0 else 0
            self_ns = node_self_ns.get(name, edge_total_ns)
            self_ms = self_ns / 1e6
            self_pct = (self_ns / data['frame_time_ns'] * 100) if data['frame_time_ns'] > 0 else 0
            indent = "  " * depth
            result = []
            result.append(f"{indent}- `{name}` — {total_ms:.3f} ms ({pct:.1f}%), "
                          f"self: {self_ms:.3f} ms ({self_pct:.1f}%), x{edge_call_count}")
            for child_edge in children_of.get(name, []):
                if child_edge['total_ns'] / 1e6 >= 0.001:  # skip sub-microsecond
                    result.extend(format_tree_node(
                        child_edge['child'], child_edge['total_ns'],
                        child_edge['call_count'], depth + 1))
            return result

        root_edges = children_of.get('', [])
        for edge in root_edges:
            lines.extend(format_tree_node(edge['child'], edge['total_ns'],
                                          edge['call_count'], 0))
        lines.append("")

    lines.append("---")
    lines.append("")
    lines.append("## How to use this report")
    lines.append("")
    lines.append("Paste this report into an LLM (e.g. Claude) and ask:")
    lines.append("- \"What are the main performance bottlenecks and how can I optimize them?\"")
    lines.append("- \"Which functions could benefit from batching or caching?\"")
    lines.append("- \"Are there any obvious algorithmic improvements?\"")
    lines.append("")

    return '\n'.join(lines)


# --- Main ---

def main():
    parser = argparse.ArgumentParser(description="Realm Profiler Report Generator")
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--snapshot", action="store_true", help="Read from shared memory (live snapshot)")
    group.add_argument("--session", metavar="FILE", help="Read from session binary file")
    parser.add_argument("--source-root", metavar="DIR", help="Source tree root for finding function definitions")
    parser.add_argument("--output", metavar="FILE", help="Override output file path")
    args = parser.parse_args()

    # Read data
    if args.snapshot:
        data = read_shared_memory()
        mode = "snapshot"
    else:
        data = read_binary_file(args.session)
        mode = "session"

    if data is None:
        print("ERROR: No profiler data available.", file=sys.stderr)
        sys.exit(1)

    if not data['zones']:
        print("WARNING: No profiler zones found (profiler may not have collected data yet).", file=sys.stderr)
        sys.exit(1)

    # Generate report
    report = generate_report(data, source_root=args.source_root, mode=mode)

    # Write output
    if args.output:
        output_path = args.output
    else:
        ts = datetime.now().strftime("%Y%m%d_%H%M%S")
        output_path = f"profiler_{mode}_{ts}.md"

    with open(output_path, 'w') as f:
        f.write(report)

    print(f"Report written to: {output_path}")


if __name__ == "__main__":
    main()
