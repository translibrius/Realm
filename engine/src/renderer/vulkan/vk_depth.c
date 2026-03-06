#include "vk_depth.h"
#include "vk_util.h"
#include "vk_image.h"

#include <vulkan/vulkan_core.h>

b8 vk_depth_res_create(VK_Context* ctx) {
    VkFormat depth_format = find_depth_format(ctx);

    if (!vk_image_create(
        ctx,
        ctx->swapchain.chosen_extent.width,
        ctx->swapchain.chosen_extent.height,
        depth_format,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &ctx->depth_image,
        &ctx->depth_image_memory)) {
        RL_ERROR("Failed to create depth image");
        return false;
    }

    if (!vk_image_view_create(ctx, VK_IMAGE_ASPECT_DEPTH_BIT, ctx->depth_image, depth_format, &ctx->depth_image_view)) {
        RL_ERROR("Failed to create depth image view");
        return false;
    }

    vk_image_transition_layout(ctx, ctx->depth_image, depth_format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    return true;
}

VkFormat find_depth_format(VK_Context *ctx) {
    VkFormat candidates[3] = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT
    };

    return find_supported_format(ctx, candidates, 3, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}
