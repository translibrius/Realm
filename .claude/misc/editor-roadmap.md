# Realm Editor — Development Roadmap

## Instructions for agents

When starting a session from this roadmap:

1. **Generate a scoped plan first.** Read the next incomplete phase below, then produce a concrete implementation plan (files to create/modify, API sketches, wiring steps) scoped to what can be done in **~50% of your context window**. Do not attempt to entire roadmap in one session — pick the next logical chunk and plan it thoroughly before writing code.
2. **Update this file at the end of a successful session.** Check off completed items, add notes on any deferred work or decisions made, and update the dependency graph if needed. This keeps the roadmap accurate for the next agent.

---

## Context

The editor skeleton is built and running (`realm_editor/`). Phases 1-3 extracted shared logic into `engine/include/host/`, built GUI widgets, and added a scene/entity system. Before building further editing features, we need a project system so the editor works with discrete project directories rather than a hardcoded shared asset root.

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

### 4d. Editor bootstrap refactor — next up

- [ ] Editor skips splash screen — boots straight to project picker or last project
- [ ] After bootstrap, if no project open → show project picker; if project open → show editor layout
- [ ] Editor stores last-opened project path + recent projects list in `editor.toml`
- [ ] Add `recent_projects` field to `rl_config` or a separate editor config struct
- [ ] Verify: editor boots to picker, game host unchanged

**Implementation notes from 4a-4c:**
- `host_bootstrap()` now takes 3 args: `(asset_root, window_title, config_filename)` — no separate editor mode flag needed, the config filename already differentiates
- Content loading is already decoupled — editor doesn't call `asset_system_load_content()`, so it boots with engine assets only
- `project_open()` calls `asset_set_content_root()` automatically, so opening a project is enough to redirect texture/mesh loading to the project's `assets/` directory

### 4e. Project picker screen — next up (after 4d)

- [ ] Create `realm_editor/src/ed_project_picker.h/.c`
- [ ] Landing screen with: "New Project" button, "Open Project" button/path input, recent projects list
- [ ] "New Project" flow: pick directory, enter name → `project_create()` → open it
- [ ] "Open Project" flow: pick directory containing `project.realm` → `project_open()`
- [ ] Recent projects loaded from `editor.toml`
- [ ] On project selected → transition to full editor layout with scene loaded
- [ ] New project creates a default scene with a camera and a light (minimal starting point)
- [ ] Need a text input widget for path entry — use existing `gui_text_input` or add path input variant
- [ ] Consider native file dialog (`NSOpenPanel` on macOS, `IFileOpenDialog` on Win32) for directory picking
- [ ] Verify: full flow — launch editor → pick/create project → editor layout with scene

---

## Phase 5: Editable Properties + Undo

### 5a. Property inspector

- [ ] Create `realm_editor/src/ed_inspector.h/.c`
- [ ] Generic component inspector using existing GUI widgets
- [ ] Transform → 3 vec3 fields (position, rotation, scale)
- [ ] Mesh → asset path display (dropdown later when asset browser exists)
- [ ] Light → color RGB sliders, intensity

### 5b. Number input widget

- [ ] Create `engine/include/gui/gui_number_input.h` + `engine/src/gui/gui_number_input.c`
- [ ] Numeric text input with drag-to-adjust (click-drag on label changes value)

### 5c. Undo/redo system

- [ ] Create `engine/include/core/undo.h` + `engine/src/core/undo.c`
- [ ] Command pattern with fixed-size ring buffer (256 entries)
- [ ] `undo_push(description, apply_fn, revert_fn, data, data_size)`
- [ ] Ctrl+Z / Ctrl+Shift+Z hotkeys

---

## Phase 6: Viewport Interaction

### 6a. Editor camera

- [ ] Create `realm_editor/src/ed_camera.h/.c`
- [ ] Orbit camera: middle-mouse orbit, scroll zoom, right-click WASD fly, F to frame selection

### 6b. Origin axis gizmo

- [ ] Colored XYZ axis indicator in viewport corner (red=X, green=Y, blue=Z)
- [ ] Always visible, shows current camera orientation
- [ ] Rendered as overlay (not affected by scene depth)

### 6c. Transform gizmos

- [ ] Create `realm_editor/src/ed_gizmo.h/.c`
- [ ] Translate/rotate/scale handles as overlay geometry
- [ ] Requires overlay render pass in both GL and Vulkan backends
- [ ] W/E/R to switch between translate/rotate/scale modes

### 6d. Entity picking

- [ ] CPU ray casting against entity bounding boxes
- [ ] Click in viewport to select entity (syncs with hierarchy panel)

### 6e. Infinite grid (optional/toggleable)

- [ ] Infinite ground plane grid rendered in world space
- [ ] Fades with distance, major/minor grid lines
- [ ] Toggle via View menu or hotkey

---

## Phase 7: Scene I/O + Asset Browser

### 7a. Scene save/load

- [ ] JSON serialization via yyjson (already vendored)
- [ ] Save to / load from project's `scenes/` directory
- [ ] Wire to File > New Scene / Open Scene / Save Scene / Save As
- [ ] Track dirty state, prompt on unsaved changes

### 7b. Asset browser panel

- [ ] Panel showing project's `assets/` directory tree using tree view widget
- [ ] Thumbnail previews (future)
- [ ] Click mesh → create entity with that mesh
- [ ] Double-click to open/inspect asset

### 7c. Drag-and-drop import

- [ ] Wire `EVENT_FILE_DROP` to copy dropped files into project's asset directory
- [ ] Auto-load dropped assets

---

## Future (not scoped)

- Context menu widget (`gui_context_menu`) — right-click in hierarchy/viewport
- Multi-select, copy/paste entities
- Transform hierarchy (parent/child relationships)
- Multiple viewports (offscreen render targets)
- Play mode (launch game module inside editor with project path)
- Snap-to-grid
- Editor preferences panel
- Prefab system
- Material system / material editor
- Game host project integration (realm/ opens project directory for assets/scenes)

---

## Dependency Graph

```
Phase 1: Deduplication              ✓ done
    │
Phase 2: GUI Widgets                ✓ done (context menu deferred)
    │
Phase 3: Scene/Entity System        ✓ done
    │
Phase 4: Project System & Config    ◐ in progress (4a-4c done, 4d-4e next)
    │       (config split ✓, asset split ✓, project dirs ✓, bootstrap refactor + picker screen next)
    │
    ├── Phase 5: Properties + Undo
    │
    ├── Phase 6: Viewport Interaction
    │       (camera, axis gizmo, transform gizmos, picking, grid)
    │
    └── Phase 7: Scene I/O + Asset Browser
            (requires project system for paths)
```

Phases 5 and 6 can run in parallel after Phase 4.
Phase 7 depends on Phase 4 (project paths) and benefits from Phase 5 (dirty tracking).
