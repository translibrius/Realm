# Realm Editor — Development Roadmap

## Instructions for agents

When starting a session from this roadmap:

1. **Generate a scoped plan first.** Read the next incomplete phase below, then produce a concrete implementation plan (files to create/modify, API sketches, wiring steps) scoped to what can be done in **~50% of your context window**. Do not attempt to entire roadmap in one session — pick the next logical chunk and plan it thoroughly before writing code. Also feel free to interview the user for any questions or clarifications if needed.
2. **Update this file at the end of a successful session.** Check off completed items, add notes on any deferred work or decisions made, and update the dependency graph if needed. This keeps the roadmap accurate for the next agent.

---

## Context

Phases 1–14 are complete. The editor and game host share a project system, scene I/O (JSON via yyjson), dynamic asset discovery, a full property inspector with undo/redo, viewport interaction (camera, gizmos, picking, grid), and a polished GUI with themes, focus management, and centralized keyboard routing.

### Current architecture

- **Engine** (`engine/`): public API — renderer, scene/entity/components, assets, config, GUI widgets, math, platform abstraction
- **Realm host** (`realm/`): game executable with hot-reloadable game module DLL, console, event handling
- **Editor** (`realm_editor/`): scene authoring tool with project picker, hierarchy, inspector, asset browser, viewport
- **Game module** (`realm/realm_app_module/`): gameplay code — scene logic, camera, behaviors (hot-reloaded via F5)
- **Project data**: `project.realm` + `assets/` + `scenes/` — shared between editor and game host

### Next priorities

1. **Infrastructure cleanup**: ~~centralized TOML parser~~ ✓, ~~consolidate `game/` project data into `realm/`~~ ✓, string utils consolidation (see `.claude/misc/string-utils-refactor.md`)
2. **Behavior system**: name-addressed entity update functions — the architectural seam for future scripting language support
3. **Project scaffolding**: "New Project" generates a buildable game module template
4. **Binary scenes + export pipeline**: custom binary format for fast loading, editor export for shipping

---

## Completed Phases (1–14)

- **Phase 1: Shared Host Utilities** — Deduplicated console, renderer, events, bootstrap into `engine/src/host/`
- **Phase 2: GUI Widgets** — Tree view, splitter, context menu, menu bar wiring
- **Phase 3: Scene/Entity System** — Entity handles (20-bit index + 12-bit gen), parallel-array components, scene container, `scene_build_frame_data()`
- **Phase 4: Project System & Config Split** — Separate editor/game configs, asset system split (engine vs content), `project.realm` + directory structure, project picker UI
- **Phase 5: Scene I/O** — JSON via yyjson, `scene_save`/`scene_load`, wired into editor and game host
- **Phase 6: Dynamic Asset Discovery** — Platform directory scanning, `project_load_assets()`, asset browser panel with file drop
- **Phase 7: Game Host Integration** — CLI `--project` arg, migrated game content to `game/` project, removed legacy asset tables
- **Phase 8: Editable Properties + Undo** — Property inspector (transform/mesh/material/light), number input widget, 256-entry ring buffer undo/redo
- **Phase 9: Viewport Interaction** — Editor camera (orbit/fly/scroll), constrained 3D viewport, tab bar, origin axis gizmo, translate/rotate/scale gizmos (W/E/R), CPU ray-AABB entity picking, infinite grid
- **Phase 10: Editor Polish & Theme System** — Theme compliance across all widgets, light entity visualization, asset browser double-click, settings tab with theme dropdown
- **Phase 11: Editor GUI Polish** — FPS overlay, tree arrow triangles, toolbar, context menus (hierarchy + viewport), expanded settings (camera speed/sensitivity/FOV), inspector section headers
- **Phase 12: Renderer Visual Polish** — MSDF text weight uniform, SDF rounded corners, menu bar divider
- **Phase 13: GUI Focus System & Interaction Fixes** — Global `gui_focus` system, text/number input focus-aware blink, console focus gating, scrollbar drag, settings config sync
- **Phase 14: GUI Architecture Refactor** — Centralized keyboard routing (`gui_input`), widget lifecycle standardization via `gui_focus`, main loop cleanup (~180→~50 lines), pixel-rect cursor overlay

---

## Phase 15: Centralized TOML Parser ✓

Three independent hand-rolled TOML parsers existed (`config.c`, `project.c`, `ed_config.c`) doing identical line-by-line parsing. Replaced with a single reusable parser in the engine utility layer.

### 15a. TOML parser implementation ✓

