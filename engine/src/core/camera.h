#pragma once

#include "defines.h"

#include "../vendor/cglm/cglm.h"

typedef struct rl_camera {
    vec3 pos;
    vec3 forward;
    vec3 up;

    f32 yaw; // degrees
    f32 pitch; // degrees
    f32 fov; // degrees
} rl_camera;

void camera_init(rl_camera *camera);
void camera_get_view(const rl_camera *camera, mat4 out_view);
void camera_get_projection(const rl_camera *camera, f32 aspect, mat4 out_proj);
void camera_update(rl_camera *camera, f64 dt);