#include "ed_gizmo_transform.h"

#include "engine.h"
#include "memory/arena.h"

#include <math.h>
#include <string.h>

// --- Constants ---

#define SHAFT_LENGTH   1.5f
#define SHAFT_THICK    0.04f
#define TIP_SCALE      0.1f
#define CENTER_SCALE   0.1f
#define HANDLE_INFLATE 0.08f   // extra AABB padding for easier clicking

#define GIZMO_MESH_COUNT 7     // 3 shafts + 3 tips + 1 center cube

static const vec3 AXIS_DIRS[3] = {
    {1.0f, 0.0f, 0.0f},
    {0.0f, 1.0f, 0.0f},
    {0.0f, 0.0f, 1.0f},
};

static const vec3 AXIS_COLORS[3] = {
    {0.9f, 0.2f, 0.2f},  // X = red
    {0.2f, 0.9f, 0.2f},  // Y = green
    {0.2f, 0.4f, 0.9f},  // Z = blue
};

static const vec3 AXIS_COLORS_BRIGHT[3] = {
    {1.0f, 0.5f, 0.5f},
    {0.5f, 1.0f, 0.5f},
    {0.5f, 0.7f, 1.0f},
};

// --- Helpers ---

// Project ray onto an axis-constrained plane. Returns the scalar value along the axis.
// Constraint plane: normal = cross(A, cross(V, A)) where V = ray direction, A = axis dir.
// Plane passes through `center`.
static b8 project_ray_onto_axis(const rl_ray *ray, const vec3 center,
                                 ED_GIZMO_AXIS axis, f32 *out_value) {
    u32 a = (u32)axis - 1; // 0=X, 1=Y, 2=Z
    vec3 A;
    glm_vec3_copy((f32 *)AXIS_DIRS[a], A);

    // N = cross(A, cross(V, A))
    vec3 VxA, N;
    glm_vec3_cross((f32 *)ray->direction, A, VxA);
    glm_vec3_cross(A, VxA, N);

    f32 denom = glm_vec3_dot((f32 *)ray->direction, N);
    if (fabsf(denom) < 1e-6f) return false;

    vec3 diff;
    glm_vec3_sub((f32 *)center, (f32 *)ray->origin, diff);
    f32 t = glm_vec3_dot(diff, N) / denom;

    // Hit point on ray
    vec3 hit;
    glm_vec3_scale((f32 *)ray->direction, t, hit);
    glm_vec3_add((f32 *)ray->origin, hit, hit);

    // Project hit onto axis to get scalar value
    vec3 from_center;
    glm_vec3_sub(hit, (f32 *)center, from_center);
    *out_value = glm_vec3_dot(from_center, A);

    return true;
}

// Build a model matrix for a shaft along a given axis
static void build_shaft_model(mat4 out, const vec3 entity_pos, u32 axis_idx) {
    vec3 shaft_scales[3] = {
        {SHAFT_LENGTH, SHAFT_THICK, SHAFT_THICK},
        {SHAFT_THICK, SHAFT_LENGTH, SHAFT_THICK},
        {SHAFT_THICK, SHAFT_THICK, SHAFT_LENGTH},
    };

    glm_mat4_identity(out);
    glm_translate(out, (f32 *)entity_pos);

    // Offset to center of shaft along axis
    vec3 offset;
    glm_vec3_scale((f32 *)AXIS_DIRS[axis_idx], SHAFT_LENGTH * 0.5f, offset);
    glm_translate(out, offset);
    glm_scale(out, shaft_scales[axis_idx]);
}

// Build a model matrix for a tip at the end of an axis
static void build_tip_model(mat4 out, const vec3 entity_pos, u32 axis_idx) {
    glm_mat4_identity(out);
    glm_translate(out, (f32 *)entity_pos);

    vec3 tip_pos;
    glm_vec3_scale((f32 *)AXIS_DIRS[axis_idx], SHAFT_LENGTH, tip_pos);
    glm_translate(out, tip_pos);

    vec3 s = {TIP_SCALE, TIP_SCALE, TIP_SCALE};
    glm_scale(out, s);
}

// Build AABB for a shaft handle (inflated for clicking)
static void build_shaft_aabb(rl_aabb *out, const vec3 entity_pos, u32 axis_idx) {
    mat4 model;
    build_shaft_model(model, entity_pos, axis_idx);
    aabb_from_unit_cube(model, out);

    // Inflate for easier picking
    for (u32 i = 0; i < 3; i++) {
        out->min[i] -= HANDLE_INFLATE;
        out->max[i] += HANDLE_INFLATE;
    }
}

// Build AABB for a tip handle (inflated for clicking)
static void build_tip_aabb(rl_aabb *out, const vec3 entity_pos, u32 axis_idx) {
    mat4 model;
    build_tip_model(model, entity_pos, axis_idx);
    aabb_from_unit_cube(model, out);

    for (u32 i = 0; i < 3; i++) {
        out->min[i] -= HANDLE_INFLATE;
        out->max[i] += HANDLE_INFLATE;
    }
}

// --- Public API ---

void ed_gizmo_transform_init(ed_gizmo_transform *g) {
    memset(g, 0, sizeof(*g));
    g->mode = ED_GIZMO_TRANSLATE;
}

