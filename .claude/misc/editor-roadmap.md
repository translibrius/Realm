# Realm Editor — Development Roadmap

## Instructions for agents

When starting a session from this roadmap:

1. **Generate a scoped plan first.** Read the next incomplete phase below, then produce a concrete implementation plan (files to create/modify, API sketches, wiring steps) scoped to what can be done in **~50% of your context window**. Do not attempt to entire roadmap in one session — pick the next logical chunk and plan it thoroughly before writing code. Also feel free to interview the user for any questions or clarifications if needed.
2. **Update this file at the end of a successful session.** Check off completed items, add notes on any deferred work or decisions made, and update the dependency graph if needed. This keeps the roadmap accurate for the next agent.

---

## Context

The editor skeleton is built and running (`realm_editor/`). Phases 1-4 extracted shared logic into `engine/include/host/`, built GUI widgets, added a scene/entity system, and created a project system with picker UI. The next priority is **editor↔game integration**: making the editor actually useful for authoring content that the Realm game executable can load and run. This means scene serialization, dynamic asset discovery, and getting the game host onto the project system — before investing in editor polish features like property editing or viewport gizmos.

### The integration problem

Right now the editor and game are disconnected:
- **Content assets** are a compile-time `content_asset_table[]` — no discovery from disk
- **Scenes** are built in code (game module), not loaded from files — nothing to share
- **Game host** doesn't use the project system — it has its own hardcoded asset root
- **Editor** creates throwaway empty scenes — can't load or save anything real

The fix is a shared project directory that both hosts point at, with scenes as files and assets discovered from disk.

---

## Phase 1: Deduplication — Shared Host Utilities

### Strategy

Add `engine/include/host/` and `engine/src/host/` inside the existing Engine library. Both `Realm` and `RealmEditor` already link Engine — no new CMake targets or link lines needed. The engine's existing `file(GLOB_RECURSE ...)` picks up new `.c` files automatically.

### Step 1: `host_console.h/.c` — shared console core

- [x] Create `engine/include/host/host_console.h` + `engine/src/host/host_console.c`
- [x] Extract shared struct: `host_console` with lines, head, count, visible, scroll, input
- [x] Move into shared: log callback, level→color mapping, key/char handlers, init, shutdown, toggle, on_scroll
- [x] Add `host_console_prepare_lines()` — arena-copies line data for rendering
- [x] Simplify `realm/src/app_console.c` — embed `host_console` + `gui_window_state`, thin render wrapper
- [x] Simplify `realm_editor/src/ed_console.c` — embed `host_console`, thin docked render wrapper
- [x] Verify: both Realm and RealmEditor build and console works identically

### Step 2: `host_renderer.h/.c` — window create + backend switch

- [x] Create `engine/include/host/host_renderer.h` + `engine/src/host/host_renderer.c`
- [x] Extract `host_window_create()` from `app_window_create()`
- [x] Extract `host_renderer_switch_backend()` — destroy/recreate/rollback dance
- [x] Returns `host_switch_result { success, rolled_back, active_backend }` — host reacts to result
- [x] Simplify `realm/src/app_renderer.c` — call shared, then update `app_context` + `reload_requested`
- [x] Simplify `realm_editor/src/ed_application.c` — call shared
- [x] Verify: backend switch works on both executables

### Step 3: `host_events.h/.c` — shared event handlers

- [x] Create `engine/include/host/host_events.h` + `engine/src/host/host_events.c`
- [x] `host_event_ctx` struct: window ptr, focused ptr, console ptr, backend_switch callback
- [x] `host_events_init()` registers: focus, resize, scroll, file_drop, grave/F9/F10/F11
- [x] Simplify `realm/src/event_handler.c` — call `host_events_init()` + register game-only extras (F3/F4/F5/M)
- [x] Simplify `realm_editor/src/ed_event_handler.c` — just call `host_events_init()`
- [x] Verify: all hotkeys work on both executables

### Step 4: `host_bootstrap.h/.c` — engine init sequence

- [x] Create `engine/include/host/host_bootstrap.h` + `engine/src/host/host_bootstrap.c`
- [x] `host_bootstrap()` — engine create → config → window → renderer (GL fallback) → init_gui → config_track
- [x] Returns `host_bootstrap_result { window, success }`
- [x] Simplify `realm/src/application.c` — call `host_bootstrap("Realm")`, then init subsystems
- [x] Simplify `realm_editor/src/ed_application.c` — call `host_bootstrap("Realm Editor")`, then init subsystems
- [x] Verify: both executables start correctly, config saves work

---

## Phase 2: GUI Widgets for Editor

### 2a. Tree View

- [x] Create `engine/include/gui/gui_tree.h` + `engine/src/gui/gui_tree.c`
- [x] Immediate-mode `gui_tree_node_begin/end()` — caller manages nesting, widget handles expand/collapse/select
- [x] Wired into hierarchy panel with dummy scene data (Scene Root, Camera, Objects, Cube, Sphere, Light)

### 2b. Drag-Handle Splitter

- [x] Create `engine/include/gui/gui_splitter.h` + `engine/src/gui/gui_splitter.c`
- [x] `gui_splitter_v` / `gui_splitter_h` — draggable bar with hover highlight, min/max clamping, invert flag
- [x] Wired into `ed_layout` between hierarchy/viewport/properties panels and above console

### 2c. Context Menu (moved to Phase 11e)

- [x] Created in Phase 11e with full editor wiring (hierarchy + viewport context menus)

### 2d. Wire up menu bar

- [x] Added `label` field to `gui_dropdown_cfg` for menu-bar-style trigger text
- [x] File > Quit (`rl_engine_stop()`), Edit > Undo/Redo (placeholders), View > Toggle Console
- [x] Fire-and-forget pattern: `selected` reset to `-1` after handling action

---

## Phase 3: Scene/Entity System (critical path)

### 3a. Entity store

- [x] Create `engine/include/core/entity.h` + `engine/src/core/entity.c`
- [x] Entity handles: `u32` with 20-bit index + 12-bit generation
- [x] Create/destroy with generation bump for dangling-handle safety

