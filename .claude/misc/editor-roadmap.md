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

1. **Infrastructure cleanup**: ~~centralized TOML parser~~ ✓, ~~consolidate `game/` project data into `realm/`~~ ✓, ~~behavior system~~ ✓, string utils consolidation (see `.claude/misc/string-utils-refactor.md`)
2. **Project scaffolding**: "New Project" generates a buildable game module template
3. **Binary scenes + export pipeline**: custom binary format for fast loading, editor export for shipping
4. **Asset drag-and-drop + entity highlighting**: thumbnail previews, drag assets into scene/onto entities, hover outline shader

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

## Phase 17: Entity Behavior System ✓

The architectural seam that enables future scripting. Behaviors are name-addressed update functions attached to entities as a component. Today they resolve to C function pointers; tomorrow they could resolve to script functions in any language.

### 17a. Behavior registry ✓

- [x] Created `engine/include/core/behavior.h` + `engine/src/core/behavior.c`
- [x] Static registry (64 entries max), `behavior_register(name, fn)` / `behavior_find(name)` / `behavior_registry_clear()`
- [x] `behavior_registry_init()` called at engine startup, `behavior_registry_clear()` on hot-reload
- [x] Added `cstr_eq()` to `str.h`/`str.c` (was missing, needed for registry lookups)
- [x] 5 unit tests: register+find, find-missing, clear, overwrite-duplicate, capacity

### 17b. Behavior component ✓

- [x] `rl_behavior_component` (stores `char name[64]`) added to `rl_component_store`
- [x] `behavior_comp_add/get/remove` accessors (prefixed `_comp_` to avoid collision with registry functions)
- [x] `behavior_update_all(scene, dt)` iterates entities, resolves name→fn, calls it
- [x] Unknown behavior names logged once (static warned-name set, cleared on registry clear)
- [x] `scene_entity_destroy` cleans up behavior component

### 17c. Scene I/O integration ✓

- [x] Serialize `"behavior": "rotate"` as plain string in entity JSON
- [x] Deserialize and attach behavior component on load
- [x] Roundtrip test added to `test_scene_io.c`

### 17d. Migrate sandbox game + editor ✓

- [x] `rotate_update` behavior in `scene_game.c` — adds 100*dt to `rotation[1]`, wraps at 360
- [x] `scene_game_register_behaviors()` called from `game_init()` after `behavior_registry_clear()`
- [x] `scene_game_update` calls `behavior_update_all` (respects pause/freeze logic)
- [x] Removed `scene_angle` and `rotating_cube_entity` from `rl_game`, bumped `RL_GAME_STATE_VERSION` to 11
- [x] `gameplay.scene` has `"behavior": "rotate"` on RotatingCube entity
- [x] Editor inspector shows "Behavior" section with read-only name when entity has behavior component

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

## Phase 21: Asset Drag-and-Drop & Entity Highlighting

Rich asset interaction: thumbnail previews in the asset browser, drag-and-drop assets into the scene or onto entities, and a hover highlight shader for entity feedback.

### 21a. Asset browser thumbnails

- [ ] Generate preview thumbnails for known asset types (textures: downscaled GPU read-back or CPU resize; meshes: rendered preview to FBO; materials: swatch)
- [ ] Thumbnail cache — store on disk in `.realm_cache/thumbnails/` alongside project, keyed by asset path + mtime
- [ ] Asset browser grid/list view toggle — grid shows thumbnail + filename, list shows icon + name + type
- [ ] Lazy loading — generate thumbnails on first view or in background, show placeholder until ready

### 21b. Drag-and-drop assets into scene

- [ ] Drag source: asset browser items — initiate drag on mouse-down + move, show floating thumbnail under cursor
- [ ] Drop target: viewport empty space — creates new entity with appropriate components (mesh asset → entity with transform + mesh, texture → entity with transform + mesh + applied material)
- [ ] Drop target: entity in viewport or hierarchy — applies asset contextually (texture → sets diffuse map on existing mesh, mesh → replaces mesh asset, behavior name → assigns behavior component)
- [ ] Visual feedback during drag: valid/invalid drop zone indication
- [ ] Undo support for all drop actions

### 21c. Entity highlight on hover (outline shader)

- [ ] Outline/silhouette shader — render selected/hovered entity to stencil buffer, then draw expanded outline in a neon/highlight color
- [ ] Two modes: **hover** (subtle, e.g. thin white/blue outline) and **selected** (stronger, e.g. bright orange/yellow)
- [ ] Works in both OpenGL and Vulkan backends — stencil-based approach for portability
- [ ] Configurable highlight color (per-theme or user setting)
- [ ] Performance: only re-render the highlighted entity to stencil, not the whole scene

### 21d. Inspector drop targets

- [ ] Texture fields in material section become drop targets — drag texture from asset browser onto "Diffuse" field
- [ ] Behavior field becomes a dropdown populated from `behavior_registry` (registered names), or accepts drag-and-drop of a behavior name
- [ ] Mesh asset field becomes a drop target — drag mesh from asset browser to swap mesh
- [ ] Preview thumbnail shown inline next to texture/mesh fields when assigned

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
    └── Phase 17: Entity Behavior System     ✓ done
            │
            ├── Phase 18: Project Scaffolding        ○ depends on 16 + 17
            │
            ├── Phase 19: Binary Scene Format        ○ depends on 17
            │       │
            │       └── Phase 20: Export Pipeline    ○ depends on 19
            │
            ├── Phase 21: Asset Drag-Drop + Highlight  ○ depends on 17 (behavior drop) + renderer (outline shader)
            │
            └── Future: Scripting Runtime            ○ depends on 17 (behavior seam)
```

Phases 18, 19, 21 are independent of each other and can be done in any order.
Phase 21 is renderer-heavy (outline shader) and GUI-heavy (drag-and-drop) — good candidate for splitting across sessions.

---

## Loose Ends

- Entity create/destroy undo action types exist but the recreation logic for undo isn't wired yet. Context menu delete works but doesn't push undo entries.
- File browser widget (`gui_file_browser`) exists but isn't wired into project picker's Browse button yet (native dialog preferred long-term).
