#include "vk_texture.h"
#include "vk_util.h"
#include "vk_buffer.h"
#include "vk_image.h"

b8 vk_texture_upload(VK_Context *ctx, u32 w, u32 h, VkFormat format, void *pixels, VkDeviceSize size, VkImage *out_img, VkDeviceMemory *out_mem, VkImageView *out_view) {
    VkBuffer staging_buffer;
    VkDeviceMemory staging_memory;

    if (!vk_buffer_create(ctx, size,
                          VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                          &staging_buffer, &staging_memory)) {
        RL_ERROR("Failed to create staging buffer for texture upload");
        return false;
    }

    void *data;
    vkMapMemory(ctx->device, staging_memory, 0, size, 0, &data);
    mem_copy(data, pixels, size);
    vkUnmapMemory(ctx->device, staging_memory);

    if (!vk_image_create(ctx, w, h, format,
                         VK_IMAGE_TILING_OPTIMAL,
                         VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                         out_img, out_mem)) {
        vk_buffer_destroy(ctx, staging_buffer, staging_memory);
        return false;
    }

    vk_image_transition_layout(ctx, *out_img, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    vk_buffer_copy_to_image(ctx, staging_buffer, *out_img, w, h);
    vk_image_transition_layout(ctx, *out_img, format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vk_buffer_destroy(ctx, staging_buffer, staging_memory);

    if (!vk_image_view_create(ctx, VK_IMAGE_ASPECT_COLOR_BIT, *out_img, format, out_view)) {
        return false;
    }

    return true;
}

b8 vk_sampler_create(VK_Context *ctx, VkFilter filter, VkSamplerAddressMode addr_mode, VkSampler *out) {
    VkSamplerCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = filter,
        .minFilter = filter,
        .addressModeU = addr_mode,
        .addressModeV = addr_mode,
        .addressModeW = addr_mode,
        .borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
    };

    if (addr_mode == VK_SAMPLER_ADDRESS_MODE_REPEAT && ctx->device_properties.features.samplerAnisotropy) {
        ci.anisotropyEnable = VK_TRUE;
        ci.maxAnisotropy = ctx->device_properties.properties.limits.maxSamplerAnisotropy;
    } else {
        ci.anisotropyEnable = VK_FALSE;
        ci.maxAnisotropy = 1.0f;
    }

    VK_CHECK_RETURN_FALSE(vkCreateSampler(ctx->device, &ci, nullptr, out), "Failed to create sampler");
    return true;
}

b8 vk_texture_create(VK_Context *ctx, VK_Texture *vk_texture) {
    rl_asset *asset = get_asset_by_id(ASSET_ID_TEXTURE_WOOD_CONTAINER2);
    if (!asset) {
        return false;
    }

    rl_texture *texture = asset->handle;

    // The existing texture needs a vertical flip, so we can't use vk_texture_upload directly.
    // Stage with flip, then upload manually.
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
    u8 *dst_bytes = data;
    u8 *src_bytes = texture->data;

    for (int y = 0; y < texture->height; y++) {
        // read from the bottom row upward
        u8 *src_row = src_bytes + (texture->height - 1 - y) * texture->width * texture->channels;
        u8 *dst_row = dst_bytes + y * texture->width * texture->channels;
        mem_copy(dst_row, src_row, texture->width * texture->channels);
    }
    vkUnmapMemory(ctx->device, staging_buffer_memory);

    success = vk_image_create(
        ctx,
        texture->width,
        texture->height,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &vk_texture->texture_image, &vk_texture->texture_memory);

    if (!success) {
        return false;
    }

    vk_image_transition_layout(ctx, vk_texture->texture_image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    vk_buffer_copy_to_image(ctx, staging_buffer, vk_texture->texture_image, texture->width, texture->height);
    vk_image_transition_layout(ctx, vk_texture->texture_image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(ctx->device, staging_buffer, nullptr);
    vkFreeMemory(ctx->device, staging_buffer_memory, nullptr);

    vk_image_view_create(ctx, VK_IMAGE_ASPECT_COLOR_BIT, vk_texture->texture_image, VK_FORMAT_R8G8B8A8_UNORM, &vk_texture->texture_image_view);

    return true;
}

void vk_texture_destroy(VK_Context *ctx, VK_Texture *vk_texture) {
    vkDestroyImage(ctx->device, vk_texture->texture_image, nullptr);
    vkDestroyImageView(ctx->device, vk_texture->texture_image_view, nullptr);
    vkFreeMemory(ctx->device, vk_texture->texture_memory, nullptr);
}

b8 vk_texture_create_sampler(VK_Context *ctx) {
    return vk_sampler_create(ctx, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, &ctx->texture_sampler);
}

void vk_texture_destroy_sampler(VK_Context *ctx) {
    vkDestroySampler(ctx->device, ctx->texture_sampler, nullptr);
}