### 3b. Component storage

- [x] Create `engine/include/core/component.h` + `engine/src/core/component.c`
- [x] Parallel arrays keyed by entity index
- [x] Initial types: `rl_transform`, `rl_mesh_component`, `rl_light_component`, `rl_name_component`

### 3c. Scene container

- [x] Create `engine/include/core/scene.h` + `engine/src/core/scene.c`
- [x] Named container owning entity store (4 MiB arena)
- [x] `scene_build_frame_data()` — walks entities with transform+mesh, fills `rl_frame_data`

### 3d. Wire editor to scene

- [x] Editor creates default scene on startup (Cube + Light)
- [x] Hierarchy panel shows real entities via tree view
- [x] Properties panel shows component data for selected entity
- [x] Viewport submits `scene_build_frame_data()` each frame
- [x] 9 unit tests in `tests/cases/test_entity.c` — all passing

---

## Phase 4: Project System & Config Split

The editor needs to work with projects — each project is a directory with its own scenes, assets, and settings. This phase also splits config so editor and game have independent settings, and decouples asset loading so engine assets load at bootstrap while project assets load on demand.

### 4a. Config split — DONE

- [x] Added `config_filename` field to `rl_engine_config`, propagated through `rl_engine_create()`
- [x] Renamed `RL_CONFIG_FILENAME` → `RL_CONFIG_FILENAME_DEFAULT`, parameterized `config_system_start(void *memory, const char *filename)`
- [x] Stored filename in `config_state`, used in `config_load`/`config_save`
- [x] Added `config_filename` param to `host_bootstrap()`
- [x] Game host passes `"config.toml"`, editor passes `"editor.toml"`
- [x] Updated `test_config.c` for new API — all tests pass
- [x] Verify: editor and game each persist their own settings independently

### 4b. Asset system split — DONE

- [x] Split `asset_table[]` into `engine_asset_table[]` (1 font + 14 shaders) and `content_asset_table[]` (3 textures + 1 mesh)
- [x] Replaced `asset_system_load_all()` with `asset_system_load_engine()` (with splash) + `asset_system_load_content()` (no splash)
- [x] Added `asset_system_clear_content()` — truncates asset array back to engine-only
- [x] Added `asset_set_content_root(path)` / `asset_clear_content_root()` — override resolve root for TEXTURE/MESH types
- [x] Added `asset_get_resolve_root(ASSET_TYPE)` — used by `texture.c`, `mesh.c`, and `asset_compute_source_hash`
- [x] GL and Vulkan renderers gracefully handle missing content textures at init
- [x] Vulkan descriptor sets skip image descriptor write when `texture_count == 0`
- [x] Game host calls `asset_system_load_content()` after bootstrap
- [x] Verify: editor starts with only engine assets loaded, game host still works as before

### 4c. Project file & directory structure — DONE

- [x] Added `platform_dir_create()` on all 3 platforms (macOS, Linux, Win32) in `file_io.h`
- [x] Created `engine/include/core/project.h` + `engine/src/core/project.c`
- [x] Project directory layout:
  ```
  my_project/
  ├── project.realm          (TOML — project name, engine version, default scene)
  ├── scenes/
  └── assets/
      ├── models/
      ├── textures/
      └── materials/
  ```
- [x] `rl_project` struct: name, root_path, asset_path, scenes_path, default_scene, open flag
- [x] `project_create(path, name)` — create directory structure + write `project.realm`
- [x] `project_open(path)` — parse `project.realm`, set content root via `asset_set_content_root()`
- [x] `project_close()` — clear content assets + content root, zero state
- [x] `project_is_open()` / `project_get()` — trivial accessors
- [x] 4 unit tests in `tests/cases/test_project.c` — all passing
- [x] Verify: can create and open a project from code

### 4d. Editor bootstrap refactor — DONE

- [x] Editor skips splash screen — `b8 skip_splash` added to `rl_engine_config`, propagated through `host_bootstrap()`
- [x] After bootstrap, if no project open → show project picker; if project open → show editor layout
- [x] Editor stores last-opened project path + recent projects list in `editor_state.toml` (separate `ed_config` struct)
- [x] `ED_MODE` enum (`ED_MODE_PICKER`, `ED_MODE_EDITOR`) drives frame loop branching
- [x] Verify: editor boots to picker, game host unchanged (splash still works)

### 4e. Project picker screen — DONE

- [x] Created `realm_editor/src/ed_project_picker.h/.c`
- [x] Landing screen with: "New Project" button, "Open Project" button, recent projects list
- [x] "New Project" flow: path + name text inputs → `project_create()` → `project_open()`
- [x] "Open Project" flow: path text input → `project_open()`
- [x] Recent projects loaded from `editor_state.toml` via `ed_config`
- [x] On project selected → transition to full editor layout with default scene (Light entity only)
- [x] Tab cycles focus between inputs, Escape returns to home view, Enter submits
- [x] File menu: New Project / Open Project / Close Project → returns to picker
- [x] Failed recent project opens remove entry from list
- [x] Native file dialog deferred to future (uses `gui_text_input` for now)

---

## Phase 5: Scene I/O (critical path — enables editor↔game)

Scenes are currently built in code. Both editor and game need a shared file format so the editor can author scenes that the game loads at runtime. This is the single biggest blocker to making the editor useful.

### 5a. Scene serialization — DONE

- [x] Create `engine/include/core/scene_io.h` + `engine/src/core/scene_io.c`
- [x] JSON format via yyjson (vendored, now compiled into Engine) — one `.scene` file per scene
- [x] `scene_save(scene, path)` — serialize all entities + components to JSON (pretty-print 2-space)
- [x] `scene_load(path)` → `rl_scene *` — deserialize from JSON, create entities + components
- [x] Format: `{ "name": "...", "entities": [ { "name": "Cube", "transform": {...}, "mesh": {...}, "light": {...} } ] }`
- [x] Component serializers: transform (position/rotation/scale), mesh (primitive, kind, wireframe), light (ambient/diffuse/specular)
- [x] 6 unit tests in `tests/cases/test_scene_io.c` — all passing
- [x] Added `vendor/yyjson/*.c` to engine CMake GLOB so yyjson compiles into the shared library

