#include "core/scene_io.h"

#include "asset/asset.h"
#include "core/component.h"
#include "core/entity.h"
#include "core/logger.h"
#include "util/str.h"

#include "yyjson.h"

#include <string.h>

// --- String ↔ enum helpers ---

static const char *primitive_to_str(rl_frame_primitive p) {
    switch (p) {
        case RL_FRAME_PRIMITIVE_CUBE: return "cube";
        default:                      return "cube";
    }
}

static rl_frame_primitive str_to_primitive(const char *s) {
    if (!s) return RL_FRAME_PRIMITIVE_CUBE;
    if (strcmp(s, "cube") == 0) return RL_FRAME_PRIMITIVE_CUBE;
    return RL_FRAME_PRIMITIVE_CUBE;
}

static const char *kind_to_str(rl_frame_mesh_kind k) {
    switch (k) {
        case RL_FRAME_MESH_KIND_LIT:   return "lit";
        case RL_FRAME_MESH_KIND_UNLIT: return "unlit";
        default:                       return "lit";
    }
}

static rl_frame_mesh_kind str_to_kind(const char *s) {
    if (!s) return RL_FRAME_MESH_KIND_LIT;
    if (strcmp(s, "unlit") == 0) return RL_FRAME_MESH_KIND_UNLIT;
    return RL_FRAME_MESH_KIND_LIT;
}

// --- vec3 helpers ---

static yyjson_mut_val *serialize_vec3(yyjson_mut_doc *doc, vec3 v) {
    yyjson_mut_val *arr = yyjson_mut_arr(doc);
    yyjson_mut_arr_add_real(doc, arr, (f64)v[0]);
    yyjson_mut_arr_add_real(doc, arr, (f64)v[1]);
    yyjson_mut_arr_add_real(doc, arr, (f64)v[2]);
    return arr;
}

static void deserialize_vec3(yyjson_val *arr, vec3 out) {
    if (!arr || yyjson_arr_size(arr) < 3) {
        out[0] = out[1] = out[2] = 0.0f;
        return;
    }
    out[0] = (f32)yyjson_get_num(yyjson_arr_get(arr, 0));
    out[1] = (f32)yyjson_get_num(yyjson_arr_get(arr, 1));
    out[2] = (f32)yyjson_get_num(yyjson_arr_get(arr, 2));
}

// --- Component serializers ---

static yyjson_mut_val *serialize_transform(yyjson_mut_doc *doc, const rl_transform *t) {
    yyjson_mut_val *obj = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_val(doc, obj, "position", serialize_vec3(doc, (f32 *)t->position));
    yyjson_mut_obj_add_val(doc, obj, "rotation", serialize_vec3(doc, (f32 *)t->rotation));
    yyjson_mut_obj_add_val(doc, obj, "scale",    serialize_vec3(doc, (f32 *)t->scale));
    return obj;
}

static void deserialize_transform(yyjson_val *obj, rl_transform *t) {
    deserialize_vec3(yyjson_obj_get(obj, "position"), t->position);
    deserialize_vec3(yyjson_obj_get(obj, "rotation"), t->rotation);
    deserialize_vec3(yyjson_obj_get(obj, "scale"),    t->scale);
    t->dirty = true;
}

static yyjson_mut_val *serialize_mesh(yyjson_mut_doc *doc, const rl_mesh_component *m) {
    yyjson_mut_val *obj = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_str(doc, obj, "primitive", primitive_to_str(m->primitive));
    yyjson_mut_obj_add_str(doc, obj, "kind",      kind_to_str(m->kind));
    yyjson_mut_obj_add_bool(doc, obj, "wireframe", m->wireframe);

    // Mesh asset path
    if (m->mesh_asset) {
        rl_asset *a = asset_get(m->mesh_asset);
        if (a && a->source_path) {
            yyjson_mut_obj_add_strcpy(doc, obj, "mesh_asset_path", a->source_path);
        }
    }

    // Material
    {
        yyjson_mut_val *mat = yyjson_mut_obj(doc);
        if (m->material.diffuse_map) {
            rl_asset *a = asset_get(m->material.diffuse_map);
            if (a && a->source_path) {
                yyjson_mut_obj_add_strcpy(doc, mat, "diffuse_map_path", a->source_path);
            }
        }
        yyjson_mut_obj_add_val(doc, mat, "specular", serialize_vec3(doc, (f32 *)m->material.specular));
        yyjson_mut_obj_add_real(doc, mat, "shininess", (f64)m->material.shininess);
        yyjson_mut_obj_add_val(doc, obj, "material", mat);
    }

    return obj;
}

static void deserialize_mesh(yyjson_val *obj, rl_mesh_component *m) {
    m->primitive = str_to_primitive(yyjson_get_str(yyjson_obj_get(obj, "primitive")));
    m->kind      = str_to_kind(yyjson_get_str(yyjson_obj_get(obj, "kind")));
    m->wireframe = yyjson_get_bool(yyjson_obj_get(obj, "wireframe"));

    // Mesh asset path
    const char *mesh_path = yyjson_get_str(yyjson_obj_get(obj, "mesh_asset_path"));
    m->mesh_asset = mesh_path ? asset_find(mesh_path) : 0;

    // Material
    memset(&m->material, 0, sizeof(rl_material));
    yyjson_val *mat = yyjson_obj_get(obj, "material");
    if (mat) {
        const char *diffuse_path = yyjson_get_str(yyjson_obj_get(mat, "diffuse_map_path"));
        m->material.diffuse_map = diffuse_path ? asset_find(diffuse_path) : 0;
        deserialize_vec3(yyjson_obj_get(mat, "specular"), m->material.specular);
        yyjson_val *shin = yyjson_obj_get(mat, "shininess");
        m->material.shininess = shin ? (f32)yyjson_get_num(shin) : 0.0f;
    }
}

