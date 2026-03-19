#include "core/scene.h"
#include "asset/asset.h"
#include "asset/model.h"
#include "engine.h"
#include "util/str.h"

#include <string.h>

rl_scene *scene_create(const char *name) {
    rl_arena *arena = rl_arena_create(MiB(4), KiB(64), MEM_APPLICATION);
    if (!arena) return nullptr;

    rl_scene *scene = rl_arena_push(arena, sizeof(rl_scene), true);
    scene->arena = arena;
    cstr_copy(scene->name, RL_SCENE_NAME_MAX, name ? name : "Untitled");

    entity_store_init(&scene->entities, RL_SCENE_DEFAULT_CAP, arena);
    component_store_init(&scene->components, RL_SCENE_DEFAULT_CAP, arena);

    return scene;
}

void scene_destroy(rl_scene *scene) {
    if (!scene) return;
    rl_arena_destroy(scene->arena);
}

rl_entity scene_entity_create(rl_scene *scene, const char *name) {
    if (!scene) return RL_ENTITY_INVALID;

    rl_entity e = entity_create(&scene->entities);
    if (e == RL_ENTITY_INVALID) return e;

    name_add(&scene->components, e, name);
    return e;
}

void scene_entity_destroy(rl_scene *scene, rl_entity e) {
    if (!scene || e == RL_ENTITY_INVALID) return;

    transform_remove(&scene->components, e);
    mesh_remove(&scene->components, e);
    light_remove(&scene->components, e);
    name_remove(&scene->components, e);
    behavior_comp_remove(&scene->components, e);
    camera_comp_remove(&scene->components, e);

    entity_destroy(&scene->entities, e);
}

b8 scene_entity_is_alive(const rl_scene *scene, rl_entity e) {
    if (!scene) return false;
    return entity_is_alive(&scene->entities, e);
}

rl_entity scene_entity_find(const rl_scene *scene, const char *name) {
    if (!scene || !name) return RL_ENTITY_INVALID;

    const rl_entity_store *es = &scene->entities;
    const rl_component_store *cs = &scene->components;

    for (u32 i = 1; i < es->high_water; i++) {
        if (!es->alive[i]) continue;
        if (cs->has_name[i] && cstr_eq(cs->names[i].name, name)) {
            return rl_entity_pack(i, es->generation[i]);
        }
    }

    return RL_ENTITY_INVALID;
}

rl_entity scene_get_main_camera(const rl_scene *scene) {
    if (!scene) return RL_ENTITY_INVALID;

    const rl_entity_store *es    = &scene->entities;
    const rl_component_store *cs = &scene->components;

    for (u32 i = 1; i < es->high_water; i++) {
        if (!es->alive[i]) continue;
        if (cs->has_camera[i] && cs->cameras[i].is_main) {
            return rl_entity_pack(i, es->generation[i]);
        }
    }

    return RL_ENTITY_INVALID;
}

