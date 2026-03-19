#include "vk_mesh.h"
#include "vk_buffer.h"
#include "asset/asset.h"
#include "asset/model.h"
#include "renderer/mesh_data.h"

b8 vk_mesh_create_cube(VK_Context *ctx) {
    VkDeviceSize buffer_size = sizeof(vertex) * cube_vertex_count;

    // Staging buffer
    VkBuffer staging;
    VkDeviceMemory staging_mem;
    if (!vk_buffer_create(ctx, buffer_size,
                          VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                          &staging, &staging_mem)) {
        return false;
    }

    void *data;
    vkMapMemory(ctx->device, staging_mem, 0, buffer_size, 0, &data);
    mem_copy(data, cube_vertices, buffer_size);
    vkUnmapMemory(ctx->device, staging_mem);

    // Device-local vertex buffer
    if (!vk_buffer_create(ctx, buffer_size,
                          VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                          &ctx->cube_vertex_buffer, &ctx->cube_vertex_memory)) {
        vk_buffer_destroy(ctx, staging, staging_mem);
        return false;
    }

    VkCommandPool pool = ctx->queue_families.transfer_is_separate ? ctx->transfer_pool : ctx->graphics_pool;
    vk_buffer_copy(ctx, pool, staging, ctx->cube_vertex_buffer, buffer_size);
    vk_buffer_destroy(ctx, staging, staging_mem);

    ctx->cube_vertex_count = cube_vertex_count;
    return true;
}

void vk_mesh_destroy_cube(VK_Context *ctx) {
    vk_buffer_destroy(ctx, ctx->cube_vertex_buffer, ctx->cube_vertex_memory);
}

// Upload vertex (+ optional index) data via staging buffer
static b8 vk_upload_buffer(VK_Context *ctx, void *data, VkDeviceSize size,
                           VkBufferUsageFlagBits usage, VkBuffer *out_buf, VkDeviceMemory *out_mem) {
    VkBuffer staging;
    VkDeviceMemory staging_mem;
    if (!vk_buffer_create(ctx, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                          &staging, &staging_mem)) {
        return false;
    }

    void *mapped;
    vkMapMemory(ctx->device, staging_mem, 0, size, 0, &mapped);
    mem_copy(mapped, data, size);
    vkUnmapMemory(ctx->device, staging_mem);

    if (!vk_buffer_create(ctx, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage,
                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, out_buf, out_mem)) {
        vk_buffer_destroy(ctx, staging, staging_mem);
        return false;
    }

    VkCommandPool pool = ctx->queue_families.transfer_is_separate ? ctx->transfer_pool : ctx->graphics_pool;
    vk_buffer_copy(ctx, pool, staging, *out_buf, size);
    vk_buffer_destroy(ctx, staging, staging_mem);
    return true;
}

// Upload a single sub-mesh's vertex + index buffers into a VK_CachedMesh slot.
static b8 vk_upload_submesh(VK_Context *ctx, VK_CachedMesh *out,
                             vertex *vertices, u32 vertex_count,
                             u32 *indices, u32 index_count) {
    VkDeviceSize vb_size = (VkDeviceSize)vertex_count * sizeof(vertex);
    if (!vk_upload_buffer(ctx, vertices, vb_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                          &out->vertex_buffer, &out->vertex_memory)) {
        return false;
    }
    out->vertex_count = vertex_count;

    if (indices && index_count > 0) {
        VkDeviceSize ib_size = (VkDeviceSize)index_count * sizeof(u32);
        if (!vk_upload_buffer(ctx, indices, ib_size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                              &out->index_buffer, &out->index_memory)) {
            vk_buffer_destroy(ctx, out->vertex_buffer, out->vertex_memory);
            return false;
        }
        out->index_count = index_count;
    } else {
        out->index_buffer = VK_NULL_HANDLE;
        out->index_memory = VK_NULL_HANDLE;
        out->index_count = 0;
    }
    return true;
}

i32 vk_model_cache_find(VK_Context *ctx, asset_id id) {
    for (u32 i = 0; i < ctx->model_cache_count; i++) {
        if (ctx->model_cache[i].model_id == id) return (i32)i;
    }
    return -1;
}

i32 vk_model_cache_upload(VK_Context *ctx, asset_id id) {
    i32 existing = vk_model_cache_find(ctx, id);
    if (existing >= 0) return existing;

    rl_asset *asset = asset_get(id);
    if (!asset || !asset->data) {
        RL_ERROR("vk_model_cache_upload: invalid asset %u", id);
        return -1;
    }

    if (ctx->model_cache_count >= VK_MAX_MODELS) {
        RL_ERROR("VK model cache full");
        return -1;
    }

    u32 idx = ctx->model_cache_count;

    if (asset->type == ASSET_MODEL) {
        rl_model *model = (rl_model *)asset->data;
        if (model->mesh_count == 0) {
            RL_ERROR("vk_model_cache_upload: model has no meshes");
            return -1;
        }

        VK_CachedMesh *meshes = rl_arena_push(&ctx->arena, sizeof(VK_CachedMesh) * model->mesh_count, true);
        for (u32 i = 0; i < model->mesh_count; i++) {
            rl_model_mesh *mm = &model->meshes[i];
            if (!vk_upload_submesh(ctx, &meshes[i], mm->vertices, mm->vertex_count, mm->indices, mm->index_count)) {
                // Clean up already-uploaded sub-meshes
                for (u32 j = 0; j < i; j++) {
                    vk_buffer_destroy(ctx, meshes[j].vertex_buffer, meshes[j].vertex_memory);
                    if (meshes[j].index_buffer) vk_buffer_destroy(ctx, meshes[j].index_buffer, meshes[j].index_memory);
                }
                return -1;
            }
        }

        ctx->model_cache[idx].model_id = id;
        ctx->model_cache[idx].meshes = meshes;
        ctx->model_cache[idx].mesh_count = model->mesh_count;
        ctx->model_cache_count++;

        RL_DEBUG("Uploaded model asset %u to VK (%u sub-meshes)", id, model->mesh_count);
    } else {
        RL_ERROR("vk_model_cache_upload: asset %u is not a model or mesh", id);
        return -1;
    }

    return (i32)idx;
}

void vk_model_cache_destroy_all(VK_Context *ctx) {
    for (u32 i = 0; i < ctx->model_cache_count; i++) {
        for (u32 j = 0; j < ctx->model_cache[i].mesh_count; j++) {
            VK_CachedMesh *cm = &ctx->model_cache[i].meshes[j];
            vk_buffer_destroy(ctx, cm->vertex_buffer, cm->vertex_memory);
            if (cm->index_buffer) vk_buffer_destroy(ctx, cm->index_buffer, cm->index_memory);
        }
    }
    ctx->model_cache_count = 0;
}
