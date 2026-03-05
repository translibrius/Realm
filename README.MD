# Realm Engine

[![Build & Test](https://github.com/translibrius/Realm/actions/workflows/build.yml/badge.svg)](https://github.com/translibrius/Realm/actions/workflows/build.yml)
![Last Commit](https://img.shields.io/github/last-commit/translibrius/Realm?style=flat)
![License](https://img.shields.io/github/license/translibrius/Realm?style=flat)
![Lines of Code](https://img.shields.io/endpoint?url=https%3A%2F%2Fghloc.vercel.app%2Fapi%2Ftranslibrius%2FRealm%2Fbadge%3Ffilter%3D.c%2C.h%2C.m&style=flat&label=lines%20of%20code)

![Showcase](showcase.png)

A from-scratch game engine written in **C23** with dual **Vulkan** and **OpenGL 3.3** backends. No frameworks, no ECS libraries, no hidden dependencies — just C, graphics APIs, and a lot of manual memory management.

Built as a hobbyist deep dive into engine architecture, renderer design, and the kind of low-level plumbing that game frameworks usually hide from you.

## Features

- **Dual renderer backend** — OpenGL 3.3 and Vulkan, switchable at runtime with a single keypress
- **Hot-reload game module** — edit gameplay code, hit F5, and see changes without restarting. State is preserved across reloads when the layout is compatible
- **Virtual memory arena allocator** — reserve/commit scheme with page-aligned growth, per-frame scratch arenas that auto-clear, and thread-local scratch for async work
- **Clay-based GUI toolkit** — panels, buttons, text inputs, sliders, checkboxes, dropdowns, tabs, tooltips, and draggable windows with Catppuccin Mocha theming
- **MTSDF text rendering** — multi-channel signed distance field fonts via msdf-atlas-gen, sharp at any size
- **Async threaded logger** — lock-free ring buffer with a dedicated writer thread, compile-time level stripping in release
- **Developer console** — `~` to toggle a scrollable log viewer with input bar, fed by logger callbacks
- **Persistent TOML config** — window size/position, renderer backend, VSync, FOV, mouse sensitivity, and log level auto-saved with a 5-second dirty-flag debounce
- **Custom function-level profiler** — Clang `-finstrument-functions` with shared-memory broadcast, real-time DearPyGui visualizer, and markdown session reports
- **Custom test harness** — grouped output, colored terminal (Catppuccin), typed equality and float assertions, skip/fatal/quiet modes, test timing, substring filtering, and fail-fast
- **Splash screen** — native platform splash (no GL/Vulkan required) while assets load

## Architecture

```
realm/ (host executable)
  ├── main.c                → bootstraps engine, enters frame loop
  ├── application.c         → orchestrator: module lifecycle, state allocation
  ├── event_handler.c       → hotkey dispatch (F3–F11, ~, M)
  ├── app_console.c         → scrollable log viewer + input bar
  ├── app_renderer.c        → runtime backend switching
  ├── app_debug_panel.c     → debug overlay
  ├── realm_app_loader      → loads/reloads game module DLL/dylib
  ├── realm_app_watcher     → file watcher for rebuild detection
  └── realm_app_module/     → hot-reloadable game module
        ├── scene_main_menu  → start / settings / exit
        ├── scene_game       → camera, geometry, pause menu
        └── menu_settings    → FOV, sensitivity, backend, VSync, log level

engine/ (shared library)
  ├── include/            → stable public API (the only headers realm/ may use)
  └── src/
      ├── renderer/       → backend-agnostic frontend + opengl/ and vulkan/ implementations
      ├── asset/          → static asset registry indexed by enum ID
      ├── core/           → logger, events, camera, config
      ├── memory/         → arena allocator, tracked malloc/free
      ├── gui/            → Clay integration, widget implementations
      ├── profiler/       → function-level profiler, shared memory broadcast
      └── platform/       → win32, macOS, linux abstraction
```

The host owns game state memory. The game module exports `init` / `update` / `render` / `shutdown` and declares its state size and version. On hot reload, state is reused if the layout matches — otherwise it reinitializes cleanly.

The renderer is never touched by game code directly. The game fills an `rl_frame_data` struct (camera, meshes, lights, text) and submits it — the active backend handles the rest.

## Platforms

| Platform | Status |
|----------|--------|
| Windows  | Primary development platform |
| macOS    | Complete |
| Linux    | Complete |

CI runs on all three platforms. The Linux job also builds with AddressSanitizer and UndefinedBehaviorSanitizer.

## Building

**Prerequisites:** CMake 3.20+, Ninja, Clang, Git

```bash
git clone --recursive https://github.com/translibrius/Realm
cd Realm

cmake --preset debug && cmake --build --preset debug
# or
cmake --preset release && cmake --build --preset release
```

Dependencies (FreeType, shaderc, Vulkan headers) are managed via vcpkg manifest mode. If `VCPKG_ROOT` is set, your existing installation is used. Otherwise, vcpkg is fetched automatically into `.vcpkg/` on first configure.

### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `REALM_WERROR` | `OFF` | Treat compiler warnings as errors |
| `REALM_BUILD_TESTS` | `ON` | Build the unit test executable |
| `RL_PROFILE` | `OFF` | Enable function-level profiler (`-finstrument-functions`) |

```bash
# Enable warnings-as-errors (always on in CI)
cmake --preset debug -DREALM_WERROR=ON

# Enable profiling
cmake --preset debug -DRL_PROFILE=ON
```

## Running

```bash
./build/debug/bin/Realm
```

## Keybindings

| Key | Action |
|-----|--------|
| `W` `A` `S` `D` | Move camera |
| `Space` / `LShift` | Move up / down |
| Mouse | Look around |
| `Enter` | Pause / resume |
| `Escape` | Pause (press again to quit) |
| `~` | Toggle developer console |
| `F3` | Launch profiler visualizer (profile build only) |
| `F4` | Snapshot profiler report (profile build only) |
| `F5` | Rebuild and hot-reload game module |
| `F10` | Switch renderer backend (OpenGL / Vulkan) |
| `F11` | Toggle borderless / windowed |
| `M` | Print memory usage to console |

## Tests

```bash
ctest --preset debug

# Run tests, then launch the app if they pass
cmake --build --preset debug --target run_realm_checked

# CLI flags
./build/debug/bin/RealmTests --list              # list all tests by group
./build/debug/bin/RealmTests --filter <pattern>   # run matching tests only
./build/debug/bin/RealmTests --fail-fast          # stop after first failure
./build/debug/bin/RealmTests --quiet              # suppress passing test output
```

## Profiling

Build with profiling enabled, then use the in-engine hotkeys or the report scripts:

```bash
cmake --preset debug -DRL_PROFILE=ON && cmake --build --preset debug
./build/debug/bin/Realm
```

- **F3** — opens the real-time DearPyGui visualizer (flat, call tree, and flame chart views)
- **F4** — writes a markdown snapshot report from live shared memory
- **On exit** — `profiler_session.bin` is written automatically

Generate a report from the session file:

```bash
python3 tools/profiler_report.py --session profiler_session.bin --source-root .
```

## Contact

**Discord:** `friezy`
