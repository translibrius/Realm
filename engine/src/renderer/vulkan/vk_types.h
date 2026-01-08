#pragma once

#include "defines.h"

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include "../vendor/cglm/cglm.h"
#include "memory/containers/dynamic_array.h"

DA_DEFINE(VKExtensions, VkExtensionProperties);

typedef struct VK_Context {
    u32 api_version;
    VkInstance instance;
    VKExtensions extensions;
} VK_Context;