- [x] Created `engine/include/util/toml.h` + `engine/src/util/toml.c`
- [x] Flat section+key→value design: `toml_parse(text, len)` / `toml_parse_file(path)` → queryable table
- [x] `toml_get_string`, `toml_get_int`, `toml_get_float`, `toml_get_bool` — all with explicit fallback parameter
- [x] Table backed by own arena (`rl_arena_create`), `toml_free` destroys arena
- [x] Handles: `[section]` headers, `key = value`, quoted strings, booleans, ints, floats, inline `#` comments
- [x] Also: `toml_has_key`, `toml_entry_count`
- [x] 13 unit tests covering all types, sections, comments, whitespace, fallbacks, edge cases, file I/O

### 15b. Migrate consumers ✓

- [x] Rewrote `config.c` — deleted ~150 lines of inline parser, replaced with ~25 lines
- [x] Rewrote `project.c` — deleted ~75 lines of inline parser, replaced with ~4 lines
- [x] Rewrote `ed_config.c` — deleted ~90 lines of inline parser, replaced with ~18 lines
- [x] All 165 tests pass (including existing `test_config`, `test_project`)
- [x] `project.realm` remains TOML format (same file, cleaner parser)

---

## Phase 16: Project Consolidation ✓

Moved `game/` project data into `realm/`, making it a self-contained project directory. Fixed `project.realm` to use `[project]` section header (was using flat keys, falling back to defaults). Added `--project` args to CMake launch target and Zed debug configs.

### 16a. Move project data into realm/ ✓

- [x] Move `game/project.realm` → `realm/project.realm` (added `[project]` section header)
- [x] Move `game/assets/` → `realm/assets/` (textures, models, materials)
- [x] Move `game/scenes/` → `realm/scenes/`
- [x] Delete `game/` directory
- [x] Update `--project` default path / launch configs to point at `realm/`
- [x] `.gitignore` — no changes needed (no `game/`-specific entries)
- [x] Verify: `Realm --project realm/` loads sandbox game correctly

### 16b. Clean up references ✓

- [x] Updated CMake `run_realm_checked` targets with `--project` arg
- [x] Updated `.zed/debug.json.in` Debug Realm and Profile Realm entries with `--project` arg
- [x] No hardcoded `game/` paths in C code — project path is always passed at runtime
- [x] Editor recent projects stored in `editor_state.toml` at runtime — auto-updates on next open

---

## Phase 17: Entity Behavior System

The architectural seam that enables future scripting. Behaviors are name-addressed update functions attached to entities as a component. Today they resolve to C function pointers; tomorrow they could resolve to script functions in any language.

### Design

```c
// A behavior is a named update function
typedef void (*behavior_fn)(rl_scene *scene, rl_entity entity, f32 dt);

// Registration (game module does this at init)
behavior_register("patrol", patrol_update);
behavior_register("rotate", rotate_update);

// Attachment (scene file or editor assigns these)
behavior_set(scene, entity, "patrol");

// Frame update (host calls once per frame)
behavior_update_all(scene, dt);
```

### 17a. Behavior registry

- [ ] Create `engine/include/core/behavior.h` + `engine/src/core/behavior.c`
- [ ] `behavior_registry` — name→function_pointer table (fixed capacity, string-keyed)
- [ ] `behavior_register(name, fn)` / `behavior_find(name)` → `behavior_fn` or NULL
- [ ] `behavior_registry_clear()` for hot-reload (re-register after module load)
- [ ] Unit tests: register, find, find-missing, clear

### 17b. Behavior component

- [ ] Add `rl_behavior_component` to component system — stores behavior name (`char[64]`)
- [ ] `behavior_set(scene, entity, name)` / `behavior_get(scene, entity)` → name
- [ ] `behavior_update_all(scene, dt)` — iterates entities with behavior component, looks up function, calls it
- [ ] Graceful no-op if behavior name not found in registry (log warning once)

### 17c. Scene I/O integration

- [ ] Serialize `"behavior": "rotate"` in scene JSON
- [ ] Deserialize and attach behavior component on load
- [ ] Editor inspector shows behavior name as read-only text (or dropdown of registered names)

### 17d. Migrate sandbox game

- [ ] Convert rotating cube logic in `scene_game.c` to a `"rotate"` behavior
- [ ] Register behaviors in game module `game_init()`
- [ ] Re-register in hot-reload path so F5 works
- [ ] Verify: sandbox game works identically, behaviors survive hot-reload

---

## Phase 18: Project Scaffolding

Editor "New Project" should generate a buildable game module, not just empty directories. Users get a working starting point.

### 18a. Project template