### 5b. Wire scene I/O into editor — DONE

- [x] File menu: New Scene, Save Scene (Open Scene deferred — needs file browser)
- [x] Editor tracks current scene file path (`scene_path[512]`, empty = unsaved)
- [x] New project creates `scenes/default.scene` with a Light entity on disk
- [x] Opening a project loads its `default_scene` from `project.realm`
- [x] Dirty state flag (`scene_dirty`) + asterisk indicator in menu bar title
- [x] Auto-save on: close project, quit, new scene (if dirty + path set)
- [x] `ed_save_scene`, `ed_load_scene`, `ed_new_scene` helpers in `ed_application.c`

### 5c. Wire scene loading into game host — DONE

- [x] Game host (`realm/src/application.c`) gains `project_open()` call after bootstrap
- [x] Game module can call `scene_load()` to load editor-authored scenes
- [x] Expand `realm_app_context` with project pointer so module knows project paths
- [x] Proof of life: game loads a `.scene` file saved by the editor and renders it

---

## Phase 6: Dynamic Asset Discovery — DONE

Content assets are currently a compile-time `content_asset_table[]`. For a shared project to work, both hosts need to discover assets from the project directory at runtime.

### 6a. Directory scanning — DONE

- [x] Created `engine/include/platform/io/file_scan.h` — `platform_dir_scan()` API with `DirEntries` DA type and `platform_dir_entry` struct
- [x] `engine/src/platform/io/file_scan_macos.c` — `opendir`/`readdir`/`closedir` with `d_type` + `stat` fallback
- [x] `engine/src/platform/io/file_scan_linux.c` — same POSIX calls
- [x] `engine/src/platform/io/file_scan_win32.c` — `FindFirstFileA`/`FindNextFileA`/`FindClose`
- [x] Extension filter: comma-separated (e.g. `".jpg,.png"`), parsed into local array, matched with `cstr_ends_with()`
- [x] Skips hidden files (`.` prefix) and `.`/`..` entries
- [x] 6 unit tests in `tests/cases/test_file_scan.c` — all passing (138 total)

### 6b. Project asset loading — DONE

- [x] Created `engine/include/core/project_assets.h` + `engine/src/core/project_assets.c`
- [x] `project_load_assets()` scans `textures/` and `models/` subdirs under project asset path
- [x] Handles nested model folders (e.g. `models/lion_head_4k.gltf/lion_head_4k.gltf` pattern)
- [x] Logs summary: "Loaded N textures, M meshes from project"
- [x] Fixed `asset_load()` source_path lifetime — now copies to `asset_arena` via `cstr_format()` so dynamic paths don't dangle

### 6c. Wired into both hosts — DONE

- [x] Editor: `project_load_assets()` called in `ed_enter_editor_mode()` after project open
- [x] Game host: `project_load_assets()` called after `project_open()`, falls back to `asset_system_load_content()` if no project
- [x] `content_asset_table[]` remains as demo/fallback — only loaded when no project is open
- [x] Verify: game works standalone with hardcoded assets and with project

### 6d. Asset browser panel — DONE

- [x] Created `realm_editor/src/ed_asset_browser.h/.c` — tree-based browser with "Textures" and "Models" top-level nodes
- [x] Node IDs use `ED_ASSET_NODE_BASE = 0x20000u` to avoid collision with entity node IDs
- [x] Click on a mesh leaf node → create entity in scene with that mesh asset
- [x] `ed_asset_browser_refresh()` scans project dirs, caches results with heap-copied names
- [x] Left panel split: hierarchy (top) + asset browser (bottom) with horizontal splitter (`splitter_left_h`)
- [x] Added `on_file_drop` callback to `host_event_ctx` — dispatched before logging in `host_events.c`
- [x] Editor file drop handler: detects type by extension, copies to project `assets/` subdir, calls `asset_load()`, sets `needs_refresh`
- [x] Asset browser field added to `ed_application`, lifecycle wired (init/refresh/shutdown)

---

## Phase 7: Game Host Integration

Make the Realm game executable a first-class consumer of editor-authored projects. The goal: edit a scene in the editor, hit play in the game, see the same thing.

### 7a. Game host project support — DONE

- [x] Game host opens project via CLI arg: `Realm --project /path/to/my_project`
- [x] Project is now required — prints usage and exits if no `--project` arg
- [x] On project open: discovers + loads project assets via `project_load_assets()`
- [x] `realm_app_context` exposes `rl_project *project` to game module
- [x] Host loads default scene from project and owns scene lifetime
- [x] `realm_app_context` exposes `rl_scene *scene` so module uses editor-authored scene data
- [x] Removed `asset_system_load_content()` and `content_asset_table[]` — no legacy fallback
- [x] Scene I/O now serializes/deserializes `mesh_asset_path` and `material` fields
- [x] GL renderer lazily uploads textures on first use (no hardcoded pre-load)
- [x] VK renderer removed hardcoded wood texture pre-load
- [x] Added `scene_entity_find(scene, name)` convenience helper

### 7b. Migrate existing game content to a project — DONE

- [x] Created `game/` project directory with `project.realm`
- [x] Copied textures and models from `assets/` into `game/assets/`
- [x] Created `game/scenes/gameplay.scene` with 5 entities (RotatingCube, GroundPlane, LionHead, PointLight, LightCube)
- [x] Game module loads scenes from files, no more hardcoded `scene_game_render_legacy()`
- [x] `scene_game.c` uses `ctx->scene` and `scene_build_frame_data()` exclusively
- [x] Rotating cube animation via `scene_entity_find` + `transform_get` + rotation update
- [x] Deleted old `assets/textures/` and `assets/models/` — `assets/` keeps only `fonts/` and `shaders/`
- [x] Bumped `RL_GAME_STATE_VERSION` to 10

### 7c. Shared workflow validation

