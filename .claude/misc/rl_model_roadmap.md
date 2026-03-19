# rl_model: Full Model Abstraction for Realm Engine

## Context

The engine's cgltf importer correctly loads all primitives and materials from glTF files into `rl_mesh`, but both GL and Vulkan backends only render `primitives[0]`. Node transforms from the glTF hierarchy are also discarded. The current `rl_mesh` is conceptually a "model" (multiple primitives + materials) but isn't named or treated as one. This plan introduces a proper `rl_model` asset type, reworks the import pipeline to preserve node transforms (baked into vertices), and wires the new type through both backends, the scene system, serialization, editor, and undo.

## Target Data Model

```c
// engine/src/asset/model.h (NEW — replaces mesh.h for loaded files)

typedef struct rl_model_mesh {
    vertex *vertices;
    u32 vertex_count;
    u32 *indices;           // nullptr if non-indexed
    u32 index_count;
    u32 material_index;     // into model->materials[]
    vec3 aabb_min, aabb_max;
} rl_model_mesh;

typedef struct rl_model_material {
    asset_id base_color_texture;
    vec3 base_color_factor;
    f32 metallic_factor;
    f32 roughness_factor;
} rl_model_material;

typedef struct rl_model {
    rl_model_mesh *meshes;
    u32 mesh_count;
    rl_model_material *materials;
    u32 material_count;
    vec3 aabb_min, aabb_max;    // union of all mesh AABBs
} rl_model;
```

**Frame data change** — `rl_frame_mesh` gains `mesh_index`:
```c
typedef struct rl_frame_mesh {
    rl_frame_primitive primitive;
    rl_frame_mesh_kind kind;
    mat4 model;
    rl_material material;
    b8 wireframe;
    asset_id model_asset;       // renamed from mesh_asset
    u32 mesh_index;             // which sub-mesh within the model (NEW)
    rl_entity source_entity;
} rl_frame_mesh;
```

**Component change** — `rl_mesh_component.mesh_asset` → `model_asset`:
```c
typedef struct rl_mesh_component {
    rl_frame_primitive primitive;
    rl_frame_mesh_kind kind;
    rl_material material;       // per-entity override (diffuse_map takes priority)
    b8 wireframe;
    asset_id model_asset;       // renamed from mesh_asset
} rl_mesh_component;
```

---

## Session 1 — Data Layer: `rl_model` type + cgltf importer rework [DONE]

**Completed.** All types defined, `ASSET_MODEL` registered, cgltf importer rewritten with node tree traversal and baked transforms. Build + tests pass.

### What was done
- **Created** `engine/src/asset/model.h` — `rl_model`, `rl_model_mesh`, `rl_model_material` structs + `load_model()` declaration
- **Created** `engine/src/asset/model.c` — full cgltf importer with:
  - Recursive `process_node()` walking the glTF scene node tree
  - `cgltf_node_transform_local()` → parent×local multiplication for world transform
  - Vertex position baking via `glm_mat4_mulv()` (vec4 w=1)
  - Normal baking via proper inverse-transpose of upper-left 3×3
  - Per-mesh AABB computed post-transform, whole-model AABB as union of all mesh AABBs
  - Flat `data->meshes[]` fallback for glTF files with no scene/nodes
  - Material loading with texture path resolution (same logic as old mesh.c)
- **Modified** `engine/include/asset/asset.h` — added `ASSET_MODEL` to `ASSET_TYPE` enum
- **Modified** `engine/src/asset/asset.c` — registered `load_model` loader, added `ASSET_MODEL` to `asset_get_resolve_root()` and `get_assets_dir()`
- **Modified** `engine/src/asset/mesh.c` — moved `#define CGLTF_IMPLEMENTATION` to model.c (only one TU can define it)

### Notes for future sessions
- `CGLTF_IMPLEMENTATION` now lives in `model.c`, not `mesh.c`. If mesh.c is removed in Session 7, this is already correct.
- The old `ASSET_MESH` / `rl_mesh` path is fully untouched — existing rendering still works.
- `load_model()` uses `asset_get_resolve_root(ASSET_MODEL)` which resolves to the same content root as `ASSET_MESH`.

