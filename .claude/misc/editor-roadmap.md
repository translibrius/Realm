# Realm Editor — Development Roadmap

## Instructions for agents

When starting a session from this roadmap:

1. **Generate a scoped plan first.** Read the next incomplete phase below, then produce a concrete implementation plan (files to create/modify, API sketches, wiring steps) scoped to what can be done in **~50% of your context window**. Do not attempt the entire roadmap in one session — pick the next logical chunk and plan it thoroughly before writing code. Also feel free to interview the user for any questions or clarifications if needed.
2. **Update this file at the end of a successful session.** Check off completed items, add notes on any deferred work or decisions made, and update the dependency graph if needed. This keeps the roadmap accurate for the next agent.

---

## Context

### Current architecture

- **Engine** (`engine/`): public API — renderer, scene/entity/components, assets, config, GUI widgets, math, platform abstraction
- **Realm host** (`realm/`): game executable with hot-reloadable game module DLL, console, event handling
- **Editor** (`realm_editor/`): scene authoring tool with project picker, hierarchy, inspector, asset browser, viewport
- **Game module** (`realm/realm_app_module/`): gameplay code — scene logic, camera, behaviors (hot-reloaded via F5)
- **Project data**: `project.realm` + `assets/` + `scenes/` — shared between editor and game host

---

## Completed Phases (1–20)

- **Phase 1:** Shared host utilities — deduplicated console, renderer, events, bootstrap into `engine/src/host/`
- **Phase 2:** GUI widgets — tree view, splitter, context menu, menu bar
- **Phase 3:** Scene/entity system — entity handles (20-bit index + 12-bit gen), parallel-array components, scene container
- **Phase 4:** Project system & config split — editor/game configs, asset system split, `project.realm`, project picker UI
- **Phase 5:** Scene I/O — JSON via yyjson, save/load wired into editor and game host
- **Phase 6:** Dynamic asset discovery — platform directory scanning, asset browser panel with file drop
- **Phase 7:** Game host integration — CLI `--project` arg, migrated game content
- **Phase 8:** Property inspector + undo — editable transform/mesh/material/light, 256-entry ring buffer undo
- **Phase 9:** Viewport interaction — editor camera (orbit/fly), axis gizmo, translate/rotate/scale gizmos (W/E/R), ray-AABB picking, infinite grid
- **Phase 10:** Editor polish & themes — theme compliance, light visualization, settings tab with theme dropdown
- **Phase 11:** GUI polish — FPS overlay, toolbar, context menus, expanded settings, inspector headers
- **Phase 12:** Renderer visual polish — MSDF text weight, SDF rounded corners
- **Phase 13:** GUI focus system — global focus, input focus-aware blink, console gating, scrollbar drag
- **Phase 14:** GUI architecture refactor — centralized keyboard routing (`gui_input`), widget lifecycle, main loop cleanup
- **Phase 15:** Centralized TOML parser — 3 duplicate parsers replaced with shared `util/toml.h`
- **Phase 16:** Project consolidation — moved `game/` into `realm/`
- **Phase 17:** Entity behavior system — name-addressed update functions, behavior component, scene I/O, inspector
- **Phase 18a:** Project scaffolding — "New Project" generates buildable game module template (18b docs/IDE integration remaining)
- **Phase 19:** Binary scene format — custom RLSC format, auto-detect JSON vs binary on load
- **Phase 20:** Export pipeline — editor "Export" produces standalone game directory with cooked scenes

### Completed interludes

- **Arena alignment fix** — `rl_arena_push_array(arena, T, count, zero)` macro that derives alignment from `_Alignof(T)`. Fixed release-only crash caused by `-march=native` (AVX) requiring 32-byte alignment for `mat4`-containing structs, while arena allocs hardcoded 16. Migrated all `rl_frame_mesh` and `rl_transform` allocations (component store, scene frame data, vulkan renderer, editor gizmos).
- **Input, scroll & theme polish** — mouse capture on drag (all platforms), Clay scroll fixes, slider widgets, viewport clear color from theme, 6 new themes (8 total)
- **Theme architecture & editor polish** — `bg_titlebar`/`bg_elevated` surface hierarchy, theme corrections (Tokyo Night, Nord, Rose Pine), slider/menu bar/scrollbar UX improvements
- **Custom title bar & app icon** — Win32 custom title bar + window controls, per-corner radius rendering, app icon infrastructure (macOS .icns + Win32 .ico), `platform_get_executable_dir()`. macOS/Linux custom title bar deferred.
- **Vulkan backend parity** — per-mesh texture binding (placeholder texture, lazy upload, per-texture descriptor sets), imported mesh rendering (mesh cache, staging buffer upload, per-mesh draw), dedicated overlay UBO for gizmo pass
- **Grid & camera prep** — adaptive multi-level grid shader, camera near/far clip fields, mesh AABB caching at load time
- **Profiler editor wiring** — moved profiler init/shutdown/frame_mark into engine, editor CMake profiler support, F3/F4 hotkeys, settings panel profiler toggle

### Binary scene format reference

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
  bit 5: camera (f32 fov, f32 near, f32 far, u32 is_main)
