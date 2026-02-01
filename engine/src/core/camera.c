#include "core/camera.h"

#include "../vendor/cglm/clipspace/persp_rh_zo.h"
#include "core/camera.h"
#include "platform/input.h"
#include "renderer/renderer_backend.h"

static void camera_update_vectors(rl_camera *camera) {
    vec3 dir = {
        cosf(glm_rad(camera->yaw)) * cosf(glm_rad(camera->pitch)),
        sinf(glm_rad(camera->pitch)),
        sinf(glm_rad(camera->yaw)) * cosf(glm_rad(camera->pitch))};

    glm_vec3_normalize_to(dir, camera->forward);
}

void camera_init(rl_camera *camera) {
    glm_vec3_copy((vec3){0.0f, 0.0f, 3.0f}, camera->pos);
    glm_vec3_copy((vec3){0.0f, 1.0f, 0.0f}, camera->up);

    camera->yaw = -90.0f;
    camera->pitch = 0.0f;
    camera->fov = 90.0f;

    camera_update_vectors(camera);
}

void camera_get_view(const rl_camera *camera, mat4 out_view) {
    vec3 target;
    glm_vec3_add(camera->pos, camera->forward, target);

    glm_lookat(camera->pos, target, camera->up, out_view);
}

void camera_get_projection(const rl_camera *camera, f32 aspect, mat4 out_proj, RENDERER_BACKEND renderer_backend) {

    if (renderer_backend == BACKEND_OPENGL) {
        glm_perspective(
            glm_rad(camera->fov),
            aspect,
            0.1f,
            100.0f,
            out_proj);
    } else if (renderer_backend == BACKEND_VULKAN) {
        glm_perspective_rh_zo(
            glm_rad(camera->fov),
            aspect,
            0.1f,
            100.0f,
            out_proj);
    }
}

void camera_update(rl_camera *camera, f64 dt) {
    const f32 move_speed = 5.0f; // units / second
    const f32 look_speed = 0.1f; // degrees per pixel

    f32 velocity = move_speed * (f32)dt;

    // --- Movement (keyboard) ---
    vec3 right;
    glm_cross(camera->forward, camera->up, right);
    glm_normalize(right);

    if (input_is_key_down(KEY_W))
        glm_vec3_muladds(camera->forward, velocity, camera->pos);

    if (input_is_key_down(KEY_S))
        glm_vec3_muladds(camera->forward, -velocity, camera->pos);

    if (input_is_key_down(KEY_D))
        glm_vec3_muladds(right, velocity, camera->pos);

    if (input_is_key_down(KEY_A))
        glm_vec3_muladds(right, -velocity, camera->pos);

    if (input_is_key_down(KEY_SPACE))
        glm_vec3_muladds(camera->up, velocity, camera->pos);

    if (input_is_key_down(KEY_L_SHIFT))
        glm_vec3_muladds(camera->up, -velocity, camera->pos);

    // --- Look (mouse) ---
    vec2 mouse_delta;
    input_get_mouse_delta(mouse_delta);

    camera->yaw += mouse_delta[0] * look_speed;
    camera->pitch -= mouse_delta[1] * look_speed; // invert Y for natural feel

    // Clamp pitch to avoid flipping
    if (camera->pitch > 89.0f)
        camera->pitch = 89.0f;
    if (camera->pitch < -89.0f)
        camera->pitch = -89.0f;

    // Recompute forward vector
    camera_update_vectors(camera);
}
