#include "viewport/ed_picking.h"

#include "core/component.h"
#include "math/ray.h"

#include <string.h>

rl_entity ed_pick_entity(rl_scene *scene, f32 screen_x, f32 screen_y,
                          const rl_viewport_rect *viewport,
                          const rl_frame_camera *camera,
                          f32 ndc_near_z) {
    if (!scene || !viewport || !camera || !camera->valid) return RL_ENTITY_INVALID;

    // Build inverse matrices (memcpy to mutable local to satisfy cglm's non-const API)
    mat4 view_copy, proj_copy, inv_view, inv_proj;
    memcpy(view_copy, camera->view, sizeof(mat4));
    memcpy(proj_copy, camera->projection, sizeof(mat4));
    glm_mat4_inv(view_copy, inv_view);
    glm_mat4_inv(proj_copy, inv_proj);

    rl_ray ray = ray_from_screen(screen_x, screen_y,
                                  viewport->x, viewport->y, viewport->w, viewport->h,
                                  inv_view, inv_proj, ndc_near_z);

    rl_entity_store *es = &scene->entities;
    rl_component_store *cs = &scene->components;

    f32 closest_t = 1e30f;
    rl_entity closest = RL_ENTITY_INVALID;

    for (u32 i = 1; i < es->high_water; i++) {
        if (!es->alive[i]) continue;
        if (!cs->has_transform[i]) continue;

        rl_transform *t = &cs->transforms[i];
        if (t->dirty) transform_update_matrix(t);

        rl_aabb aabb;

        if (cs->has_mesh[i]) {
            asset_id mesh_id = cs->meshes[i].model_asset;
            if (mesh_id) {
                aabb_from_model_asset(mesh_id, t->local_to_world, &aabb);
            } else {
                aabb_from_unit_cube(t->local_to_world, &aabb);
            }
        } else if (cs->has_light[i]) {
            // Light-only entities use a small cube matching the viz size (0.15)
            mat4 scaled;
            glm_mat4_copy(t->local_to_world, scaled);
            glm_scale_uni(scaled, 0.15f);
            aabb_from_unit_cube(scaled, &aabb);
        } else if (cs->has_camera[i]) {
            // Camera-only entities use a small cube matching the viz size (0.15)
            mat4 scaled;
            glm_mat4_copy(t->local_to_world, scaled);
            glm_scale_uni(scaled, 0.15f);
            aabb_from_unit_cube(scaled, &aabb);
        } else {
            continue;
        }

        f32 hit_t;
        if (ray_intersect_aabb(&ray, &aabb, &hit_t)) {
            if (hit_t < closest_t) {
                closest_t = hit_t;
                closest = rl_entity_pack(i, es->generation[i]);
            }
        }
    }

    return closest;
}
