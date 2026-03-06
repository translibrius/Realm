#pragma once

#include "defines.h"
#include "vk_types.h"

b8 vk_gui_pipeline_init(VK_Context *ctx);
void vk_gui_pipeline_destroy(VK_Context *ctx);

void vulkan_render_gui(void *commands, i32 command_count);
void vulkan_gui_record_commands(VK_Context *ctx, VkCommandBuffer cmd);
