#pragma once

#include "defines.h"
#include "core/camera.h"
#include "platform/platform.h"
#include "renderer/vulkan/vk_types.h"

#ifndef _DEBUG
#define VK_CHECK(vkFnc) vkFnc
#else
#define VK_CHECK(vkFnc)                                                                                         \
    {                                                                                                           \
        if(const VkResult checkResult = (vkFnc); checkResult != VK_SUCCESS)                                     \
        {                                                                                                       \
            const char* errMsg = string_VkResult(checkResult);                                                  \
            RL_ERROR("Vulkan error: %s", errMsg);                                                               \
            ASSERT(checkResult == VK_SUCCESS, errMsg);                                                          \
        }                                                                                                       \
    }
#endif

b8 vulkan_initialize(platform_window *window, rl_camera *camera);
void vulkan_destroy();
void vulkan_begin_frame(f64 delta_time);
void vulkan_end_frame();
void vulkan_swap_buffers();
void vulkan_set_view_projection(mat4 view, mat4 projection);