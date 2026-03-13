# Realm Editor — Deduplication & Development Roadmap

## Instructions for agents

When starting a session from this roadmap:

1. **Generate a scoped plan first.** Read the next incomplete phase below, then produce a concrete implementation plan (files to create/modify, API sketches, wiring steps) scoped to what can be done in **~50% of your context window**. Do not attempt to implement the entire roadmap in one session — pick the next logical chunk and plan it thoroughly before writing code.
2. **Update this file at the end of a successful session.** Check off completed items, add notes on any deferred work or decisions made, and update the dependency graph if needed. This keeps the roadmap accurate for the next agent.

---

## Context

The editor skeleton is built and running (`realm_editor/`). It duplicates significant code from the game host (`realm/`). Before building further, we need to extract shared logic so that changes to console, window management, event handling, and bootstrap don't require updating two places. After that, the roadmap describes how to grow the editor into something useful.

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

### 2c. Context Menu (deferred — not useful until entities exist in Phase 3)

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

## Phase 4: Editable Properties + Undo

### 4a. Property inspector

- [ ] Create `realm_editor/src/ed_inspector.h/.c`
- [ ] Generic component inspector using existing gui widgets
- [ ] Transform → 3 vec3 fields, Mesh → asset dropdown + material, Light → RGB sliders

### 4b. Number input widget

- [ ] Create `engine/include/gui/gui_number_input.h` + `engine/src/gui/gui_number_input.c`
- [ ] Numeric text input with drag-to-adjust

### 4c. Undo/redo system

- [ ] Create `engine/include/core/undo.h` + `engine/src/core/undo.c`
- [ ] Command pattern with fixed-size ring buffer (256 entries)
- [ ] Ctrl+Z / Ctrl+Shift+Z hotkeys

---

## Phase 5: Viewport Interaction

### 5a. Editor camera

- [ ] Create `realm_editor/src/ed_camera.h/.c`
- [ ] Orbit camera: middle-mouse orbit, scroll zoom, right-click WASD fly, F to frame selection

### 5b. Transform gizmos

- [ ] Create `realm_editor/src/ed_gizmo.h/.c`
- [ ] Translate/rotate/scale handles as overlay meshes
- [ ] Requires overlay render pass in both GL and Vulkan backends

### 5c. Entity picking

- [ ] CPU ray casting against entity bounding boxes
- [ ] Click in viewport to select entity

---

## Phase 6: Asset Management + Scene I/O

### 6a. Asset browser panel

- [ ] File browser showing `assets/` directory tree using tree view widget
- [ ] Click mesh → create entity

### 6b. Scene save/load

- [ ] JSON serialization via yyjson (already vendored)
- [ ] Wire to File > New/Open/Save

### 6c. Drag-and-drop import

- [ ] Wire `EVENT_FILE_DROP` to copy dropped files to asset dir and load them

---

## Future (not scoped)

- Multi-select, copy/paste entities
- Transform hierarchy (parent/child)
- Multiple viewports (offscreen rendering)
- Play mode (load game module inside editor)
- Grid rendering, snap-to-grid
- Editor preferences panel
- Prefab system

---

## Dependency Graph

```
Phase 1: Deduplication              ✓ done
    │
Phase 2: GUI Widgets                ✓ done (2c deferred)
    │
Phase 3: Scene/Entity System     ✓ done
    │
    ├── Phase 4: Properties + Undo  ◄── next up
    │       │
    │       └── Phase 6: Asset Browser + Scene I/O
    │
    └── Phase 5: Viewport Interaction (camera, gizmo, picking)
```

Phases 4 and 5 can run in parallel after Phase 3.
