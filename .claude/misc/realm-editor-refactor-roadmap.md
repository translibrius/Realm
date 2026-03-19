# realm_editor/ Refactoring Roadmap

Structural reorganization of `realm_editor/src/` from flat layout to semantic subfolders, plus extraction of duplicated logic and oversized responsibilities from `ed_application.c` (414 lines) and `ed_layout.c` (607 lines).

## Current State

32 files (16 pairs), all flat in `src/`:

| File | Lines | Role |
|------|-------|------|
| main.c | ~10 | Entry point |
| ed_application.h/c | 55/414 | Bootstrap, frame loop, scene I/O, mode switching, request handling |
| ed_layout.h/c | 59/607 | Full editor GUI: menu bar, hierarchy, viewport, properties, context menus |
| ed_config.h/c | ~40/~100 | Editor persistent config (theme, recents, camera prefs) |
| ed_event_handler.h/c | ~20/~100 | Key/event routing |
| ed_camera.h/c | ~30/~100 | Orbit/fly camera for viewport |
| ed_gizmo.h/c | ~30/~80 | Gizmo shape rendering (axis overlay) |
| ed_gizmo_transform.h/c | ~50/~200 | Transform gizmo drag logic |
| ed_picking.h/c | ~20/~60 | Mouse picking |
| ed_inspector.h/c | ~40/~200 | Entity property inspector panel |
| ed_asset_browser.h/c | ~30/~150 | Asset browser panel |
| ed_console.h/c | ~20/~60 | Editor console overlay |
| ed_settings.h/c | ~20/~100 | Settings panel (inside viewport tab) |
| ed_toolbar.h/c | ~15/~60 | Viewport toolbar strip |
| ed_project_picker.h/c | ~30/~150 | Project picker mode UI |
| ed_undo.h/c | ~40/~120 | Undo/redo stack |

## Key Problems

1. **`ed_application.c` (414 lines)** does too much: bootstrap, scene I/O (save/load/new), editor/picker mode transitions, the entire frame loop (camera update, frame data building, gizmo orchestration, GUI dispatch), and a 80-line `ed_handle_requests()`.

2. **`ed_layout.c` (607 lines)** has duplicated entity factory code: "Add Light" appears identically in both the hierarchy context menu (lines 511-523) and viewport context menu (lines 576-588). "Add Empty Entity" (hierarchy, lines 503-510) is nearly identical to the entity portion of "Add Cube" (viewport, lines 562-574).

3. **All 32 files flat** in `src/` with no grouping.

## Target Structure

```
realm_editor/src/
  main.c                                (unchanged)
  core/
    ed_application.h/c                  (move + slimmed: boot, loop shell, mode transitions)
    ed_config.h/c                       (move, no changes)
    ed_event_handler.h/c                (move, no changes)
  scene/
    ed_scene_io.h/c                     (NEW: save/load/new scene extracted from ed_application.c)
    ed_entity_ops.h/c                   (NEW: entity factory functions deduplicated from ed_layout.c)
    ed_undo.h/c                         (move, no changes)
  viewport/
    ed_camera.h/c                       (move, no changes)
    ed_frame.h/c                        (NEW: frame data building + gizmo orchestration from loop)
    ed_gizmo.h/c                        (move, no changes)
    ed_gizmo_transform.h/c              (move, no changes)
    ed_picking.h/c                      (move, no changes)
  panels/
    ed_layout.h/c                       (move + slimmed: context menus use ed_entity_ops)
    ed_inspector.h/c                    (move, no changes)
    ed_asset_browser.h/c                (move, no changes)
    ed_console.h/c                      (move, no changes)
    ed_settings.h/c                     (move, no changes)
    ed_toolbar.h/c                      (move, no changes)
  project/
    ed_project_picker.h/c               (move, no changes)
```

## Prerequisites

- CMakeLists already uses `GLOB_RECURSE` over `src/*.c` — subfolders auto-discovered.
- CMakeLists already has `src/` as a private include dir (line 24) — cross-folder includes like `#include "core/ed_application.h"` will resolve.

## Verification (after every phase)

