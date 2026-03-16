#include "ed_gizmo_transform.h"

#include "cglm.h"
#include "engine.h"
#include "memory/arena.h"
#include "platform/input.h"

#include <math.h>
#include <string.h>

// --- Constants ---

#define SHAFT_LENGTH   1.5f
#define SHAFT_THICK    0.04f
#define TIP_SCALE      0.1f
#define CENTER_SCALE   0.1f
#define HANDLE_INFLATE 0.08f   // extra AABB padding for easier clicking

#define TRANSLATE_MESH_COUNT 7     // 3 shafts + 3 tips + 1 center cube
#define SCALE_MESH_COUNT     7     // same layout, different center
#define SCALE_CENTER_SIZE    0.15f

#define RING_RADIUS    1.2f
#define RING_SEGMENTS  24
#define RING_THICK     0.03f
#define RING_TOLERANCE 0.15f
#define ROTATE_MESH_COUNT (3 * RING_SEGMENTS + 1) // 3 rings + center

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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
static b8 project_ray_onto_axis(const rl_ray *ray, const vec3 center,
                                 ED_GIZMO_AXIS axis, f32 *out_value) {
    u32 a = (u32)axis - 1;
    vec3 A;
    glm_vec3_copy((f32 *)AXIS_DIRS[a], A);

    vec3 VxA, N;
    glm_vec3_cross((f32 *)ray->direction, A, VxA);
    glm_vec3_cross(A, VxA, N);

    f32 denom = glm_vec3_dot((f32 *)ray->direction, N);
    if (fabsf(denom) < 1e-6f) return false;

    vec3 diff;
    glm_vec3_sub((f32 *)center, (f32 *)ray->origin, diff);
    f32 t = glm_vec3_dot(diff, N) / denom;

    vec3 hit;
    glm_vec3_scale((f32 *)ray->direction, t, hit);
    glm_vec3_add((f32 *)ray->origin, hit, hit);

    vec3 from_center;
    glm_vec3_sub(hit, (f32 *)center, from_center);
    *out_value = glm_vec3_dot(from_center, A);

    return true;
}

// Intersect ray with plane perpendicular to ring axis, return angle on that plane.
static b8 project_ray_onto_ring_plane(const rl_ray *ray, const vec3 center,
                                       u32 axis_idx, f32 *out_angle) {
    vec3 normal;
    glm_vec3_copy((f32 *)AXIS_DIRS[axis_idx], normal);

    f32 denom = glm_vec3_dot((f32 *)ray->direction, normal);
    if (fabsf(denom) < 1e-6f) return false;

    vec3 diff;
    glm_vec3_sub((f32 *)center, (f32 *)ray->origin, diff);
    f32 t = glm_vec3_dot(diff, normal) / denom;

    vec3 hit;
    glm_vec3_scale((f32 *)ray->direction, t, hit);
    glm_vec3_add((f32 *)ray->origin, hit, hit);

    vec3 from_center;
    glm_vec3_sub(hit, (f32 *)center, from_center);

    // Project onto the two perpendicular axes to get the angle
    u32 a1, a2;
    switch (axis_idx) {
    case 0: a1 = 2; a2 = 1; break; // X ring: angle in YZ plane
    case 1: a1 = 0; a2 = 2; break; // Y ring: angle in XZ plane
    case 2: a1 = 0; a2 = 1; break; // Z ring: angle in XY plane
    default: return false;
    }

    *out_angle = atan2f(from_center[a2], from_center[a1]);
    return true;
}

// Pick test for a ring: ray-plane intersection, check distance from center ~= RING_RADIUS
static b8 pick_ring(const rl_ray *ray, const vec3 center, u32 axis_idx, f32 *out_t) {
    vec3 normal;
    glm_vec3_copy((f32 *)AXIS_DIRS[axis_idx], normal);

    f32 denom = glm_vec3_dot((f32 *)ray->direction, normal);
    if (fabsf(denom) < 1e-6f) return false;

    vec3 diff;
    glm_vec3_sub((f32 *)center, (f32 *)ray->origin, diff);
    f32 t = glm_vec3_dot(diff, normal) / denom;
    if (t < 0.0f) return false;

    vec3 hit;
    glm_vec3_scale((f32 *)ray->direction, t, hit);
    glm_vec3_add((f32 *)ray->origin, hit, hit);

    vec3 from_center;
    glm_vec3_sub(hit, (f32 *)center, from_center);
    f32 dist = glm_vec3_norm(from_center);

    if (fabsf(dist - RING_RADIUS) < RING_TOLERANCE) {
        *out_t = t;
        return true;
    }

    return false;
}

