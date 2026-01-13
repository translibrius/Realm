#pragma once

#include "defines.h"
#include "vk_types.h"

b8 vk_command_pool_create(VK_Context *context, VkCommandPool *out_pool);
void vk_command_pool_destroy(VK_Context *context, VkCommandPool pool);

b8 vk_command_buffers_create(VK_Context *context, VkCommandPool pool);

b8 vk_command_buffer_record(VK_Context *context, VkCommandBuffer buffer, u32 image_index);