#include "scene/ed_entity_ops.h"

#include "core/component.h"
#include "core/scene.h"
#include "renderer/frame_data.h"

rl_entity ed_entity_create_empty(rl_scene *scene, const char *name) {
    rl_entity e = scene_entity_create(scene, name);
    rl_transform *tr = transform_add(&scene->components, e);
    tr->scale[0] = tr->scale[1] = tr->scale[2] = 1.0f;
    tr->dirty = true;
    return e;
}

rl_entity ed_entity_create_light(rl_scene *scene) {
    rl_entity e = scene_entity_create(scene, "Light");
    rl_component_store *cs = &scene->components;
    rl_transform *tr = transform_add(cs, e);
    tr->position[1] = 3.0f;
    tr->scale[0] = tr->scale[1] = tr->scale[2] = 1.0f;
    tr->dirty = true;
    rl_light_component *lc = light_add(cs, e);
    lc->ambient[0] = lc->ambient[1] = lc->ambient[2] = 0.1f;
    lc->diffuse[0] = lc->diffuse[1] = lc->diffuse[2] = 0.8f;
    lc->specular[0] = lc->specular[1] = lc->specular[2] = 1.0f;
    return e;
}

rl_entity ed_entity_create_cube(rl_scene *scene) {
    rl_entity e = scene_entity_create(scene, "Cube");
    rl_component_store *cs = &scene->components;
    rl_transform *tr = transform_add(cs, e);
    tr->scale[0] = tr->scale[1] = tr->scale[2] = 1.0f;
    tr->dirty = true;
    rl_mesh_component *mc = mesh_add(cs, e);
    mc->kind = RL_FRAME_MESH_KIND_LIT;
    mc->primitive = RL_FRAME_PRIMITIVE_CUBE;
    mc->material.specular[0] = mc->material.specular[1] = mc->material.specular[2] = 0.5f;
    mc->material.shininess = 32.0f;
    return e;
}

rl_entity ed_entity_create_camera(rl_scene *scene) {
    rl_entity e = scene_entity_create(scene, "Camera");
    rl_component_store *cs = &scene->components;
    rl_transform *tr = transform_add(cs, e);
    tr->position[2] = 5.0f;
    tr->scale[0] = tr->scale[1] = tr->scale[2] = 1.0f;
    tr->dirty = true;
    rl_camera_component *cc = camera_comp_add(cs, e);
    cc->fov = 90.0f;
    cc->near_clip = 0.1f;
    cc->far_clip = 100.0f;
    cc->is_main = false;
    return e;
}

rl_entity ed_entity_duplicate(rl_scene *scene, rl_entity source) {
    u32 idx = rl_entity_index(source);
    rl_component_store *cs = &scene->components;
    const char *name = cs->has_name[idx] ? cs->names[idx].name : "Entity";
    rl_entity dup = scene_entity_create(scene, name);
    rl_transform *tr = transform_get(cs, source);
    if (tr) {
        rl_transform *dst = transform_add(cs, dup);
        *dst = *tr;
        dst->dirty = true;
    }
    return dup;
}

void ed_entity_delete(rl_scene *scene, rl_entity entity) {
    scene_entity_destroy(scene, entity);
}
