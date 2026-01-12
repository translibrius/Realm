#include "renderer/vulkan/vk_instance.h"

#include "core/logger.h"
#include "platform/platform.h"
#include "util/assert.h"

#include <string.h>

b8 vk_instance_create(VK_Context *context) {
    ARENA_SCRATCH_START();
#ifdef _DEBUG
    constexpr bool enable_validation = true;
#else
    constexpr bool enable_validation = false;
#endif

    vkEnumerateInstanceVersion(&context->api_version);
    RL_INFO("Vulkan api version: %d.%d", VK_VERSION_MAJOR(context->api_version), VK_VERSION_MINOR(context->api_version));
    RL_ASSERT_MSG(context->api_version >= VK_MAKE_API_VERSION(0, 1, 4, 0), "Requires Vulkan 1.4 loader");

    u32 instance_ext_count = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &instance_ext_count, nullptr);

    VkExtensionProperties *available_extensions = rl_arena_push(scratch.arena, sizeof(VkExtensionProperties) * instance_ext_count, true);
    vkEnumerateInstanceExtensionProperties(nullptr, &instance_ext_count, available_extensions);

    RL_DEBUG("Available vulkan extensions for platform (%s): %d", platform_get_info()->platform_name, instance_ext_count);
    for (u32 i = 0; i < instance_ext_count; i++) {
        RL_DEBUG("Available vulkan ext: %s", available_extensions[i].extensionName);
    }

    const char **platform_exts = nullptr;
    u32 platform_ext_count = platform_get_required_vulkan_extensions(&platform_exts, enable_validation);

    RL_DEBUG("Required vulkan extensions for platform (%s): %d", platform_get_info()->platform_name, platform_ext_count);
    for (u32 i = 0; i < platform_ext_count; i++) {
        RL_DEBUG("Required vulkan ext: %s", platform_exts[i]);
        b8 found = false;
        for (u32 j = 0; j < instance_ext_count; j++) {
            if (strcmp(platform_exts[i], available_extensions[j].extensionName) == 0) {
                found = true;
                break;
            }
        }

        if (!found) {
            RL_FATAL("Failed to find required vulkan extension: %s", platform_exts[i]);
        }
    }

    u32 layer_count = 0;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);

    VkLayerProperties *layers =
        rl_arena_push(scratch.arena, sizeof(VkLayerProperties) * layer_count, true);

    vkEnumerateInstanceLayerProperties(&layer_count, layers);

    RL_DEBUG("Available vulkan layers for platform (%s): %d", platform_get_info()->platform_name, layer_count);

    const char *validation_layers[] = {
        "VK_LAYER_KHRONOS_validation"
    };

    b8 found_validation = false;

    u32 enabled_layer_count = 0;
    const char **enabled_layers = nullptr;

    if (enable_validation) {
        enabled_layers = validation_layers;
        enabled_layer_count = 1;
    }

    for (u32 i = 0; i < layer_count; i++) {
        RL_DEBUG("Available vulkan layer: %s", layers[i].layerName);
        if (strcmp("VK_LAYER_KHRONOS_validation", layers[i].layerName) == 0) {
            found_validation = true;
        }
    }

    if (enable_validation && !found_validation) {
        RL_FATAL("Vulkan missing debug validation layer");
    }

    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Realm";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "RealmEngine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_4;

    VkInstanceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    create_info.flags = 0;
    create_info.enabledLayerCount = enabled_layer_count;
    create_info.ppEnabledLayerNames = enabled_layers;
    create_info.enabledExtensionCount = platform_ext_count;
    create_info.ppEnabledExtensionNames = platform_exts;

    VkResult result = vkCreateInstance(&create_info, nullptr, &context->instance);
    if (result != VK_SUCCESS) {
        return false;
    }

    RL_TRACE("Vulkan instance created successfully");

    ARENA_SCRATCH_RELEASE();
    return true;
}

void vk_instance_destroy(VK_Context *context) {
    vkDestroyInstance(context->instance, nullptr);
}