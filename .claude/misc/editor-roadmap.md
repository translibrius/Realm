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

### 2c. Context Menu (deferred — useful once entities are editable)

- [ ] Create `engine/include/gui/gui_context_menu.h` + `engine/src/gui/gui_context_menu.c`
- [ ] Right-click popup positioned at mouse cursor, builds on `gui_dropdown` pattern
- [ ] Test with right-click in hierarchy panel

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

### 9d. Origin axis gizmo

- [ ] Colored XYZ axis indicator in viewport corner (red=X, green=Y, blue=Z)
- [ ] Always visible, shows current camera orientation
- [ ] Rendered as overlay (not affected by scene depth)

### 9e. Transform gizmos

- [ ] Create `realm_editor/src/ed_gizmo.h/.c`
- [ ] Translate/rotate/scale handles as overlay geometry
- [ ] Requires overlay render pass in both GL and Vulkan backends
- [ ] W/E/R to switch between translate/rotate/scale modes

### 9f. Entity picking

- [ ] CPU ray casting against entity bounding boxes
- [ ] Click in viewport to select entity (syncs with hierarchy panel)

### 9g. Infinite grid (optional/toggleable)

- [ ] Infinite ground plane grid rendered in world space
- [ ] Fades with distance, major/minor grid lines
- [ ] Toggle via View menu or hotkey

---

## Phase 10: Editor Polish & Theme System — DONE (partial)

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

### 10e. Theme dropdown & editor settings tab

- [ ] Add theme selector dropdown (dark, catppuccin, etc.) in a new "Settings" center tab
- [ ] Persist selected theme in `editor_state.toml`
- [ ] Tab switching logic in viewport area (Viewport vs Settings)

---

## Future (not scoped)

- Native file/directory picker dialog (`NSOpenPanel` on macOS, `IFileOpenDialog` on Win32, GTK/portal on Linux) — "Browse" button next to path inputs in project picker
- Context menu widget (`gui_context_menu`) — right-click in hierarchy/viewport
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
Phase 2: GUI Widgets                ✓ done (context menu deferred)
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
    ├── Phase 9: Viewport Interaction
    │       9a. Editor camera              ✓ done
    │       9b. Viewport 3D rendering      ✓ done
    │       9c. Viewport tab bar           ✓ done
    │       9d. Origin axis gizmo          ← next
    │       9e. Transform gizmos
    │       9f. Entity picking
    │       9g. Infinite grid
    │
    └── Phase 10: Editor Polish & Theme
            10a. Theme compliance          ✓ done
            10b. Light visualization       ✓ done
            10c. Asset browser fix         ✓ done
            10d. Project fallback fix      ✓ done
            10e. Theme dropdown/settings   ← next
```

**Next session: Phase 9d (Origin Axis Gizmo), Phase 9e (Transform Gizmos), Phase 9f (Entity Picking), or Phase 10e (Theme Dropdown & Settings Tab).**
Phase 9b-c and 10a-d are complete. The editor viewport now renders 3D content correctly within its bounds, light entities have visible markers, asset browser double-click works for directory-based models, and all GUI colors are theme-driven.
Name editing in the inspector is wired for keyboard but not yet clickable (no click-to-focus on the name text). Could be added as a small follow-up.
Entity create/destroy undo action types exist but the recreation logic isn't wired — no UI for entity delete yet. Wire when adding context menu (Phase 2c) or delete hotkey.
