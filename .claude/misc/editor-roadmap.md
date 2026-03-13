# Realm Editor — Development Roadmap

## Instructions for agents

When starting a session from this roadmap:

1. **Generate a scoped plan first.** Read the next incomplete phase below, then produce a concrete implementation plan (files to create/modify, API sketches, wiring steps) scoped to what can be done in **~50% of your context window**. Do not attempt to entire roadmap in one session — pick the next logical chunk and plan it thoroughly before writing code.
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

### 5a. Scene serialization

- [ ] Create `engine/include/core/scene_io.h` + `engine/src/core/scene_io.c`
- [ ] JSON format via yyjson (already vendored) — one `.scene` file per scene
- [ ] `scene_save(scene, path)` — serialize all entities + components to JSON
- [ ] `scene_load(path)` → `rl_scene *` — deserialize from JSON, create entities + components
- [ ] Format: `{ "name": "...", "entities": [ { "name": "Cube", "transform": {...}, "mesh": {...}, "light": {...} } ] }`
- [ ] Component serializers: transform (position/rotation/scale), mesh (primitive, kind), light (ambient/diffuse/specular)
- [ ] Unit tests: round-trip save→load, empty scene, entities with various component combos

### 5b. Wire scene I/O into editor

- [ ] File menu: New Scene, Open Scene, Save Scene, Save Scene As
- [ ] Editor tracks current scene file path (null = unsaved)
- [ ] New project creates `scenes/default.scene` with a Light entity on disk
- [ ] Opening a project loads its `default_scene` from `project.realm`
- [ ] Dirty state tracking — mark scene dirty on any entity/component change
- [ ] Unsaved changes prompt before close/open/new (simple confirm dialog or just auto-save)

### 5c. Wire scene loading into game host

- [ ] Game host (`realm/src/application.c`) gains `project_open()` call after bootstrap
- [ ] Game module can call `scene_load()` to load editor-authored scenes
- [ ] Expand `realm_app_context` with project pointer so module knows project paths
- [ ] Proof of life: game loads a `.scene` file saved by the editor and renders it

---

## Phase 6: Dynamic Asset Discovery

Content assets are currently a compile-time `content_asset_table[]`. For a shared project to work, both hosts need to discover assets from the project directory at runtime.

### 6a. Directory scanning

- [ ] Create `engine/include/platform/io/file_scan.h` + platform implementations
- [ ] `platform_dir_scan(path, extension_filter, results)` — list files in a directory
- [ ] Returns array of relative paths (caller provides arena)
- [ ] Extension filter: `".jpg,.png,.gltf"` style comma-separated list

### 6b. Project asset manifest

- [ ] On `project_open()`, scan `assets/textures/`, `assets/models/` etc.
- [ ] Build a runtime asset list from what's on disk (replaces `content_asset_table[]`)
- [ ] `project_load_assets()` — calls `asset_load()` for each discovered file
- [ ] Editor calls this after opening a project; game host calls it too
- [ ] Assets show up in hierarchy / can be assigned to entities

### 6c. Decouple content_asset_table

- [ ] `content_asset_table[]` becomes the "demo/fallback" set — only used when no project is open
- [ ] Game host: if project open → `project_load_assets()`, else → `asset_system_load_content()`
- [ ] Editor: always uses `project_load_assets()` (never loads demo content)
- [ ] Verify: game still works standalone with hardcoded assets, also works with project

### 6d. Asset browser panel

- [ ] Create `realm_editor/src/ed_asset_browser.h/.c`
- [ ] Panel showing project's `assets/` directory tree using tree view widget
- [ ] Click mesh → create entity with that mesh
- [ ] Drag-and-drop import: `EVENT_FILE_DROP` → copy file into project's `assets/` dir → auto-load

---

## Phase 7: Game Host Integration

Make the Realm game executable a first-class consumer of editor-authored projects. The goal: edit a scene in the editor, hit play in the game, see the same thing.

### 7a. Game host project support

- [ ] Game host opens project via CLI arg or config: `Realm --project /path/to/my_project`
- [ ] Falls back to legacy mode (hardcoded `content_asset_table[]`) when no project specified
- [ ] On project open: discover + load project assets, load default scene
- [ ] `realm_app_context` exposes `rl_project *project` and `rl_scene *scene` to game module

### 7b. Migrate existing game content to a project

