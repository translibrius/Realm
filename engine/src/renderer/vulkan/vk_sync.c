#include "vk_sync.h"

b8 vk_sync_objects_create(VK_Context *context) {
    context->image_available_semaphores = rl_arena_push(&context->arena, sizeof(VkSemaphore) * context->max_frames_in_flight, true);
    context->render_finished_semaphores = rl_arena_push(&context->arena, sizeof(VkSemaphore) * context->max_frames_in_flight, true);
    context->in_flight_fences = rl_arena_push(&context->arena, sizeof(VkFence) * context->max_frames_in_flight, true);

    VkSemaphoreCreateInfo semaphore_create = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };

    VkFenceCreateInfo fence_create = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };

    for (u32 i = 0; i < context->max_frames_in_flight; i++) {
        VkResult result = vkCreateSemaphore(context->device, &semaphore_create, nullptr, &context->image_available_semaphores[i]);
        if (result != VK_SUCCESS) {
            RL_ERROR("Failed to create semaphore");
            return false;
        }

        result = vkCreateSemaphore(context->device, &semaphore_create, nullptr, &context->render_finished_semaphores[i]);
        if (result != VK_SUCCESS) {
            RL_ERROR("Failed to create semaphore");
            return false;
        }

        result = vkCreateFence(context->device, &fence_create, nullptr, &context->in_flight_fences[i]);
        if (result != VK_SUCCESS) {
            RL_ERROR("Failed to create fence");
            return false;
        }
    }

    return true;
}

void vk_sync_objects_destroy(VK_Context *context) {
    for (u32 i = 0; i < context->max_frames_in_flight; i++) {
        vkDestroySemaphore(context->device, context->image_available_semaphores[i], nullptr);
        vkDestroySemaphore(context->device, context->render_finished_semaphores[i], nullptr);
        vkDestroyFence(context->device, context->in_flight_fences[i], nullptr);
    }
}