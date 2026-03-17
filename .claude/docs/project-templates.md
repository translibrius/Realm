# Project Templates

## Overview

When the editor's "New Project" creates a project, `project_create()` (in `engine/src/core/project.c`) generates the directory structure, writes `project.realm`, and then calls `project_template_generate()` to scaffold a buildable game module from day one.

## Key files

| File | Purpose |
|------|---------|
| `engine/include/core/project_template.h` | Public API: `project_template_generate(root, project_name)` |
| `engine/src/core/project_template.c` | Template generation — 5 static writers |
| `engine/src/core/project.c` | Calls `project_template_generate()` after writing `project.realm` |
| `engine/include/util/str.h` | `cstr_sanitize_identifier()` — project name → C/CMake identifier |

## Generated files

`project_template_generate()` creates these inside the project root:

| File | Writer | Content |
|------|--------|---------|
| `scenes/default.scene` | `write_default_scene()` | JSON scene with 1 Light entity |
| `src/game.h` | `write_game_h()` | Minimal `rl_game` struct + function declarations |
| `src/game.c` | `write_game_c()` | `game_init`/`update`/`render`/`destroy` + "rotate" behavior |
| `src/realm_app_api.c` | `write_api_c()` | 7 DLL export wrappers (same pattern as `realm/realm_app_module/`) |
| `CMakeLists.txt` | `write_cmake()` | Builds `realm_app` SHARED library, links Engine |

## When to update templates

**Any change to the game module API or engine public headers that affects game code must be reflected in the templates.** Specifically:

- **`realm_app_api.h` changes** (new exports, signature changes) → update `write_api_c()`
- **`realm_app_context` or `realm_app_output` field changes** → update `write_game_c()` if template uses affected fields
- **Camera/scene/behavior API changes** → update `write_game_c()` and `write_game_h()` includes
- **Component system changes** (new component types, changed accessors) → update `write_default_scene()` if it uses affected components
- **Build system changes** (new compile definitions, link targets, CMake functions) → update `write_cmake()`
- **New engine headers that game modules commonly need** → consider adding to template includes

## Template content guidelines

- Templates mirror `realm/realm_app_module/` patterns — same includes, same API wrappers, same coding style
- Keep templates minimal but functional — a new project should compile and show a lit scene
- The "rotate" behavior in template `game.c` demonstrates the behavior system; keep it as the example
- Scene JSON format must match what `scene_load()` / `scene_save()` produce (see `engine/src/core/scene_io.c`)

## Testing

- `test_project.c` → `project_create_generates_template_files`: verifies all 5 template files exist
- `test_project.c` → `project_create_scene_is_loadable`: loads generated `default.scene`, checks scene name ("Default Scene"), entity count (1), entity name ("Light"), and light component values
- `test_str.c` → 5 tests for `cstr_sanitize_identifier` (basic, leading digit, all invalid, empty, already valid)

Run: `bin/RealmTests --filter project` and `bin/RealmTests --filter str`
