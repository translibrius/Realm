# Realm Editor — Development Roadmap

## Instructions for agents

When starting a session from this roadmap:

1. **Generate a scoped plan first.** Read the next incomplete phase below, then produce a concrete implementation plan (files to create/modify, API sketches, wiring steps) scoped to what can be done in **~50% of your context window**. Do not attempt to entire roadmap in one session — pick the next logical chunk and plan it thoroughly before writing code. Also feel free to interview the user for any questions or clarifications if needed.
2. **Update this file at the end of a successful session.** Check off completed items, add notes on any deferred work or decisions made, and update the dependency graph if needed. This keeps the roadmap accurate for the next agent.

---

## Context

Phases 1–14 are complete. The editor and game host share a project system, scene I/O (JSON via yyjson + binary via custom RLSC format), dynamic asset discovery, a full property inspector with undo/redo, viewport interaction (camera, gizmos, picking, grid), and a polished GUI with themes, focus management, and centralized keyboard routing.

### Current architecture

- **Engine** (`engine/`): public API — renderer, scene/entity/components, assets, config, GUI widgets, math, platform abstraction
- **Realm host** (`realm/`): game executable with hot-reloadable game module DLL, console, event handling
- **Editor** (`realm_editor/`): scene authoring tool with project picker, hierarchy, inspector, asset browser, viewport
- **Game module** (`realm/realm_app_module/`): gameplay code — scene logic, camera, behaviors (hot-reloaded via F5)
- **Project data**: `project.realm` + `assets/` + `scenes/` — shared between editor and game host

### Next priorities

1. **Infrastructure cleanup**: ~~centralized TOML parser~~ ✓, ~~consolidate `game/` project data into `realm/`~~ ✓, ~~behavior system~~ ✓, ~~string utils consolidation~~ ✓
2. **Project scaffolding**: "New Project" generates a buildable game module template
3. **Binary scenes + export pipeline**: ~~custom binary format for fast loading~~ ✓ (19a+19b), ~~editor export for shipping~~ ✓ (19c + Phase 20)
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

### 18a. Project template ✓

- [x] "New Project" creates: `project.realm`, `assets/`, `scenes/default.scene`, `src/game.c`, `src/game.h`, `src/realm_app_api.c`, `CMakeLists.txt`
- [x] Template `game.c`: skeleton with `game_init`/`game_update`/`game_render`/`game_destroy`, one "rotate" behavior
- [x] Template `CMakeLists.txt`: builds `realm_app` shared library, links against Engine
- [x] Template `scenes/default.scene`: single Light entity with ambient/diffuse/specular
- [x] `cstr_sanitize_identifier()` in `str.h`/`str.c` — project name → C/CMake identifier
- [x] `project_template.h`/`project_template.c` — template generation (5 file writers)
- [x] Wired into `project_create()` — non-fatal on failure
- [x] 5 sanitizer tests + 2 template tests (file existence + scene loadable)

### 18b. Build integration

- [ ] Document the workflow: create project in editor → write code in IDE → build module → `Realm --project ./my_project`
- [ ] Editor "Open in Terminal" or "Open in IDE" menu item (optional, nice-to-have)

---

## Interlude: Input, Scroll & Theme Polish ✓

Cross-cutting bugfixes and UX improvements that don't belong to a specific feature phase.

### Input & drag fixes ✓

- [x] **Mouse capture on drag** — `SetCapture`/`ReleaseCapture` in Win32 window proc so drags continue seamlessly outside the OS window, with proper `WM_LBUTTONUP` delivery on release
- [x] **Per-frame mouse state sync** — `GetAsyncKeyState` (Win32), `[NSEvent pressedMouseButtons]` (macOS), `XQueryPointer` (Linux) poll real button state every frame as safety net for missed release events
- [x] **Focus-loss button reset** — all platforms release mouse buttons on window focus loss (`WM_KILLFOCUS`, `windowDidResignKey`, `FocusOut`)
- [x] **Clay scroll drag outside bounds** — patched `Clay_UpdateScrollContainers` to continue tracking active content drags when pointer leaves the scroll element, with force-clear on release
- [x] **Clay swapback iteration fix** — `RemoveSwapback` in scroll container cleanup loop skipped swapped elements; added `i--` to re-check

