#include "viewport/ed_frustum.h"

#include "core/camera.h"
#include "core/component.h"
#include "core/scene.h"
#include "engine.h"
#include "gui/gui_theme.h"
#include "memory/arena.h"

#include <math.h>

#define FRUSTUM_ASPECT    (16.0f / 9.0f)
#define FRUSTUM_EDGES     12
#define FRUSTUM_MAX_FAR   3.0f // cap displayed far distance so the viz stays readable

// Frustum corner indices:
//   0=NTL 1=NTR 2=NBL 3=NBR (near plane)
//   4=FTL 5=FTR 6=FBL 7=FBR (far plane)
static const u32 edge_a[FRUSTUM_EDGES] = {0, 1, 3, 2, 4, 5, 7, 6, 0, 1, 2, 3};
static const u32 edge_b[FRUSTUM_EDGES] = {1, 3, 2, 0, 5, 7, 6, 4, 4, 5, 6, 7};

static void compute_frustum_corners(const rl_camera_component *cc,
                                    const rl_transform *t,
                                    vec3 corners[8]) {
    // Build a temporary camera to get the correct forward vector from yaw/pitch
    rl_camera cam;
    camera_init(&cam);
    camera_from_entity(&cam, t, cc);

    // Derive right/up from the camera's forward + world up
    vec3 world_up = {0.0f, 1.0f, 0.0f};
    vec3 right, up;
    glm_vec3_cross(cam.forward, world_up, right);
    glm_vec3_normalize(right);
    glm_vec3_cross(right, cam.forward, up);
    glm_vec3_normalize(up);

    f32 fov_rad = glm_rad(cc->fov);
    f32 display_far = glm_min(cc->far_clip, FRUSTUM_MAX_FAR);
    f32 near_h = cc->near_clip * tanf(fov_rad * 0.5f);
    f32 near_w = near_h * FRUSTUM_ASPECT;
    f32 far_h  = display_far * tanf(fov_rad * 0.5f);
    f32 far_w  = far_h * FRUSTUM_ASPECT;

    // Near/far plane centers
    vec3 nc, fc;
    glm_vec3_scale(cam.forward, cc->near_clip, nc);
    glm_vec3_add(cam.pos, nc, nc);
    glm_vec3_scale(cam.forward, display_far, fc);
    glm_vec3_add(cam.pos, fc, fc);

    // Corner layout: TL=0, TR=1, BL=2, BR=3, then +4 for far
    for (u32 i = 0; i < 8; i++) {
        b8 is_far    = (i >= 4);
        b8 is_right  = (i & 1);
        b8 is_bottom = (i & 2);

        f32 hw = is_far ? far_w : near_w;
        f32 hh = is_far ? far_h : near_h;
        vec3 *center = is_far ? &fc : &nc;

        glm_vec3_copy(*center, corners[i]);

        vec3 r_off, u_off;
        glm_vec3_scale(right, is_right ? hw : -hw, r_off);
        glm_vec3_scale(up, is_bottom ? -hh : hh, u_off);

        glm_vec3_add(corners[i], r_off, corners[i]);
        glm_vec3_add(corners[i], u_off, corners[i]);
    }
}

static void emit_frustum_lines(const rl_camera_component *cc,
                               const rl_transform *t,
                               const vec3 color,
                               rl_frame_line *out) {
    vec3 corners[8];
    compute_frustum_corners(cc, t, corners);

    for (u32 i = 0; i < FRUSTUM_EDGES; i++) {
        glm_vec3_copy(corners[edge_a[i]], out[i].a);
        glm_vec3_copy(corners[edge_b[i]], out[i].b);
        glm_vec3_copy((f32 *)color, out[i].color);
    }
}

void ed_frustum_build(rl_scene *scene, rl_entity selected, rl_entity hovered,
                      rl_frame_data *frame) {
    if (!scene || !frame) return;

    rl_component_store *cs = &scene->components;

    // Check which targets have camera components
    b8 sel_has_cam = false;
    b8 hov_has_cam = false;

    if (selected) {
        rl_camera_component *cc = camera_comp_get(cs, selected);
        if (cc) sel_has_cam = true;
    }
    if (hovered && hovered != selected) {
        rl_camera_component *cc = camera_comp_get(cs, hovered);
        if (cc) hov_has_cam = true;
    }

    u32 frustum_count = (sel_has_cam ? 1 : 0) + (hov_has_cam ? 1 : 0);
    if (frustum_count == 0) return;

    u32 total_lines = frustum_count * FRUSTUM_EDGES;
    rl_arena *fa = rl_engine_get_frame_arena();
    rl_frame_line *lines = rl_arena_push_array(fa, rl_frame_line, total_lines, true);
    u32 li = 0;

    const gui_theme *theme = gui_theme_get();

    if (sel_has_cam) {
        u32 idx = rl_entity_index(selected);
        rl_camera_component *cc = &cs->cameras[idx];
        rl_transform *t = &cs->transforms[idx];
        if (t->dirty) transform_update_matrix(t);

        vec3 color = {theme->accent.r / 255.0f, theme->accent.g / 255.0f, theme->accent.b / 255.0f};
        emit_frustum_lines(cc, t, color, &lines[li]);
        li += FRUSTUM_EDGES;
    }

    if (hov_has_cam) {
        u32 idx = rl_entity_index(hovered);
        rl_camera_component *cc = &cs->cameras[idx];
        rl_transform *t = &cs->transforms[idx];
        if (t->dirty) transform_update_matrix(t);

        vec3 color = {theme->accent_hover.r / 255.0f, theme->accent_hover.g / 255.0f, theme->accent_hover.b / 255.0f};
        emit_frustum_lines(cc, t, color, &lines[li]);
        li += FRUSTUM_EDGES;
    }

    frame->lines = lines;
    frame->line_count = total_lines;
}