- [x] End-to-end: create project in editor → add entities → save scene → launch game with same project → scene renders
- [x] Editor and game read the same `project.realm`, same `assets/`, same `scenes/`
- [x] Hot-reload still works: game module reloads, scene persists from host-owned lifetime
- [x] No separate repos needed — single repo, shared `game/` project directory

---

## Phase 8: Editable Properties + Undo

With scene I/O in place, property editing becomes meaningful — changes can be saved and loaded.

### 8a. Property inspector — DONE

- [x] Created `realm_editor/src/ed_inspector.h/.c`
- [x] `ed_inspector` struct with per-component widget state (vec3 groups for transform/light, dropdown for mesh kind, checkbox for wireframe, number inputs for material)
- [x] `ed_inspector_init` registers `EVENT_KEY_PRESS` / `EVENT_CHAR_INPUT` handlers for keyboard routing
- [x] `ed_inspector_bind` syncs widget state from component data on entity selection change
- [x] `ed_inspector_render` lays out all component sections with `gui_field` rows; writes back changes immediately to components; sets `scene_dirty` and `tr->dirty`
- [x] Focus tracking: `focused_input` pointer routes keyboard events to the active number input; console still takes priority when visible
- [x] Transform: Position/Rotation/Scale vec3 rows with colored X/Y/Z labels
- [x] Mesh: Kind dropdown (Lit/Unlit), wireframe checkbox
- [x] Material: Specular vec3 (0–1 range), shininess float (0–256)
- [x] Light: Ambient/Diffuse/Specular vec3 rows (0–1 range)
- [x] Wired into `ed_layout`: inspector field in struct, init in `ed_layout_init`, properties panel calls bind/render

### 8b. Number input widget — DONE

- [x] Created `engine/include/gui/gui_number_input.h` + `engine/src/gui/gui_number_input.c`
- [x] `gui_number_input_state` with value, editing/dragging flags, text buffer, cursor, auto-generated Clay ID
- [x] `gui_number_input_cfg` with min/max/step/format/width/height (sensible defaults, both-zero min/max = no limit)
- [x] Drag-to-scrub: click starts tracking, >2px horizontal movement becomes drag (value += delta_x × step, clamped); <2px release enters text editing mode
- [x] Text editing: blinking caret, digits/dot/minus only, Enter confirms (parsed via `strtod`), Escape cancels (reverts to drag_start_value)
- [x] `gui_number_input_handle_key` / `gui_number_input_handle_char` for event-driven input routing
- [x] Added `#include "gui/gui_number_input.h"` to `gui_widgets.h`

### 8c. Undo/redo system — DONE

- [x] Created `realm_editor/src/ed_undo.h/.c` (editor-only, not in engine)
- [x] Memcpy-snapshot design: `ed_undo_entry` stores `{entity, action_type, before_data, after_data}` as component-typed union
- [x] Fixed-size ring buffer (256 entries) with head/count/redo_count arithmetic
- [x] `ed_undo_push`, `ed_undo_perform` (Ctrl+Z), `ed_undo_redo` (Ctrl+Shift+Z), `ed_undo_clear`
- [x] Action types: `ED_UNDO_TRANSFORM`, `ED_UNDO_MESH`, `ED_UNDO_LIGHT`, `ED_UNDO_NAME`, `ED_UNDO_CREATE_ENTITY`, `ED_UNDO_DESTROY_ENTITY`
- [x] Drag coalescing in inspector: tracks `was_any_dragging` → one undo entry per drag operation (not per frame)
- [x] Discrete undo for checkbox toggle, dropdown select, name confirm — immediate push inline
- [x] Ctrl+Z / Ctrl+Shift+Z hotkeys in `ed_event_handler.c`, guard against inspector text editing
- [x] Edit menu Undo/Redo items wired via `undo_requested`/`redo_requested` flags on `ed_application`
- [x] Inspector rebind after undo/redo to resync widget state
- [x] Undo stack clears on scene load/new scene
- [x] Entity create/destroy action types defined but recreation logic not yet wired (no UI for entity delete yet)

---

## Phase 9: Viewport Interaction

### 9a. Editor camera — DONE

- [x] Created `realm_editor/src/ed_camera.h/.c`
- [x] `ed_camera` struct wraps `rl_camera` with orbit target, distance, fly/orbit/viewport_hovered flags
- [x] Orbit mode (middle-mouse drag): rotates yaw/pitch around target, derives position from `target - forward * distance`
- [x] Fly mode (right-click hold): WASD + Space/Shift movement, mouse look with raw input + cursor lock, orbit center tracks fly position
- [x] Scroll zoom: proportional `distance -= z_delta * distance * 0.1f`, clamped [0.1, 500]
- [x] F-key frame selection: snaps orbit center to selected entity transform position, distance = 5
- [x] Viewport bounds tracking: viewport panel uses `CLAY_ID("EditorViewport")`, bounds queried via `Clay_GetElementData` after layout
- [x] `viewport_hovered` check: mouse position vs Clay bounding box, used to gate scroll/orbit/fly entry
- [x] Scroll event handler registered before host events (FIFO), consumes when over viewport so console scroll still works
- [x] Key handler guards against inspector text editing before processing F/Ctrl+Z/Ctrl+Shift+Z

### 9b. Viewport-constrained 3D rendering — DONE

The viewport Clay panel was drawing an opaque background over the 3D scene, and the camera used full-window aspect ratio. This phase made the 3D scene actually visible inside the editor viewport.

- [x] Added `rl_viewport_rect` struct to `engine/include/renderer/frame_data.h` — `{x, y, w, h}` in pixels, all-zero = full window (no breakage for game host)
- [x] OpenGL: `glViewport` + `glScissor` constrain 3D passes to viewport rect, clear depth+color within scissor, restore full-window viewport before GUI overlay
- [x] Vulkan: `VkViewport`/`VkRect2D` constrain 3D passes in `vk_commands.c`, restore full-swapchain viewport before `vulkan_gui_record_commands`
- [x] Viewport panel background changed to transparent `{0,0,0,0}` — 3D scene shows through
- [x] Root GUI panel also made transparent — was `t->bg` (alpha 230) covering the white cube
- [x] Camera aspect ratio uses `viewport_bounds.width/height` instead of full window dimensions
- [x] `frame.viewport_rect` set from `app.layout.viewport_bounds` before `renderer_submit_frame_data()`

