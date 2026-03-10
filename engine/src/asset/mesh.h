#pragma once

#include "defines.h"
#include "asset/asset.h"
#include "memory/arena.h"
#include "renderer/renderer_types.h"

typedef struct rl_mesh_material {
    asset_id base_color_texture; // 0 = none
    vec3 base_color_factor;      // default {1,1,1}
    f32 metallic_factor;
    f32 roughness_factor;
} rl_mesh_material;

typedef struct rl_mesh_primitive {
    vertex *vertices;
    u32 vertex_count;
    u32 *indices;      // nullptr if non-indexed
    u32 index_count;   // 0 if non-indexed
    u32 material_index; // index into rl_mesh.materials
} rl_mesh_primitive;

typedef struct rl_mesh {
    rl_mesh_primitive *primitives;
    u32 primitive_count;
    rl_mesh_material *materials;
    u32 material_count;
} rl_mesh;

b8 load_mesh(rl_arena *asset_arena, rl_asset *asset);
