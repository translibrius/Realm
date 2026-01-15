#include "vk_texture.h"

#include "vk_buffer.h"
#include "vk_image.h"

b8 vk_texture_create(VK_Context *ctx, VK_Texture *vk_texture) {
    rl_asset *asset = get_asset("wood_container.jpg");
    rl_texture *texture = asset->handle;

    VkBuffer staging_buffer = nullptr;
    VkDeviceMemory staging_buffer_memory = nullptr;

    b8 success = vk_buffer_create(
        ctx,
        texture->size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &staging_buffer,
        &staging_buffer_memory);

    if (!success) {
        RL_ERROR("Failed to create texture staging buffer");
        return false;
    }

    void *data;
    vkMapMemory(ctx->device, staging_buffer_memory, 0, texture->size, 0, &data);
    mem_copy(texture->data, data, texture->size);
    vkUnmapMemory(ctx->device, staging_buffer_memory);

    success = vk_image_create(
        ctx,
        texture->width,
        texture->height,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &vk_texture->texture_image, &vk_texture->texture_memory);

    if (!success) {
        return false;
    }

    // Transition for copying from staging buffer -> transition to shader ready
    vk_image_transition_layout(ctx, vk_texture->texture_image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    vk_buffer_copy_to_image(ctx, staging_buffer, vk_texture->texture_image, texture->width, texture->height);
    vk_image_transition_layout(ctx, vk_texture->texture_image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(ctx->device, staging_buffer, nullptr);
    vkFreeMemory(ctx->device, staging_buffer_memory, nullptr);

    // Create view
    vk_image_view_create(ctx, vk_texture->texture_image, VK_FORMAT_R8G8B8A8_SRGB, &vk_texture->texture_image_view);

    return true;
}

void vk_texture_destroy(VK_Context *ctx, VK_Texture *vk_texture) {
    vkDestroyImage(ctx->device, vk_texture->texture_image, nullptr);
    vkDestroyImageView(ctx->device, vk_texture->texture_image_view, nullptr);
    vkFreeMemory(ctx->device, vk_texture->texture_memory, nullptr);
}

b8 vk_texture_create_sampler(VK_Context *ctx) {
    VkSamplerCreateInfo sampler_create_info = {0};
    sampler_create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

    // Filters for scaling the texels
    sampler_create_info.magFilter = VK_FILTER_LINEAR; // OR NEAREST for blocky effect
    sampler_create_info.minFilter = VK_FILTER_LINEAR; // OR NEAREST for blocky effect

    // Reference modes: https://vulkan-tutorial.com/images/texture_addressing.png
    sampler_create_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_create_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_create_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

    // 4x, 8x, 16x etc. anisotropic filtering: https://vulkan-tutorial.com/images/anisotropic_filtering.png
    if (ctx->device_properties.features.samplerAnisotropy) {
        sampler_create_info.anisotropyEnable = VK_TRUE;
        sampler_create_info.maxAnisotropy = ctx->device_properties.properties.limits.maxSamplerAnisotropy;
    } else {
        sampler_create_info.anisotropyEnable = VK_FALSE;
        sampler_create_info.maxAnisotropy = 1.0f;
    }

    // Color when sampling beyond the image with clamp to border addressing mode
    sampler_create_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;

    sampler_create_info.unnormalizedCoordinates = VK_FALSE;

    /*
     * If a comparison function is enabled, then texels will first be compared to a value,
     * and the result of that comparison is used in filtering operations.
     * This is mainly used for percentage-closer filtering on shadow maps.
     */
    sampler_create_info.compareEnable = VK_FALSE;
    sampler_create_info.compareOp = VK_COMPARE_OP_ALWAYS;

    sampler_create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler_create_info.mipLodBias = 0.0f;
    sampler_create_info.minLod = 0.0f;
    sampler_create_info.maxLod = 0.0f;

    VK_CHECK_RETURN_FALSE(vkCreateSampler(ctx->device, &sampler_create_info, nullptr, &ctx->texture_sampler), "Failed to create texture sampler");

    return true;
}

void vk_texture_destroy_sampler(VK_Context *ctx) {
    vkDestroySampler(ctx->device, ctx->texture_sampler, nullptr);
}

