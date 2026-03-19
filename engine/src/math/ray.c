#include "math/ray.h"

#include "asset/asset.h"
#include "asset/mesh.h"

#include <math.h>

rl_ray ray_from_screen(f32 sx, f32 sy,
                        f32 vp_x, f32 vp_y, f32 vp_w, f32 vp_h,
                        mat4 inv_view, mat4 inv_proj) {
    // Screen → NDC (Y flipped: top=+1, bottom=-1)
    f32 ndc_x = 2.0f * (sx - vp_x) / vp_w - 1.0f;
    f32 ndc_y = 1.0f - 2.0f * (sy - vp_y) / vp_h;

    // Near/far clip points in clip space
    vec4 near_clip = {ndc_x, ndc_y, -1.0f, 1.0f};
    vec4 far_clip  = {ndc_x, ndc_y,  1.0f, 1.0f};

    // Unproject to view space
    vec4 near_view, far_view;
    glm_mat4_mulv(inv_proj, near_clip, near_view);
    glm_mat4_mulv(inv_proj, far_clip, far_view);

    // Perspective divide
    if (fabsf(near_view[3]) > 1e-6f) glm_vec4_divs(near_view, near_view[3], near_view);
    if (fabsf(far_view[3]) > 1e-6f) glm_vec4_divs(far_view, far_view[3], far_view);

    // Unproject to world space
    vec4 near_world, far_world;
    glm_mat4_mulv(inv_view, near_view, near_world);
    glm_mat4_mulv(inv_view, far_view, far_world);

    rl_ray ray;
    glm_vec3_copy(near_world, ray.origin);

    vec3 dir;
    glm_vec3_sub(far_world, near_world, dir);
    glm_vec3_normalize(dir);
    glm_vec3_copy(dir, ray.direction);

    return ray;
}

b8 ray_intersect_aabb(const rl_ray *ray, const rl_aabb *aabb, f32 *out_t) {
    f32 tmin = -1e30f;
    f32 tmax = 1e30f;

    for (i32 i = 0; i < 3; i++) {
        f32 inv_d = 1.0f / ray->direction[i];
        f32 t0 = (aabb->min[i] - ray->origin[i]) * inv_d;
        f32 t1 = (aabb->max[i] - ray->origin[i]) * inv_d;
        if (inv_d < 0.0f) {
            f32 tmp = t0;
            t0 = t1;
            t1 = tmp;
        }
        if (t0 > tmin) tmin = t0;
        if (t1 < tmax) tmax = t1;
        if (tmax < tmin) return false;
    }

    if (tmax < 0.0f) return false; // AABB is behind the ray
    if (out_t) *out_t = tmin >= 0.0f ? tmin : tmax;
    return true;
}

void aabb_from_unit_cube(mat4 model, rl_aabb *out) {
    // Unit cube corners: -0.5 to +0.5 on each axis
    static const f32 corners[8][3] = {
        {-0.5f, -0.5f, -0.5f}, {+0.5f, -0.5f, -0.5f},
        {-0.5f, +0.5f, -0.5f}, {+0.5f, +0.5f, -0.5f},
        {-0.5f, -0.5f, +0.5f}, {+0.5f, -0.5f, +0.5f},
        {-0.5f, +0.5f, +0.5f}, {+0.5f, +0.5f, +0.5f},
    };

    vec4 transformed;
    vec4 corner;

    // First corner
    corner[0] = corners[0][0];
    corner[1] = corners[0][1];
    corner[2] = corners[0][2];
    corner[3] = 1.0f;
    glm_mat4_mulv(model, corner, transformed);

    glm_vec3_copy(transformed, out->min);
    glm_vec3_copy(transformed, out->max);

    // Remaining corners
    for (u32 i = 1; i < 8; i++) {
        corner[0] = corners[i][0];
        corner[1] = corners[i][1];
        corner[2] = corners[i][2];
        corner[3] = 1.0f;
        glm_mat4_mulv(model, corner, transformed);

        glm_vec3_minv(out->min, transformed, out->min);
        glm_vec3_maxv(out->max, transformed, out->max);
    }
}

void aabb_from_mesh_asset(asset_id mesh_id, mat4 model, rl_aabb *out) {
    rl_asset *asset = asset_get(mesh_id);
    if (!asset || asset->type != ASSET_MESH || !asset->data) {
        aabb_from_unit_cube(model, out);
        return;
    }

    rl_mesh *mesh = (rl_mesh *)asset->data;
    if (mesh->primitive_count == 0 || mesh->primitives[0].vertex_count == 0) {
        aabb_from_unit_cube(model, out);
        return;
    }

    // Transform 8 corners of the cached local AABB instead of all vertices
    rl_mesh_primitive *prim = &mesh->primitives[0];
    f32 *mn = prim->local_aabb_min;
    f32 *mx = prim->local_aabb_max;
    f32 corners[8][3] = {
        {mn[0], mn[1], mn[2]}, {mx[0], mn[1], mn[2]},
        {mn[0], mx[1], mn[2]}, {mx[0], mx[1], mn[2]},
        {mn[0], mn[1], mx[2]}, {mx[0], mn[1], mx[2]},
        {mn[0], mx[1], mx[2]}, {mx[0], mx[1], mx[2]},
    };

    vec4 corner, transformed;
    corner[0] = corners[0][0]; corner[1] = corners[0][1]; corner[2] = corners[0][2]; corner[3] = 1.0f;
    glm_mat4_mulv(model, corner, transformed);
    glm_vec3_copy(transformed, out->min);
    glm_vec3_copy(transformed, out->max);

    for (u32 i = 1; i < 8; i++) {
        corner[0] = corners[i][0]; corner[1] = corners[i][1]; corner[2] = corners[i][2]; corner[3] = 1.0f;
        glm_mat4_mulv(model, corner, transformed);
        glm_vec3_minv(out->min, transformed, out->min);
        glm_vec3_maxv(out->max, transformed, out->max);
    }
}
