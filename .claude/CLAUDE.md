# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

C23 graphics engine (`engine/`) with a Vulkan and OpenGL 3.3 backend, plus a thin host executable (`realm/`) that hot-reloads a game module at runtime. Windows is the primary development platform; macOS and Linux are complete.

## Build

Requires: CMake 3.20+, Ninja, Clang, vcpkg (`VCPKG_ROOT`), Vulkan SDK (`VULKAN_SDK` for shaderc).

```bash
cmake --preset debug && cmake --build --preset debug
cmake --preset release && cmake --build --preset release
```

## Tests

```bash
ctest --preset debug

# Or run the binary directly from build/debug/
bin/RealmTests --list
bin/RealmTests --filter <pattern>
bin/RealmTests --fail-fast

# Run tests then launch the app
cmake --build --preset debug --target run_realm_checked
```

Adding a test: `tests/cases/test_<name>.c` → implement `register_<name>_tests()` → register in `tests/main.c` → add to `tests/CMakeLists.txt`.

## Boot and frame loop

```
main.c → create_application()
  rl_engine_create()          # memory → events → logger → platform → input → assets
  platform_create_window()
  renderer_init()
  realm_app_module_load/init()
  realm_app_watcher_start()

loop:
  rl_engine_begin_frame()     # input_update, platform_pump_messages, renderer_begin_frame
  app_module.update()
  app_module.render()         # calls renderer_submit_frame_data(rl_frame_data*)
  rl_engine_end_frame()       # renderer_end_frame, swap_buffers, rl_arena_clear
```

F5 → rebuild + reload app module. F7 → switch backend (destroys renderer + window, recreates both, then reloads app module).

## Architecture

**`engine/include/`** is the stable public API. `realm/` must only include from here — no private engine headers. Any `engine/ → realm/` include is a bug.

**Renderer** (`engine/src/renderer/`) is dispatched through `renderer_interface` function pointers (`renderer_types.h`). The game submits `rl_frame_data` (camera + meshes + lights + text) to `renderer_submit_frame_data()` — it never touches pipelines or backend types. When changing the renderer API, update both `opengl/` and `vulkan/` backends.

**Assets** are identified by `ASSET_ID` enum only — no string lookups. Registry lives in `engine/src/asset/asset_table.h` as a static array indexed by ID. `asset_root` comes from `rl_engine_config` (default `../../../assets/` relative to `build/*/bin`). To add an asset: extend `ASSET_ID` in `engine/include/asset/asset.h`, add an entry to `asset_table.h`, add a loader under `engine/src/asset/`, update the switch in `asset_system_load_all()`.

**Hot reload** (`realm/include/realm_app_api.h`): host owns game state memory; module exposes `get_state_size` / `get_state_version` / `init` / `update` / `render` / `shutdown`. On reload, state is reused if size+version match, otherwise reset.

**Game module boundary** (`realm/realm_app_module/`): The hot-reloaded game module should only contain game-specific logic — scene, camera, gameplay, and GUI layout. Host-level concerns (toasts, input capture, pause/focus state, window management) belong in the host (`realm/src/`). The module reads shared state through `realm_app_context`; it should never duplicate state the host already owns. When adding new functionality, prefer expanding `realm_app_context` over adding new DLL-exported functions.

**Memory**: use `mem_alloc(size, MEM_TYPE)` / `mem_free` / `mem_zero` — not `malloc`. Use `rl_arena` for frame-local scratch; the engine clears `frame_arena` at the end of every frame (`rl_engine_end_frame`).

**Events**: `event_register(EVENT_..., cb, userdata)` / `event_fire(EVENT_..., &payload)`. All cross-subsystem notifications go here.

## Conventions

- Types from `engine/include/defines.h`: `u8`, `i32`, `f32`, `b8`, etc. — not `size_t` or bare `int`.
- Logging: `RL_INFO` / `RL_DEBUG` / `RL_ERROR` / `RL_FATAL` — not `printf`.
- Platform-specific code stays in `engine/src/platform/`.
- Style is straight C with explicit structs and function tables. Don't introduce abstraction layers that fight this.
- Don't change global build flags (warnings, CRT, vcpkg triplets) silently.

## Git

- Never add a `Co-Authored-By` line to commit messages.
- Do not write commit message bodies — use a single summary line only.
- Always ask before committing. Never commit automatically — let the user review changes in their IDE first.
- Always ask before pushing. Never push automatically.

## Gotchas

- Vulkan: any change touching swapchain must handle the resize/recreation path.
- Backend switch fully destroys and recreates the window and renderer — it is not a hot swap.
- `asset_root` is config-driven; hardcoding paths elsewhere will break non-default working directories.
