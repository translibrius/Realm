# AI Agent Guide: Realm

This repository contains a C23 game/graphics engine ("engine") plus a small host app/sample ("realm"). This file is written for AI coding agents to quickly orient, navigate, and make safe, consistent changes.

## Quick Map

- `engine/`: Engine library (mostly C, plus a small C++ wrapper for MSDF font generation)
- `engine/include/`: Public, stable C API headers (planned/being introduced)
- `realm/`: Executable that links the engine (acts as the current app/game layer)
- `assets/`: Runtime data (`fonts/`, `shaders/`, `textures/`)
- `build/`: CMake/Ninja output (do not hand-edit; often untracked/generated)
- `docs/`: Roadmap and decision log
- Top-level build config: `CMakeLists.txt`, `CMakePresets.json`, `vcpkg.json`, `compile_commands.json`

## Build + Run (Windows-first)

Prereqs (typical):

- CMake + Ninja
- Clang/clang++
- vcpkg (manifest mode) with `VCPKG_ROOT` set
- Vulkan SDK for shaderc (`VULKAN_SDK` is used by `engine/CMakeLists.txt`)

Common commands:

```bash
cmake --preset debug
cmake --build --preset debug

cmake --preset release
cmake --build --preset release
```

Important runtime assumption:

- Asset loading uses relative paths like `../../../assets/...` (see `engine/src/asset/asset.c`), which generally assumes the working directory is the `bin/` folder under `build/*/bin`.

## Program Flow (Entry Points)

- App entry: `realm/src/main.c`
  - Calls `create_application()`.
- App orchestration + main loop: `realm/src/application.c`
  - Boot order: `create_engine()` -> create window -> `renderer_init()` -> `game_init()`
  - Loop: `engine_begin_frame()` -> `game_update()` -> `game_render()` -> `engine_end_frame()`
  - Shutdown: `destroy_engine()`
- Engine boot/shutdown + per-frame glue: `engine/src/engine.c`
  - Starts subsystems: memory, events, logger, platform, input, assets
  - Drives: `platform_pump_messages()`, `renderer_begin_frame()`, `renderer_end_frame()`, swap

## Subsystems and Where to Look

### Rendering

- Frontend interface (backend selection via function pointers):
  - `engine/src/renderer/renderer_frontend.c`
  - `engine/src/renderer/renderer_types.h`
- OpenGL backend:
  - `engine/src/renderer/opengl/`
- Vulkan backend:
  - `engine/src/renderer/vulkan/`
  - Important files often touched: `vk_renderer.*`, `vk_swapchain.*`, `vk_pipeline.*`, `vk_descriptor.*`, `vk_image.*`, `vk_buffer.*`

### Platform + Input

- Platform abstraction: `engine/src/platform/platform.h`
- Implementations:
  - Windows: `engine/src/platform/platform_win32.c`
  - Linux (WIP): `engine/src/platform/platform_linux.c`
- Input system:
  - API/types: `engine/src/platform/input.h`
  - Implementation: `engine/src/platform/input.c`

### Events + Logging

- Event system: `engine/src/core/event.*`
  - Register callbacks: `event_register(EVENT_..., callback, user_data)`
  - Broadcast: `event_fire(EVENT_..., payload_ptr)`
- Logger: `engine/src/core/logger.*`
  - Prefer `RL_INFO/RL_DEBUG/RL_ERROR/RL_FATAL` macros over printf

### Assets

- Asset system: `engine/src/asset/asset.*`
  - Asset registry/list: `engine/src/asset/asset_table.h`
  - Typed loaders: `engine/src/asset/font.*`, `engine/src/asset/shader.*`, `engine/src/asset/texture.*`
- Assets on disk:
  - `assets/fonts/`, `assets/shaders/`, `assets/textures/`

### Memory

- Memory tracking + allocators:
  - `engine/src/memory/memory.*`
  - Arenas: `engine/src/memory/arena.*`
  - Dynamic arrays: `engine/src/memory/containers/dynamic_array.h`

### Math/Utilities

- Core types/macros/platform defines: `engine/src/defines.h`
- Utilities: `engine/src/util/` (`clock.*`, `str.*`, `assert.*`, etc.)
- Math library: `engine/vendor/cglm/`

## CMake Target Layout

- Root: `CMakeLists.txt` adds subdirs `engine/` then `realm/`.
- `engine/CMakeLists.txt` builds:
  - `EngineC` (static) from `engine/src/**/*.c` plus vendored loaders (e.g. glad)
  - `EngineCPP` (static) for `engine/src/core/font/msdf_wrapper.cpp`
  - `Engine` (INTERFACE) that consumers link against
- `realm/CMakeLists.txt` builds executable `Realm` and links `Engine`.

## Do / Don't (High-Signal)

### Do