- [ ] "New Project" creates: `project.realm`, `assets/`, `scenes/default.scene`, `src/game.c`, `CMakeLists.txt`
- [ ] Template `game.c`: skeleton with `game_init`/`game_update`/`game_shutdown`, one example behavior registration
- [ ] Template `CMakeLists.txt`: builds game module DLL, links against engine headers
- [ ] Template `scenes/default.scene`: single light + camera setup

### 18b. Build integration

- [ ] Document the workflow: create project in editor → write code in IDE → build module → `Realm --project ./my_project`
- [ ] Editor "Open in Terminal" or "Open in IDE" menu item (optional, nice-to-have)

---

## Phase 19: Binary Scene Format

Custom binary format for fast loading and opaque shipping. Editor always saves JSON (human-readable, diffable). Export/runtime prefers binary.

### 19a. Binary writer

- [ ] `scene_save_binary(scene, path)` — custom binary layout with magic bytes + version header
- [ ] Component data written as flat structs — no parsing overhead on load
- [ ] String table for entity names, behavior names, asset paths

### 19b. Binary reader

- [ ] `scene_load_binary(path)` → `rl_scene *`
- [ ] Magic bytes detection: `scene_load(path)` auto-detects JSON vs binary
- [ ] Version check — reject incompatible versions with clear error

### 19c. Wire into pipeline

- [ ] Editor saves `.scene` as JSON (authoring format, unchanged)
- [ ] Export step converts `.scene` → `.scene.bin` (Phase 20)
- [ ] Game host prefers `.scene.bin` if present, falls back to `.scene`
- [ ] Unit tests: round-trip binary save/load, format detection, version mismatch handling

---

## Phase 20: Build/Export Pipeline (skeleton)

Editor "Export" button that produces a standalone game directory — cooked scenes, copied assets, ready to ship alongside a Realm executable + game module DLL.

### 20a. Export function

- [ ] `project_export(project, output_path)` in engine
- [ ] Cooks all scenes: JSON → binary
- [ ] Copies asset directory
- [ ] Copies `project.realm` (with paths adjusted for export layout)
- [ ] Output: flat directory ready to zip/distribute

### 20b. Editor wiring

- [ ] File menu → "Export Project..." → path input → export
- [ ] Progress feedback (log messages or simple status text)
- [ ] Error handling: missing assets, write failures

---

## Future (not scoped)

### Editor features
- Native file/directory picker dialog (`NSOpenPanel` on macOS, `IFileOpenDialog` on Win32, GTK/portal on Linux)
- Multi-select, copy/paste entities
- Transform hierarchy (parent/child relationships)
- Multiple viewports (offscreen render targets)
- Play mode — launch game module inside editor with project path (F5 = play, Esc = stop)
- Snap-to-grid
- Prefab system
- Material system / material editor
- Live sync — editor watches `.scene` files for external changes, game watches for editor saves

### Scripting runtime
- Language TBD — candidates: Wren (purpose-built for games), QuickJS (everyone knows JS), daScript (AAA-proven, fast), C# via Mono (Unity-proven), or compile-to-C languages (Nim, Zig, Odin) that leverage existing hot-reload
- Key property: fast iteration (interpreted/JIT or fast-compile + hot-reload)
- Hooks into behavior system (Phase 17) — script functions register as behaviors, same interface as C
- Script component on entities, editor shows script fields
- Decision criteria: embedding complexity, iteration speed, community/docs, performance

### Shipping
- Custom-named executable (build step produces `MyGame.exe` instead of `Realm.exe`)
- Asset packing (single archive file instead of loose directory)
- Asset compression
- Standalone redistributable (bundle engine runtime + game module + cooked assets)

---

## Dependency Graph

```
Phase 1–14: Foundation                       ✓ all done
    │
    ├── Phase 15: Centralized TOML Parser    ✓ done
    │
    ├── Phase 16: Project Consolidation      ✓ done
    │
    └── Phase 17: Entity Behavior System     ○ depends on scene I/O (done)
            │
            ├── Phase 18: Project Scaffolding        ○ depends on 16 + 17
            │
            ├── Phase 19: Binary Scene Format        ○ depends on 17 (behaviors in scene I/O)
            │       │
            │       └── Phase 20: Export Pipeline    ○ depends on 19
            │
            └── Future: Scripting Runtime            ○ depends on 17 (behavior seam)
```

Phases 15 and 16 are independent and can be done in either order or in parallel.
Phase 17 is the critical path — it's the architectural seam for everything that follows.

---

## Loose Ends

- Entity create/destroy undo action types exist but the recreation logic for undo isn't wired yet. Context menu delete works but doesn't push undo entries.
- File browser widget (`gui_file_browser`) exists but isn't wired into project picker's Browse button yet (native dialog preferred long-term).