// --- Model matrix builders ---

static void build_shaft_model(mat4 out, const vec3 entity_pos, u32 axis_idx) {
    vec3 shaft_scales[3] = {
        {SHAFT_LENGTH, SHAFT_THICK, SHAFT_THICK},
        {SHAFT_THICK, SHAFT_LENGTH, SHAFT_THICK},
        {SHAFT_THICK, SHAFT_THICK, SHAFT_LENGTH},
    };

    glm_mat4_identity(out);
    glm_translate(out, (f32 *)entity_pos);

    vec3 offset;
    glm_vec3_scale((f32 *)AXIS_DIRS[axis_idx], SHAFT_LENGTH * 0.5f, offset);
    glm_translate(out, offset);
    glm_scale(out, shaft_scales[axis_idx]);
}

static void build_tip_model(mat4 out, const vec3 entity_pos, u32 axis_idx) {
    glm_mat4_identity(out);
    glm_translate(out, (f32 *)entity_pos);

    vec3 tip_pos;
    glm_vec3_scale((f32 *)AXIS_DIRS[axis_idx], SHAFT_LENGTH, tip_pos);
    glm_translate(out, tip_pos);

    vec3 s = {TIP_SCALE, TIP_SCALE, TIP_SCALE};
    glm_scale(out, s);
}

static void build_shaft_aabb(rl_aabb *out, const vec3 entity_pos, u32 axis_idx) {
    mat4 model;
    build_shaft_model(model, entity_pos, axis_idx);
    aabb_from_unit_cube(model, out);

    for (u32 i = 0; i < 3; i++) {
        out->min[i] -= HANDLE_INFLATE;
        out->max[i] += HANDLE_INFLATE;
    }
}

static void build_tip_aabb(rl_aabb *out, const vec3 entity_pos, u32 axis_idx) {
    mat4 model;
    build_tip_model(model, entity_pos, axis_idx);
    aabb_from_unit_cube(model, out);

    for (u32 i = 0; i < 3; i++) {
        out->min[i] -= HANDLE_INFLATE;
        out->max[i] += HANDLE_INFLATE;
    }
}

// --- Build overlay helpers ---

static void build_translate_overlays(ed_gizmo_transform *g, rl_transform *t, rl_frame_data *frame) {
    rl_arena *fa = rl_engine_get_frame_arena();
    rl_frame_mesh *meshes = rl_arena_push(fa, TRANSLATE_MESH_COUNT * sizeof(rl_frame_mesh), true);

    for (u32 a = 0; a < 3; a++) {
        b8 highlight = g->dragging && (u32)g->drag_axis == a + 1;

        rl_frame_mesh *shaft = &meshes[a * 2];
        shaft->primitive = RL_FRAME_PRIMITIVE_CUBE;
        shaft->kind = RL_FRAME_MESH_KIND_UNLIT;
        glm_vec3_copy((f32 *)(highlight ? AXIS_COLORS_BRIGHT[a] : AXIS_COLORS[a]),
                      shaft->material.specular);
        build_shaft_model(shaft->model, t->position, a);

        rl_frame_mesh *tip = &meshes[a * 2 + 1];
        tip->primitive = RL_FRAME_PRIMITIVE_CUBE;
        tip->kind = RL_FRAME_MESH_KIND_UNLIT;
        glm_vec3_copy((f32 *)(highlight ? AXIS_COLORS_BRIGHT[a] : AXIS_COLORS[a]),
                      tip->material.specular);
        build_tip_model(tip->model, t->position, a);
    }

    rl_frame_mesh *center = &meshes[6];
    center->primitive = RL_FRAME_PRIMITIVE_CUBE;
    center->kind = RL_FRAME_MESH_KIND_UNLIT;
    glm_vec3_copy((vec3){0.9f, 0.9f, 0.9f}, center->material.specular);

    glm_mat4_identity(center->model);
    glm_translate(center->model, t->position);
    vec3 cs = {CENTER_SCALE, CENTER_SCALE, CENTER_SCALE};
    glm_scale(center->model, cs);

    frame->world_overlays = meshes;
    frame->world_overlay_count = TRANSLATE_MESH_COUNT;
}

