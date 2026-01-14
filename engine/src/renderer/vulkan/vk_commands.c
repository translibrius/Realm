#include "vk_commands.h"

b8 vk_command_pool_create(VK_Context *context, VkCommandPool *out_pool, u32 family_index) {

    /*
    There are two possible flags for command pools:
        VK_COMMAND_POOL_CREATE_TRANSIENT_BIT: Hint that command buffers are rerecorded with new commands very often (may change memory allocation behavior)
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT: Allow command buffers to be rerecorded individually, without this flag they all have to be reset together
    */

    VkCommandPoolCreateInfo pool_create_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = family_index
    };

    VkResult result = vkCreateCommandPool(context->device, &pool_create_info, nullptr, out_pool);
    if (result != VK_SUCCESS) {
        RL_ERROR("failed to create command pool. VkResult=%s", string_VkResult(result));
        return false;
    }

    return true;
}

void vk_command_pool_destroy(VK_Context *context, VkCommandPool pool) {
    vkDestroyCommandPool(context->device, pool, nullptr);
}

b8 vk_command_buffers_create(VK_Context *context, VkCommandPool pool) {
    context->command_buffers = rl_arena_push(&context->arena, sizeof(VkCommandBuffer) * context->max_frames_in_flight, true);

    VkCommandBufferAllocateInfo allocate_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = context->max_frames_in_flight
    };

    VkResult result = vkAllocateCommandBuffers(context->device, &allocate_info, context->command_buffers);
    if (result != VK_SUCCESS) {
        RL_ERROR("Failed to allocate command buffer. VkResult=%s", string_VkResult(result));
        return false;
    }

    return true;
}

b8 vk_command_buffer_record(VK_Context *context, VkCommandBuffer buffer, u32 image_index) {
    /*
    The flags parameter specifies how we're going to use the command buffer. The following values are available:

    VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT: The command buffer will be rerecorded right after executing it once.
    VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT: This is a secondary command buffer that will be entirely within a single render pass.
    VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT: The command buffer can be resubmitted while it is also already pending execution.
    */

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = 0,
        .pInheritanceInfo = nullptr
    };

    VkResult result = vkBeginCommandBuffer(buffer, &begin_info);
    if (result != VK_SUCCESS) {
        RL_ERROR("Failed to begin recording a command buffer");
        return false;
    }

    VkClearValue clear_color = {{{0.0f, 0.0f, 0.0f, 1.0f}}};

    VkRenderPassBeginInfo render_pass_begin_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = context->graphics_pipeline.render_pass,
        .framebuffer = context->swapchain.frame_buffers[image_index],
        .renderArea = {
            .offset = {0, 0},
            .extent = context->swapchain.chosen_extent
        },
        .clearValueCount = 1,
        .pClearValues = &clear_color
    };

    vkCmdBeginRenderPass(buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
    {
        vkCmdBindPipeline(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, context->graphics_pipeline.handle);

        // Dynamic viewport
        VkViewport viewport = {
            .x = 0.0f,
            .y = 0.0f,
            .width = (f32)context->swapchain.chosen_extent.width,
            .height = (f32)context->swapchain.chosen_extent.height,
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };
        vkCmdSetViewport(buffer, 0, 1, &viewport);

        // Dynamic scissor
        VkRect2D scissor = {
            .offset = {0, 0},
            .extent = context->swapchain.chosen_extent
        };
        vkCmdSetScissor(buffer, 0, 1, &scissor);

        // Bind vertex buffer
        VkBuffer vertex_buffers[] = {context->vertex_buffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(buffer, 0, 1, vertex_buffers, offsets);

        vkCmdDraw(buffer, 3, 1, 0, 0);
    }
    vkCmdEndRenderPass(buffer);

    result = vkEndCommandBuffer(buffer);
    if (result != VK_SUCCESS) {
        RL_ERROR("failed to record command buffer");
        return false;
    }

    return true;
}