### 9c. Viewport tab bar — DONE

Replaced centered "Viewport" text with a proper tab strip, matching the VS Code / Unity panel tab pattern.

- [x] Tab strip: `bg_input` background, 1px bottom border in `border` color
- [x] Active tab: `bg_secondary` background, 2px accent-colored top line
- [x] Uses raw Clay API (`Clay__OpenElement`/`Clay__ConfigureOpenElement`/`Clay__CloseElement`) for per-side borders
- [x] `EditorViewport` Clay ID remains on the 3D area below the tab bar (not the tab strip), so camera bounds are correct
- [x] Structure supports adding more tabs later (e.g. Settings, Game View) — inactive tabs would use strip background + dim text

### 9d. Origin axis gizmo — DONE

Colored XYZ axis indicator rendered in a 100x100 px sub-viewport in the bottom-left corner. Uses the overlay render pass infrastructure (separate camera, no depth test).

- [x] Added `flat_color` uniform to GL unlit shader, push constant color to VK unlit shader
- [x] Extended `rl_frame_data` with `overlay_camera`, `overlay_meshes`, `overlay_count`
- [x] GL overlay pass: gizmo sub-viewport, `glDisable(GL_DEPTH_TEST)`, draws with light shader + flat_color
- [x] VK overlay pipeline: `vk_overlay_pipeline_create/destroy` (depth_test=false, depth_write=false)
- [x] VK overlay pass in `vk_commands.c`: gizmo sub-viewport with negative-height Y-flip, temporary UBO swap
- [x] Created `realm_editor/src/ed_gizmo.h/.c` — builds 6 overlay meshes (3 shafts + 3 tips, RGB = XYZ)
- [x] Rotation-only view matrix (zeroed translation column) + small ortho projection
- [x] Light viz cubes now set `material.specular = {1,1,1}` so they remain white with flat_color shader
- [x] Wired into `ed_application.c` frame loop before `renderer_submit_frame_data`

### 9e. Transform gizmos — DONE

- [x] Added `world_overlays` / `world_overlay_count` to `rl_frame_data` — world-space overlays rendered with main camera, depth test off
- [x] GL + VK backends: world overlay render pass inserted before axis gizmo overlay pass
- [x] Created `realm_editor/src/ed_gizmo_transform.h/.c` — `ed_gizmo_transform` struct with mode/drag state
- [x] **Translate mode (W)**: 7 overlay meshes (3 shafts + 3 tips + center cube), AABB picking, axis-constrained drag via ray-plane projection
- [x] **Rotate mode (E)**: 73 overlay meshes (3 rings × 24 segments + center), analytical ring picking (ray-plane intersection + distance check), atan2 angle drag with [-PI, PI] wrapping, delta applied as degrees to `rotation[axis]`
- [x] **Scale mode (R)**: Same shaft/tip layout as translate but larger yellow center cube (0.15), reuses AABB picking, additive scale drag with 0.01 min clamp
- [x] All `_build`/`_pick`/`_drag_begin`/`_drag_update` dispatch via `switch (g->mode)` to static helpers
- [x] Undo change detection checks position + rotation + scale (was position-only)
- [x] W/E/R hotkeys switch gizmo mode; gizmo pick priority over entity picking
- [x] Drag end pushes `ED_UNDO_TRANSFORM` entry; Escape cancels drag and restores original transform
- [x] Inspector rebound after drag end to resync widget state
- [x] Fixed macOS ⌘+Z/⌘+Shift+Z: `ctrl` check now includes `KEY_L_SUPER`/`KEY_R_SUPER`

### 9f. Entity picking — DONE

CPU ray casting against entity AABBs. Click in viewport to select/deselect entities.

- [x] Created `engine/include/math/ray.h` + `engine/src/math/ray.c` — `rl_ray`, `rl_aabb`, `ray_from_screen`, `ray_intersect_aabb`, `aabb_from_unit_cube`
- [x] Screen→NDC (Y-flip) → unproject through inv_proj + inv_view → world ray
- [x] Slab method intersection, 8-corner AABB from unit cube × model matrix
- [x] Created `realm_editor/src/ed_picking.h/.c` — iterates alive entities with transforms, tests mesh + light-only entities
- [x] `EVENT_MOUSE_CLICK` handler in `ed_event_handler.c`: left button, pressed, viewport hovered, not flying/orbiting
- [x] Hit → set `hierarchy_tree.selected_id` + `ed_inspector_bind`; miss → deselect
- [x] 8 unit tests in `tests/cases/test_ray.c` — slab hit/miss/behind/inside, AABB identity/translated/scaled, screen-center ray direction

### 9g. Infinite grid — DONE

- [x] Fullscreen quad technique: 6 verts from `gl_VertexID`/`gl_VertexIndex`, fragment shader ray-casts onto Y=0 plane
- [x] Procedural grid at two scales (minor 0.1, major 1.0) with `fwidth()` anti-aliasing
- [x] Red X-axis line (Z=0), blue Z-axis line (X=0)
- [x] Quadratic distance fade (50–100 unit radius)
- [x] Writes `gl_FragDepth` for proper object occlusion
- [x] Both GL (330 core) and VK (450) shader pairs: `assets/shaders/{opengl,vulkan}/grid.{vert,frag}`
- [x] GL: `grid_shader` + empty `grid_vao` in `GL_Context`, drawn before mesh passes with blending
- [x] VK: `grid_pipeline` (no vertex input, depth+blend, cull none), drawn before mesh passes
- [x] `show_grid` flag on `rl_frame_data`, wired from `ed_application.show_grid` (default on)
- [x] View menu "Toggle Grid" item, G hotkey (guarded by `!fly_mode`)

---

## Phase 10: Editor Polish & Theme System — DONE

### 10a. Theme compliance — DONE

All GUI widgets now read colors from `gui_theme_get()` at render time. Swapping themes via `gui_theme_set()` instantly updates all UI colors.

