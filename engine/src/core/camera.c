#include "core/camera.h"

#include "../vendor/cglm/clipspace/persp_rh_zo.h"
#include "../vendor/cglm/clipspace/ortho_rh_zo.h"
#include "core/component.h"
#include "platform/input.h"
#include "platform/platform.h"
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
    camera->near_clip = 0.1f;
    camera->far_clip = 100.0f;
    camera->look_speed = 0.1f;
    camera->move_speed = 5.0f;

    camera_update_vectors(camera);
}

void camera_get_view(const rl_camera *camera, mat4 out_view) {
    vec3 target;
    glm_vec3_add((float *)camera->pos, (float *)camera->forward, target);

    glm_lookat((float *)camera->pos, target, (float *)camera->up, out_view);
}

void camera_get_projection(const rl_camera *camera, f32 aspect, mat4 out_proj, RENDERER_BACKEND renderer_backend) {

    if (renderer_backend == BACKEND_OPENGL) {
        glm_perspective(
            glm_rad(camera->fov),
            aspect,
            camera->near_clip,
            camera->far_clip,
            out_proj);
    } else if (renderer_backend == BACKEND_VULKAN) {
        glm_perspective_rh_zo(
            glm_rad(camera->fov),
            aspect,
            camera->near_clip,
            camera->far_clip,
            out_proj);
    }
}

void camera_get_ortho_projection(f32 left, f32 right, f32 bottom, f32 top, f32 near, f32 far, mat4 out_proj, RENDERER_BACKEND renderer_backend) {
    if (renderer_backend == BACKEND_OPENGL) {
        glm_ortho(left, right, bottom, top, near, far, out_proj);
    } else if (renderer_backend == BACKEND_VULKAN) {
        glm_ortho_rh_zo(left, right, bottom, top, near, far, out_proj);
    }
}

void camera_update(rl_camera *camera, f64 dt) {
    f32 velocity = camera->move_speed * (f32)dt;

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

    // --- Look (mouse) --- only when raw input is active (cursor locked)
    if (!platform_get_raw_input()) {
        camera_update_vectors(camera);
        return;
    }

    vec2 mouse_delta;
    input_get_mouse_delta(mouse_delta);

    camera->yaw += mouse_delta[0] * camera->look_speed;
    camera->pitch -= mouse_delta[1] * camera->look_speed; // invert Y for natural feel

    // Clamp pitch to avoid flipping
    if (camera->pitch > 89.0f)
        camera->pitch = 89.0f;
    if (camera->pitch < -89.0f)
        camera->pitch = -89.0f;

    // Recompute forward vector
    camera_update_vectors(camera);
}

void camera_look_at(rl_camera *camera, const vec3 target) {
    vec3 dir;
    glm_vec3_sub((f32 *)target, camera->pos, dir);

    f32 len = glm_vec3_norm(dir);
    if (len < 1e-6f) return;

    glm_vec3_scale(dir, 1.0f / len, dir);
    camera->yaw   = glm_deg(atan2f(dir[2], dir[0]));
    camera->pitch = glm_deg(asinf(glm_clamp(dir[1], -1.0f, 1.0f)));

    camera_update_vectors(camera);
}

void camera_from_entity(rl_camera *camera, const rl_transform *t, const rl_camera_component *cc) {
    if (!camera || !t || !cc) return;

    glm_vec3_copy((f32 *)t->position, camera->pos);

    // Entity rotation stores euler degrees (pitch, yaw, roll)
    camera->pitch = t->rotation[0];
    camera->yaw   = t->rotation[1];

    camera->fov       = cc->fov;
    camera->near_clip = cc->near_clip;
    camera->far_clip  = cc->far_clip;

    camera_update_vectors(camera);
}

void camera_sync_to_transform(const rl_camera *camera, rl_transform *t) {
    if (!camera || !t) return;

    glm_vec3_copy((f32 *)camera->pos, t->position);
    t->rotation[0] = camera->pitch;
    t->rotation[1] = camera->yaw;
    t->dirty = true;
}