```

---

## Phase 18b: Project Scaffolding — Remaining

- [ ] Document the workflow: create project in editor -> write code in IDE -> build module -> `Realm --project ./my_project`
- [ ] Editor "Open in Terminal" or "Open in IDE" menu item (optional, nice-to-have)

---

## Phase 21: Asset Drag-and-Drop & Entity Highlighting

Rich asset interaction: thumbnail previews in the asset browser, drag-and-drop assets into the scene or onto entities, and hover highlight for entity feedback.

### 21a. Asset browser thumbnails

- [ ] Generate preview thumbnails for known asset types (textures: downscaled; meshes: rendered preview; materials: swatch)
- [ ] Thumbnail cache in `.realm_cache/thumbnails/` keyed by asset path + mtime
- [ ] Grid/list view toggle — grid shows thumbnail + filename, list shows icon + name + type
- [ ] Lazy loading — generate on first view or background, placeholder until ready

### 21b. Drag-and-drop assets into scene

- [ ] Drag source: asset browser items — drag on mouse-down + move, floating thumbnail under cursor
- [ ] Drop target: viewport empty space — creates new entity with appropriate components
- [ ] Drop target: entity in viewport or hierarchy — applies asset contextually (texture -> diffuse map, mesh -> replace, behavior -> assign)
- [ ] Visual feedback during drag: valid/invalid drop zone indication
- [ ] Undo support for all drop actions

### 21c. Entity highlight on hover — picking infrastructure done, outline rendering reverted

Hover picking infrastructure is complete. Stencil-based outline rendering was implemented but macOS OpenGL's deprecated driver silently drops all stencil writes. The stencil outline code was fully removed pending a stencil-free technique (post-process edge detection, jump flood, or similar).

**Kept (picking infrastructure):**
- [x] Hover picking via `EVENT_MOUSE_MOVE` within viewport bounds
- [x] Imported mesh AABB picking — `aabb_from_mesh_asset()` uses actual vertex bounds
- [x] `source_entity` field on `rl_frame_mesh` for highlight matching
- [x] `hovered_entity` on editor app state

**Next:** choose a stencil-free outline technique and re-implement for both backends

### 21d. Inspector drop targets

- [ ] Texture fields become drop targets — drag texture onto "Diffuse" field
- [ ] Behavior field becomes dropdown from `behavior_registry`, or accepts drag-and-drop
- [ ] Mesh asset field becomes a drop target
- [ ] Preview thumbnail shown inline next to texture/mesh fields when assigned

---

## Phase 22: Camera Component & Visualization

### 22a. Camera component ✓

- [x] `rl_camera_component` — fov, near_clip, far_clip, is_main
- [x] Scene I/O (JSON + binary), inspector section with FOV slider, undo support
- [x] `scene_get_main_camera(scene)` helper

### 22b. Game module migration ✓

- [x] `camera_from_entity` / `camera_sync_to_transform` helpers
- [x] Game module reads/syncs camera from scene entity, falls back to `camera_init`
- [x] Existing scenes + project template updated with Camera entity

### 22c. Frustum visualization

- [ ] Wireframe frustum for camera entities in editor viewport (like light visualization)
- [ ] Only draw when camera entity is selected or hovered
- [ ] Frustum lines from camera component FOV/near/far + entity transform

### 22d. Camera preview (deferred — needs offscreen render targets)

- [ ] Picture-in-picture preview of selected camera's POV
- [ ] Requires offscreen FBO/render target infrastructure

---

## Future (not scoped)

### Editor features
- Custom title bar on macOS/Linux (stubs exist)
- Native file/directory picker dialog (NSOpenPanel, IFileOpenDialog, GTK/portal)
- Multi-select, copy/paste entities
- Transform hierarchy (parent/child relationships)
- Multiple viewports (offscreen render targets — shared infra with 22d)
- Play mode — launch game module in editor (F5 = play, Esc = stop) — depends on Phase 22
- Snap-to-grid
- Prefab system
- Material system / material editor
- Live sync — editor watches `.scene` files, game watches for editor saves

### Scripting runtime
- Language TBD — candidates: Wren, QuickJS, daScript, C# via Mono, or compile-to-C (Nim, Zig, Odin)
- Hooks into behavior system (Phase 17) — script functions register as behaviors
- Decision criteria: embedding complexity, iteration speed, community/docs, performance

### Shipping
- Custom-named executable (`MyGame.exe` instead of `Realm.exe`)
- Asset packing (single archive instead of loose directory)
- Asset compression
- Standalone redistributable (engine runtime + game module + cooked assets)

---

## Dependency Graph

```
Phase 1–20: Foundation + infrastructure       all done
    │
    ├── Phase 18b: Project Scaffolding docs    ○ remaining (18a template done)
    │
    ├── Phase 21: Asset Drag-Drop + Highlight  ◐ 21c picking done, 21a/21b/21d remaining
    │                                            outline rendering needs stencil-free technique
    │
    ├── Phase 22: Camera Component             ◐ 22a+22b done, 22c remaining, 22d deferred
    │       │
    │       └── Future: Play Mode              ○ depends on 22
    │
    └── Future: Scripting Runtime              ○ depends on 17 (behavior seam)
```

Phases 18b, 21, 22c are independent and can be done in any order.

---

## Loose Ends

- Entity create/destroy undo action types exist but recreation logic isn't wired. Context menu delete works but doesn't push undo entries.
- File browser wired for export (directory picker) but not for project picker's Browse button (native dialog preferred long-term).
- File browser "New Folder" mkdir logic duplicated between Enter key handler and button click — could extract helper.