### Settings & theme ✓

- [x] **Slider widgets in settings** — camera Speed, Sensitivity, FOV all have sliders alongside number inputs, synced bidirectionally
- [x] **Viewport clear color from theme** — added `viewport_bg` to `gui_theme`, `renderer_set_clear_color()` API on frontend + both backends, applied on theme switch
- [x] **6 new themes** — Dracula, Gruvbox, Nord, Tokyo Night, One Dark, Rose Pine (all with full semantic color mapping including viewport bg)
- [x] Theme dropdown expanded to 8 entries, `ed_settings_apply_theme` uses table-driven lookup

---

## Interlude: Theme Architecture & Editor Polish ✓

Semantic theme system overhaul and cross-cutting UI polish.

### Theme surface hierarchy ✓

- [x] **`bg_titlebar` + `bg_elevated` color roles** — added to `gui_theme` for proper visual hierarchy (title bar chrome vs floating surfaces)
- [x] **Menu bar, tab strip, toolbar** now use `bg_titlebar` (darkest chrome) instead of `bg_secondary`
- [x] **Context menus, dropdown lists** now default to `bg_elevated` for floating-surface depth
- [x] **`list_color` on dropdown cfg** — trigger button and floating list can have different backgrounds
- [x] **Tokyo Night** — completely remapped (`bg_secondary` was `#414868` comment color, now `#1f2335` sidebar; border/separator/controls all corrected from canonical palette)
- [x] **Nord** — `text_dim` bumped from nearly-invisible nord3 to readable `{129,140,160}`; `control` differentiated from `bg_secondary` (were identical); `control_hover` bumped above `bg_elevated`
- [x] **Rose Pine** — `control_hover` bumped above `bg_elevated` for visible hover feedback
- [x] All 8 themes verified with correct `bg_titlebar`/`bg_elevated` values from canonical palettes

### Editor UI polish ✓

- [x] **Inspector section headers** — more padding, spacers above/below, white text instead of dim, consistent with settings sections
- [x] **Properties panel** — increased padding for breathing room
- [x] **Slider thumb** — now floats over fill bar; fill extends to thumb center for visual continuity (no gap)
- [x] **Menu bar hover-switch** — click to open first menu, then hover to switch between File/Edit/View (standard menu bar UX)
- [x] **Menu bar item padding** — doubled (4→8px) so items feel interactive
- [x] **Scrollbar auto-hide** — track collapses to 0 width when content doesn't overflow
- [x] **`_trigger_hovered`** exposed on `gui_dropdown_state` for menu bar hover logic

---

## Interlude: Custom Title Bar & Application Icon ✓

Windows-only custom title bar (editor) and application icon infrastructure. macOS/Linux stubs in place, full implementation deferred.

### Platform API additions ✓

- [x] `WINDOW_FLAG_CUSTOM_TITLEBAR` flag (1 << 4)
- [x] `platform_window_minimize/maximize/restore`, `platform_window_is_maximized`
- [x] `platform_titlebar_layout` struct (`height`, `drag_start_x`, `drag_end_x`) + `platform_set_titlebar_layout`
- [x] `host_bootstrap` gains `extra_window_flags` param — editor passes `WINDOW_FLAG_CUSTOM_TITLEBAR`, game host passes `0`
- [x] macOS/Linux stubs (no-op)

### Win32 implementation ✓

- [x] `WM_NCCALCSIZE` — returns 0 to remove non-client area; clamps to work area when maximized
- [x] `WM_NCHITTEST` — resize borders (6px), draggable caption between menus and window buttons, DwmDefWindowProc passthrough
- [x] `DwmExtendFrameIntoClientArea` with `MARGINS {0,0,0,1}` for DWM shadow
- [x] Service thread messages: `MINIMIZE_WINDOW`, `MAXIMIZE_WINDOW`, `RESTORE_WINDOW`
- [x] `WM_NCMOUSEMOVE` → client coords forwarding so Clay hover stays in sync over HTCAPTION
- [x] `WM_MOUSELEAVE` / `WM_NCMOUSELEAVE` tracking for hover cleanup
- [x] Resize handler guard: reject minimized sentinel position (-32000,-32000) and zero-size client rects to prevent config poisoning
- [x] Linked `dwmapi` on Win32