---

## Session 2 — Backend Rendering: OpenGL

**Goal**: GL backend renders all meshes of a model asset, each with its own material/texture.

### Files to modify
- `engine/src/renderer/opengl/gl_renderer.c` — model cache + draw loop
- `engine/src/renderer/opengl/gl_mesh.h` — (if cache struct lives here, otherwise gl_types or gl_renderer static)
- `engine/include/renderer/frame_data.h` — add `mesh_index` to `rl_frame_mesh`, rename `mesh_asset` → `model_asset`

### Cache changes
Current: one `GL_Mesh` per asset_id.
New: array of `GL_Mesh` per model asset_id:
```c
struct {
    asset_id model_asset;
    GL_Mesh *meshes;        // allocated from context arena
    u32 mesh_count;
} model_cache[64];
```

### `gl_ensure_model(asset_id)` (replaces `gl_ensure_mesh`)
- First-time: get `rl_model *` from asset, loop all `model->mesh_count` meshes
- Call `gl_mesh_create_from_primitive()` for each mesh's vertices/indices
- Ensure each mesh's material texture is uploaded via `gl_load_texture()`
- Cache the array

### Draw loop changes
Currently: `gl_ensure_mesh()` returns one GL_Mesh, draw it.
New: caller provides `model_asset + mesh_index`, look up `model_cache[i].meshes[mesh_index]`

Per-primitive material resolution:
```
1. Entity material override (fm->material.diffuse_map) — wins if set
2. Model mesh material (model->materials[mesh->material_index].base_color_texture)
3. Placeholder texture
```

### Destroy cleanup
Loop all cache entries, destroy all GL_Mesh in each entry's array.

### Verification
- Load a multi-mesh model, hardcode frame_data with multiple mesh_indices
- Confirm all sub-meshes render with correct textures in GL backend
- Build + run with GL backend (F7 to switch if needed)

---

## Session 3 — Backend Rendering: Vulkan

**Goal**: Same as Session 2 but for Vulkan.

### Files to modify
- `engine/src/renderer/vulkan/vk_types.h` — model cache struct
- `engine/src/renderer/vulkan/vk_mesh.c` → rename to `vk_model.c` (or keep, add model functions)
- `engine/src/renderer/vulkan/vk_mesh.h` → corresponding header
- `engine/src/renderer/vulkan/vk_commands.c` — draw loop changes
- `engine/src/renderer/vulkan/vk_renderer.c` — texture pre-upload for all model materials

### Cache changes
```c
typedef struct VK_CachedMesh {
    VkBuffer vertex_buffer;
    VkDeviceMemory vertex_memory;
    VkBuffer index_buffer;      // VK_NULL_HANDLE if non-indexed
    VkDeviceMemory index_memory;
    u32 vertex_count;
    u32 index_count;
} VK_CachedMesh;

// In VK_Context:
struct {
    asset_id model_asset;
    VK_CachedMesh *meshes;
    u32 mesh_count;
} model_cache[VK_MAX_MESHES];
```

### `vk_model_cache_upload(ctx, asset_id)` (replaces `vk_mesh_cache_upload`)
- Upload ALL meshes of the model (vertex + index buffers each)
- Ensure ALL material textures are uploaded via `vk_ensure_texture()`

### `vk_cmd_bind_and_draw` changes
- Takes `model_asset + mesh_index`
- Looks up `model_cache[i].meshes[mesh_index]`, binds its buffers, draws

### `vk_cmd_resolve_diffuse` changes
- Takes `model_asset + mesh_index`, resolves per-mesh material texture

### Texture pre-upload in `vulkan_submit_frame_data`
```c
if (fm->model_asset) {
    vk_model_cache_upload(&context, fm->model_asset);
    // All textures already ensured by cache upload
}
```

### Destroy cleanup
Loop all cache entries, destroy all VK buffer pairs in each entry.

