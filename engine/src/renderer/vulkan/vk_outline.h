#pragma once

#include "defines.h"
#include "vk_types.h"

b8   vk_outline_init(VK_Context *ctx);
void vk_outline_destroy(VK_Context *ctx);
void vk_outline_resize(VK_Context *ctx, u32 width, u32 height);

// Record offscreen passes (mask, JFA) before main render pass.
void vk_outline_record_offscreen(VK_Context *ctx, VkCommandBuffer cmd);

// Record composite pass inside main render pass, after geometry.
void vk_outline_record_composite(VK_Context *ctx, VkCommandBuffer cmd);
