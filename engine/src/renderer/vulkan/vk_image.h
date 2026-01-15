#pragma once

#include "defines.h"
#include "vk_types.h"

b8 vk_image_create(VK_Context *ctx, u32 w, u32 h, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags mem_props, VkImage *out_img, VkDeviceMemory *out_mem);

b8 vk_image_view_create(VK_Context *ctx, VkImage img, VkFormat format, VkImageView *out_view);
void vk_image_view_destroy(VK_Context *ctx, VkImageView view);

void vk_image_transition_layout(VK_Context *ctx, VkImage image, VkFormat format, VkImageLayout old_layout, VkImageLayout new_layout);