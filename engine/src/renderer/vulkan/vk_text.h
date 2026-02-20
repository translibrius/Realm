#pragma once

#include "defines.h"
#include "asset/font.h"
#include "renderer/frame_data.h"
#include "renderer/vulkan/vk_types.h"

b8 vulkan_text_pipeline_init(VK_Context *ctx);
void vulkan_text_pipeline_destroy(VK_Context *ctx);

void vulkan_set_active_font(rl_font *font);
void vulkan_render_text(const char *text, f32 size_px, f32 x, f32 y, vec4 color);
void vulkan_render_text_batch(VK_Context *ctx, rl_frame_text *texts, u32 text_count);
void vulkan_text_record_commands(VK_Context *ctx, VkCommandBuffer cmd);
