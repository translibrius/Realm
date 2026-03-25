#pragma once

#include "defines.h"

#include "cglm.h"
#include "renderer/renderer_backend.h"

typedef struct rl_camera {
    vec3 pos;
    vec3 forward;
    vec3 up;

    f32 yaw;   // degrees
    f32 pitch; // degrees
    f32 fov;      // degrees
    f32 near_clip; // near plane distance (default 0.1)
    f32 far_clip;  // far plane distance (default 100.0)

    f32 look_speed; // degrees per pixel (default 0.1)
    f32 move_speed; // units per second (default 5.0)
} rl_camera;

typedef struct rl_transform rl_transform;
typedef struct rl_camera_component rl_camera_component;

REALM_API void camera_init(rl_camera *camera);
REALM_API void camera_get_view(const rl_camera *camera, mat4 out_view);
REALM_API void camera_get_projection(const rl_camera *camera, f32 aspect, mat4 out_proj, RENDERER_BACKEND renderer_backend);
REALM_API void camera_get_ortho_projection(f32 left, f32 right, f32 bottom, f32 top, f32 near, f32 far, mat4 out_proj, RENDERER_BACKEND renderer_backend);
REALM_API void camera_update(rl_camera *camera, f64 dt);

// Set yaw/pitch so the camera faces target from its current position.
REALM_API void camera_look_at(rl_camera *camera, const vec3 target);

// Sync helpers for camera-as-entity workflow
REALM_API void camera_from_entity(rl_camera *camera, const rl_transform *t, const rl_camera_component *cc);
REALM_API void camera_sync_to_transform(const rl_camera *camera, rl_transform *t);