static void build_rotate_overlays(ed_gizmo_transform *g, rl_transform *t, rl_frame_data *frame) {
    rl_arena *fa = rl_engine_get_frame_arena();
    rl_frame_mesh *meshes = rl_arena_push(fa, ROTATE_MESH_COUNT * sizeof(rl_frame_mesh), true);

    f32 seg_length = 2.0f * RING_RADIUS * sinf((f32)M_PI / (f32)RING_SEGMENTS);
    u32 idx = 0;

    for (u32 a = 0; a < 3; a++) {
        b8 highlight = g->dragging && (u32)g->drag_axis == a + 1;

        // Perpendicular translation offset and segment scale depend on ring axis
        vec3 translate_dir = {0};
        vec3 seg_scale;

        switch (a) {
        case 0: // X ring: rotate around X, translate along Y, tangent along Z
            translate_dir[1] = RING_RADIUS;
            glm_vec3_copy((vec3){RING_THICK, RING_THICK, seg_length}, seg_scale);
            break;
        case 1: // Y ring: rotate around Y, translate along X, tangent along Z
            translate_dir[0] = RING_RADIUS;
            glm_vec3_copy((vec3){RING_THICK, RING_THICK, seg_length}, seg_scale);
            break;
        case 2: // Z ring: rotate around Z, translate along X, tangent along Y
            translate_dir[0] = RING_RADIUS;
            glm_vec3_copy((vec3){RING_THICK, seg_length, RING_THICK}, seg_scale);
            break;
        }

        for (u32 i = 0; i < RING_SEGMENTS; i++) {
            f32 theta = (f32)i * 2.0f * (f32)M_PI / (f32)RING_SEGMENTS;

            rl_frame_mesh *seg = &meshes[idx++];
            seg->primitive = RL_FRAME_PRIMITIVE_CUBE;
            seg->kind = RL_FRAME_MESH_KIND_UNLIT;
            glm_vec3_copy((f32 *)(highlight ? AXIS_COLORS_BRIGHT[a] : AXIS_COLORS[a]),
                          seg->material.specular);

            glm_mat4_identity(seg->model);
            glm_translate(seg->model, t->position);
            glm_rotate(seg->model, theta, (f32 *)AXIS_DIRS[a]);
            glm_translate(seg->model, translate_dir);
            glm_scale(seg->model, seg_scale);
        }
    }

    // Center sphere (small white cube)
    rl_frame_mesh *center = &meshes[idx];
    center->primitive = RL_FRAME_PRIMITIVE_CUBE;
    center->kind = RL_FRAME_MESH_KIND_UNLIT;
    glm_vec3_copy((vec3){0.9f, 0.9f, 0.9f}, center->material.specular);
    glm_mat4_identity(center->model);
    glm_translate(center->model, t->position);
    vec3 cs = {CENTER_SCALE, CENTER_SCALE, CENTER_SCALE};
    glm_scale(center->model, cs);

    frame->world_overlays = meshes;
    frame->world_overlay_count = ROTATE_MESH_COUNT;
}

