#include "ed_camera.h"

#include "platform/input.h"

#include <math.h>

static void update_vectors(ed_camera *ec) {
    vec3 dir = {
        cosf(glm_rad(ec->cam.yaw)) * cosf(glm_rad(ec->cam.pitch)),
        sinf(glm_rad(ec->cam.pitch)),
        sinf(glm_rad(ec->cam.yaw)) * cosf(glm_rad(ec->cam.pitch)),
    };
    glm_vec3_normalize_to(dir, ec->cam.forward);
}

void ed_camera_init(ed_camera *ec) {
    camera_init(&ec->cam);
    ec->distance = 3.0f;
    ec->fly_mode = false;
    ec->orbiting = false;
    ec->viewport_hovered = false;

    // Derive orbit target from initial camera position + forward
    // camera_init sets pos=(0,0,3), forward=(0,0,-1), so target = (0,0,0)
    vec3 offset;
    glm_vec3_scale(ec->cam.forward, ec->distance, offset);
    glm_vec3_add(ec->cam.pos, offset, ec->target);
}

void ed_camera_update(ed_camera *ec, f64 dt, const Clay_BoundingBox *viewport,
                      platform_window *window) {
    // Check mouse position vs viewport bounds
    vec2 mouse_pos;
    input_get_mouse_position(mouse_pos);
    ec->viewport_hovered = (viewport->width > 0 &&
        mouse_pos[0] >= viewport->x && mouse_pos[0] <= viewport->x + viewport->width &&
        mouse_pos[1] >= viewport->y && mouse_pos[1] <= viewport->y + viewport->height);

    // --- Fly mode (right-click hold) ---
    if (input_mouse_pressed(MOUSE_RIGHT) && ec->viewport_hovered && !ec->fly_mode) {
        ec->fly_mode = true;
        platform_set_raw_input(window, true);
        platform_set_cursor_mode(window, CURSOR_MODE_LOCKED);
    }

    if (ec->fly_mode && !input_is_mouse_down(MOUSE_RIGHT)) {
        ec->fly_mode = false;
        platform_set_raw_input(window, false);
        platform_set_cursor_mode(window, CURSOR_MODE_NORMAL);
        input_flush_mouse_delta();
    }

    if (ec->fly_mode) {
        f32 velocity = ec->cam.move_speed * (f32)dt;

        vec3 right;
        glm_cross(ec->cam.forward, ec->cam.up, right);
        glm_normalize(right);

        if (input_is_key_down(KEY_W))
            glm_vec3_muladds(ec->cam.forward, velocity, ec->cam.pos);
        if (input_is_key_down(KEY_S))
            glm_vec3_muladds(ec->cam.forward, -velocity, ec->cam.pos);
        if (input_is_key_down(KEY_D))
            glm_vec3_muladds(right, velocity, ec->cam.pos);
        if (input_is_key_down(KEY_A))
            glm_vec3_muladds(right, -velocity, ec->cam.pos);
        if (input_is_key_down(KEY_SPACE))
            glm_vec3_muladds(ec->cam.up, velocity, ec->cam.pos);
        if (input_is_key_down(KEY_L_SHIFT))
            glm_vec3_muladds(ec->cam.up, -velocity, ec->cam.pos);

        // Mouse look
        vec2 mouse_delta;
        input_get_mouse_delta(mouse_delta);
        ec->cam.yaw += mouse_delta[0] * ec->cam.look_speed;
        ec->cam.pitch -= mouse_delta[1] * ec->cam.look_speed;

        if (ec->cam.pitch > 89.0f) ec->cam.pitch = 89.0f;
        if (ec->cam.pitch < -89.0f) ec->cam.pitch = -89.0f;

        update_vectors(ec);

        // Keep orbit center tracking with fly position
        vec3 offset;
        glm_vec3_scale(ec->cam.forward, ec->distance, offset);
        glm_vec3_add(ec->cam.pos, offset, ec->target);
        return;
    }

    // --- Orbit mode (middle-mouse drag) ---
    if (input_mouse_pressed(MOUSE_MIDDLE) && ec->viewport_hovered) {
        ec->orbiting = true;
    }
    if (ec->orbiting && !input_is_mouse_down(MOUSE_MIDDLE)) {
        ec->orbiting = false;
    }

    if (ec->orbiting) {
        vec2 mouse_delta;
        input_get_mouse_delta(mouse_delta);

        ec->cam.yaw += mouse_delta[0] * ec->cam.look_speed;
        ec->cam.pitch -= mouse_delta[1] * ec->cam.look_speed;

        if (ec->cam.pitch > 89.0f) ec->cam.pitch = 89.0f;
        if (ec->cam.pitch < -89.0f) ec->cam.pitch = -89.0f;
    }

    // Recompute position from orbit
    update_vectors(ec);
    vec3 offset;
    glm_vec3_scale(ec->cam.forward, ec->distance, offset);
    glm_vec3_sub(ec->target, offset, ec->cam.pos);
}

void ed_camera_on_scroll(ed_camera *ec, f32 z_delta) {
    ec->distance -= z_delta * ec->distance * 0.1f;
    if (ec->distance < 0.1f) ec->distance = 0.1f;
    if (ec->distance > 500.0f) ec->distance = 500.0f;
}

void ed_camera_frame_selection(ed_camera *ec, const vec3 target_pos) {
    glm_vec3_copy((f32 *)target_pos, ec->target);
    ec->distance = 5.0f;
}