- [x] `gui_text.c`: white fallback `{255,255,255,255}` → `t->text`
- [x] `gui_field.c`: label color `{180,180,185,255}` → `t->text_dim`
- [x] `gui_text_input.c`: bg/text/border hardcoded colors → `t->bg_input`, `t->text`, `t->border`
- [x] `gui_panel.c` (separator): `{60,60,65,255}` → `t->separator`
- [x] `ed_project_picker.c`: `{40,40,45,255}` → `t->control_press`
- [x] `ed_layout.c` (tab strip): verbose `(Clay_Color){t->...r, ...g, ...b, 255}` → direct `t->bg_input`, `t->bg_secondary`
- [x] Audited all `realm_editor/src/` and `engine/src/gui/` — remaining `{0,0,0,0}` literals are intentional "transparent"

### 10b. Light entity visualization — DONE

- [x] `scene_build_frame_data()` now emits a small (0.15x scale) unlit cube for light entities without explicit mesh components
- [x] Drawn with the unlit pipeline (`RL_FRAME_MESH_KIND_UNLIT`) — pure white `vec4(1.0)` in both GL and VK shaders
- [x] Count pass accounts for visualization cubes so arena allocation is correct
- [x] Light defaults brightened: ambient `0.2→0.3`, diffuse `0.5→0.9` (new lights only; saved scenes keep their values)
- [x] `RL_DEFAULT_POINT_LIGHT` constant updated to match

### 10c. Asset browser double-click fix — DONE

- [x] Removed `!is_dir` guard — directory-based models (e.g. `lion_head_4k.gltf/`) are now clickable
- [x] Directory entries construct nested path `"models/<dirname>/<dirname>"` matching how `project_load_assets` registers them
- [x] Single-click selects, double-click creates entity (0.4s threshold, matching `gui_file_browser` pattern)
- [x] Added `time_acc`, `last_click_time`, `last_click_node_id` to `ed_asset_browser` struct
- [x] `ed_asset_browser_render` signature updated to take `f32 dt`

### 10d. Project default_scene fallback — DONE

- [x] `project_open()` now falls back to `"scenes/default.scene"` when `default_scene` key is missing from project file
- [x] Fixes `scene_save` error when opening older projects that predate the `default_scene` field

### 10e. Theme dropdown & editor settings tab — DONE

- [x] Added `theme[32]` field to `ed_config`, parsed/saved in `editor_state.toml`, defaults to `"dark"`
- [x] Added `viewport_tab` (i32) and `theme_dropdown` to `ed_layout`
- [x] Created `realm_editor/src/ed_settings.h/.c` — settings panel with theme dropdown ("Dark" / "Catppuccin")
- [x] `ed_settings_apply_theme(key)` maps string → theme pointer → `gui_theme_set()`; `ed_settings_theme_index(key)` for dropdown sync
- [x] Replaced hardcoded viewport tab with `gui_tabs()` widget: ["Viewport", "Settings"] — tab 0 = 3D viewport, tab 1 = settings
- [x] Theme applied on startup after `ed_config_load`; dropdown synced after `ed_layout_init`
- [x] Camera update skipped when settings tab is active (`viewport_tab != 0`)

---

## Phase 11: Editor GUI Polish — DONE

Visual refinements and missing interactive elements to make the editor feel like a real tool.

### 11a. FPS overlay — DONE

- [x] Semi-transparent floating panel in viewport top-right (`{0,0,0,140}` bg, corner radius 4)
- [x] Shows `FPS` and `ms` from `rl_engine_get_stats()` using `gui_textf` in `debug_highlight` color
- [x] Gated by `ed_cfg.show_fps` (default true), only when viewport tab active
- [x] Persisted via `ed_config` (`show_fps` field in `editor_state.toml`)

### 11b. Tree arrow triangles — DONE

- [x] Created `engine/include/gui/gui_icon.h` + `engine/src/gui/gui_icon.c`
- [x] `gui_icon()` emits Clay custom element with `type` in `customData`, color in `backgroundColor`
- [x] `push_tri()` helper in both `gl_gui.c` and `vk_gui.c` — 3 vertices, UV sentinel, conservative clip
- [x] `CLAY_RENDER_COMMAND_TYPE_CUSTOM` case renders `TRIANGLE_RIGHT` and `TRIANGLE_DOWN` with 2px inset
- [x] `gui_tree.c`: replaced `gui_text("v"/">"...)` with `gui_icon(TRIANGLE_DOWN/RIGHT, ...)`
- [x] Bumped `row_height` default 22→24, `indent` default 16→20

### 11c. Tree visual polish — DONE

- [x] Alternating row tint: odd rows get `{255,255,255,6}` background
- [x] Rounded selection highlight: `cornerRadius = 4` on selected row
- [x] Leaf dimming: leaf nodes use `arrow_color` (text_dim) for label text

### 11d. Viewport toolbar — DONE

- [x] Created `realm_editor/src/ed_toolbar.h/.c`
- [x] 28px horizontal strip between tab bar and viewport
- [x] Gizmo mode buttons (W/E/R): active mode = accent bg, others = control bg
- [x] Grid toggle button: accent bg when `show_grid` active
- [x] Play placeholder button (non-functional)
- [x] Vertical dividers (1px × 16px separator color)
- [x] Tooltips on all buttons via wrapped named elements
- [x] Button clicks set `gizmo.mode` or toggle `show_grid`

### 11e. Context menu widget — DONE

- [x] Created `engine/include/gui/gui_context_menu.h` + `engine/src/gui/gui_context_menu.c`
- [x] `gui_context_menu_open()` captures mouse position, `gui_context_menu()` renders floating panel
- [x] `CLAY_ATTACH_TO_ROOT` with absolute offset, `zIndex = 250`, pointer capture when open
- [x] Auto-closes on selection or click-outside (left or right click)
- [x] Added to `gui_widgets.h` umbrella include

#### Hierarchy context menu
- [x] Right-click detection on "HierarchyPanel" named element
- [x] Items: "Add Empty Entity", "Add Light", "Duplicate" (when selected), "Delete" (when selected)
- [x] Handles: `scene_entity_create`, `transform_add`, `light_add`, `scene_entity_destroy`
- [x] Duplicate copies name and transform from source entity

