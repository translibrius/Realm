# realm/ Refactoring Roadmap

Structural reorganization of `realm/src/` from flat layout to semantic subfolders, plus extraction of responsibilities from `application.c` (264 lines) into focused modules.

## Current State

15 files, all flat in `src/`:

| File | Lines | Role |
|------|-------|------|
| main.c | ~10 | Entry point |
| application.h/c | 34/264 | Bootstrap, frame loop, module lifecycle, output processing, shutdown |
| app_console.h/c | 17/~80 | Console GUI overlay |
| app_debug_panel.h/c | 11/~50 | Debug panel GUI overlay |
| app_renderer.h/c | 9/30 | Backend switch helper |
| event_handler.h/c | 14/~80 | Key/event routing |
| realm_app_loader.h/c | 35/~180 | DLL load/unload/reload/rebuild |
| realm_app_watcher.h/c | 24/~100 | File-change polling for hot reload |

## Target Structure

```
realm/src/
  main.c                           (unchanged)
  application.h/c                  (thin orchestrator: boot, loop shell, shutdown)
  gui/
    app_console.h/c                (move, no changes)
    app_debug_panel.h/c            (move, no changes)
  host/
    event_handler.h/c              (move, no changes)
    app_renderer.h/c               (move, no changes)
    app_output.h/c                 (NEW: extracted output-processing from application.c)
  module/
    realm_app_loader.h/c           (move, no changes)
    realm_app_watcher.h/c          (move, no changes)
    realm_app_module.h/c           (NEW: extracted create/destroy module lifecycle)
```

## Prerequisites

- Both CMakeLists use `GLOB_RECURSE` over `src/*.c`, so new subfolders are auto-discovered. No CMake source list changes needed.
- realm/ currently only has `include/` as a private include dir (line 23-25 of CMakeLists.txt). Adding `src/` as an include dir is required so that `#include "gui/app_console.h"` resolves from any subfolder.

## Verification (after every phase)

```bash
cmake --preset debug && cmake --build --preset debug
```

Then run `bin/Realm.exe` and verify:
- Hot reload works (F5)
- Console toggles (~)
- Debug panel renders
- Backend switch works (F7)
- Scene loads and renders

---

## Phase 0: Add `src/` as CMake Include Directory

**Why:** Without this, files in subfolders like `gui/app_console.c` can't `#include "application.h"` (which lives in `src/`). The editor already does this (line 24 of its CMakeLists).

**Changes:**

In `realm/CMakeLists.txt`, change:
```cmake
target_include_directories(Realm
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
```
to:
```cmake
target_include_directories(Realm
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
```

**Verify:** Build succeeds, no behavioral change.

---

## Phase 1: Move Self-Contained Files to Subfolders

These files have no circular dependencies with `application.h` — they can be moved with only include-path updates.

### Step 1a: Create `src/gui/` and move GUI files

Move:
- `src/app_console.h` -> `src/gui/app_console.h`
- `src/app_console.c` -> `src/gui/app_console.c`
- `src/app_debug_panel.h` -> `src/gui/app_debug_panel.h`
- `src/app_debug_panel.c` -> `src/gui/app_debug_panel.c`

Update includes in:
- `application.h`: `#include "app_console.h"` -> `#include "gui/app_console.h"`, same for `app_debug_panel.h`
- `application.c`: `#include "app_console.h"` -> `#include "gui/app_console.h"` (if present — check; it may come via application.h)

No changes to the moved files themselves — they don't include each other or `application.h`.

### Step 1b: Create `src/module/` and move module files

Move:
- `src/realm_app_loader.h` -> `src/module/realm_app_loader.h`
- `src/realm_app_loader.c` -> `src/module/realm_app_loader.c`
- `src/realm_app_watcher.h` -> `src/module/realm_app_watcher.h`
- `src/realm_app_watcher.c` -> `src/module/realm_app_watcher.c`

Update includes in:
- `application.h`: `#include "realm_app_watcher.h"` -> `#include "module/realm_app_watcher.h"`, `#include "realm_app_loader.h"` -> `#include "module/realm_app_loader.h"`

Check `realm_app_loader.c` — it may include `realm_app_watcher.h` or vice versa. If so, update those cross-references to use the new paths (both are in `module/` so they can use relative `"realm_app_watcher.h"` or qualified `"module/realm_app_watcher.h"`).

**Verify:** Build + run.

---

## Phase 2: Move Coupled Files to `host/`

These files reference `application.h` (or are referenced by it), but only through forward declarations or the struct pointer — no circular include issues.

### Step 2a: Create `src/host/` and move files