- Prefer editing code under `engine/src/`, `engine/include/`, and `realm/src/`.
- Keep long-term plans and decisions in `docs/` (see `docs/roadmap.md`, `docs/decisions.md`).
- Keep platform-specific code isolated to `engine/src/platform/`.
- Route cross-system notifications through `engine/src/core/event.*`.
- Use arenas (`rl_arena`) for transient allocations; clear per frame where appropriate.
- Use existing logging macros (`RL_*`) for diagnostics.
- When touching rendering, update both backends if the API contract changes.
- Use `compile_commands.json` for navigation/clangd tooling (it is copied to repo root by CMake).

### Don't

- Don't edit `build/` outputs or vendored third-party code unless absolutely necessary.
- Don't add new hard-coded relative paths without documenting the expected working directory.
- Don't add new engine->app includes. Treat any existing engine/app coupling as technical debt to remove.
- Don't introduce heavy abstraction layers that fight the current style (straight C, explicit structs, function tables).
- Don't silently change global build flags (especially warnings/CRT/vcpkg triplets) unless required.

## Repo-Specific "Gotchas"

- Engine/app coupling: some engine headers include app headers (e.g. `engine/src/engine.h` includes `realm/src/application.h`). This is convenient locally but makes it harder to treat `engine/` as a standalone library. Avoid increasing this coupling; prefer pushing shared types into `engine/` if needed.
- Asset paths are relative: `get_assets_dir()` returns `../../../assets/...` (see `engine/src/asset/asset.c`). Running the executable from a different working directory may fail to load assets.
- Backend selection is centralized: use `renderer_init(..., RENDERER_BACKEND, ...)` and update `prepare_interface()` in `engine/src/renderer/renderer_frontend.c` when adding or changing backend capabilities.
- Windows is primary: Linux exists but is partial; avoid breaking Windows while improving parity.

## Project Direction (Read This Before Big Refactors)

- Roadmap: `docs/roadmap.md`
- Decision log: `docs/decisions.md`

### Public C API Boundary (Target State)

- Public headers live in `engine/include/` and define the stable API surface.
- Apps (including `realm/`) should include only public headers.
- Engine implementation stays in `engine/src/`; private headers are not included by apps.
- Prefer opaque handles in public APIs; keep allocations and ownership inside the engine.
- If a choice materially affects the public API shape, record it in `docs/decisions.md`.

### Renderer Contract (Target State)

- Renderer should consume a mid-level "render packet" (frame data), not game logic.
- App/game code should not deal with pipelines, descriptor sets, swapchains, or backend-specific types.
- Backend switching should not change scene semantics; differences should be limited to feature support/perf.

### Assets (Target State)

- Use a configured asset root and canonical root-relative paths (avoid fragile working-directory assumptions).
- Systems should store asset handles/IDs (not raw file paths).

## Working With AI Agents

- Prefer asking a targeted question over guessing when a choice affects architecture, ABI, assets, or serialization.
- If blocked by ambiguity, do all non-blocked work first, then ask exactly one focused question; include a recommended default and what would change.
- For multi-step changes, keep the current direction aligned with `docs/roadmap.md` and record key decisions in `docs/decisions.md`.
- Avoid "drive-by" refactors; land foundational boundaries first (public API, renderer contract, asset identity).

## Common Change Patterns

- Add a new asset type:
  - Extend `ASSET_TYPE` in `engine/src/asset/asset.h`
  - Add loader implementation in `engine/src/asset/`
  - Add entries to `engine/src/asset/asset_table.h`
  - Update `get_assets_dir()` and `asset_system_load()` switch in `engine/src/asset/asset.c`

- Add a new renderer feature (API surface):
  - Add to `renderer_interface` in `engine/src/renderer/renderer_types.h`
  - Wire in `engine/src/renderer/renderer_frontend.c`
  - Implement in both `engine/src/renderer/opengl/` and `engine/src/renderer/vulkan/`

- Handle a new platform event:
  - Extend `EVENT_TYPE` in `engine/src/core/event.h`
  - Fire from platform layer or subsystem
  - Register handler in `engine/src/engine.c` or in game/app code

## Guidance for AI Changes

When you implement something:

1. Start at the call site (often `realm/src/application.c` or `realm/src/game.c`) and follow into `engine/`.
2. Keep changes localized to one subsystem unless the feature truly spans multiple layers.
3. Prefer small, composable functions and explicit data flow (structs in/out) over global state growth.
4. If a change touches Vulkan, scan for swapchain recreation/resizing paths and ensure you handle window resize.
5. If a change affects per-frame memory, confirm arenas are cleared (frame arenas in `engine/src/engine.c` and `realm/src/game.c`).

## Useful Files to Read First

- `realm/src/application.c` (boot order + loop)
- `engine/src/engine.c` (subsystem bootstrap + per-frame)
- `engine/src/renderer/renderer_frontend.c` (backend dispatch)
- `engine/src/platform/platform.h` (platform API surface)
- `engine/src/asset/asset.c` + `engine/src/asset/asset_table.h` (asset loading)
- `engine/src/defines.h` (types/macros/platform defines)
