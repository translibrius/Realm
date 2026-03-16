#pragma once

#include "defines.h"
#include "vk_types.h"

#define VK_GUI_MAX_VERTS (6 * 8192)

b8 vk_gui_pipeline_init(VK_Context *ctx);
void vk_gui_pipeline_destroy(VK_Context *ctx);

void vulkan_render_gui(void *commands, i32 command_count);
void vulkan_gui_record_commands(VK_Context *ctx, VkCommandBuffer cmd);

// Allow vk_text.c to write directly into the GUI vertex buffer.
VK_GuiVertex *vk_gui_pipeline_get_write_ptr(VK_Context *ctx, u32 *out_remaining);
void vk_gui_pipeline_commit_verts(VK_Context *ctx, VK_Font *font, u32 count);
