#include "vk_texture.h"
#include "asset/asset.h"
#include "vk_util.h"
#include "vk_buffer.h"
#include "vk_image.h"

#include <math.h>

static u32 calc_mip_levels(u32 w, u32 h) {
    u32 max_dim = w > h ? w : h;
    return (u32)floor(log2((f64)max_dim)) + 1;
}

b8 vk_texture_upload(VK_Context *ctx, u32 w, u32 h, u32 mip_levels, VkFormat format, void *pixels, VkDeviceSize size, VkImage *out_img, VkDeviceMemory *out_mem, VkImageView *out_view) {
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

    // Create image with all mip levels; TRANSFER_SRC needed for blit source
    VkImageCreateInfo image_ci = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = {.width = w, .height = h, .depth = 1},
        .mipLevels = mip_levels,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    VK_CHECK_RETURN_FALSE(vkCreateImage(ctx->device, &image_ci, nullptr, out_img), "Failed to create texture image");

    VkMemoryRequirements mem_reqs;
    vkGetImageMemoryRequirements(ctx->device, *out_img, &mem_reqs);

    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_reqs.size,
        .memoryTypeIndex = find_memory_type(ctx, mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
    };

    if (vkAllocateMemory(ctx->device, &alloc_info, nullptr, out_mem) != VK_SUCCESS) {
        RL_ERROR("Failed to allocate texture image memory");
        vkDestroyImage(ctx->device, *out_img, nullptr);
        *out_img = VK_NULL_HANDLE;
        return false;
    }
    vkBindImageMemory(ctx->device, *out_img, *out_mem, 0);

    // Single command buffer for staging copy + mip chain generation
    VkCommandBuffer cmd = vk_buffer_begin_single_use(ctx, ctx->graphics_pool);

    // Transition all levels to TRANSFER_DST
    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = *out_img,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = mip_levels,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
    };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    // Copy staging buffer into mip 0
    VkBufferImageCopy region = {
        .imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        .imageExtent = {w, h, 1},
    };
    vkCmdCopyBufferToImage(cmd, staging_buffer, *out_img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    // Generate mip chain via blit
    i32 mip_w = (i32)w;
    i32 mip_h = (i32)h;

    for (u32 i = 1; i < mip_levels; i++) {
        // Transition level i-1: TRANSFER_DST → TRANSFER_SRC
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.subresourceRange.levelCount = 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        i32 next_w = mip_w > 1 ? mip_w / 2 : 1;
        i32 next_h = mip_h > 1 ? mip_h / 2 : 1;

        VkImageBlit blit = {
            .srcSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel = i - 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
            .srcOffsets = {{0, 0, 0}, {mip_w, mip_h, 1}},
            .dstSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel = i,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
            .dstOffsets = {{0, 0, 0}, {next_w, next_h, 1}},
        };
        vkCmdBlitImage(cmd, *out_img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *out_img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

        // Transition level i-1: TRANSFER_SRC → SHADER_READ_ONLY
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        mip_w = next_w;
        mip_h = next_h;
    }

    // Transition last level: TRANSFER_DST → SHADER_READ_ONLY
    barrier.subresourceRange.baseMipLevel = mip_levels - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    vk_buffer_end_single_use(ctx, ctx->graphics_pool, cmd, ctx->graphics_queue);
    vk_buffer_destroy(ctx, staging_buffer, staging_memory);

    // Image view spanning all mip levels
    VkImageViewCreateInfo view_ci = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = *out_img,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = mip_levels,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };
    if (vkCreateImageView(ctx->device, &view_ci, nullptr, out_view) != VK_SUCCESS) {
        RL_ERROR("Failed to create texture image view");
        vkDestroyImage(ctx->device, *out_img, nullptr);
        vkFreeMemory(ctx->device, *out_mem, nullptr);
        *out_img = VK_NULL_HANDLE;
        *out_mem = VK_NULL_HANDLE;
        return false;
    }

    return true;
}

b8 vk_sampler_create(VK_Context *ctx, VkFilter filter, VkSamplerAddressMode addr_mode, f32 max_lod, VkSampler *out) {
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
        .minLod = 0.0f,
        .maxLod = max_lod,
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

b8 vk_texture_create(VK_Context *ctx, asset_id id, VK_Texture *vk_texture) {
    rl_asset *asset = asset_get(id);
    if (!asset) {
        return false;
    }

    rl_texture *texture = asset->data;
    u32 mip_levels = calc_mip_levels(texture->width, texture->height);

    if (!vk_texture_upload(ctx, texture->width, texture->height, mip_levels,
                           VK_FORMAT_R8G8B8A8_UNORM, texture->data, texture->size,
                           &vk_texture->texture_image, &vk_texture->texture_memory,
                           &vk_texture->texture_image_view)) {
        return false;
    }

    vk_texture->mip_levels = mip_levels;
    return true;
}

void vk_texture_destroy(VK_Context *ctx, VK_Texture *vk_texture) {
    vkDestroyImage(ctx->device, vk_texture->texture_image, nullptr);
    vkDestroyImageView(ctx->device, vk_texture->texture_image_view, nullptr);
    vkFreeMemory(ctx->device, vk_texture->texture_memory, nullptr);
}

b8 vk_texture_create_sampler(VK_Context *ctx) {
    // Use the first loaded texture's mip levels for the sampler (they're all similar)
    u32 mip_levels = ctx->texture_count > 0 ? ctx->textures[0].texture.mip_levels : 1;
    f32 max_lod = (f32)(mip_levels > 0 ? mip_levels - 1 : 0);
    return vk_sampler_create(ctx, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, max_lod, &ctx->texture_sampler);
}

void vk_texture_destroy_sampler(VK_Context *ctx) {
    vkDestroySampler(ctx->device, ctx->texture_sampler, nullptr);
}