void scene_build_frame_data(rl_scene *scene, const rl_frame_camera *camera, rl_frame_data *out) {
    if (!scene || !camera || !out) return;

    rl_entity_store *es    = &scene->entities;
    rl_component_store *cs = &scene->components;

    // Count pass — ASSET_MODEL entities expand to N frame_meshes
    u32 mesh_count  = 0;
    u32 light_count = 0;
    for (u32 i = 1; i < es->high_water; i++) {
        if (!es->alive[i]) continue;
        if (cs->has_transform[i] && cs->has_mesh[i]) {
            asset_id mid = cs->meshes[i].model_asset;
            if (mid) {
                rl_asset *a = asset_get(mid);
                if (a && a->type == ASSET_MODEL && a->data) {
                    mesh_count += ((rl_model *)a->data)->mesh_count;
                } else {
                    mesh_count++;
                }
            } else {
                mesh_count++;
            }
        }
        if (cs->has_transform[i] && cs->has_light[i]) {
            light_count++;
            if (!cs->has_mesh[i]) mesh_count++;
        }
    }

    // Allocate from frame arena
    rl_arena *frame_arena = rl_engine_get_frame_arena();
    rl_frame_mesh        *meshes = nullptr;
    rl_frame_point_light *lights = nullptr;

    if (mesh_count > 0)
        meshes = rl_arena_push_array(frame_arena, rl_frame_mesh, mesh_count, true);
    if (light_count > 0)
        lights = rl_arena_push_array(frame_arena, rl_frame_point_light, light_count, true);

    // Fill pass
    u32 mi = 0;
    u32 li = 0;
    for (u32 i = 1; i < es->high_water; i++) {
        if (!es->alive[i]) continue;

        if (cs->has_transform[i]) {
            rl_transform *t = &cs->transforms[i];
            if (t->dirty) transform_update_matrix(t);

            if (cs->has_mesh[i]) {
                rl_mesh_component *mc = &cs->meshes[i];
                rl_entity ent = rl_entity_pack(i, es->generation[i]);

                // Determine sub-mesh count for this entity
                u32 sub_count = 1;
                rl_model *model = nullptr;
                if (mc->model_asset) {
                    rl_asset *a = asset_get(mc->model_asset);
                    if (a && a->type == ASSET_MODEL && a->data) {
                        model = (rl_model *)a->data;
                        sub_count = model->mesh_count;
                    }
                }

                for (u32 p = 0; p < sub_count; p++) {
                    rl_frame_mesh *fm = &meshes[mi++];
                    fm->primitive      = mc->primitive;
                    fm->kind           = mc->kind;
                    fm->wireframe      = mc->wireframe;
                    fm->model_asset    = mc->model_asset;
                    fm->mesh_index     = p;
                    fm->source_entity  = ent;
                    glm_mat4_copy(t->local_to_world, fm->model);

                    // Resolve material: entity override > model per-mesh material > nothing
                    if (mc->material.diffuse_map) {
                        fm->material = mc->material;
                    } else if (model && p < model->mesh_count) {
                        rl_model_mesh *mm = &model->meshes[p];
                        if (mm->material_index < model->material_count) {
                            fm->material.diffuse_map = model->materials[mm->material_index].base_color_texture;
                        }
                        fm->material.specular[0] = mc->material.specular[0];
                        fm->material.specular[1] = mc->material.specular[1];
                        fm->material.specular[2] = mc->material.specular[2];
                        fm->material.shininess   = mc->material.shininess;
                    } else {
                        fm->material = mc->material;
                    }
                }
            }

            if (cs->has_light[i]) {
                rl_light_component *lc = &cs->lights[i];
                rl_frame_point_light *fl = &lights[li++];
                glm_vec3_copy(t->position, fl->position);
                glm_vec3_copy(lc->ambient, fl->ambient);
                glm_vec3_copy(lc->diffuse, fl->diffuse);
                glm_vec3_copy(lc->specular, fl->specular);

                // Emit a small unlit cube to visualize the light source
                if (!cs->has_mesh[i]) {
                    rl_frame_mesh *fm = &meshes[mi++];
                    fm->primitive      = RL_FRAME_PRIMITIVE_CUBE;
                    fm->kind           = RL_FRAME_MESH_KIND_UNLIT;
                    fm->wireframe      = false;
                    fm->model_asset    = 0;
                    fm->source_entity  = rl_entity_pack(i, es->generation[i]);
                    glm_vec3_copy((vec3){1.0f, 1.0f, 1.0f}, fm->material.specular);
                    glm_mat4_copy(t->local_to_world, fm->model);
                    glm_scale_uni(fm->model, 0.15f);
                }
            }
        }
    }

    out->camera           = *camera;
    out->meshes           = meshes;
    out->mesh_count       = mesh_count;
    out->point_lights     = lights;
    out->point_light_count = light_count;
    out->texts            = nullptr;
    out->text_count       = 0;
}