static void build_scale_overlays(ed_gizmo_transform *g, rl_transform *t, rl_frame_data *frame) {
    rl_arena *fa = rl_engine_get_frame_arena();
    rl_frame_mesh *meshes = rl_arena_push(fa, SCALE_MESH_COUNT * sizeof(rl_frame_mesh), true);

    for (u32 a = 0; a < 3; a++) {
        b8 highlight = g->dragging && (u32)g->drag_axis == a + 1;

        rl_frame_mesh *shaft = &meshes[a * 2];
        shaft->primitive = RL_FRAME_PRIMITIVE_CUBE;
        shaft->kind = RL_FRAME_MESH_KIND_UNLIT;
        glm_vec3_copy((f32 *)(highlight ? AXIS_COLORS_BRIGHT[a] : AXIS_COLORS[a]),
                      shaft->material.specular);
        build_shaft_model(shaft->model, t->position, a);

        rl_frame_mesh *tip = &meshes[a * 2 + 1];
        tip->primitive = RL_FRAME_PRIMITIVE_CUBE;
        tip->kind = RL_FRAME_MESH_KIND_UNLIT;
        glm_vec3_copy((f32 *)(highlight ? AXIS_COLORS_BRIGHT[a] : AXIS_COLORS[a]),
                      tip->material.specular);
        build_tip_model(tip->model, t->position, a);
    }

    // Center cube: larger + yellow to distinguish from translate mode
    rl_frame_mesh *center = &meshes[6];
    center->primitive = RL_FRAME_PRIMITIVE_CUBE;
    center->kind = RL_FRAME_MESH_KIND_UNLIT;
    glm_vec3_copy((vec3){0.9f, 0.9f, 0.2f}, center->material.specular);
    glm_mat4_identity(center->model);
    glm_translate(center->model, t->position);
    vec3 cs = {SCALE_CENTER_SIZE, SCALE_CENTER_SIZE, SCALE_CENTER_SIZE};
    glm_scale(center->model, cs);

    frame->world_overlays = meshes;
    frame->world_overlay_count = SCALE_MESH_COUNT;
}

// --- Pick helpers ---

