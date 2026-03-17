# Architecture Details

## Public API boundary

`engine/include/` is the stable public API. `realm/` must only include from here — no private engine headers. Any `engine/ → realm/` include is a bug.

## Renderer

Dispatched through `renderer_interface` function pointers (`renderer_types.h`). The game submits `rl_frame_data` (camera + meshes + lights + text) to `renderer_submit_frame_data()` — it never touches pipelines or backend types. When changing the renderer API, update both `opengl/` and `vulkan/` backends.

## Assets

Identified by `ASSET_ID` enum only — no string lookups. Registry lives in `engine/src/asset/asset_table.h` as a static array indexed by ID. `asset_root` comes from `rl_engine_config` (default `../../../assets/` relative to `build/*/bin`).

**Adding an asset:** extend `ASSET_ID` in `engine/include/asset/asset.h` → add entry to `asset_table.h` → add loader under `engine/src/asset/` → update switch in `asset_system_load_all()`.

## Hot reload

`realm/include/realm_app_api.h`: host owns game state memory; module exposes `get_state_size` / `get_state_version` / `init` / `update` / `render` / `shutdown`. On reload, state is reused if size+version match, otherwise reset.

## Game module boundary

`realm/realm_app_module/`: only game-specific logic — scene, camera, gameplay, GUI layout. Host-level concerns (input capture, pause/focus, window management) belong in `realm/src/`. Module reads shared state through `realm_app_context`; prefer expanding `realm_app_context` over adding new DLL-exported functions.

## Project templates

`engine/src/core/project_template.c` generates a starter game module when "New Project" runs. The templates mirror `realm/realm_app_module/` patterns. **When changing the game module API or build system, update the templates too** — see [project-templates.md](project-templates.md) for the full sync checklist.

## Memory

Use `mem_alloc(size, MEM_TYPE)` / `mem_free` / `mem_zero` — not `malloc`. Use `rl_arena` for frame-local scratch; cleared at `rl_engine_end_frame()`. For per-frame temp data, use `rl_engine_get_frame_arena()` or `cstr_format(arena, ...)` — never `static char` buffers (hidden globals, hurt reentrancy).

## Events

`event_register(EVENT_..., cb, userdata)` / `event_fire(EVENT_..., &payload)`. All cross-subsystem notifications go here.
