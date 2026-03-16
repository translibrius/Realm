#pragma once

#include "asset/asset.h"
#include "cglm.h"
#include "defines.h"

typedef struct rl_font rl_font;

typedef struct rl_frame_camera {
    mat4 view;
    mat4 projection;
    vec3 position;
    b8 valid;
} rl_frame_camera;

typedef struct rl_frame_text {
    const char *text;
    rl_font *font;
    f32 size_px;
    f32 x;
    f32 y;
    vec4 color;
} rl_frame_text;

typedef enum rl_frame_primitive {
    RL_FRAME_PRIMITIVE_CUBE = 0,
} rl_frame_primitive;

typedef enum rl_frame_mesh_kind {
    RL_FRAME_MESH_KIND_LIT = 0,
    RL_FRAME_MESH_KIND_UNLIT = 1,
} rl_frame_mesh_kind;

typedef struct rl_material {
    asset_id diffuse_map;
    vec3 specular;
    f32 shininess;
} rl_material;

typedef struct rl_frame_mesh {
    rl_frame_primitive primitive;
    rl_frame_mesh_kind kind;
    mat4 model;
    rl_material material;
    b8 wireframe;
    asset_id mesh_asset; // 0 = use primitive (cube), non-zero = loaded mesh
} rl_frame_mesh;

typedef struct rl_frame_point_light {
    vec3 position;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
} rl_frame_point_light;

static const rl_frame_point_light RL_DEFAULT_POINT_LIGHT = {
    .position = {1.2f, 1.0f, 2.0f},
    .ambient  = {0.3f, 0.3f, 0.3f},
    .diffuse  = {0.9f, 0.9f, 0.9f},
    .specular = {1.0f, 1.0f, 1.0f},
};

typedef struct rl_viewport_rect {
    f32 x, y, w, h; // pixels; all-zero = full window
} rl_viewport_rect;

typedef struct rl_frame_data {
    rl_frame_camera camera;

    rl_frame_mesh *meshes;
    u32 mesh_count;

    rl_frame_point_light *point_lights;
    u32 point_light_count;

    rl_frame_text *texts;
    u32 text_count;

    rl_viewport_rect viewport_rect;
} rl_frame_data;
