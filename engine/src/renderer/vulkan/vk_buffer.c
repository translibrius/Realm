#include "vk_buffer.h"
#include "vk_util.h"

u32 find_memory_type(VK_Context *context, u32 type_filter, VkMemoryPropertyFlags properties) {
    const VkPhysicalDeviceMemoryProperties *mem_properties = &context->device_properties.memory_properties;

    for (u32 i = 0; i < mem_properties->memoryTypeCount; i++) {
        if ((type_filter & (1 << i)) && (mem_properties->memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    RL_FATAL("failed to find suitable memory type");
    return -1;
}

// ------- GENERAL_BUF ----------

b8 vk_buffer_create(VK_Context *context, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags mem_props, VkBuffer *buffer, VkDeviceMemory *memory) {
    u32 queue_family_indices[2] = {
        context->queue_families.graphics_index,
        context->queue_families.transfer_index
    };

    VkBufferCreateInfo buffer_create_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
    };

    if (context->queue_families.transfer_is_separate) {
        buffer_create_info.sharingMode = VK_SHARING_MODE_CONCURRENT;
        buffer_create_info.queueFamilyIndexCount = 2;
        buffer_create_info.pQueueFamilyIndices = queue_family_indices;
    } else {
        buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        buffer_create_info.queueFamilyIndexCount = 0; // Optional
        buffer_create_info.pQueueFamilyIndices = nullptr; // Optional
    }

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
        vkDestroyBuffer(context->device, *buffer, nullptr);
        *buffer = VK_NULL_HANDLE;
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
    VkQueue q = context->queue_families.transfer_is_separate
                    ? context->transfer_queue
                    : context->graphics_queue;

    VkCommandBuffer command_buffer = vk_buffer_begin_single_use(context, cmd_pool);
    VkBufferCopy copy_region = {.srcOffset = 0, .dstOffset = 0, .size = size};
    vkCmdCopyBuffer(command_buffer, src, dst, 1, &copy_region);
    vk_buffer_end_single_use(context, cmd_pool, command_buffer, q);

    return true;
}

VkCommandBuffer vk_buffer_begin_single_use(VK_Context *ctx, VkCommandPool cmd_pool) {
    VkCommandBufferAllocateInfo alloc_info = {0};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool = cmd_pool;
    alloc_info.commandBufferCount = 1;

    VkCommandBuffer cmd_buffer = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateCommandBuffers(ctx->device, &alloc_info, &cmd_buffer));

    VkCommandBufferBeginInfo begin_info = {0};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(cmd_buffer, &begin_info);

    return cmd_buffer;
}

void vk_buffer_end_single_use(VK_Context *ctx, VkCommandPool cmd_pool, VkCommandBuffer cmd_buffer, VkQueue q) {
    vkEndCommandBuffer(cmd_buffer);

    VkSubmitInfo submitInfo = {0};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd_buffer;

    VK_CHECK(vkQueueSubmit(q, 1, &submitInfo, VK_NULL_HANDLE));
    vkQueueWaitIdle(q);

    vkFreeCommandBuffers(ctx->device, cmd_pool, 1, &cmd_buffer);
}

// ------- MAPPED_BUFS ----------

b8 vk_buffers_create_mapped(VK_Context *ctx, VkDeviceSize size, VkBufferUsageFlags usage, u32 count, VkBuffer *out_bufs, VkDeviceMemory *out_mem, void **out_mapped) {
    for (u32 i = 0; i < count; i++) {
        if (!vk_buffer_create(ctx, size, usage,
                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                              &out_bufs[i], &out_mem[i])) {
            RL_ERROR("Failed to create mapped buffer %u", i);
            return false;
        }

        VK_CHECK_RETURN_FALSE(
            vkMapMemory(ctx->device, out_mem[i], 0, size, 0, &out_mapped[i]),
            "Failed to map buffer memory");
    }

    return true;
}

// ------- UNIFORM_BUF ----------

b8 vk_buffers_create_uniform(VK_Context *context) {
    context->uniform_buffers = rl_arena_push(&context->arena, sizeof(VkBuffer) * context->max_frames_in_flight, true);
    context->uniform_buffers_memory = rl_arena_push(&context->arena, sizeof(VkDeviceMemory) * context->max_frames_in_flight, true);
    context->uniform_buffers_mapped = rl_arena_push(&context->arena, sizeof(void *) * context->max_frames_in_flight, true);

    return vk_buffers_create_mapped(context, sizeof(ubo), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                    context->max_frames_in_flight,
                                    context->uniform_buffers, context->uniform_buffers_memory, context->uniform_buffers_mapped);
}

void vk_buffers_destroy_uniform(VK_Context *context) {
    for (u32 i = 0; i < context->max_frames_in_flight; i++) {
        vk_buffer_destroy(context, context->uniform_buffers[i], context->uniform_buffers_memory[i]);
    }
}

b8 vk_buffers_create_overlay_uniform(VK_Context *context) {
    context->overlay_uniform_buffers = rl_arena_push(&context->arena, sizeof(VkBuffer) * context->max_frames_in_flight, true);
    context->overlay_uniform_buffers_memory = rl_arena_push(&context->arena, sizeof(VkDeviceMemory) * context->max_frames_in_flight, true);
    context->overlay_uniform_buffers_mapped = rl_arena_push(&context->arena, sizeof(void *) * context->max_frames_in_flight, true);

    return vk_buffers_create_mapped(context, sizeof(ubo), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                    context->max_frames_in_flight,
                                    context->overlay_uniform_buffers, context->overlay_uniform_buffers_memory, context->overlay_uniform_buffers_mapped);
}

void vk_buffers_destroy_overlay_uniform(VK_Context *context) {
    for (u32 i = 0; i < context->max_frames_in_flight; i++) {
        vk_buffer_destroy(context, context->overlay_uniform_buffers[i], context->overlay_uniform_buffers_memory[i]);
    }
}

// ----------------------------

void vk_buffer_copy_to_image(VK_Context *ctx, VkBuffer buffer, VkImage image, u32 w, u32 h) {
    VkCommandBuffer cmd_buf = vk_buffer_begin_single_use(ctx, ctx->graphics_pool);

    VkBufferImageCopy region = {0};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;

    region.imageOffset = (VkOffset3D){0, 0, 0};
    region.imageExtent = (VkExtent3D){
        w,
        h,
        1
    };

    vkCmdCopyBufferToImage(cmd_buf, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    vk_buffer_end_single_use(ctx, ctx->graphics_pool, cmd_buf, ctx->graphics_queue);
}
