#pragma once

#include "defines.h"
#include "asset/font.h"
#include "vk_types.h"

b8 vk_text_pipeline_init(VK_Context *ctx);
void vk_text_pipeline_destroy(VK_Context *ctx);

VK_Font *vk_find_font(VK_Context *ctx, rl_font *font);

void vulkan_render_text(const char *text, f32 size_px, f32 x, f32 y, vec4 color);
void vulkan_set_active_font(rl_font *font);
void vulkan_text_record_commands(VK_Context *ctx, VkCommandBuffer cmd);
