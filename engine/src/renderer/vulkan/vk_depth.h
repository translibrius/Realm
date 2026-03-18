#include "renderer/vulkan/vk_types.h"
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <cglm.h>

#include "defines.h"

b8 vk_depth_res_create(VK_Context* ctx);

VkFormat find_depth_format(VK_Context *ctx);
b8 vk_depth_format_has_stencil(VkFormat format);