- [ ] Create a `game/` project directory (or use repo root with `project.realm`)
- [ ] Move/symlink textures and models from `assets/` into `game/assets/`
- [ ] Create `game/scenes/main_menu.scene`, `game/scenes/gameplay.scene` from existing hardcoded scenes
- [ ] Update game module to load scenes from files instead of building in code
- [ ] Existing `scene_main_menu.c` etc. become gameplay logic layers on top of loaded scenes

### 7c. Shared workflow validation

- [ ] End-to-end: create project in editor → add entities → save scene → launch game with same project → scene renders
- [ ] Editor and game read the same `project.realm`, same `assets/`, same `scenes/`
- [ ] Hot-reload still works: game module reloads, scene persists from file
- [ ] No separate repos needed — single repo, shared project directory

---

## Phase 8: Editable Properties + Undo

With scene I/O in place, property editing becomes meaningful — changes can be saved and loaded.

### 8a. Property inspector

- [ ] Create `realm_editor/src/ed_inspector.h/.c`
- [ ] Generic component inspector using existing GUI widgets
- [ ] Transform → 3 vec3 fields (position, rotation, scale)
- [ ] Mesh → asset path display (dropdown from asset browser)
- [ ] Light → color RGB sliders, intensity

### 8b. Number input widget

- [ ] Create `engine/include/gui/gui_number_input.h` + `engine/src/gui/gui_number_input.c`
- [ ] Numeric text input with drag-to-adjust (click-drag on label changes value)

### 8c. Undo/redo system

- [ ] Create `engine/include/core/undo.h` + `engine/src/core/undo.c`
- [ ] Command pattern with fixed-size ring buffer (256 entries)
- [ ] `undo_push(description, apply_fn, revert_fn, data, data_size)`
- [ ] Ctrl+Z / Ctrl+Shift+Z hotkeys
- [ ] Scene marked dirty on any undo-able action

---

## Phase 9: Viewport Interaction

### 9a. Editor camera

- [ ] Create `realm_editor/src/ed_camera.h/.c`
- [ ] Orbit camera: middle-mouse orbit, scroll zoom, right-click WASD fly, F to frame selection

### 9b. Origin axis gizmo

- [ ] Colored XYZ axis indicator in viewport corner (red=X, green=Y, blue=Z)
- [ ] Always visible, shows current camera orientation
- [ ] Rendered as overlay (not affected by scene depth)

### 9c. Transform gizmos

- [ ] Create `realm_editor/src/ed_gizmo.h/.c`
- [ ] Translate/rotate/scale handles as overlay geometry
- [ ] Requires overlay render pass in both GL and Vulkan backends
- [ ] W/E/R to switch between translate/rotate/scale modes

### 9d. Entity picking

- [ ] CPU ray casting against entity bounding boxes
- [ ] Click in viewport to select entity (syncs with hierarchy panel)

### 9e. Infinite grid (optional/toggleable)

- [ ] Infinite ground plane grid rendered in world space
- [ ] Fades with distance, major/minor grid lines
- [ ] Toggle via View menu or hotkey

---

## Future (not scoped)

- Native file/directory picker dialog (`NSOpenPanel` on macOS, `IFileOpenDialog` on Win32, GTK/portal on Linux) — "Browse" button next to path inputs in project picker
- Context menu widget (`gui_context_menu`) — right-click in hierarchy/viewport
- Multi-select, copy/paste entities
- Transform hierarchy (parent/child relationships)
- Multiple viewports (offscreen render targets)
- Play mode — launch game module inside editor with project path (F5 = play, Esc = stop)
- Snap-to-grid
- Editor preferences panel
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
    ├── Phase 5: Scene I/O          ← critical path (editor↔game bridge)
    │       │
    │       ├── Phase 6: Asset Discovery
    │       │       │
    │       │       └── Phase 7: Game Host Integration
    │       │               (game loads editor-authored scenes + project assets)
    │       │
    │       └── Phase 8: Properties + Undo
    │               (meaningful once scenes are saveable)
    │
    └── Phase 9: Viewport Interaction
            (camera, gizmos, picking — independent of integration)
```

**Critical path to editor usability: 5 → 6 → 7.**
Phases 8 and 9 can run in parallel once Phase 5 is done.
Phase 7 is the "it all comes together" milestone — editor authors, game runs.
