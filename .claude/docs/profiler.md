# Profiler

Build with profiling: `cmake --preset debug -DRL_PROFILE=ON && cmake --build --preset debug`

## Runtime hotkeys (profile builds only)

- **F3** — launch external DearPyGui visualizer (`tools/profiler_view.py`)
- **F4** — generate a snapshot report (`tools/profiler_report.py --snapshot`)

## Session report

On exit, the engine writes `profiler_session.bin` (binary session summary with lifetime-averaged stats).

## Generating reports

```bash
# From build/debug/bin/ (scripts are copied there on profile builds):
python3 profiler_report.py --snapshot --source-root /path/to/Realm    # live snapshot from shared memory
python3 profiler_report.py --session profiler_session.bin --source-root /path/to/Realm  # session summary
```

Reports are LLM-friendly markdown with function hotspots, call counts, percentages, source locations, and code snippets. To review a report: read the generated `.md` file and provide optimization advice based on the hotspots, source code, and call patterns.

## Key files

- `engine/src/profiler/profiler.c` — instrumentation hooks, aggregation, shared memory broadcast, session report writer
- `engine/include/profiler/profiler.h` — public API (`rl_profiler_init/shutdown/frame_mark/write_session_report`)
- `tools/profiler_view.py` — DearPyGui real-time visualizer (requires `pip install dearpygui`)
- `tools/profiler_report.py` — markdown report generator (stdlib only, no pip deps)
- `realm/src/event_handler.c` — F3/F4 hotkey handlers (guarded by `#if RL_PROFILE_ENABLED`)