#### Viewport context menu
- [x] Right-click in viewport when not in fly mode
- [x] Items: "Add Cube", "Add Light", "Frame Selection", "Reset Camera"
- [x] Add Cube: creates entity with transform + lit mesh + default material
- [x] Frame Selection: calls `ed_camera_frame_selection` on selected entity
- [x] Reset Camera: calls `ed_camera_init`

### 11f. Expanded settings — DONE

- [x] Added `show_fps`, `camera_speed`, `camera_sensitivity`, `camera_fov` to `ed_config`
- [x] Parsed/saved in `editor_state.toml` with defaults (true, 5.0, 0.3, 60.0)
- [x] Settings panel: Display section (Show FPS checkbox), Camera section (Speed/Sensitivity/FOV number inputs), Theme section (dropdown)
- [x] Section headers with `bg_secondary` background, corner radius 3
- [x] Camera reads config values each frame (`move_speed`, `look_speed`, `fov`)
- [x] Auto-saves config on any setting change

### 11g. Inspector section headers — DONE

- [x] Transform, Mesh, Material, Light labels wrapped in `bg_secondary` panel with 4px padding and corner radius 3
- [x] `section_header()` helper function in `ed_inspector.c`

---

## Phase 12: Renderer Visual Polish — DONE

### 12a. MSDF text weight — DONE

- [x] Added `u_weight` uniform (GL) / push constant float (VK) to GUI shaders
- [x] Formula: `sd - (0.5 - weight)` with weight = 0.12 — gives subtle "medium" weight
- [x] Fixed min `screen_px_range` floor to `1/(1 - 2*weight)` — prevents alpha leakage at glyph quad edges
- [x] Files: `gui.frag` (both backends), `gl_gui.c` (loc_weight), `gl_types.h`, `vk_gui.c` (push constant struct)

### 12b. SDF rounded corners — DONE

- [x] New `GL_GuiVertex` / `VK_GuiVertex` (48 bytes) with `vec4 rect_info` (half_w, half_h, corner_radius, 0)
- [x] Fragment shader SDF branch: `length(max(abs(uv) - half_size + radius, 0)) - radius` with `smoothstep` AA
- [x] `push_rect` reads `cornerRadius.topLeft` from Clay `RectangleRenderData`
- [x] Three-way shader dispatch: `rect_info.z > 0` → rounded SDF, `uv.x < 0` → plain rect, else → MSDF text
- [x] VK push constant range expanded from 12 → 16 bytes (added weight float)
- [x] `vk_gui.h` return type updated to `VK_GuiVertex *`, `vk_text.c` updated accordingly
- [x] Borders remain sharp (rounded border SDF deferred)

### 12c. Menu bar divider — DONE

- [x] Added `gui_separator()` call after `ed_layout_menu_bar()` in `ed_layout.c`

---

## Phase 13: GUI Focus System & Interaction Fixes — DONE

### 13a. Global GUI focus system — DONE

- [x] Created `engine/include/gui/gui_focus.h` + `engine/src/gui/gui_focus.c`
- [x] Single active widget ID — `gui_focus_set/clear/get/is`
- [x] `gui_focus_begin_frame()` clears focus on any mouse click (left or right), widgets re-claim during render
- [x] Hooked into `gui_layout_begin()` in `gui.c`

### 13b. Text input focus-aware blink — DONE

- [x] Added `_id` field to `gui_text_input_state` (auto-assigned via `gui__next_id`)
- [x] `gui_text_input_display()` only blinks caret when `gui_focus_is(_id)` — no more phantom cursors
- [x] `gui_text_input_render()` claims focus on click, shows accent border when focused

### 13c. Number input focus integration — DONE

- [x] `gui_number_input()` detects focus loss (`editing && !gui_focus_is`) → auto-confirms edit
- [x] Entering editing mode calls `gui_focus_set(id)` — previous widget loses focus
- [x] Blink timer only advances when focused — no fast blinking at high FPS
- [x] Escape clears global focus via `gui_focus_clear()`

### 13d. Console focus gating — DONE

- [x] `host_console_on_key/on_char` check `gui_focus_is(c->input._id)` instead of `c->visible`
- [x] Console toggle sets/clears focus — open = focused, close = unfocused
- [x] Right-click on viewport clears console focus → WASD works while console is visible
- [x] Console input `_id` assigned in `host_console_init()`

### 13e. Settings number input keyboard support — DONE

- [x] Moved settings number input state to file scope in `ed_settings.c`
- [x] Added `settings_on_key/on_char` event handlers — route to whichever input is editing
- [x] `ed_settings_init()` registers handlers, called from `ed_application.c`
- [x] Fixed hardcoded `0.016f` dt → real frame dt passed through `ed_settings_render(layout, cfg, dt)`

### 13f. Scrollbar drag interaction — DONE

- [x] Added `_dragging`, `_drag_start_y`, `_drag_start_scroll` to `gui_scroll_state`
- [x] Click on scrollbar track: jumps thumb to click position, starts drag
- [x] Drag: continuously updates scroll position proportional to mouse delta vs track height
- [x] Release: ends drag. Auto-scroll disabled during manual scrollbar interaction.

### 13g. Settings config sync — DONE

- [x] Replaced one-time `s_initialized` pattern with per-frame sync from config
- [x] Number input values always reflect `cfg->camera_*` when not editing/dragging
- [x] Handles first-run defaults, config reloads, and external changes

---

## Phase 14: GUI Architecture Refactor ✓ done

Consolidated keyboard routing into one engine-level dispatcher, standardized widget lifecycle around `gui_focus`, extracted gizmo logic from the main loop, and replaced the pipe cursor with a pixel-rect overlay.

### 14a. Centralized keyboard routing ✓ done

