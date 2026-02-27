#pragma once

#include "cglm.h"
#include "defines.h"

typedef struct rl_debug_scene {
    vec3 light_position;
    vec3 light_ambient;
    vec3 light_diffuse;
    vec3 light_specular;

    f32  material_shininess;
    vec3 material_specular;

    f32  rotation_speed;

    b8   initialized;
} rl_debug_scene;

REALM_API rl_debug_scene *rl_debug_scene_get(void);
