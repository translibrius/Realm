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
        RL_ERROR("Failed to create vertex buffer");
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
        RL_ERROR("failed to allocate vertex buffer memory");
        return false;
    }

    VK_CHECK(vkBindBufferMemory(context->device, *buffer, *memory, 0));

    return true;
}

void vk_buffer_destroy(VK_Context *context, VkBuffer buffer, VkDeviceMemory memory) {
    vkDestroyBuffer(context->device, buffer, nullptr);
    vkFreeMemory(context->device, memory, nullptr);
}

void vk_buffer_copy(VK_Context *context, VkCommandPool cmd_pool, VkBuffer src, VkBuffer dst, VkDeviceSize size) {
    // Allocate command buffer to record the copy command
    VkCommandBufferAllocateInfo allocate_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandPool = cmd_pool,
        .commandBufferCount = 1
    };

    VkCommandBuffer command_buffer;
    vkAllocateCommandBuffers(context->device, &allocate_info, &command_buffer);

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };

    // Record copy command
    vkBeginCommandBuffer(command_buffer, &begin_info);
    {
        VkBufferCopy copy_region = {
            .srcOffset = 0,
            .dstOffset = 0,
            .size = size,
        };
        vkCmdCopyBuffer(command_buffer, src, dst, 1, &copy_region);
    }
    vkEndCommandBuffer(command_buffer);

    // Submit command buffer to transfer queue
    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &command_buffer,
    };

    vkResetFences(context->device, 1, &context->transfer_fence);
    VkQueue q = context->queue_families.transfer_is_separate
                    ? context->transfer_queue
                    : context->graphics_queue;
    vkQueueSubmit(q, 1, &submit_info, context->transfer_fence);
    vkWaitForFences(context->device, 1, &context->transfer_fence, VK_TRUE, UINT64_MAX);

    // Wait for the copy operation to finish
    vkQueueWaitIdle(context->transfer_queue);

    // Clean
    vkFreeCommandBuffers(context->device, cmd_pool, 1, &command_buffer);
}