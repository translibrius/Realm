#pragma once

#include "defines.h"
#include "renderer/vulkan/vk_types.h"
#include <vulkan/vulkan_core.h>

VkFormat find_supported_format(VK_Context *ctx, VkFormat *candidates, u32 count, VkImageTiling tiling, VkFormatFeatureFlags features);
b8 has_stencil_component(VkFormat format);
const char *string_VkResult(VkResult input_value);
const char *string_VkFormat(VkFormat input_value);
