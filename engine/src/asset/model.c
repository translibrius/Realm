#define CGLTF_IMPLEMENTATION
#include "model.h"

#include "core/logger.h"
#include "memory/arena.h"
#include "platform/io/file_io.h"
#include "util/str.h"

#include "cgltf.h"
#include "cglm.h"

// ── flat-normal generation (same as mesh.c) ─────────────────────────────

static void generate_flat_normals_indexed(vertex *verts, const u32 *indices, u32 index_count) {
    for (u32 i = 0; i + 2 < index_count; i += 3) {
        vec3 edge1, edge2, normal;
        glm_vec3_sub(verts[indices[i + 1]].pos, verts[indices[i]].pos, edge1);
        glm_vec3_sub(verts[indices[i + 2]].pos, verts[indices[i]].pos, edge2);
        glm_vec3_cross(edge1, edge2, normal);
        glm_vec3_normalize(normal);
        for (u32 j = 0; j < 3; j++) {
            glm_vec3_copy(normal, verts[indices[i + j]].normal);
        }
    }
}

static void generate_flat_normals(vertex *verts, u32 vertex_count) {
    for (u32 i = 0; i + 2 < vertex_count; i += 3) {
        vec3 edge1, edge2, normal;
        glm_vec3_sub(verts[i + 1].pos, verts[i].pos, edge1);
        glm_vec3_sub(verts[i + 2].pos, verts[i].pos, edge2);
        glm_vec3_cross(edge1, edge2, normal);
        glm_vec3_normalize(normal);
        for (u32 j = 0; j < 3; j++) {
            glm_vec3_copy(normal, verts[i + j].normal);
        }
    }
}

// ── primitive loading ───────────────────────────────────────────────────

static b8 load_primitive(rl_arena *arena, const cgltf_primitive *prim, const cgltf_data *gltf,
                          mat4 world_transform, rl_model_mesh *out) {
    const cgltf_accessor *pos_acc = nullptr;
    const cgltf_accessor *norm_acc = nullptr;
    const cgltf_accessor *uv_acc = nullptr;

    for (cgltf_size a = 0; a < prim->attributes_count; a++) {
        switch (prim->attributes[a].type) {
        case cgltf_attribute_type_position:
            pos_acc = prim->attributes[a].data;
            break;
        case cgltf_attribute_type_normal:
            norm_acc = prim->attributes[a].data;
            break;
        case cgltf_attribute_type_texcoord:
            if (prim->attributes[a].index == 0) uv_acc = prim->attributes[a].data;
            break;
        default:
            break;
        }
    }

    if (!pos_acc) {
        RL_ERROR("glTF primitive has no POSITION attribute");
        return false;
    }

    u32 vert_count = (u32)pos_acc->count;
    vertex *verts = rl_arena_push(arena, sizeof(vertex) * vert_count, true);

    for (u32 v = 0; v < vert_count; v++) {
        cgltf_accessor_read_float(pos_acc, v, verts[v].pos, 3);
        if (norm_acc) cgltf_accessor_read_float(norm_acc, v, verts[v].normal, 3);
        if (uv_acc) cgltf_accessor_read_float(uv_acc, v, verts[v].tex_coord, 2);
    }

    // Bake world transform into positions and normals
    {
        // Normal matrix: inverse-transpose of upper-left 3x3
        mat3 normal_mat;
        glm_mat4_pick3(world_transform, normal_mat);
        // For uniform scale + rotation, transpose of the 3x3 is the inverse-transpose.
        // For non-uniform scale we'd need full inverse-transpose. Doing it properly:
        mat4 inv;
        glm_mat4_inv(world_transform, inv);
        mat3 inv3;
        glm_mat4_pick3(inv, inv3);
        glm_mat3_transpose(inv3); // inv3 is now the normal matrix

        for (u32 v = 0; v < vert_count; v++) {
            // Transform position
            vec4 p = {verts[v].pos[0], verts[v].pos[1], verts[v].pos[2], 1.0f};
            vec4 tp;
            glm_mat4_mulv(world_transform, p, tp);
            glm_vec3_copy(tp, verts[v].pos);

            // Transform normal
            if (norm_acc) {
                glm_mat3_mulv(inv3, verts[v].normal, verts[v].normal);
                glm_vec3_normalize(verts[v].normal);
            }
        }
    }

    out->vertices = verts;
    out->vertex_count = vert_count;

    // Indices
    if (prim->indices) {
        u32 idx_count = (u32)prim->indices->count;
        u32 *indices = rl_arena_push(arena, sizeof(u32) * idx_count, false);
        for (u32 j = 0; j < idx_count; j++) {
            indices[j] = (u32)cgltf_accessor_read_index(prim->indices, j);
        }
        out->indices = indices;
        out->index_count = idx_count;
    }

    // Generate flat normals if the source had none (after transform bake)
    if (!norm_acc) {
        if (out->indices) {
            generate_flat_normals_indexed(verts, out->indices, out->index_count);
        } else {
            generate_flat_normals(verts, vert_count);
        }
    }

    // Material index
    if (prim->material) {
        out->material_index = (u32)(prim->material - gltf->materials);
    }

    // Compute AABB (post-transform)
    if (vert_count > 0) {
        glm_vec3_copy(verts[0].pos, out->aabb_min);
        glm_vec3_copy(verts[0].pos, out->aabb_max);
        for (u32 v = 1; v < vert_count; v++) {
            glm_vec3_minv(out->aabb_min, verts[v].pos, out->aabb_min);
            glm_vec3_maxv(out->aabb_max, verts[v].pos, out->aabb_max);
        }
    }

    return true;
}

