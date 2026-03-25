#pragma once

#include "core/camera.h"
#include "defines.h"
#include "platform/platform.h"

#include "cglm.h"
#include "clay.h"

typedef struct ed_camera {
    rl_camera cam;
    vec3 target;     // orbit center point
    f32 distance;    // orbit radius
    b8 fly_mode;     // right-click held = WASD fly mode
    b8 orbiting;     // middle-mouse held = orbit mode
    b8 viewport_hovered;

    // Right-click disambiguation: tap = context menu, hold/drag = fly mode
    b8  fly_pending;         // right mouse down, waiting to disambiguate
    f32 fly_pending_elapsed; // seconds since right mouse went down
    vec2 fly_pending_mouse;  // mouse position at right mouse down
    b8  right_click_tap;     // one-frame pulse: right-click was a tap, not a hold
} ed_camera;

void ed_camera_init(ed_camera *ec);
void ed_camera_update(ed_camera *ec, f64 dt, const Clay_BoundingBox *viewport,
                      platform_window *window, const f32 *selection_pos);
void ed_camera_on_scroll(ed_camera *ec, f32 z_delta);
void ed_camera_frame_selection(ed_camera *ec, const vec3 target_pos);