```bash
cmake --preset debug && cmake --build --preset debug
```

Then run `bin/RealmEditor.exe` and verify:
- Project picker opens, can select a project
- Scene loads with entities visible
- Entity creation (hierarchy context menu: Add Empty, Add Light; viewport: Add Cube, Add Light)
- Entity selection, inspector binds, property editing
- Gizmo drag works, undo/redo records transforms
- Save scene (Ctrl+S or File > Save), load scene
- New scene (File > New Scene)
- Console toggle, grid toggle
- Settings tab renders
- Backend switch, window minimize/maximize/close

---

## Phase 1: Extract `ed_entity_ops.h/c` (Deduplicate Entity Creation)

**What:** The entity creation code is duplicated across two context menus in `ed_layout.c`. Extract reusable factory functions.

### New file: `src/ed_entity_ops.h`

```c
#pragma once

#include "core/entity.h"
#include "defines.h"

typedef struct rl_scene rl_scene;

// Creates an empty entity with a default transform (identity position, unit scale).
rl_entity ed_entity_create_empty(rl_scene *scene, const char *name);

// Creates a light entity with default properties.
rl_entity ed_entity_create_light(rl_scene *scene);

// Creates a cube entity with default mesh + material.
rl_entity ed_entity_create_cube(rl_scene *scene);

// Duplicates an existing entity (copies name, transform).
rl_entity ed_entity_duplicate(rl_scene *scene, rl_entity source);

// Destroys an entity.
void ed_entity_delete(rl_scene *scene, rl_entity entity);
```

### New file: `src/ed_entity_ops.c`

Extract the bodies from `ed_layout.c`:

- `ed_entity_create_empty`: from hierarchy context menu `picked == 0` block (lines 503-510)
- `ed_entity_create_light`: from hierarchy context menu `picked == 1` block (lines 511-523) — identical to viewport `picked == 1` (lines 576-588)
- `ed_entity_create_cube`: from viewport context menu `picked == 0` block (lines 562-574)
- `ed_entity_duplicate`: from hierarchy context menu `picked == 2` block (lines 525-539)
- `ed_entity_delete`: from hierarchy context menu `picked == 3` block (lines 540-547), just wraps `scene_entity_destroy`

Each function creates the entity with all components and returns the `rl_entity`. The caller in `ed_layout.c` handles `selected_id` and `scene_dirty`.

Required includes:
```c
#include "ed_entity_ops.h"
#include "core/component.h"
#include "core/scene.h"
#include "renderer/frame_data.h"  // for RL_FRAME_MESH_KIND_LIT, RL_FRAME_PRIMITIVE_CUBE
```

### Changes to `ed_layout.c`

- Add `#include "ed_entity_ops.h"`
- Replace the hierarchy context menu entity creation blocks with calls to `ed_entity_create_empty()`, `ed_entity_create_light()`, `ed_entity_duplicate()`, `ed_entity_delete()`
- Replace the viewport context menu entity creation blocks with calls to `ed_entity_create_cube()`, `ed_entity_create_light()`
- Each call site still handles `layout->hierarchy_tree.selected_id = ED_ENTITY_NODE_BASE + rl_entity_index(e)` and `app->scene_dirty = true`

**Result:** `ed_layout.c` drops from 607 to ~500 lines. The "Add Light" duplication is eliminated.

Also update `ed_application.c` `ed_enter_editor_mode()` (lines 99-109) where a default Light entity is created — use `ed_entity_create_light()` there too. Check the position override (`lt->position = {1.2, 1.0, 2.0}`) — the factory uses `y=3.0` by default, so either:
- Have `ed_entity_create_light` return the entity and let the caller override position, or
- Accept the factory default and update the default scene light position to `y=3.0`

Recommend option (a): the factory sets defaults, caller tweaks position after.

**Verify:** Build + run. Test all entity creation paths (hierarchy menu, viewport menu, default scene creation).

---

## Phase 2: Extract `ed_scene_io.h/c` (Scene Save/Load/New)

**What:** `ed_application.c` has `ed_save_scene()`, `ed_load_scene()`, `ed_new_scene()`, and `ed_build_scene_abs_path()` as static functions (lines 37-83). These are pure scene I/O with no dependency on the frame loop.

