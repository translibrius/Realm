#pragma once

#include "defines.h"

#include <vulkan/vk_enum_string_helper.h>
#include <../vendor/volk/volk.h>

#include "../vendor/cglm/cglm.h"
#include "memory/containers/dynamic_array.h"
#include "core/logger.h"

#ifndef _DEBUG
#define VK_CHECK(vkFnc) vkFnc
#else
#define VK_CHECK(vkFnc)                                                 \
{                                                                       \
    const VkResult checkResult = (vkFnc);                               \
    if(checkResult != VK_SUCCESS)                                       \
    {                                                                   \
            const char* errMsg = string_VkResult(checkResult);          \
            RL_ERROR("Vulkan error: %s", errMsg);                       \
            RL_ASSERT_MSG(checkResult == VK_SUCCESS, errMsg);           \
    }                                                                   \
}
#endif

/*-- Callback function to catch validation errors  -*/
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                                    VkDebugUtilsMessageTypeFlagsEXT,
                                                    const VkDebugUtilsMessengerCallbackDataEXT *callbackData,
                                                    void *) {
    if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0) {
        RL_ERROR("[VULKAN] %s", callbackData->pMessage);
    } else {
        if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) != 0) {
            RL_WARN("[VULKAN] %s", callbackData->pMessage);
        } else {
            RL_INFO("[VULKAN] %s", callbackData->pMessage);
        }
    }
    if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0) {
        debugBreak();
    }
    return VK_FALSE;
}

typedef struct VK_Context {
    u32 api_version;
    VkInstance instance;
    VkDebugUtilsMessengerEXT debug_messenger;
    VkPhysicalDevice device;
} VK_Context;
