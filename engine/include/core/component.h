#pragma once

#include "core/entity.h"
#include "defines.h"
#include "memory/arena.h"
#include "renderer/frame_data.h"

#include "cglm.h"

typedef struct rl_transform {
    vec3 position;
    vec3 rotation;       // euler degrees (pitch, yaw, roll)
    vec3 scale;
    mat4 local_to_world;
    b8   dirty;
} rl_transform;

typedef struct rl_mesh_component {
    rl_frame_primitive primitive;
    rl_frame_mesh_kind kind;
    rl_material        material;
    b8                 wireframe;
    asset_id           model_asset; // 0 = use primitive
} rl_mesh_component;

typedef struct rl_light_component {
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
} rl_light_component;

#define RL_NAME_MAX 64

typedef struct rl_name_component {
    char name[RL_NAME_MAX];
} rl_name_component;

#define RL_BEHAVIOR_NAME_MAX 64

typedef struct rl_behavior_component {
    char name[RL_BEHAVIOR_NAME_MAX];
} rl_behavior_component;

typedef struct rl_camera_component {
    f32 fov;       // degrees (default 90)
    f32 near_clip; // default 0.1
    f32 far_clip;  // default 100.0
    b8  is_main;   // main game camera — first one found wins
} rl_camera_component;

typedef struct rl_component_store {
    u32 capacity;
    rl_transform          *transforms;   b8 *has_transform;
    rl_mesh_component     *meshes;       b8 *has_mesh;
    rl_light_component    *lights;       b8 *has_light;
    rl_name_component     *names;        b8 *has_name;
    rl_behavior_component *behaviors;    b8 *has_behavior;
    rl_camera_component   *cameras;      b8 *has_camera;
} rl_component_store;

REALM_API void component_store_init(rl_component_store *store, u32 capacity, rl_arena *arena);

REALM_API rl_transform       *transform_add(rl_component_store *s, rl_entity e);
REALM_API rl_transform       *transform_get(rl_component_store *s, rl_entity e);
REALM_API void                transform_remove(rl_component_store *s, rl_entity e);
REALM_API void                transform_update_matrix(rl_transform *t);

REALM_API rl_mesh_component  *mesh_add(rl_component_store *s, rl_entity e);
REALM_API rl_mesh_component  *mesh_get(rl_component_store *s, rl_entity e);
REALM_API void                mesh_remove(rl_component_store *s, rl_entity e);

REALM_API rl_light_component *light_add(rl_component_store *s, rl_entity e);
REALM_API rl_light_component *light_get(rl_component_store *s, rl_entity e);
REALM_API void                light_remove(rl_component_store *s, rl_entity e);

REALM_API rl_name_component  *name_add(rl_component_store *s, rl_entity e, const char *name);
REALM_API rl_name_component  *name_get(rl_component_store *s, rl_entity e);
REALM_API void                name_remove(rl_component_store *s, rl_entity e);

REALM_API rl_behavior_component *behavior_comp_add(rl_component_store *s, rl_entity e, const char *name);
REALM_API rl_behavior_component *behavior_comp_get(rl_component_store *s, rl_entity e);
REALM_API void                   behavior_comp_remove(rl_component_store *s, rl_entity e);

REALM_API rl_camera_component *camera_comp_add(rl_component_store *s, rl_entity e);
REALM_API rl_camera_component *camera_comp_get(rl_component_store *s, rl_entity e);
REALM_API void                 camera_comp_remove(rl_component_store *s, rl_entity e);