### New file: `src/ed_scene_io.h`

```c
#pragma once

#include "defines.h"

typedef struct ed_application ed_application;

void ed_scene_save(ed_application *app, const char *path);
void ed_scene_load(ed_application *app, const char *path);
void ed_scene_new(ed_application *app);

// Builds the absolute path for the project's default scene into buf.
void ed_scene_build_abs_path(char *buf, u32 buf_size);
```

### New file: `src/ed_scene_io.c`

Move the four static functions from `ed_application.c`. Change them from referencing the file-static `app` to taking `ed_application *app`. Required includes:

```c
#include "ed_scene_io.h"
#include "ed_application.h"
#include "ed_undo.h"
#include "core/logger.h"
#include "core/project.h"
#include "core/scene.h"
#include "core/scene_io.h"
#include "platform/io/file_io.h"
#include "util/str.h"
```

### Changes to `ed_application.c`

- Remove the four static functions (lines 37-83)
- Add `#include "ed_scene_io.h"`
- Replace all calls:
  - `ed_save_scene(path)` -> `ed_scene_save(&app, path)`
  - `ed_load_scene(path)` -> `ed_scene_load(&app, path)`
  - `ed_new_scene()` -> `ed_scene_new(&app)`
  - `ed_build_scene_abs_path(buf, size)` -> `ed_scene_build_abs_path(buf, size)` (this one doesn't need `app`, it uses `project_get()`)
- Update `ed_handle_requests()` and `ed_enter_editor_mode()` / `ed_enter_picker_mode()` accordingly

**Result:** `ed_application.c` drops by ~50 lines.

**Verify:** Build + run. Test save (Ctrl+S), load (project open), new scene (File > New Scene).

---

## Phase 3: Extract `ed_frame.h/c` (Frame Data Building + Gizmo Orchestration)

**What:** The editor-mode branch of the frame loop in `ed_application.c` (lines 294-387) builds frame data, runs the camera, orchestrates gizmo transforms, and submits to the renderer. This is ~90 lines that can be a single function.

### New file: `src/ed_frame.h`

```c
#pragma once

#include "defines.h"

typedef struct ed_application ed_application;

// Builds and submits one editor frame: camera update, frame data, gizmo, GUI.
void ed_frame_update(ed_application *app, f64 dt);
```

### New file: `src/ed_frame.c`

Move the editor-mode frame building block from the loop. Required includes:

```c
#include "ed_frame.h"
#include "ed_application.h"
#include "ed_camera.h"
#include "ed_gizmo.h"
#include "ed_gizmo_transform.h"
#include "ed_inspector.h"
#include "ed_layout.h"
#include "ed_picking.h"
#include "ed_settings.h"
#include "ed_console.h"
#include "ed_project_picker.h"
#include "core/config.h"
#include "core/scene.h"
#include "engine.h"
#include "cglm.h"
#include "clay.h"
#include "gui/gui.h"
#include "gui/gui_file_browser.h"
#include "core/project_export.h"
#include "core/project.h"
#include "renderer/renderer_frontend.h"
```

The function handles both editor mode and picker mode rendering. The frame loop in `ed_application.c` becomes:

```c
while (rl_engine_is_running()) {
    if (!rl_engine_begin_frame(&dt)) continue;
    ed_frame_update(&app, dt);
    rl_engine_end_frame();
    ed_handle_requests();
}
```

### Changes to `ed_application.c`

- Remove the entire editor-mode and picker-mode frame building blocks (lines 294-396)
- Add `#include "ed_frame.h"`
- Replace with single `ed_frame_update(&app, dt)` call
- Can also remove includes that were only needed for frame building: `cglm.h`, `clay.h`, `gui/gui.h`, `gui/gui_theme.h`, `renderer/renderer_frontend.h`, `ed_gizmo.h`, `ed_gizmo_transform.h`, `ed_inspector.h`, `ed_picking.h`, `ed_settings.h`

**Result:** `ed_application.c` drops to ~150 lines total: bootstrap, mode transitions, request handling, shutdown.

**Verify:** Build + run. Test viewport rendering, camera movement, gizmo interaction, settings tab, picker mode rendering.

---

## Phase 4: Create Subfolder Structure and Move All Files

**What:** Now that extractions are done, move everything into semantic subfolders. This is purely file moves + include path updates.

### Moves

**`core/`** (application infrastructure):
- `ed_application.h/c` -> `core/ed_application.h/c`
- `ed_config.h/c` -> `core/ed_config.h/c`
- `ed_event_handler.h/c` -> `core/ed_event_handler.h/c`

**`scene/`** (scene data operations):
- `ed_scene_io.h/c` -> `scene/ed_scene_io.h/c` (created in P2)
- `ed_entity_ops.h/c` -> `scene/ed_entity_ops.h/c` (created in P1)
- `ed_undo.h/c` -> `scene/ed_undo.h/c`

**`viewport/`** (3D viewport + interaction):
- `ed_camera.h/c` -> `viewport/ed_camera.h/c`
- `ed_frame.h/c` -> `viewport/ed_frame.h/c` (created in P3)
- `ed_gizmo.h/c` -> `viewport/ed_gizmo.h/c`
- `ed_gizmo_transform.h/c` -> `viewport/ed_gizmo_transform.h/c`
- `ed_picking.h/c` -> `viewport/ed_picking.h/c`

**`panels/`** (GUI panels):
- `ed_layout.h/c` -> `panels/ed_layout.h/c`
- `ed_inspector.h/c` -> `panels/ed_inspector.h/c`
- `ed_asset_browser.h/c` -> `panels/ed_asset_browser.h/c`
- `ed_console.h/c` -> `panels/ed_console.h/c`
- `ed_settings.h/c` -> `panels/ed_settings.h/c`
- `ed_toolbar.h/c` -> `panels/ed_toolbar.h/c`

**`project/`** (project management):
- `ed_project_picker.h/c` -> `project/ed_project_picker.h/c`

`main.c` stays at `src/main.c`.

### Include Path Updates

This is the bulk of the work. Every `#include "ed_*.h"` must be prefixed with its subfolder. Since `src/` is already an include dir, all paths are relative to `src/`.

Strategy: do a grep for every `ed_` include pattern and update them. Examples:
- `#include "ed_application.h"` -> `#include "core/ed_application.h"`
- `#include "ed_camera.h"` -> `#include "viewport/ed_camera.h"`
- `#include "ed_layout.h"` -> `#include "panels/ed_layout.h"`
- `#include "ed_undo.h"` -> `#include "scene/ed_undo.h"`
- `#include "ed_entity_ops.h"` -> `#include "scene/ed_entity_ops.h"`
- `#include "ed_scene_io.h"` -> `#include "scene/ed_scene_io.h"`
- etc.

For files within the same subfolder, you can use either the bare name or the qualified path. Prefer the qualified path for consistency (e.g. in `viewport/ed_gizmo.c`, use `#include "viewport/ed_camera.h"` not just `#include "ed_camera.h"`).

### Batch approach

1. Create all subdirectories: `core/`, `scene/`, `viewport/`, `panels/`, `project/`
2. Move all files (git mv to preserve history)
3. Do a project-wide find-and-replace on `#include "ed_` patterns
4. Build and fix any remaining include issues

**Verify:** Build + run. Full functional test (all verification items listed above).

---

## Summary

| Phase | What | New files | `ed_application.c` | `ed_layout.c` |
|-------|------|-----------|--------------------|--------------|
| P1 | Entity ops dedup | ed_entity_ops.h/c | -5 lines | -100 lines |
| P2 | Scene I/O extract | ed_scene_io.h/c | -50 lines | unchanged |
| P3 | Frame building extract | ed_frame.h/c | -100 lines | unchanged |
| P4 | Subfolder moves | 0 (moves only) | include paths | include paths |

Final state: `ed_application.c` ~150 lines, `ed_layout.c` ~500 lines, 20 files across 5 subfolders.

Each phase is independently buildable and testable. No behavioral changes in any phase.
