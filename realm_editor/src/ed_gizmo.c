#include "ed_gizmo.h"

#include "core/camera.h"
#include "core/config.h"
#include "engine.h"
#include "memory/arena.h"

#define GIZMO_MESH_COUNT 6 // 3 shafts + 3 tips

void ed_gizmo_build_axis_overlay(const ed_camera *cam,
                                  const Clay_BoundingBox *viewport_bounds,
                                  rl_frame_data *frame) {
    if (!cam || !frame) return;

    rl_arena *fa = rl_engine_get_frame_arena();
    rl_frame_mesh *meshes = rl_arena_push_array(fa, rl_frame_mesh, GIZMO_MESH_COUNT, true);

    // Build rotation-only view matrix: copy camera view, zero translation column
    mat4 view;
    camera_get_view(&cam->cam, view);
    view[3][0] = 0.0f;
    view[3][1] = 0.0f;
    view[3][2] = 0.0f;

    // Small ortho projection
    mat4 proj;
    rl_config *cfg = config_get();
    camera_get_ortho_projection(-1.5f, 1.5f, -1.5f, 1.5f, -10.0f, 10.0f, proj, cfg->renderer_backend);

    // Overlay camera
    frame->overlay_camera.valid = true;
    glm_mat4_copy(view, frame->overlay_camera.view);
    glm_mat4_copy(proj, frame->overlay_camera.projection);
    glm_vec3_zero(frame->overlay_camera.position);

    // Axis colors
    vec3 colors[3] = {
        {1.0f, 0.0f, 0.0f}, // X = red
        {0.0f, 1.0f, 0.0f}, // Y = green
        {0.0f, 0.0f, 1.0f}, // Z = blue
    };

    // Axis directions
    vec3 dirs[3] = {
        {1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f},
    };

    // Shaft scales per axis
    vec3 shaft_scales[3] = {
        {1.0f, 0.05f, 0.05f},  // X shaft
        {0.05f, 1.0f, 0.05f},  // Y shaft
        {0.05f, 0.05f, 1.0f},  // Z shaft
    };

    for (u32 a = 0; a < 3; a++) {
        // Shaft: centered at half-way along axis
        rl_frame_mesh *shaft = &meshes[a * 2];
        shaft->primitive = RL_FRAME_PRIMITIVE_CUBE;
        shaft->kind = RL_FRAME_MESH_KIND_UNLIT;
        glm_vec3_copy(colors[a], shaft->material.specular);

        mat4 m;
        glm_mat4_identity(m);
        vec3 shaft_pos;
        glm_vec3_scale(dirs[a], 0.5f, shaft_pos);
        glm_translate(m, shaft_pos);
        glm_scale(m, shaft_scales[a]);
        glm_mat4_copy(m, shaft->model);

        // Tip: small cube at end of axis
        rl_frame_mesh *tip = &meshes[a * 2 + 1];
        tip->primitive = RL_FRAME_PRIMITIVE_CUBE;
        tip->kind = RL_FRAME_MESH_KIND_UNLIT;
        glm_vec3_copy(colors[a], tip->material.specular);

        mat4 tm;
        glm_mat4_identity(tm);
        glm_translate(tm, dirs[a]);
        vec3 tip_scale = {0.12f, 0.12f, 0.12f};
        glm_scale(tm, tip_scale);
        glm_mat4_copy(tm, tip->model);
    }

    frame->overlay_meshes = meshes;
    frame->overlay_count = GIZMO_MESH_COUNT;

    (void)viewport_bounds; // bounds used by caller for sub-viewport; renderer handles placement
}