### Verification
- Same test model as Session 2, now with Vulkan backend
- F7 to switch backends, confirm both render identically
- Build + run

---

## Session 4 — Scene Integration: Components + Frame Building

**Goal**: Scene system produces correct frame_data for model entities. One model entity → N frame_meshes (one per sub-mesh).

### Files to modify
- `engine/include/core/component.h` — rename `mesh_asset` → `model_asset` in `rl_mesh_component`
- `engine/src/core/component.c` — update any references
- `engine/src/core/scene.c` — rework `scene_build_frame_data()`
- `engine/src/math/ray.c` — AABB computation for picking uses whole-model bounds
- `engine/include/renderer/frame_data.h` — already done in Session 2

### Frame building changes in `scene_build_frame_data`

**Count pass**: For entities with `model_asset != 0`, count the model's `mesh_count` instead of 1:
```c
if (mc->model_asset) {
    rl_asset *a = asset_get(mc->model_asset);
    rl_model *m = (rl_model *)a->data;
    mesh_count += m->mesh_count;
} else {
    mesh_count += 1; // primitive cube
}
```

**Fill pass**: For model entities, emit N frame_meshes:
```c
if (mc->model_asset) {
    rl_model *m = get_model(mc->model_asset);
    for (u32 p = 0; p < m->mesh_count; p++) {
        rl_frame_mesh *fm = &meshes[mi++];
        fm->kind          = mc->kind;
        fm->wireframe     = mc->wireframe;
        fm->model_asset   = mc->model_asset;
        fm->mesh_index    = p;
        fm->source_entity = entity;
        glm_mat4_copy(t->local_to_world, fm->model);

        // Resolve material: entity override > mesh material > nothing
        if (mc->material.diffuse_map) {
            fm->material = mc->material;
        } else {
            rl_model_mesh *mm = &m->meshes[p];
            if (mm->material_index < m->material_count) {
                fm->material.diffuse_map = m->materials[mm->material_index].base_color_texture;
            }
            fm->material.specular  = mc->material.specular;
            fm->material.shininess = mc->material.shininess;
        }
    }
}
```

### Picking / AABB (`ray.c`)
Use `model->aabb_min/max` (whole-model bounds) instead of `primitives[0]` AABB. Transform the 8 corners by the entity's model matrix for world-space ray test.

### Verification
- Place a multi-mesh model entity in a scene
- Confirm all sub-meshes render at the entity's transform
- Confirm picking/hover selects the entity correctly
- Both backends
- `ctest --preset debug`

---

## Session 5 — Scene IO: Serialization

**Goal**: Model component data saves and loads correctly in both JSON and binary scene formats.

### Files to modify
- `engine/src/core/scene_io.c` — JSON read/write: rename `mesh_asset` → `model_asset` in mesh component serialization
- `engine/src/core/scene_io_binary.c` — binary read/write: same rename
- Existing scene files (`.scene`) — migration: accept both `mesh_asset` and `model_asset` keys during read, always write `model_asset`

### Migration strategy
- JSON reader: check for `"model_asset"` first, fall back to `"mesh_asset"` for old files
- Binary format: if using a version field, bump it; otherwise the field position is the same (asset_id is same type), just the semantic changes
- On next save, old files get written with new key name

### Callers to update
- `realm/realm_app_module/src/scene_game.c` — if it sets `mesh_asset` directly
- `realm/realm_app_module/src/game.c` — if it creates entities with mesh assets
- `realm_editor/src/ed_inspector.c` — field references
- `realm_editor/src/ed_undo.c` — undo snapshots reference the field

### Verification
- Save a scene with model entities, reload, confirm they appear correctly
- Load an OLD scene file (with `mesh_asset`), confirm it still loads
- `ctest --preset debug` (scene IO tests)

---

## Session 6 — Editor: Inspector + Undo

**Goal**: Editor inspector shows the model_asset field with proper UI, undo supports model component changes.

