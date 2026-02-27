#include "debug/debug_scene.h"

static rl_debug_scene scene;

rl_debug_scene *rl_debug_scene_get(void) {
    if (!scene.initialized) {
        glm_vec3_copy((vec3){1.2f, 1.0f, 2.0f}, scene.light_position);
        glm_vec3_copy((vec3){0.2f, 0.2f, 0.2f}, scene.light_ambient);
        glm_vec3_copy((vec3){0.5f, 0.5f, 0.5f}, scene.light_diffuse);
        glm_vec3_copy((vec3){1.0f, 1.0f, 1.0f}, scene.light_specular);

        scene.material_shininess = 32.0f;
        glm_vec3_copy((vec3){0.5f, 0.5f, 0.5f}, scene.material_specular);

        scene.rotation_speed = 100.0f;
        scene.initialized = true;
    }
    return &scene;
}