Move:
- `src/event_handler.h` -> `src/host/event_handler.h`
- `src/event_handler.c` -> `src/host/event_handler.c`
- `src/app_renderer.h` -> `src/host/app_renderer.h`
- `src/app_renderer.c` -> `src/host/app_renderer.c`

Update includes in:
- `application.h`: `#include "event_handler.h"` -> `#include "host/event_handler.h"`
- `application.c`: `#include "app_renderer.h"` -> `#include "host/app_renderer.h"`, `#include "event_handler.h"` -> `#include "host/event_handler.h"` (check if it's included directly or via application.h)
- `app_renderer.c`: `#include "application.h"` stays as-is (resolves via `src/` include path from Phase 0)

**Verify:** Build + run.

---

## Phase 3: Extract Module Lifecycle -> `module/realm_app_module.h/c`

**What:** The static functions `create_app_module()` and `destroy_app_module()` in `application.c` (lines 214-263) are self-contained module lifecycle helpers. Extract them.

### New file: `src/module/realm_app_module.h`

```c
#pragma once

#include "defines.h"

typedef struct rl_application rl_application;

// Loads the app module, allocates game state, initializes the module.
b8 app_module_create(rl_application *app);

// Shuts down the module, frees game state, unloads the DLL.
void app_module_destroy(rl_application *app);
```

### New file: `src/module/realm_app_module.c`

Move the bodies of `create_app_module()` and `destroy_app_module()` from `application.c`. Change them from operating on the file-static `app` to taking an `rl_application *app` parameter. Required includes:

```c
#include "realm_app_module.h"      // own header (same folder)
#include "application.h"           // rl_application struct
#include "realm_app_loader.h"      // realm_app_module_load etc. (same folder)
#include "core/config.h"
#include "core/logger.h"
#include "core/project.h"
#include "memory/memory.h"
```

### Changes to `application.c`

- Remove `static b8 create_app_module(void)` and `static void destroy_app_module(void)` (lines 214-263)
- Remove the forward declarations (lines 24-25)
- Add `#include "module/realm_app_module.h"`
- Replace `create_app_module()` call (line 83) with `app_module_create(&app)`
- Replace `destroy_app_module()` call (line 198) with `app_module_destroy(&app)`

**Verify:** Build + run. Module init/reload/shutdown must still work.

---

## Phase 4: Extract Output Processing -> `host/app_output.h/c`

**What:** The "Process module requests" block in `application.c` (lines 157-186) is a pure switch-on-flags handler. Extract it.

### New file: `src/host/app_output.h`

```c
#pragma once

#include "defines.h"

typedef struct rl_application rl_application;
typedef struct realm_app_output realm_app_output;

// Applies module output requests (quit, vsync, window mode, backend switch, etc.)
void app_output_process(rl_application *app, const realm_app_output *output);
```

### New file: `src/host/app_output.c`

Move the output processing block. Required includes:

```c
#include "app_output.h"
#include "application.h"
#include "core/config.h"
#include "core/logger.h"
#include "engine.h"
#include "platform/platform.h"
```

The function body is the `// Process module requests` block (lines 157-186), adapted to take `rl_application *app` and `const realm_app_output *output` parameters.

### Changes to `application.c`

- Remove lines 157-186 (the output processing block)
- Add `#include "host/app_output.h"`
- Replace the block with `app_output_process(&app, &module_output);`

**Result:** `application.c` drops from 264 to ~100 lines. The frame loop becomes:
1. Apply cursor state
2. Begin frame
3. Poll watcher / handle reload
4. Update module
5. Render GUI
6. Process output (one function call)
7. End frame / backend switch

**Verify:** Build + run. Test vsync toggle, window mode change, backend switch, quit.

---

## Phase 5 (Optional): Extract Hot-Reload Polling

**What:** The reload/rebuild block in the frame loop (lines 111-141) could move into a `module/realm_app_hot_reload.h/c` helper that encapsulates the poll-rebuild-reload sequence. This is optional because the block is only ~30 lines and already reads cleanly.

If done, the function signature would be something like:
```c
// Returns true if a reload happened (caller may want to log or react).
b8 app_hot_reload_tick(rl_application *app);
```

**Skip this phase** unless the frame loop still feels too busy after P3+P4.

---

## Summary

| Phase | Files touched | New files | Risk |
|-------|--------------|-----------|------|
| P0 | CMakeLists.txt | 0 | None |
| P1 | application.h + moved files' includers | 0 (moves only) | Low — no logic changes |
| P2 | application.h/c + moved files' includers | 0 (moves only) | Low — no logic changes |
| P3 | application.c | realm_app_module.h/c | Medium — must preserve init/shutdown order |
| P4 | application.c | app_output.h/c | Low — pure extraction, no state changes |

Each phase is independently buildable and testable. No behavioral changes in any phase.