// ── node tree counting ──────────────────────────────────────────────────

static u32 count_node_primitives(const cgltf_node *node) {
    u32 count = 0;
    if (node->mesh) {
        count += (u32)node->mesh->primitives_count;
    }
    for (cgltf_size i = 0; i < node->children_count; i++) {
        count += count_node_primitives(node->children[i]);
    }
    return count;
}

// ── node tree traversal ─────────────────────────────────────────────────

typedef struct model_load_ctx {
    rl_arena *arena;
    const cgltf_data *gltf;
    rl_model_mesh *meshes;
    u32 mesh_idx;
} model_load_ctx;

static b8 process_node(model_load_ctx *ctx, const cgltf_node *node, mat4 parent_transform) {
    mat4 local;
    cgltf_node_transform_local(node, (f32 *)local);

    mat4 world;
    glm_mat4_mul(parent_transform, local, world);

    if (node->mesh) {
        for (cgltf_size p = 0; p < node->mesh->primitives_count; p++) {
            if (!load_primitive(ctx->arena, &node->mesh->primitives[p], ctx->gltf,
                                world, &ctx->meshes[ctx->mesh_idx])) {
                return false;
            }
            ctx->mesh_idx++;
        }
    }

    for (cgltf_size i = 0; i < node->children_count; i++) {
        if (!process_node(ctx, node->children[i], world)) {
            return false;
        }
    }

    return true;
}

// ── material loading ────────────────────────────────────────────────────

static void load_materials(rl_arena *arena, const cgltf_data *data, const char *source_path,
                           rl_model *model) {
    model->material_count = (u32)data->materials_count;
    if (model->material_count == 0) return;

    model->materials = rl_arena_push(arena, sizeof(rl_model_material) * model->material_count, true);

    rl_temp_arena scratch = rl_arena_scratch_get();

    for (u32 i = 0; i < model->material_count; i++) {
        const cgltf_material *mat = &data->materials[i];
        rl_model_material *out = &model->materials[i];

        if (mat->has_pbr_metallic_roughness) {
            const cgltf_pbr_metallic_roughness *pbr = &mat->pbr_metallic_roughness;
            glm_vec3_copy((vec3){pbr->base_color_factor[0], pbr->base_color_factor[1],
                                 pbr->base_color_factor[2]},
                          out->base_color_factor);
            out->metallic_factor = pbr->metallic_factor;
            out->roughness_factor = pbr->roughness_factor;

            if (pbr->base_color_texture.texture && pbr->base_color_texture.texture->image) {
                const cgltf_image *img = pbr->base_color_texture.texture->image;
                if (img->uri && !cstr_starts_with(img->uri, "data:")) {
                    const char *last_slash = cstr_find_last_char(source_path, '/');
                    if (last_slash) {
                        u64 dir_len = (u64)(last_slash - source_path + 1);
                        rl_string tex_path = rl_str_format(scratch.arena, "%.*s%s",
                                                           (int)dir_len, source_path, img->uri);
                        out->base_color_texture = asset_load(ASSET_TEXTURE, tex_path.cstr);
                    }
                } else if (img->buffer_view) {
                    RL_WARN("Embedded textures not yet supported (material %u)", i);
                }
            }
        } else {
            glm_vec3_one(out->base_color_factor);
            out->metallic_factor = 1.0f;
            out->roughness_factor = 1.0f;
        }
    }

    arena_scratch_release(scratch);
}

// ── public entry point ──────────────────────────────────────────────────