### Editor UI ✓

- [x] Window control buttons (minimize, maximize/restore, close) in menu bar with Lucide icons
- [x] Close button: red hover (#E81123), rounded top-right corner (6px)
- [x] `GUI_ICON_SQUARE` added (Lucide codepoint 57803) for maximize; reuses `MINUS`/`COPY`/`X` for others
- [x] Titlebar layout communication: `Clay_GetElementData` on MenuBarMenus + WindowControls → `platform_set_titlebar_layout`
- [x] `gui_button_cfg` gains `Clay_CornerRadius corners` for per-corner radius control

### Rendering pipeline — per-corner radius ✓

- [x] `vec4 corner_radii` added to `GL_GuiVertex` and `VK_GuiVertex` (48→64 bytes/vert)
- [x] 5th vertex attribute (location 4) in both GL and VK pipeline setup
- [x] `push_rect` takes full `Clay_CornerRadius` instead of collapsing to `topLeft`
- [x] Fragment shaders: per-quadrant radius selection via UV sign, full signed-distance formula (`+ min(max(p.x, p.y), 0.0)` interior term)
- [x] `gui_dropdown_cfg` gains `trigger_corners` for per-corner trigger button rounding
- [x] File menu: `{6,0,0,0}` (top-left), Close button: `{0,6,0,0}` (top-right) — flush with window corners
- [x] `gui_panel_cfg` gains `Clay_Padding pad` for per-side padding (overrides uniform `padding` shorthand)
- [x] Title bar uses zero padding so edge buttons sit flush against window corners

### Application icon ✓

- [x] `assets/icons/realm.ico` — multi-size Windows icon
- [x] `realm_editor/realm_editor.rc` + `realm/realm.rc` — embed icon via resource file
- [x] Window class loads icon from exe module handle (`LoadIconA` + `MAKEINTRESOURCEA(1)`)
- [x] CMake wires `.rc` files for both executables
- [x] `assets/icons/realm.png` — 1024x1024 master PNG (source of truth)
- [x] `assets/icons/realm.icns` — macOS icon via iconutil
- [x] `tools/gen_icons.py` — generates .ico + .icns from master PNG (Pillow + iconutil)
- [x] macOS .app bundles — `MACOSX_BUNDLE` in CMake, .icns embedded in Resources, no icon flash on launch
- [x] `platform_get_executable_dir()` — all 3 platforms, handles .app bundle path resolution
- [x] Application path resolution via `platform_get_executable_dir()` instead of hardcoded relative paths
- [x] `icon` field in `project.realm` + `rl_project.icon_path` — project-specific icon support
- [x] `host_bootstrap` loads project icon if available, falls back to engine icon
- [x] Win32 `platform_set_app_icon` upgraded from no-op to `WM_SETICON` (runtime icon updates for editor project switching)
- [x] Project template generates `icons/` directory + `resource.rc`

### Deferred

- [ ] macOS custom title bar (`NSWindow titlebarAppearsTransparent`, traffic light repositioning)
- [ ] Linux custom title bar (X11 `_MOTIF_WM_HINTS` or client-side decorations)

---

## Phase 19: Binary Scene Format

Custom binary format for fast loading and opaque shipping. Editor always saves JSON (human-readable, diffable). Export/runtime prefers binary.

### 19a. Binary writer ✓

- [x] `scene_save_binary(scene, path)` — custom binary layout with `RLSC` magic + version 1 header
- [x] Component data written as flat u32/f32 values — no parsing overhead on load
- [x] String table with deduplication for entity names, behavior names, asset paths
- [x] Arena-backed write buffer (scratch arena) — zero heap allocations during save

### 19b. Binary reader ✓

- [x] `scene_load_binary(path)` → `rl_scene *`
- [x] Magic bytes detection: `scene_load(path)` auto-detects JSON vs binary (peeks first 4 bytes)
- [x] Version check — rejects incompatible versions with clear error message
- [x] Robust error handling — truncated data at any point returns `nullptr` with descriptive error

### 19b-tests. Unit tests ✓

- [x] 7 new tests added to `test_scene_io.c` (15 total in group, 263 total in suite)
- [x] Binary roundtrip (all 5 component types, exact f32 precision — no float→double→float loss)
- [x] Binary empty scene, multiple entities with mixed components
- [x] Auto-detect: `scene_load` dispatches correctly for both binary and JSON files
- [x] Version mismatch rejection (crafted bad header)
- [x] Null argument handling
- [x] String deduplication (two entities with same name share one string table entry)

### 19c. Wire into pipeline ✓

- [x] Editor saves `.scene` as JSON (authoring format, unchanged)
- [x] Export step converts `.scene` → `.scene.bin` (Phase 20)
- [x] Game host prefers `.scene.bin` if present, falls back to `.scene` (`realm/src/application.c`)

### Format reference

```
[Header — 16 bytes]
  magic: u8[4] = 'R','L','S','C'   version: u32   entity_count: u32   string_count: u32
[String table] per string: { u32 len, u8[len] chars }
[Scene name]   name_str_idx: u32
[Entities]     per entity: comp_mask: u32, then conditional component data
  bit 0: name (u32 str_idx)
  bit 1: transform (f32[9] — pos/rot/scale)
  bit 2: mesh (u32 prim/kind/wire, u32 mesh_asset_idx, u32 diffuse_idx, f32[3] specular, f32 shininess)
  bit 3: light (f32[9] — ambient/diffuse/specular)
  bit 4: behavior (u32 str_idx)
```

---

## Phase 20: Build/Export Pipeline ✓

Editor "Export" button that produces a standalone game directory — cooked scenes, copied assets, ready to ship alongside a Realm executable + game module DLL.

### 20a. Export function ✓

- [x] `project_export(project, output_path)` in `engine/include/core/project_export.h` + `engine/src/core/project_export.c`
- [x] Cooks all scenes: JSON → binary (scans `scenes_path` for `.scene` files, `scene_load` → `scene_save_binary`)
- [x] Copies asset directory recursively (`copy_dir_recursive` using `platform_dir_scan` + `platform_file_copy`)
- [x] Rewrites `project.realm` with `default_scene` pointing to `.scene.bin`
- [x] Output: flat directory ready to zip/distribute
- [x] 6 unit tests: output structure, RLSC magic verification, data roundtrip, asset copy, project file rewrite, null args
- [x] Tests CMakeLists switched to `file(GLOB)` — new test files picked up automatically

### 20b. Editor wiring ✓

- [x] File menu → "Export Project..." (between "Close Project" and "Quit")
- [x] `gui_file_browser` in `GUI_FILE_BROWSER_DIRECTORY` mode for output path selection
- [x] `export_requested` flag + `export_browser` state on `ed_application`
- [x] Progress/error feedback via `RL_INFO`/`RL_ERROR` log messages (visible in editor console)
- [x] Error handling: validates inputs, logs specific failure at each step, returns false on first error

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
- Custom title bar on macOS/Linux (stubs exist, implementation deferred)
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
            ├── Phase 19: Binary Scene Format        ✓ done (19a+19b+19c)
            │       │
            │       └── Phase 20: Export Pipeline    ✓ done
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
- File browser widget (`gui_file_browser`) is wired for export (directory picker) but not yet for project picker's Browse button (native dialog preferred long-term).
- File browser has inline "New Folder" creation (nav bar button, Enter/Escape, auto-select after create). Mkdir logic is duplicated between Enter key handler and button click — could extract into a helper.
- Dropdown `min_width` field added for auto-fit menus — used by editor menu bar (140px floor).