static yyjson_mut_val *serialize_light(yyjson_mut_doc *doc, const rl_light_component *l) {
    yyjson_mut_val *obj = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_val(doc, obj, "ambient",  serialize_vec3(doc, (f32 *)l->ambient));
    yyjson_mut_obj_add_val(doc, obj, "diffuse",  serialize_vec3(doc, (f32 *)l->diffuse));
    yyjson_mut_obj_add_val(doc, obj, "specular", serialize_vec3(doc, (f32 *)l->specular));
    return obj;
}

static void deserialize_light(yyjson_val *obj, rl_light_component *l) {
    deserialize_vec3(yyjson_obj_get(obj, "ambient"),  l->ambient);
    deserialize_vec3(yyjson_obj_get(obj, "diffuse"),  l->diffuse);
    deserialize_vec3(yyjson_obj_get(obj, "specular"), l->specular);
}

// --- Public API ---

b8 scene_save(const rl_scene *scene, const char *path) {
    if (!scene || !path) return false;

    yyjson_mut_doc *doc = yyjson_mut_doc_new(nullptr);
    if (!doc) return false;

    yyjson_mut_val *root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);

    yyjson_mut_obj_add_strcpy(doc, root, "name", scene->name);

    yyjson_mut_val *entities_arr = yyjson_mut_arr(doc);
    yyjson_mut_obj_add_val(doc, root, "entities", entities_arr);

    const rl_entity_store *es    = &scene->entities;
    const rl_component_store *cs = &scene->components;

    for (u32 i = 1; i < es->high_water; i++) {
        if (!es->alive[i]) continue;

        yyjson_mut_val *ent = yyjson_mut_obj(doc);

        // Name
        if (cs->has_name[i]) {
            yyjson_mut_obj_add_strcpy(doc, ent, "name", cs->names[i].name);
        }

        // Transform
        if (cs->has_transform[i]) {
            yyjson_mut_obj_add_val(doc, ent, "transform",
                                   serialize_transform(doc, &cs->transforms[i]));
        }

        // Mesh
        if (cs->has_mesh[i]) {
            yyjson_mut_obj_add_val(doc, ent, "mesh",
                                   serialize_mesh(doc, &cs->meshes[i]));
        }

        // Light
        if (cs->has_light[i]) {
            yyjson_mut_obj_add_val(doc, ent, "light",
                                   serialize_light(doc, &cs->lights[i]));
        }

        yyjson_mut_arr_add_val(entities_arr, ent);
    }

    yyjson_write_err err;
    b8 ok = yyjson_mut_write_file(path, doc, YYJSON_WRITE_PRETTY_TWO_SPACES, nullptr, &err);
    if (!ok) {
        RL_ERROR("scene_save: failed to write '%s': %s", path, err.msg ? err.msg : "unknown");
    }

    yyjson_mut_doc_free(doc);
    return ok;
}

rl_scene *scene_load(const char *path) {
    if (!path) return nullptr;

    yyjson_read_err err;
    yyjson_doc *doc = yyjson_read_file(path, 0, nullptr, &err);
    if (!doc) {
        RL_ERROR("scene_load: failed to read '%s': %s", path, err.msg ? err.msg : "unknown");
        return nullptr;
    }

    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!root) {
        yyjson_doc_free(doc);
        return nullptr;
    }

    const char *scene_name = yyjson_get_str(yyjson_obj_get(root, "name"));
    rl_scene *scene = scene_create(scene_name ? scene_name : "Untitled");
    if (!scene) {
        yyjson_doc_free(doc);
        return nullptr;
    }

    yyjson_val *entities_arr = yyjson_obj_get(root, "entities");
    if (entities_arr) {
        size_t idx, max;
        yyjson_val *ent_val;
        yyjson_arr_foreach(entities_arr, idx, max, ent_val) {
            const char *ent_name = yyjson_get_str(yyjson_obj_get(ent_val, "name"));
            rl_entity e = scene_entity_create(scene, ent_name ? ent_name : "Entity");

            // Transform
            yyjson_val *transform_obj = yyjson_obj_get(ent_val, "transform");
            if (transform_obj) {
                rl_transform *t = transform_add(&scene->components, e);
                deserialize_transform(transform_obj, t);
            }

            // Mesh
            yyjson_val *mesh_obj = yyjson_obj_get(ent_val, "mesh");
            if (mesh_obj) {
                rl_mesh_component *m = mesh_add(&scene->components, e);
                deserialize_mesh(mesh_obj, m);
            }

            // Light
            yyjson_val *light_obj = yyjson_obj_get(ent_val, "light");
            if (light_obj) {
                rl_light_component *l = light_add(&scene->components, e);
                deserialize_light(light_obj, l);
            }
        }
    }

    yyjson_doc_free(doc);
    RL_INFO("Scene '%s' loaded from '%s'", scene->name, path);
    return scene;
}