b8 load_model(rl_arena *asset_arena, rl_asset *asset) {
    rl_temp_arena scratch = rl_arena_scratch_get();

    rl_string path = rl_str_format(scratch.arena, "%s%s",
                                   asset_get_resolve_root(ASSET_MODEL), asset->source_path);

    // Read file
    rl_file model_file = {0};
    if (!platform_file_open(path.cstr, P_FILE_READ, &model_file)) {
        RL_ERROR("Failed to open model file '%s'", path.cstr);
        arena_scratch_release(scratch);
        return false;
    }

    if (!platform_file_read_all(&model_file)) {
        RL_ERROR("Failed to read model file '%s'", path.cstr);
        platform_file_close(&model_file);
        arena_scratch_release(scratch);
        return false;
    }

    // Parse glTF/GLB
    cgltf_options options = {0};
    cgltf_data *data = nullptr;
    cgltf_result result = cgltf_parse(&options, model_file.buf, model_file.buf_len, &data);
    platform_file_close(&model_file);

    if (result != cgltf_result_success) {
        RL_ERROR("Failed to parse glTF '%s' (cgltf error %d)", asset->filename, result);
        arena_scratch_release(scratch);
        return false;
    }

    result = cgltf_load_buffers(&options, data, path.cstr);
    if (result != cgltf_result_success) {
        RL_ERROR("Failed to load glTF buffers for '%s' (cgltf error %d)", asset->filename, result);
        cgltf_free(data);
        arena_scratch_release(scratch);
        return false;
    }

    result = cgltf_validate(data);
    if (result != cgltf_result_success) {
        RL_WARN("glTF validation warnings for '%s' (cgltf result %d)", asset->filename, result);
    }

    // Allocate model
    rl_model *model = rl_arena_push(asset_arena, sizeof(rl_model), true);

    // Load materials first (primitives may reference them by index)
    load_materials(asset_arena, data, asset->source_path, model);

    // Determine if we have a scene with nodes, or just flat meshes
    b8 has_scenes = data->scenes_count > 0 && data->scene;
    b8 has_root_nodes = false;
    if (has_scenes) {
        has_root_nodes = data->scene->nodes_count > 0;
    }

    if (has_root_nodes) {
        // Count total primitives via node tree
        u32 total_meshes = 0;
        for (cgltf_size n = 0; n < data->scene->nodes_count; n++) {
            total_meshes += count_node_primitives(data->scene->nodes[n]);
        }

        if (total_meshes == 0) {
            RL_ERROR("glTF '%s' scene has nodes but no mesh primitives", asset->filename);
            cgltf_free(data);
            arena_scratch_release(scratch);
            return false;
        }

        model->mesh_count = total_meshes;
        model->meshes = rl_arena_push(asset_arena, sizeof(rl_model_mesh) * total_meshes, true);

        model_load_ctx ctx = {
            .arena = asset_arena,
            .gltf = data,
            .meshes = model->meshes,
            .mesh_idx = 0,
        };

        mat4 identity;
        glm_mat4_identity(identity);

        for (cgltf_size n = 0; n < data->scene->nodes_count; n++) {
            if (!process_node(&ctx, data->scene->nodes[n], identity)) {
                RL_ERROR("Failed to process node tree in '%s'", asset->filename);
                cgltf_free(data);
                arena_scratch_release(scratch);
                return false;
            }
        }
    } else {
        // Fallback: flat iteration over data->meshes[] with identity transform
        u32 total_primitives = 0;
        for (cgltf_size m = 0; m < data->meshes_count; m++) {
            total_primitives += (u32)data->meshes[m].primitives_count;
        }

        if (total_primitives == 0) {
            RL_ERROR("glTF '%s' contains no mesh primitives", asset->filename);
            cgltf_free(data);
            arena_scratch_release(scratch);
            return false;
        }

        model->mesh_count = total_primitives;
        model->meshes = rl_arena_push(asset_arena, sizeof(rl_model_mesh) * total_primitives, true);

        mat4 identity;
        glm_mat4_identity(identity);

        u32 mi = 0;
        for (cgltf_size m = 0; m < data->meshes_count; m++) {
            const cgltf_mesh *gltf_mesh = &data->meshes[m];
            for (cgltf_size p = 0; p < gltf_mesh->primitives_count; p++) {
                if (!load_primitive(asset_arena, &gltf_mesh->primitives[p], data,
                                    identity, &model->meshes[mi])) {
                    RL_ERROR("Failed to load primitive %u from mesh '%s' in '%s'",
                             (u32)p, gltf_mesh->name ? gltf_mesh->name : "<unnamed>", asset->filename);
                    cgltf_free(data);
                    arena_scratch_release(scratch);
                    return false;
                }
                mi++;
            }
        }
    }

    // Compute whole-model AABB (union of all mesh AABBs)
    if (model->mesh_count > 0) {
        glm_vec3_copy(model->meshes[0].aabb_min, model->aabb_min);
        glm_vec3_copy(model->meshes[0].aabb_max, model->aabb_max);
        for (u32 i = 1; i < model->mesh_count; i++) {
            glm_vec3_minv(model->aabb_min, model->meshes[i].aabb_min, model->aabb_min);
            glm_vec3_maxv(model->aabb_max, model->meshes[i].aabb_max, model->aabb_max);
        }
    }

    RL_DEBUG("Loaded model '%s': %u meshes, %u materials, AABB [%.2f,%.2f,%.2f]-[%.2f,%.2f,%.2f]",
             asset->filename, model->mesh_count, model->material_count,
             model->aabb_min[0], model->aabb_min[1], model->aabb_min[2],
             model->aabb_max[0], model->aabb_max[1], model->aabb_max[2]);

    asset->data = model;
    cgltf_free(data);
    arena_scratch_release(scratch);
    return true;
}
