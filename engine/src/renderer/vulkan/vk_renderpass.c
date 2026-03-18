#include "vk_renderpass.h"
#include "vk_depth.h"

b8 vk_renderpass_create(VK_Context *context) {
    b8 msaa = context->msaa_samples != VK_SAMPLE_COUNT_1_BIT;

    // When MSAA is off:  [0] color (1x, presentable)  [1] depth (1x)
    // When MSAA is on:   [0] color (Nx, transient)    [1] depth (Nx)    [2] resolve (1x, presentable)

    VkAttachmentDescription color_attachment = {
        .format = context->swapchain.chosen_format.surfaceFormat.format,
        .samples = context->msaa_samples,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = msaa ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = msaa ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };

    VkFormat depth_format = find_depth_format(context);
    b8 has_stencil = vk_depth_format_has_stencil(depth_format);

    VkAttachmentDescription depth_attachment = {
        .format = depth_format,
        .samples = context->msaa_samples,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .stencilLoadOp = has_stencil ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };

    VkAttachmentDescription resolve_attachment = {
        .format = context->swapchain.chosen_format.surfaceFormat.format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };

    VkAttachmentReference color_ref = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    VkAttachmentReference depth_ref = {
        .attachment = 1,
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };

    VkAttachmentReference resolve_ref = {
        .attachment = 2,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    VkSubpassDescription subpass_description = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_ref,
        .pDepthStencilAttachment = &depth_ref,
        .pResolveAttachments = msaa ? &resolve_ref : nullptr,
    };

    VkSubpassDependency dependency = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
    };

    VkAttachmentDescription attachments_no_msaa[2] = {color_attachment, depth_attachment};
    VkAttachmentDescription attachments_msaa[3] = {color_attachment, depth_attachment, resolve_attachment};

    VkRenderPassCreateInfo render_pass_create_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = msaa ? 3u : 2u,
        .pAttachments = msaa ? attachments_msaa : attachments_no_msaa,
        .subpassCount = 1,
        .pSubpasses = &subpass_description,
        .dependencyCount = 1,
        .pDependencies = &dependency,
    };

    if (VK_SUCCESS != vkCreateRenderPass(context->device, &render_pass_create_info, nullptr, &context->render_pass)) {
        RL_ERROR("Failed to create render pass");
        return false;
    }

    RL_TRACE("Successfully created renderpass (MSAA %dx)", (i32)context->msaa_samples);

    return true;
}

void vk_renderpass_destroy(VK_Context *context) {
    vkDestroyRenderPass(context->device, context->render_pass, nullptr);
}
