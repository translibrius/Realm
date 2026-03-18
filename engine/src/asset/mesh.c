#define CGLTF_IMPLEMENTATION
#include "mesh.h"

#include "core/logger.h"
#include "memory/arena.h"
#include "platform/io/file_io.h"
#include "util/str.h"

#include "cgltf.h"
#include "cglm.h"

#include <string.h>

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

static b8 load_primitive(rl_arena *asset_arena, const cgltf_primitive *prim, const cgltf_data *gltf, rl_mesh_primitive *out) {
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
            if (prim->attributes[a].index == 0) {
                uv_acc = prim->attributes[a].data;
            }
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
    vertex *verts = rl_arena_push(asset_arena, sizeof(vertex) * vert_count, true);

    for (u32 v = 0; v < vert_count; v++) {
        cgltf_accessor_read_float(pos_acc, v, verts[v].pos, 3);

        if (norm_acc) {
            cgltf_accessor_read_float(norm_acc, v, verts[v].normal, 3);
        }

        if (uv_acc) {
            cgltf_accessor_read_float(uv_acc, v, verts[v].tex_coord, 2);
        }
    }

    out->vertices = verts;
    out->vertex_count = vert_count;

    // Indices
    if (prim->indices) {
        u32 idx_count = (u32)prim->indices->count;
        u32 *indices = rl_arena_push(asset_arena, sizeof(u32) * idx_count, false);
        for (u32 j = 0; j < idx_count; j++) {
            indices[j] = (u32)cgltf_accessor_read_index(prim->indices, j);
        }
        out->indices = indices;
        out->index_count = idx_count;
    }

    // Generate flat normals if missing
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

    return true;
}

b8 load_mesh(rl_arena *asset_arena, rl_asset *asset) {
    rl_temp_arena scratch = rl_arena_scratch_get();

    rl_string path = rl_str_format(scratch.arena, "%s%s", asset_get_resolve_root(ASSET_MESH), asset->source_path);

    // Read file into memory
    rl_file model_file = {0};
    if (!platform_file_open(path.cstr, P_FILE_READ, &model_file)) {
        RL_ERROR("Failed to open mesh file '%s'", path.cstr);
        arena_scratch_release(scratch);
        return false;
    }

    if (!platform_file_read_all(&model_file)) {
        RL_ERROR("Failed to read mesh file '%s'", path.cstr);
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

    // Load external .bin buffers (for .gltf files; no-op for .glb)
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

    // Count total primitives across all meshes
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

    // Allocate output
    rl_mesh *mesh = rl_arena_push(asset_arena, sizeof(rl_mesh), true);
    mesh->primitive_count = total_primitives;
    mesh->primitives = rl_arena_push(asset_arena, sizeof(rl_mesh_primitive) * total_primitives, true);

    // Materials
    mesh->material_count = (u32)data->materials_count;
    if (mesh->material_count > 0) {
        mesh->materials = rl_arena_push(asset_arena, sizeof(rl_mesh_material) * mesh->material_count, true);

        for (u32 i = 0; i < mesh->material_count; i++) {
            const cgltf_material *mat = &data->materials[i];
            rl_mesh_material *out_mat = &mesh->materials[i];

            if (mat->has_pbr_metallic_roughness) {
                const cgltf_pbr_metallic_roughness *pbr = &mat->pbr_metallic_roughness;
                glm_vec3_copy((vec3){pbr->base_color_factor[0], pbr->base_color_factor[1], pbr->base_color_factor[2]},
                              out_mat->base_color_factor);
                out_mat->metallic_factor = pbr->metallic_factor;
                out_mat->roughness_factor = pbr->roughness_factor;

                // Load base color texture if it has an external URI
                if (pbr->base_color_texture.texture && pbr->base_color_texture.texture->image) {
                    const cgltf_image *img = pbr->base_color_texture.texture->image;
                    if (img->uri && !cstr_starts_with(img->uri, "data:")) {
                        // Build relative path: extract directory from model path
                        const char *last_slash = cstr_find_last_char(asset->source_path, '/');
                        if (last_slash) {
                            u64 dir_len = (u64)(last_slash - asset->source_path + 1);
                            rl_string tex_path = rl_str_format(scratch.arena, "%.*s%s",
                                                                  (int)dir_len, asset->source_path, img->uri);
                            out_mat->base_color_texture = asset_load(ASSET_TEXTURE, tex_path.cstr);
                        }
                    } else if (img->buffer_view) {
                        RL_WARN("Embedded textures not yet supported (material %u in '%s')", i, asset->filename);
                    }
                }
            } else {
                glm_vec3_one(out_mat->base_color_factor);
                out_mat->metallic_factor = 1.0f;
                out_mat->roughness_factor = 1.0f;
            }
        }
    }

    // Load primitives
    u32 prim_idx = 0;
    for (cgltf_size m = 0; m < data->meshes_count; m++) {
        const cgltf_mesh *gltf_mesh = &data->meshes[m];
        for (cgltf_size p = 0; p < gltf_mesh->primitives_count; p++) {
            if (!load_primitive(asset_arena, &gltf_mesh->primitives[p], data, &mesh->primitives[prim_idx])) {
                RL_ERROR("Failed to load primitive %u from mesh '%s' in '%s'",
                         (u32)p, gltf_mesh->name ? gltf_mesh->name : "<unnamed>", asset->filename);
                cgltf_free(data);
                arena_scratch_release(scratch);
                return false;
            }
            prim_idx++;
        }
    }

    RL_DEBUG("Loaded mesh '%s': %u primitives, %u materials", asset->filename, mesh->primitive_count, mesh->material_count);

    asset->data = mesh;
    cgltf_free(data);
    arena_scratch_release(scratch);
    return true;
}
