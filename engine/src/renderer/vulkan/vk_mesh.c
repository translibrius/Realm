#include "vk_mesh.h"
#include "vk_buffer.h"
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
