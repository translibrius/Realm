#pragma once

#include "defines.h"

#include "cglm.h"

typedef struct rl_camera {
    vec3 pos;
    vec3 forward;
    vec3 up;

    f32 yaw; // degrees
    f32 pitch; // degrees
    f32 fov; // degrees
} rl_camera;

REALM_API void camera_init(rl_camera *camera);
REALM_API void camera_get_view(const rl_camera *camera, mat4 out_view);
REALM_API void camera_get_projection(const rl_camera *camera, f32 aspect, mat4 out_proj);
REALM_API void camera_update(rl_camera *camera, f64 dt);