static ED_GIZMO_AXIS pick_translate_or_scale(rl_transform *t, const rl_ray *ray) {
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

static ED_GIZMO_AXIS pick_rotate(rl_transform *t, const rl_ray *ray) {
    f32 closest_t = 1e30f;
    ED_GIZMO_AXIS closest_axis = ED_GIZMO_AXIS_NONE;

    for (u32 a = 0; a < 3; a++) {
        f32 hit_t;
        if (pick_ring(ray, t->position, a, &hit_t) && hit_t < closest_t) {
            closest_t = hit_t;
            closest_axis = (ED_GIZMO_AXIS)(a + 1);
        }
    }

    return closest_axis;
}

// --- Public API ---

void ed_gizmo_transform_init(ed_gizmo_transform *g) {
    memset(g, 0, sizeof(*g));
    g->mode = ED_GIZMO_TRANSLATE;
}

void ed_gizmo_transform_build(ed_gizmo_transform *g, rl_scene *scene,
                                rl_entity selected, rl_frame_data *frame) {
    if (!g || !scene || !frame) return;
    if (selected == RL_ENTITY_INVALID) return;
    if (!scene_entity_is_alive(scene, selected)) return;

    rl_transform *t = transform_get(&scene->components, selected);
    if (!t) return;

    switch (g->mode) {
    case ED_GIZMO_TRANSLATE: build_translate_overlays(g, t, frame); break;
    case ED_GIZMO_ROTATE:    build_rotate_overlays(g, t, frame);    break;
    case ED_GIZMO_SCALE:     build_scale_overlays(g, t, frame);     break;
    }
}

ED_GIZMO_AXIS ed_gizmo_transform_pick(ed_gizmo_transform *g, rl_scene *scene,
                                        rl_entity selected, const rl_ray *ray) {
    if (!g || !scene || !ray) return ED_GIZMO_AXIS_NONE;
    if (selected == RL_ENTITY_INVALID) return ED_GIZMO_AXIS_NONE;
    if (!scene_entity_is_alive(scene, selected)) return ED_GIZMO_AXIS_NONE;

    rl_transform *t = transform_get(&scene->components, selected);
    if (!t) return ED_GIZMO_AXIS_NONE;

    switch (g->mode) {
    case ED_GIZMO_TRANSLATE: return pick_translate_or_scale(t, ray);
    case ED_GIZMO_ROTATE:    return pick_rotate(t, ray);
    case ED_GIZMO_SCALE:     return pick_translate_or_scale(t, ray);
    }

    return ED_GIZMO_AXIS_NONE;
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

    switch (g->mode) {
    case ED_GIZMO_TRANSLATE:
    case ED_GIZMO_SCALE: {
        f32 val = 0.0f;
        project_ray_onto_axis(ray, t->position, axis, &val);
        g->drag_start_axis_value = val;
        break;
    }
    case ED_GIZMO_ROTATE: {
        u32 a = (u32)axis - 1;
        f32 angle = 0.0f;
        project_ray_onto_ring_plane(ray, t->position, a, &angle);
        g->drag_start_angle = angle;
        break;
    }
    }
}

void ed_gizmo_transform_drag_update(ed_gizmo_transform *g, rl_scene *scene,
                                      const rl_ray *ray) {
    if (!g || !scene || !g->dragging) return;

    rl_transform *t = transform_get(&scene->components, g->drag_entity);
    if (!t) { g->dragging = false; return; }

    u32 a = (u32)g->drag_axis - 1;

    switch (g->mode) {
    case ED_GIZMO_TRANSLATE: {
        f32 current_val = 0.0f;
        if (!project_ray_onto_axis(ray, g->drag_start_transform.position,
                                    g->drag_axis, &current_val)) {
            return;
        }
        f32 delta = current_val - g->drag_start_axis_value;
        glm_vec3_copy(g->drag_start_transform.position, t->position);
        t->position[a] += delta;
        t->dirty = true;
        break;
    }
    case ED_GIZMO_ROTATE: {
        f32 current_angle = 0.0f;
        if (!project_ray_onto_ring_plane(ray, g->drag_start_transform.position,
                                          a, &current_angle)) {
            return;
        }
        f32 delta_angle = current_angle - g->drag_start_angle;
        // Wrap to [-PI, PI]
        while (delta_angle > (f32)M_PI)  delta_angle -= 2.0f * (f32)M_PI;
        while (delta_angle < -(f32)M_PI) delta_angle += 2.0f * (f32)M_PI;
        f32 delta_degrees = delta_angle * (180.0f / (f32)M_PI);
        glm_vec3_copy(g->drag_start_transform.rotation, t->rotation);
        t->rotation[a] += delta_degrees;
        t->dirty = true;
        break;
    }
    case ED_GIZMO_SCALE: {
        f32 current_val = 0.0f;
        if (!project_ray_onto_axis(ray, g->drag_start_transform.position,
                                    g->drag_axis, &current_val)) {
            return;
        }
        f32 delta = current_val - g->drag_start_axis_value;
        glm_vec3_copy(g->drag_start_transform.scale, t->scale);
        t->scale[a] += delta;
        if (t->scale[a] < 0.01f) t->scale[a] = 0.01f;
        t->dirty = true;
        break;
    }
    }
}

b8 ed_gizmo_transform_drag_end(ed_gizmo_transform *g) {
    if (!g || !g->dragging) return false;
    g->dragging = false;
    return true; // caller checks actual delta via drag_start_transform
}

ed_gizmo_drag_result ed_gizmo_transform_frame_update(
    ed_gizmo_transform *g, rl_scene *scene,
    const rl_frame_camera *fc, const Clay_BoundingBox *vb) {
    ed_gizmo_drag_result result = {0};
    if (!g || !scene || !g->dragging) return result;

    // Construct ray from current mouse position
    vec2 mpos;
    input_get_mouse_position(mpos);
    rl_viewport_rect vp = {vb->x, vb->y, vb->width, vb->height};

    mat4 view_copy, proj_copy, inv_view, inv_proj;
    memcpy(view_copy, fc->view, sizeof(mat4));
    memcpy(proj_copy, fc->projection, sizeof(mat4));
    glm_mat4_inv(view_copy, inv_view);
    glm_mat4_inv(proj_copy, inv_proj);

    rl_ray ray = ray_from_screen(mpos[0], mpos[1],
                                  vp.x, vp.y, vp.w, vp.h,
                                  inv_view, inv_proj);
    ed_gizmo_transform_drag_update(g, scene, &ray);
    result.scene_dirty = true;

    // End drag when mouse released
    if (!input_is_mouse_down(MOUSE_LEFT)) {
        result.drag_entity = g->drag_entity;
        result.before = g->drag_start_transform;
        if (ed_gizmo_transform_drag_end(g)) {
            result.drag_ended = true;
            rl_transform *after = transform_get(&scene->components, result.drag_entity);
            if (after) {
                result.after = *after;
                result.transform_changed =
                    glm_vec3_distance(result.before.position, after->position) > 1e-5f
                    || glm_vec3_distance(result.before.rotation, after->rotation) > 1e-5f
                    || glm_vec3_distance(result.before.scale, after->scale) > 1e-5f;
            }
        }
    }

    return result;
}
