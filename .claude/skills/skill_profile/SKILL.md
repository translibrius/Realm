---
name: skill_profile
description: Generate and analyze a Realm profiler report. Use when the user asks to profile, find performance bottlenecks, or optimize the engine.
argument-hint: [snapshot|session <path>]
allowed-tools: Bash(python3 *), Read, Grep, Glob
---

# Realm Profiler Analysis

Generate a profiler report and analyze hotspots to suggest optimizations.

## Steps

1. **Generate the report.** Use the appropriate mode based on `$ARGUMENTS`:
   - Default (no args or `snapshot`): live snapshot from shared memory (engine must be running)
   - `session <path>`: read a session binary file

   ```bash
   # Snapshot (default)
   python3 tools/profiler_report.py --snapshot --source-root . --output /tmp/realm_profile_report.md

   # Session file
   python3 tools/profiler_report.py --session <path> --source-root . --output /tmp/realm_profile_report.md
   ```

   If the snapshot fails with "Shared memory not available", tell the user the engine must be running with a profile build (`cmake --preset debug -DRL_PROFILE=ON && cmake --build --preset debug`).

2. **Read the report** from `/tmp/realm_profile_report.md`.

3. **Analyze the hotspots.** For each top function:
   - Read the source file at the location listed in the report
   - Identify why it's expensive (algorithm, I/O, driver call, per-frame allocation, etc.)
   - Suggest concrete optimizations with code-level specificity

4. **Present findings** as a ranked summary:
   - Hotspot name, % of frame time, call count
   - Root cause
   - Suggested fix (with code references)
   - Expected impact (high/medium/low)

## Key files

- `tools/profiler_report.py` — report generator (stdlib only)
- `tools/profiler_view.py` — DearPyGui real-time visualizer
- `engine/src/profiler/profiler.c` — instrumentation hooks, shared memory broadcast
- `engine/include/profiler/profiler.h` — public API

## Context

- Profile build: `cmake --preset debug -DRL_PROFILE=ON && cmake --build --preset debug`
- Runtime: F3 = launch visualizer, F4 = generate snapshot report
- On exit the engine writes `profiler_session.bin` (lifetime-averaged stats)
- Shared memory name: `/realm_profiler`, 24-byte header + 64 zones of 64 bytes each
- Data updates every 6 frames (~10 Hz at 60fps)
