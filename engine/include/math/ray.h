#pragma once

#include "asset/asset.h"
#include "cglm.h"
#include "defines.h"

typedef struct rl_ray {
    vec3 origin;
    vec3 direction;
} rl_ray;

typedef struct rl_aabb {
    vec3 min;
    vec3 max;
} rl_aabb;

// Build a world-space ray from screen coordinates and inverse camera matrices.
// (sx, sy) are in window/screen space; (vp_x, vp_y, vp_w, vp_h) define the viewport rect.
REALM_API rl_ray ray_from_screen(f32 sx, f32 sy,
                                  f32 vp_x, f32 vp_y, f32 vp_w, f32 vp_h,
                                  mat4 inv_view, mat4 inv_proj);

// Slab-method ray/AABB intersection. Returns true if the ray hits, with closest positive t.
REALM_API b8 ray_intersect_aabb(const rl_ray *ray, const rl_aabb *aabb, f32 *out_t);

// Compute a world-space AABB from a unit cube (-0.5..+0.5) transformed by a model matrix.
REALM_API void aabb_from_unit_cube(mat4 model, rl_aabb *out);

// Compute a world-space AABB from a model/mesh asset's bounds + model matrix.
REALM_API void aabb_from_model_asset(asset_id id, mat4 model, rl_aabb *out);