void ed_gizmo_transform_build(ed_gizmo_transform *g, rl_scene *scene,
                                rl_entity selected, rl_frame_data *frame) {
    if (!g || !scene || !frame) return;
    if (g->mode != ED_GIZMO_TRANSLATE) return;
    if (selected == RL_ENTITY_INVALID) return;
    if (!scene_entity_is_alive(scene, selected)) return;

    rl_transform *t = transform_get(&scene->components, selected);
    if (!t) return;

    rl_arena *fa = rl_engine_get_frame_arena();
    rl_frame_mesh *meshes = rl_arena_push(fa, GIZMO_MESH_COUNT * sizeof(rl_frame_mesh), true);

    for (u32 a = 0; a < 3; a++) {
        // Shaft
        rl_frame_mesh *shaft = &meshes[a * 2];
        shaft->primitive = RL_FRAME_PRIMITIVE_CUBE;
        shaft->kind = RL_FRAME_MESH_KIND_UNLIT;

        b8 highlight = g->dragging && (u32)g->drag_axis == a + 1;
        glm_vec3_copy((f32 *)(highlight ? AXIS_COLORS_BRIGHT[a] : AXIS_COLORS[a]),
                      shaft->material.specular);
        build_shaft_model(shaft->model, t->position, a);

        // Tip
        rl_frame_mesh *tip = &meshes[a * 2 + 1];
        tip->primitive = RL_FRAME_PRIMITIVE_CUBE;
        tip->kind = RL_FRAME_MESH_KIND_UNLIT;
        glm_vec3_copy((f32 *)(highlight ? AXIS_COLORS_BRIGHT[a] : AXIS_COLORS[a]),
                      tip->material.specular);
        build_tip_model(tip->model, t->position, a);
    }

    // Center cube
    rl_frame_mesh *center = &meshes[6];
    center->primitive = RL_FRAME_PRIMITIVE_CUBE;
    center->kind = RL_FRAME_MESH_KIND_UNLIT;
    glm_vec3_copy((vec3){0.9f, 0.9f, 0.9f}, center->material.specular);

    glm_mat4_identity(center->model);
    glm_translate(center->model, t->position);
    vec3 cs = {CENTER_SCALE, CENTER_SCALE, CENTER_SCALE};
    glm_scale(center->model, cs);

    frame->world_overlays = meshes;
    frame->world_overlay_count = GIZMO_MESH_COUNT;
}

ED_GIZMO_AXIS ed_gizmo_transform_pick(ed_gizmo_transform *g, rl_scene *scene,
                                        rl_entity selected, const rl_ray *ray) {
    if (!g || !scene || !ray) return ED_GIZMO_AXIS_NONE;
    if (g->mode != ED_GIZMO_TRANSLATE) return ED_GIZMO_AXIS_NONE;
    if (selected == RL_ENTITY_INVALID) return ED_GIZMO_AXIS_NONE;
    if (!scene_entity_is_alive(scene, selected)) return ED_GIZMO_AXIS_NONE;

    rl_transform *t = transform_get(&scene->components, selected);
    if (!t) return ED_GIZMO_AXIS_NONE;

    f32 closest_t = 1e30f;
    ED_GIZMO_AXIS closest_axis = ED_GIZMO_AXIS_NONE;

    for (u32 a = 0; a < 3; a++) {
        rl_aabb shaft_aabb, tip_aabb;
        build_shaft_aabb(&shaft_aabb, t->position, a);
        build_tip_aabb(&tip_aabb, t->position, a);

        f32 hit_t;
        if (ray_intersect_aabb(ray, &shaft_aabb, &hit_t) && hit_t < closest_t) {
            closest_t = hit_t;
            closest_axis = (ED_GIZMO_AXIS)(a + 1);
        }
        if (ray_intersect_aabb(ray, &tip_aabb, &hit_t) && hit_t < closest_t) {
            closest_t = hit_t;
            closest_axis = (ED_GIZMO_AXIS)(a + 1);
        }
    }

    return closest_axis;
}

void ed_gizmo_transform_drag_begin(ed_gizmo_transform *g, rl_scene *scene,
                                     rl_entity entity, ED_GIZMO_AXIS axis,
                                     const rl_ray *ray) {
    if (!g || !scene || axis == ED_GIZMO_AXIS_NONE) return;

    rl_transform *t = transform_get(&scene->components, entity);
    if (!t) return;

    g->dragging = true;
    g->drag_axis = axis;
    g->drag_entity = entity;
    g->drag_start_transform = *t;

    f32 val = 0.0f;
    project_ray_onto_axis(ray, t->position, axis, &val);
    g->drag_start_axis_value = val;
}

void ed_gizmo_transform_drag_update(ed_gizmo_transform *g, rl_scene *scene,
                                      const rl_ray *ray) {
    if (!g || !scene || !g->dragging) return;

    rl_transform *t = transform_get(&scene->components, g->drag_entity);
    if (!t) { g->dragging = false; return; }

    f32 current_val = 0.0f;
    if (!project_ray_onto_axis(ray, g->drag_start_transform.position,
                                g->drag_axis, &current_val)) {
        return;
    }

    f32 delta = current_val - g->drag_start_axis_value;
    u32 a = (u32)g->drag_axis - 1;

    glm_vec3_copy(g->drag_start_transform.position, t->position);
    t->position[a] += delta;
    t->dirty = true;
}

b8 ed_gizmo_transform_drag_end(ed_gizmo_transform *g) {
    if (!g || !g->dragging) return false;
    g->dragging = false;
    return true; // caller checks actual delta via drag_start_transform
}