- [x] Created `engine/include/gui/gui_input.h` + `engine/src/gui/gui_input.c` — single KEY_PRESS/CHAR_INPUT dispatcher
- [x] Extended `gui_focus` with `gui_input_type` enum + `gui_focus_set_input()` for widget registration
- [x] Widgets register via `gui_focus_set_input()` on click/edit — only the focused widget receives keys
- [x] Added `submitted` flag to `gui_text_input_state` — set by router on Enter, cleared by caller
- [x] Number input Enter confirm: `handle_key` only clears focus; `gui_number_input()`'s focus-loss handler does the actual parse/transition (critical for callers that sync state from external sources while `editing=true`)
- [x] Removed per-panel key/char handlers from inspector, settings, console, project picker
- [x] Console command submission via `submitted` flag in both `ed_console.c` and `app_console.c`
- [x] Init order: console → picker → `gui_input_init()` → event handler (both editor and game host)

### 14b. Widget lifecycle standardization ✓ done

- [x] All editing widgets (text input, number input) use `gui_focus_set_input()` consistently
- [x] Removed `focused_input` pointer from `ed_inspector` — `gui_focus_get() != 0` guards editor hotkeys
- [x] Removed `name_focused` flag — name text input uses `gui_focus_is(name_input._id)` via standard focus
- [x] Removed `focused_input` from `ed_project_picker` — Tab cycling uses `gui_focus_set(next->_id)`
- [x] Removed `update_focus()` helper and all 13 call sites from inspector

### 14c. Editor main loop cleanup ✓ done

- [x] Extracted `ed_gizmo_transform_frame_update()` — encapsulates ray construction, drag update, mouse release, transform delta detection
- [x] Extracted `ed_handle_requests()` — backend switch, save, new scene, undo/redo, close project, picker transition
- [x] Main loop body reduced from ~180 to ~50 lines
- [x] Removed unused `math/ray.h` and `platform/input.h` includes from `ed_application.c`
- [x] Skip 3D scene submission when Settings tab is active — submit empty frame so the scene doesn't render behind the settings panel

### 14d. Text cursor improvements ✓ done

- [x] Added `gui_measure_text_width()` API to `gui.h`/`gui.c` — sums glyph advances using font table
- [x] Render caret as a 1.5px wide floating Clay rect overlay at the cursor pixel position
- [x] Removed `|` insertion from `gui_text_input_display()` and number input editing render
- [x] Added floating cursor rects in `gui_text_input_render()`, `gui_number_input()`, `ed_console.c`, `app_console.c`
- [x] Caret color from theme (`t->text`), blinks at 0.5s on/off

---

## Future (not scoped)

- Native file/directory picker dialog (`NSOpenPanel` on macOS, `IFileOpenDialog` on Win32, GTK/portal on Linux) — "Browse" button next to path inputs in project picker
- Multi-select, copy/paste entities
- Transform hierarchy (parent/child relationships)
- Multiple viewports (offscreen render targets)
- Play mode — launch game module inside editor with project path (F5 = play, Esc = stop)
- Snap-to-grid
- Prefab system
- Material system / material editor
- Live sync — editor watches `.scene` files for external changes, game watches for editor saves

---

## Dependency Graph

```
Phase 1: Deduplication              ✓ done
    │
Phase 2: GUI Widgets                ✓ done (context menu moved to 11e)
    │
Phase 3: Scene/Entity System        ✓ done
    │
Phase 4: Project System & Config    ✓ done
    │
    ├── Phase 5: Scene I/O          ✓ done
    │       │
    │       ├── Phase 6: Asset Discovery    ✓ done
    │       │       │
    │       │       └── Phase 7: Game Host Integration  ✓ done
    │       │
    │       └── Phase 8: Properties + Undo     ✓ done
    │               8a. Property inspector     ✓ done
    │               8b. Number input widget     ✓ done
    │               8c. Undo/redo system        ✓ done
    │
    ├── Phase 9: Viewport Interaction          ✓ done
    │       9a. Editor camera              ✓ done
    │       9b. Viewport 3D rendering      ✓ done
    │       9c. Viewport tab bar           ✓ done
    │       9d. Origin axis gizmo          ✓ done
    │       9e. Transform gizmos           ✓ done
    │       9f. Entity picking             ✓ done
    │       9g. Infinite grid              ✓ done
    │
    ├── Phase 10: Editor Polish & Theme        ✓ done
    │       10a. Theme compliance          ✓ done
    │       10b. Light visualization       ✓ done
    │       10c. Asset browser fix         ✓ done
    │       10d. Project fallback fix      ✓ done
    │       10e. Theme dropdown/settings   ✓ done
    │
    ├── Phase 11: Editor GUI Polish            ✓ done
    │       11a. FPS overlay               ✓ done
    │       11b. Tree arrow triangles      ✓ done
    │       11c. Tree visual polish        ✓ done
    │       11d. Viewport toolbar          ✓ done
    │       11e. Context menu widget       ✓ done
    │       11f. Expanded settings         ✓ done
    │       11g. Inspector section headers ✓ done
    │
    ├── Phase 12: Renderer Visual Polish       ✓ done
    │       12a. MSDF text weight          ✓ done
    │       12b. SDF rounded corners       ✓ done
    │       12c. Menu bar divider          ✓ done
    │
    ├── Phase 13: Focus & Interaction Fixes    ✓ done
    │       13a. Global focus system       ✓ done
    │       13b. Text input focus blink    ✓ done
    │       13c. Number input focus        ✓ done
    │       13d. Console focus gating      ✓ done
    │       13e. Settings keyboard input   ✓ done
    │       13f. Scrollbar drag            ✓ done
    │       13g. Settings config sync      ✓ done
    │
    └── Phase 14: GUI Architecture Refactor    ✓ done
            14a. Centralized keyboard routing  ✓ done
            14b. Widget lifecycle standard.    ✓ done
            14c. Editor main loop cleanup      ✓ done
            14d. Text cursor improvements      ✓ done
```

Remaining loose ends:
- Entity create/destroy undo action types exist but the recreation logic for undo isn't wired yet. Context menu delete works but doesn't push undo entries.
- File browser widget (`gui_file_browser`) exists but isn't wired into project picker's Browse button yet (native dialog preferred long-term).
