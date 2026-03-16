#include "core/component.h"
#include "util/str.h"

#include <string.h>

void component_store_init(rl_component_store *store, u32 capacity, rl_arena *arena) {
    if (!store || !arena || capacity == 0) return;

    store->capacity = capacity;

    store->transforms    = rl_arena_push_aligned(arena, capacity * sizeof(rl_transform), 16, true);
    store->has_transform = rl_arena_push(arena, capacity * sizeof(b8), true);

    store->meshes   = rl_arena_push(arena, capacity * sizeof(rl_mesh_component), true);
    store->has_mesh = rl_arena_push(arena, capacity * sizeof(b8), true);

    store->lights    = rl_arena_push(arena, capacity * sizeof(rl_light_component), true);
    store->has_light = rl_arena_push(arena, capacity * sizeof(b8), true);

    store->names    = rl_arena_push(arena, capacity * sizeof(rl_name_component), true);
    store->has_name = rl_arena_push(arena, capacity * sizeof(b8), true);

    store->behaviors    = rl_arena_push(arena, capacity * sizeof(rl_behavior_component), true);
    store->has_behavior = rl_arena_push(arena, capacity * sizeof(b8), true);
}

// --- Transform ---

rl_transform *transform_add(rl_component_store *s, rl_entity e) {
    u32 idx = rl_entity_index(e);
    if (!s || idx >= s->capacity) return nullptr;

    rl_transform *t = &s->transforms[idx];
    glm_vec3_zero(t->position);
    glm_vec3_zero(t->rotation);
    glm_vec3_one(t->scale);
    glm_mat4_identity(t->local_to_world);
    t->dirty = true;
    s->has_transform[idx] = true;
    return t;
}

rl_transform *transform_get(rl_component_store *s, rl_entity e) {
    u32 idx = rl_entity_index(e);
    if (!s || idx >= s->capacity || !s->has_transform[idx]) return nullptr;
    return &s->transforms[idx];
}

void transform_remove(rl_component_store *s, rl_entity e) {
    u32 idx = rl_entity_index(e);
    if (!s || idx >= s->capacity) return;
    s->has_transform[idx] = false;
}

void transform_update_matrix(rl_transform *t) {
    if (!t) return;

    glm_mat4_identity(t->local_to_world);
    glm_translate(t->local_to_world, t->position);
    glm_rotate_y(t->local_to_world, glm_rad(t->rotation[1]), t->local_to_world);
    glm_rotate_x(t->local_to_world, glm_rad(t->rotation[0]), t->local_to_world);
    glm_rotate_z(t->local_to_world, glm_rad(t->rotation[2]), t->local_to_world);
    glm_scale(t->local_to_world, t->scale);
    t->dirty = false;
}

// --- Mesh ---

rl_mesh_component *mesh_add(rl_component_store *s, rl_entity e) {
    u32 idx = rl_entity_index(e);
    if (!s || idx >= s->capacity) return nullptr;

    rl_mesh_component *m = &s->meshes[idx];
    m->primitive  = RL_FRAME_PRIMITIVE_CUBE;
    m->kind       = RL_FRAME_MESH_KIND_LIT;
    m->wireframe  = false;
    m->mesh_asset = 0;
    memset(&m->material, 0, sizeof(rl_material));
    s->has_mesh[idx] = true;
    return m;
}

rl_mesh_component *mesh_get(rl_component_store *s, rl_entity e) {
    u32 idx = rl_entity_index(e);
    if (!s || idx >= s->capacity || !s->has_mesh[idx]) return nullptr;
    return &s->meshes[idx];
}

void mesh_remove(rl_component_store *s, rl_entity e) {
    u32 idx = rl_entity_index(e);
    if (!s || idx >= s->capacity) return;
    s->has_mesh[idx] = false;
}

// --- Light ---

rl_light_component *light_add(rl_component_store *s, rl_entity e) {
    u32 idx = rl_entity_index(e);
    if (!s || idx >= s->capacity) return nullptr;

    rl_light_component *l = &s->lights[idx];
    glm_vec3_copy((vec3){0.3f, 0.3f, 0.3f}, l->ambient);
    glm_vec3_copy((vec3){0.9f, 0.9f, 0.9f}, l->diffuse);
    glm_vec3_copy((vec3){1.0f, 1.0f, 1.0f}, l->specular);
    s->has_light[idx] = true;
    return l;
}

rl_light_component *light_get(rl_component_store *s, rl_entity e) {
    u32 idx = rl_entity_index(e);
    if (!s || idx >= s->capacity || !s->has_light[idx]) return nullptr;
    return &s->lights[idx];
}

void light_remove(rl_component_store *s, rl_entity e) {
    u32 idx = rl_entity_index(e);
    if (!s || idx >= s->capacity) return;
    s->has_light[idx] = false;
}

// --- Name ---

rl_name_component *name_add(rl_component_store *s, rl_entity e, const char *name) {
    u32 idx = rl_entity_index(e);
    if (!s || idx >= s->capacity) return nullptr;

    rl_name_component *n = &s->names[idx];
    cstr_copy(n->name, RL_NAME_MAX, name ? name : "Entity");
    s->has_name[idx] = true;
    return n;
}

rl_name_component *name_get(rl_component_store *s, rl_entity e) {
    u32 idx = rl_entity_index(e);
    if (!s || idx >= s->capacity || !s->has_name[idx]) return nullptr;
    return &s->names[idx];
}

void name_remove(rl_component_store *s, rl_entity e) {
    u32 idx = rl_entity_index(e);
    if (!s || idx >= s->capacity) return;
    s->has_name[idx] = false;
}

// --- Behavior ---

rl_behavior_component *behavior_comp_add(rl_component_store *s, rl_entity e, const char *name) {
    u32 idx = rl_entity_index(e);
    if (!s || idx >= s->capacity) return nullptr;

    rl_behavior_component *b = &s->behaviors[idx];
    cstr_copy(b->name, RL_BEHAVIOR_NAME_MAX, name ? name : "");
    s->has_behavior[idx] = true;
    return b;
}

rl_behavior_component *behavior_comp_get(rl_component_store *s, rl_entity e) {
    u32 idx = rl_entity_index(e);
    if (!s || idx >= s->capacity || !s->has_behavior[idx]) return nullptr;
    return &s->behaviors[idx];
}

void behavior_comp_remove(rl_component_store *s, rl_entity e) {
    u32 idx = rl_entity_index(e);
    if (!s || idx >= s->capacity) return;
    s->has_behavior[idx] = false;
}