### Files to modify
- `realm_editor/src/ed_inspector.c` — rename label from "Mesh" to "Model" in mesh component section, update field references
- `realm_editor/src/ed_inspector.h` — if any declarations reference mesh_asset
- `realm_editor/src/ed_undo.c` — update undo snapshot/restore for renamed field
- `realm_editor/src/ed_undo.h` — if struct fields reference mesh_asset

### Inspector changes
- Asset picker label: "Model" instead of "Mesh"
- When a model is assigned, optionally show mesh_count and material_count as read-only info
- Material override section stays the same (entity-level diffuse override)

### Verification
- Open editor, select entity with model, confirm inspector shows "Model" field
- Change model_asset in inspector, confirm renders update
- Undo/redo the change, confirm correct state restoration
- Build + run editor

---

## Session 7 — Cleanup + Template Sync

**Goal**: Remove deprecated code paths, sync project template, ensure all tests pass.

### Tasks
1. **Remove old `rl_mesh` / `mesh.h` / `mesh.c`** if fully replaced by `rl_model` — or keep for built-in primitive data only
2. **Update `engine/src/core/project_template.c`** — template uses `model_asset` in generated code
3. **Update tests**:
   - `tests/cases/test_scene_io.c` — use `model_asset` field
   - `tests/cases/test_project.c` — if template tests reference mesh_asset
4. **Grep for any remaining `mesh_asset` references** across the codebase and update
5. **Update `realm/scenes/*.scene`** files to use `model_asset` key (or rely on migration from Session 5)

### Verification
- Full clean build: `cmake --preset debug && cmake --build --preset debug`
- Full test suite: `ctest --preset debug`
- Load every existing scene file, confirm they work
- Create a new project from template, confirm it builds and runs
- Grep for `mesh_asset` — should find zero hits outside of migration compat code
- Test with GL and Vulkan backends

---

## Key files reference

| File | Role |
|------|------|
| `engine/src/asset/model.h` (NEW) | `rl_model`, `rl_model_mesh`, `rl_model_material` |
| `engine/src/asset/model.c` (NEW) | cgltf importer with node tree traversal |
| `engine/src/asset/mesh.h` | Kept for built-in primitive vertex data only |
| `engine/src/asset/mesh.c` | Deprecated / removed in Session 7 |
| `engine/include/asset/asset.h` | `ASSET_MODEL` enum value |
| `engine/include/renderer/frame_data.h` | `model_asset` + `mesh_index` on `rl_frame_mesh` |
| `engine/include/core/component.h` | `model_asset` on `rl_mesh_component` |
| `engine/src/core/scene.c` | Frame building with multi-mesh expansion |
| `engine/src/renderer/opengl/gl_renderer.c` | GL model cache + multi-mesh draw |
| `engine/src/renderer/vulkan/vk_mesh.c` | VK model cache upload |
| `engine/src/renderer/vulkan/vk_commands.c` | VK per-mesh draw + material resolve |
| `engine/src/renderer/vulkan/vk_types.h` | VK model cache struct |
| `engine/src/core/scene_io.c` | JSON serialization migration |
| `engine/src/core/scene_io_binary.c` | Binary serialization migration |
| `engine/src/math/ray.c` | Whole-model AABB for picking |
| `realm_editor/src/ed_inspector.c` | Inspector UI updates |
| `realm_editor/src/ed_undo.c` | Undo snapshot field rename |
| `engine/src/core/project_template.c` | Template sync |

## Existing utilities to reuse
- `gl_mesh_create_from_primitive()` — `engine/src/renderer/opengl/gl_mesh.c:50` — creates VAO/VBO from vertex/index arrays
- `vk_upload_buffer()` — `engine/src/renderer/vulkan/vk_mesh.c:47` — staging + device-local buffer upload
- `gl_load_texture()` / `vk_ensure_texture()` — existing texture cache functions
- `cgltf_node_transform_local()` — cgltf built-in, computes 4x4 from node TRS
- `rl_arena_push()` — arena allocation for model data
- `asset_load()` / `asset_get()` — existing asset system
