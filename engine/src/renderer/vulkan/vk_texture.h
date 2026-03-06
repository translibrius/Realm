#pragma once

#include "defines.h"
#include "vk_types.h"

b8 vk_texture_upload(VK_Context *ctx, u32 w, u32 h, VkFormat format, void *pixels, VkDeviceSize size, VkImage *out_img, VkDeviceMemory *out_mem, VkImageView *out_view);

b8 vk_sampler_create(VK_Context *ctx, VkFilter filter, VkSamplerAddressMode addr_mode, VkSampler *out);

b8 vk_texture_create(VK_Context *ctx, VK_Texture *vk_texture);
void vk_texture_destroy(VK_Context *ctx, VK_Texture *vk_texture);

b8 vk_texture_create_sampler(VK_Context *ctx);
void vk_texture_destroy_sampler(VK_Context *ctx);
