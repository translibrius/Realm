#include "vk_frame_buffers.h"

b8 vk_framebuffers_create(VK_Context *context) {
    context->swapchain.frame_buffers_count = context->swapchain.image_count;
    context->swapchain.frame_buffers = rl_arena_push(&context->arena, sizeof(VkFramebuffer) * context->swapchain.frame_buffers_count, true);

    b8 msaa = context->msaa_samples != VK_SAMPLE_COUNT_1_BIT;

    for (u32 i = 0; i < context->swapchain.image_count; i++) {
        // No MSAA:  [0] swapchain color  [1] depth
        // MSAA:     [0] msaa color        [1] depth        [2] swapchain resolve
        VkImageView attachments_no_msaa[2] = {
            context->swapchain.image_views[i],
            context->depth_image_view,
        };

        VkImageView attachments_msaa[3] = {
            context->msaa_color_view,
            context->depth_image_view,
            context->swapchain.image_views[i],
        };

        VkFramebufferCreateInfo framebuffer_create_info = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = context->graphics_pipeline.render_pass,
            .attachmentCount = msaa ? 3u : 2u,
            .pAttachments = msaa ? attachments_msaa : attachments_no_msaa,
            .width = context->swapchain.chosen_extent.width,
            .height = context->swapchain.chosen_extent.height,
            .layers = 1
        };

        if (VK_SUCCESS != vkCreateFramebuffer(context->device, &framebuffer_create_info, nullptr, &context->swapchain.frame_buffers[i])) {
            RL_ERROR("failed to create frame buffer id %d", i);
            return false;
        }
    }

    RL_TRACE("Successfully created framebuffers");
    return true;
}

void vk_framebuffers_destroy(VK_Context *context) {
    for (u32 i = 0; i < context->swapchain.frame_buffers_count; i++) {
        vkDestroyFramebuffer(context->device, context->swapchain.frame_buffers[i], nullptr);
    }
}
