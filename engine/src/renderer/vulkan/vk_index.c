#include "vk_index.h"

#include "vk_buffer.h"

b8 vk_index_create_buffer(VK_Context *context, Indices *indices) {
    if (indices->count <= 0) {
        RL_ERROR("failed to create index buffer, number of indices must be greater than 0");
        return false;
    }

    VkDeviceSize buffer_size = sizeof(indices[0]) * indices->count;

    VkBuffer staging_buffer;
    VkDeviceMemory staging_memory;
    if (!vk_buffer_create(
        context,
        buffer_size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &staging_buffer, &staging_memory)) {
        RL_ERROR("Failed to create staging buffer");
        return false;
    }

    void *data;
    VK_CHECK_RETURN_FALSE(vkMapMemory(context->device, staging_memory, 0, buffer_size, 0, &data), "Failed to map staging buffer memory.");
    mem_copy(indices->items, data, (u8)buffer_size);
    vkUnmapMemory(context->device, staging_memory);

    if (!vk_buffer_create(
        context,
        buffer_size,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &context->index_buffer,
        &context->index_buffer_memory)) {
        RL_ERROR("Failed to create index buffer");
        return false;
    }

    // Copy the data from staging buffer to index buffer
    VkCommandPool pool = context->queue_families.transfer_is_separate ? context->transfer_pool : context->graphics_pool;
    if (!vk_buffer_copy(context, pool, staging_buffer, context->index_buffer, buffer_size)) {
        RL_ERROR("Failed to copy staging buffer to index buffer");
        return false;
    }

    // Clean up staging buffer+mem on GPU
    vk_buffer_destroy(context, staging_buffer, staging_memory);
    return true;
}

void vk_index_destroy_buffer(VK_Context *context) {
    vk_buffer_destroy(context, context->index_buffer, context->index_buffer_memory);
}