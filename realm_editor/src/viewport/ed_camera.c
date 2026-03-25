#include "viewport/ed_camera.h"

#include "core/ed_config.h"
#include "platform/input.h"

#include <math.h>

#define FLY_PENDING_TIME   0.15f // seconds before hold triggers fly mode
#define FLY_PENDING_PIXELS 4.0f  // mouse movement before drag triggers fly mode

static void update_vectors(ed_camera *ec) {
    vec3 dir = {
        cosf(glm_rad(ec->cam.yaw)) * cosf(glm_rad(ec->cam.pitch)),
        sinf(glm_rad(ec->cam.pitch)),
        sinf(glm_rad(ec->cam.yaw)) * cosf(glm_rad(ec->cam.pitch)),
    };
    glm_vec3_normalize_to(dir, ec->cam.forward);
}

// Recompute orbit distance and orientation so the camera stays at its current
// position but faces the orbit target.  Call after changing target or after
// exiting fly mode.
static void recompute_orbit_from_pos(ed_camera *ec) {
    ec->distance = glm_vec3_distance(ec->cam.pos, ec->target);
    if (ec->distance < 0.1f) ec->distance = 0.1f;
    camera_look_at(&ec->cam, ec->target);
}

static void enter_fly_mode(ed_camera *ec, platform_window *window) {
    ec->fly_mode = true;
    ec->fly_pending = false;
    platform_set_raw_input(window, true);
    platform_set_cursor_mode(window, CURSOR_MODE_LOCKED);
}

void ed_camera_init(ed_camera *ec) {
    camera_init(&ec->cam);
    ec->cam.far_clip = 10000.0f;
    ec->fly_mode = false;
    ec->orbiting = false;
    ec->viewport_hovered = false;
    ec->fly_pending = false;
    ec->right_click_tap = false;

    // Default: look down at world origin from an angle
    glm_vec3_copy((vec3){8.0f, 6.0f, 8.0f}, ec->cam.pos);
    glm_vec3_zero(ec->target);
    recompute_orbit_from_pos(ec);
}

void ed_camera_restore(ed_camera *ec, const ed_config *cfg) {
    if (!cfg->cam_state_valid) return;
    glm_vec3_copy((vec3){cfg->cam_pos[0], cfg->cam_pos[1], cfg->cam_pos[2]}, ec->cam.pos);
    glm_vec3_copy((vec3){cfg->cam_target[0], cfg->cam_target[1], cfg->cam_target[2]}, ec->target);
    ec->cam.yaw = cfg->cam_yaw;
    ec->cam.pitch = cfg->cam_pitch;
    ec->distance = cfg->cam_distance;
    update_vectors(ec);
}

void ed_camera_snapshot(const ed_camera *ec, ed_config *cfg) {
    glm_vec3_copy((f32 *)ec->cam.pos, cfg->cam_pos);
    glm_vec3_copy((f32 *)ec->target, cfg->cam_target);
    cfg->cam_yaw = ec->cam.yaw;
    cfg->cam_pitch = ec->cam.pitch;
    cfg->cam_distance = ec->distance;
    cfg->cam_state_valid = true;
}

void ed_camera_update(ed_camera *ec, f64 dt, const Clay_BoundingBox *viewport,
                      platform_window *window, const f32 *selection_pos) {
    // Clear one-frame pulse
    ec->right_click_tap = false;

    // Check mouse position vs viewport bounds
    vec2 mouse_pos;
    input_get_mouse_position(mouse_pos);
    ec->viewport_hovered = (viewport->width > 0 &&
        mouse_pos[0] >= viewport->x && mouse_pos[0] <= viewport->x + viewport->width &&
        mouse_pos[1] >= viewport->y && mouse_pos[1] <= viewport->y + viewport->height);

    // --- Right-click disambiguation: tap = context menu, hold/drag = fly mode ---
    if (input_mouse_pressed(MOUSE_RIGHT) && ec->viewport_hovered &&
        !ec->fly_mode && !ec->fly_pending) {
        ec->fly_pending = true;
        ec->fly_pending_elapsed = 0.0f;
        glm_vec2_copy(mouse_pos, ec->fly_pending_mouse);
    }

    if (ec->fly_pending) {
        if (!input_is_mouse_down(MOUSE_RIGHT)) {
            // Released before threshold — it's a tap
            ec->fly_pending = false;
            ec->right_click_tap = true;
        } else {
            ec->fly_pending_elapsed += (f32)dt;
            f32 dx = mouse_pos[0] - ec->fly_pending_mouse[0];
            f32 dy = mouse_pos[1] - ec->fly_pending_mouse[1];
            f32 move_dist = sqrtf(dx * dx + dy * dy);

            if (ec->fly_pending_elapsed >= FLY_PENDING_TIME ||
                move_dist >= FLY_PENDING_PIXELS) {
                enter_fly_mode(ec, window);
            }
        }
    }

    // --- Fly mode exit ---
    if (ec->fly_mode && !input_is_mouse_down(MOUSE_RIGHT)) {
        ec->fly_mode = false;
        platform_set_raw_input(window, false);
        platform_set_cursor_mode(window, CURSOR_MODE_NORMAL);
        input_flush_mouse_delta();

        // Snap orbit state so the camera stays in place but faces the target.
        recompute_orbit_from_pos(ec);
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
        return;
    }

    // --- Orbit mode (middle-mouse drag) ---
    if (input_mouse_pressed(MOUSE_MIDDLE) && ec->viewport_hovered) {
        ec->orbiting = true;

        // Snap target to selection center if available
        if (selection_pos) {
            glm_vec3_copy((f32 *)selection_pos, ec->target);
        }
        recompute_orbit_from_pos(ec);
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
