#include "vk_mesh.h"
#include "vk_buffer.h"
#include "asset/asset.h"
#include "asset/mesh.h"
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

i32 vk_mesh_cache_find(VK_Context *ctx, asset_id id) {
    for (u32 i = 0; i < ctx->mesh_cache_count; i++) {
        if (ctx->mesh_cache[i].asset_id == id) return (i32)i;
    }
    return -1;
}

i32 vk_mesh_cache_upload(VK_Context *ctx, asset_id id) {
    i32 existing = vk_mesh_cache_find(ctx, id);
    if (existing >= 0) return existing;

    rl_asset *asset = asset_get(id);
    if (!asset || asset->type != ASSET_MESH || !asset->data) {
        RL_ERROR("vk_mesh_cache_upload: invalid mesh asset %u", id);
        return -1;
    }

    if (ctx->mesh_cache_count >= VK_MAX_MESHES) {
        RL_ERROR("VK mesh cache full");
        return -1;
    }

    rl_mesh *mesh = (rl_mesh *)asset->data;
    if (mesh->primitive_count == 0) {
        RL_ERROR("vk_mesh_cache_upload: mesh has no primitives");
        return -1;
    }

    rl_mesh_primitive *prim = &mesh->primitives[0];
    u32 idx = ctx->mesh_cache_count;

    // Upload vertex buffer
    VkDeviceSize vb_size = (VkDeviceSize)prim->vertex_count * sizeof(vertex);
    if (!vk_upload_buffer(ctx, prim->vertices, vb_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                          &ctx->mesh_cache[idx].vertex_buffer, &ctx->mesh_cache[idx].vertex_memory)) {
        return -1;
    }
    ctx->mesh_cache[idx].vertex_count = prim->vertex_count;

    // Upload index buffer if present
    if (prim->indices && prim->index_count > 0) {
        VkDeviceSize ib_size = (VkDeviceSize)prim->index_count * sizeof(u32);
        if (!vk_upload_buffer(ctx, prim->indices, ib_size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                              &ctx->mesh_cache[idx].index_buffer, &ctx->mesh_cache[idx].index_memory)) {
            vk_buffer_destroy(ctx, ctx->mesh_cache[idx].vertex_buffer, ctx->mesh_cache[idx].vertex_memory);
            return -1;
        }
        ctx->mesh_cache[idx].index_count = prim->index_count;
    } else {
        ctx->mesh_cache[idx].index_buffer = VK_NULL_HANDLE;
        ctx->mesh_cache[idx].index_memory = VK_NULL_HANDLE;
        ctx->mesh_cache[idx].index_count = 0;
    }

    ctx->mesh_cache[idx].asset_id = id;
    ctx->mesh_cache_count++;

    RL_DEBUG("Uploaded mesh asset %u to VK GPU (verts=%u, indices=%u)", id, prim->vertex_count, prim->index_count);
    return (i32)idx;
}

void vk_mesh_cache_destroy_all(VK_Context *ctx) {
    for (u32 i = 0; i < ctx->mesh_cache_count; i++) {
        vk_buffer_destroy(ctx, ctx->mesh_cache[i].vertex_buffer, ctx->mesh_cache[i].vertex_memory);
        if (ctx->mesh_cache[i].index_buffer) {
            vk_buffer_destroy(ctx, ctx->mesh_cache[i].index_buffer, ctx->mesh_cache[i].index_memory);
        }
    }
    ctx->mesh_cache_count = 0;
}
