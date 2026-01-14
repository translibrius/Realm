#include "vk_buffer.h"

u32 find_memory_type(VK_Context *context, u32 type_filter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties mem_properties;
    vkGetPhysicalDeviceMemoryProperties(context->physical_device, &mem_properties);

    for (u32 i = 0; i < mem_properties.memoryTypeCount; i++) {
        if ((type_filter & (1 << i)) && (mem_properties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    RL_FATAL("failed to find suitable memory type");
    return -1;
}

// ------- GENERAL_BUF ----------

b8 vk_buffer_create(VK_Context *context, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags mem_props, VkBuffer *buffer, VkDeviceMemory *memory) {
    VkSharingMode sharing_mode = context->queue_families.transfer_is_separate ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE;

    VkBufferCreateInfo buffer_create_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = sharing_mode, // Graphics & Transfer
    };

    VkResult result = vkCreateBuffer(context->device, &buffer_create_info, nullptr, buffer);
    if (result != VK_SUCCESS) {
        RL_ERROR("Failed to create buffer");
        return false;
    }

    VkMemoryRequirements memory_requirements;
    vkGetBufferMemoryRequirements(context->device, *buffer, &memory_requirements);

    VkMemoryAllocateInfo allocate_info = {0};
    allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocate_info.allocationSize = memory_requirements.size;
    allocate_info.memoryTypeIndex = find_memory_type(context, memory_requirements.memoryTypeBits, mem_props);

    result = vkAllocateMemory(context->device, &allocate_info, nullptr, memory);
    if (result != VK_SUCCESS) {
        RL_ERROR("failed to allocate buffer memory");
        return false;
    }

    VK_CHECK(vkBindBufferMemory(context->device, *buffer, *memory, 0));

    return true;
}

void vk_buffer_destroy(VK_Context *context, VkBuffer buffer, VkDeviceMemory memory) {
    vkDestroyBuffer(context->device, buffer, nullptr);
    vkFreeMemory(context->device, memory, nullptr);
}

b8 vk_buffer_copy(VK_Context *context, VkCommandPool cmd_pool, VkBuffer src, VkBuffer dst, VkDeviceSize size) {
    // Allocate command buffer to record the copy command
    VkCommandBufferAllocateInfo allocate_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandPool = cmd_pool,
        .commandBufferCount = 1
    };

    VkCommandBuffer command_buffer;
    VkResult result = vkAllocateCommandBuffers(context->device, &allocate_info, &command_buffer);
    if (result != VK_SUCCESS) {
        RL_ERROR("Failed to alloc command buffer. VkResult=%s", string_VkResult(result));
        return false;
    }

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };

    // Record copy command
    VK_CHECK_RETURN_FALSE(vkBeginCommandBuffer(command_buffer, &begin_info), "Failed to begin command buffer");
    {
        VkBufferCopy copy_region = {
            .srcOffset = 0,
            .dstOffset = 0,
            .size = size,
        };
        vkCmdCopyBuffer(command_buffer, src, dst, 1, &copy_region);
    }
    VK_CHECK_RETURN_FALSE(vkEndCommandBuffer(command_buffer), "Failed to end command buffer");

    // Submit command buffer to transfer queue
    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &command_buffer,
    };

    VK_CHECK_RETURN_FALSE(vkResetFences(context->device, 1, &context->transfer_fence), "Failed to reset fence");
    VkQueue q = context->queue_families.transfer_is_separate
                    ? context->transfer_queue
                    : context->graphics_queue;
    VK_CHECK_RETURN_FALSE(vkQueueSubmit(q, 1, &submit_info, context->transfer_fence), "Failed to submit queue");
    vkWaitForFences(context->device, 1, &context->transfer_fence, VK_TRUE, UINT64_MAX);

    // Wait for the copy operation to finish
    vkQueueWaitIdle(context->transfer_queue);

    // Clean
    vkFreeCommandBuffers(context->device, cmd_pool, 1, &command_buffer);

    return true;
}

// ------- VERTEX_BUF ----------

b8 vk_buffer_create_vertex(VK_Context *context, Vertices *vertices) {
    if (vertices->count <= 0) {
        RL_ERROR("failed to create vertex buffer, number of vertices must be greater than 0");
        return false;
    }

    VkDeviceSize buffer_size = sizeof(vertex) * vertices->count;

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
    VkResult result = vkMapMemory(context->device, staging_memory, 0, buffer_size, 0, &data);
    if (result != VK_SUCCESS) {
        RL_ERROR("Failed to map staging buffer memory. VkResult=%s", string_VkResult(result));
        return false;
    }

    mem_copy(vertices->items, data, buffer_size);
    vkUnmapMemory(context->device, staging_memory);

    if (!vk_buffer_create(
        context,
        buffer_size,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &context->vertex_buffer,
        &context->vertex_buffer_memory)) {
        RL_ERROR("Failed to create vertex buffer");
        return false;
    }

    // Copy the data from staging buffer to vertex buffer
    VkCommandPool pool = context->queue_families.transfer_is_separate ? context->transfer_pool : context->graphics_pool;
    if (!vk_buffer_copy(context, pool, staging_buffer, context->vertex_buffer, buffer_size)) {
        RL_ERROR("Failed to copy staging buffer to vertex buffer");
        return false;
    }

    // Clean up staging buffer+mem on GPU
    vk_buffer_destroy(context, staging_buffer, staging_memory);
    return true;
}

void vk_buffer_destroy_vertex(VK_Context *context) {
    vk_buffer_destroy(context, context->vertex_buffer, context->vertex_buffer_memory);
}

// ------- INDEX_BUF ----------

b8 vk_buffer_create_index(VK_Context *context, Indices *indices) {
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
    mem_copy(indices->items, data, buffer_size);
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

void vk_buffer_destroy_index(VK_Context *context) {
    vk_buffer_destroy(context, context->index_buffer, context->index_buffer_memory);
}

// ------- UNIFORM_BUF ----------

b8 vk_buffers_create_uniform(VK_Context *context) {
    VkDeviceSize buffer_size = sizeof(ubo);

    context->uniform_buffers = rl_arena_push(&context->arena, sizeof(VkBuffer) * context->max_frames_in_flight, true);
    context->uniform_buffers_memory = rl_arena_push(&context->arena, sizeof(VkDeviceMemory) * context->max_frames_in_flight, true);
    context->uniform_buffers_mapped = rl_arena_push(&context->arena, sizeof(void *) * context->max_frames_in_flight, true);

    for (u32 i = 0; i < context->max_frames_in_flight; i++) {
        if (!vk_buffer_create(
            context,
            buffer_size,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &context->uniform_buffers[i],
            &context->uniform_buffers_memory[i]
            )) {
            RL_ERROR("Failed to create uniform buffer");
            return false;
        }

        VK_CHECK_RETURN_FALSE(
            vkMapMemory(context->device,
                context->uniform_buffers_memory[i],
                0,
                buffer_size,
                0,
                &context->uniform_buffers_mapped[i]),
            "Failed to map uniform buffer memory");
    }

    return true;
}

void vk_buffers_destroy_uniform(VK_Context *context) {
    for (u32 i = 0; i < context->max_frames_in_flight; i++) {
        vk_buffer_destroy(context, context->uniform_buffers[i], context->uniform_buffers_memory[i]);
    }
}

// ----------------------------