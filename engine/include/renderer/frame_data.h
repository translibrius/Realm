#pragma once

#include "asset/asset.h"
#include "cglm.h"
#include "core/entity.h"
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
    asset_id model_asset;         // 0 = use primitive (cube), non-zero = loaded model/mesh
    u32 mesh_index;               // which sub-mesh within the model (0 for single-mesh)
    rl_entity source_entity;      // originating entity (for picking/hover matching)
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

typedef enum rl_outline_technique {
    RL_OUTLINE_JFA = 0, // crisp uniform-width (default)
} rl_outline_technique;

typedef struct rl_frame_outline {
    rl_entity entity;
    vec4 color;
    f32 width;
    rl_outline_technique technique;
    b8 through_walls;
} rl_frame_outline;

typedef struct rl_frame_line {
    vec3 a;     // world-space start
    vec3 b;     // world-space end
    vec3 color; // RGB [0,1]
} rl_frame_line;

typedef struct rl_frame_data {
    rl_frame_camera camera;

    rl_frame_mesh *meshes;
    u32 mesh_count;

    rl_frame_point_light *point_lights;
    u32 point_light_count;

    rl_frame_text *texts;
    u32 text_count;

    rl_viewport_rect viewport_rect;

    // World-space overlays (transform gizmos) — main camera, no depth test
    rl_frame_mesh  *world_overlays;
    u32             world_overlay_count;

    // Overlay pass (gizmo axes, etc.) — rendered with separate camera, no depth test
    rl_frame_camera overlay_camera;
    rl_frame_mesh  *overlay_meshes;
    u32             overlay_count;

    b8 show_grid;

    // World-space line segments (frustum viz, debug drawing)
    rl_frame_line *lines;
    u32            line_count;

    // Outline requests (entity highlights)
    rl_frame_outline *outlines;
    u32 outline_count;
} rl_frame_data;
