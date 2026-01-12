#pragma once

#include "defines.h"

#include "renderer/vulkan/vk_types.h"
#include "volk.h"

void vk_swapchain_fetch_support(VK_Context *context, VkPhysicalDevice physical_device);

b8 vk_swapchain_create(VK_Context *context, b8 vsync);
void vk_swapchain_destroy(VK_Context *context);