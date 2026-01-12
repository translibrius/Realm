#pragma once

#include "defines.h"

#include "renderer/vulkan/vk_types.h"
#include "volk.h"

b8 vk_swapchain_init(VK_Context *context, b8 vsync);
void vk_swapchain_destroy(VK_Context *context);