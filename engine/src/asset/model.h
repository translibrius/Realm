#pragma once

#include "defines.h"
#include "asset/asset.h"
#include "memory/arena.h"
#include "renderer/renderer_types.h"

typedef struct rl_model_mesh {
    vertex *vertices;
    u32 vertex_count;
    u32 *indices;       // nullptr if non-indexed
    u32 index_count;
    u32 material_index; // into model->materials[]
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
    vec3 aabb_min, aabb_max; // union of all mesh AABBs
} rl_model;

b8 load_model(rl_arena *asset_arena, rl_asset *asset);
