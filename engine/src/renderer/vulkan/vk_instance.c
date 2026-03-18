#include "renderer/vulkan/vk_instance.h"

#include "core/logger.h"
#include "platform/platform.h"
#include "util/assert.h"
#include "util/str.h"
#include "vk_util.h"

#include <string.h>

#define VOLK_IMPLEMENTATION
#include <volk.h>

/*-- Callback function to catch validation errors  -*/
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                                    VkDebugUtilsMessageTypeFlagsEXT type,
                                                    const VkDebugUtilsMessengerCallbackDataEXT *callbackData,
                                                    void *userData) {
    (void)type;
    (void)userData;
    if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0) {
        RL_ERROR("[VULKAN] %s", callbackData->pMessage);
    } else if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) != 0) {
        RL_WARN("[VULKAN] %s", callbackData->pMessage);
    } else {
        RL_INFO("[VULKAN] %s", callbackData->pMessage);
    }
    return VK_FALSE;
}

#if defined(PLATFORM_MACOS)
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

static void *vk_loader_handle;

static PFN_vkGetInstanceProcAddr vk_try_load_proc_addr_path(const char *path) {
    void *handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        return nullptr;
    }
    void *symbol = dlsym(handle, "vkGetInstanceProcAddr");
    if (!symbol) {
        dlclose(handle);
        return nullptr;
    }
    vk_loader_handle = handle;
    return (PFN_vkGetInstanceProcAddr)symbol;
}

static PFN_vkGetInstanceProcAddr vk_try_load_proc_addr() {
    const char *candidates[] = {
        "libvulkan.1.dylib",
        "libvulkan.dylib",
        "libMoltenVK.dylib",
        "/opt/homebrew/lib/libvulkan.1.dylib",
        "/opt/homebrew/lib/libvulkan.dylib",
        "/opt/homebrew/lib/libMoltenVK.dylib",
        "/usr/local/lib/libvulkan.1.dylib",
        "/usr/local/lib/libvulkan.dylib",
        "/usr/local/lib/libMoltenVK.dylib"
    };

    for (u32 i = 0; i < (u32)(sizeof(candidates) / sizeof(candidates[0])); i++) {
        PFN_vkGetInstanceProcAddr proc = vk_try_load_proc_addr_path(candidates[i]);
        if (proc) {
            return proc;
        }
    }

    const char *sdk_root = getenv("VULKAN_SDK");
    if (!sdk_root || sdk_root[0] == '\0') {
        return nullptr;
    }

    char path[1024];
    for (u32 i = 0; i < (u32)(sizeof(candidates) / sizeof(candidates[0])); i++) {
        int written = snprintf(path, sizeof(path), "%s/lib/%s", sdk_root, candidates[i]);
        if (written <= 0 || written >= (int)sizeof(path)) {
            continue;
        }
        PFN_vkGetInstanceProcAddr proc = vk_try_load_proc_addr_path(path);
        if (proc) {
            return proc;
        }
    }

    return nullptr;
}
#endif

b8 vk_instance_create(VK_Context *context) {
    ARENA_SCRATCH_START();
#ifdef _DEBUG
    b8 enable_validation = true;
#else
    b8 enable_validation = false;
#endif

    VkResult result = volkInitialize();
    if (result != VK_SUCCESS) {
#if defined(PLATFORM_MACOS)
        RL_WARN("Failed to initialize Vulkan loader via volk (VkResult=%s)", string_VkResult(result));
        PFN_vkGetInstanceProcAddr proc = vk_try_load_proc_addr();
        if (!proc) {
            RL_ERROR("Failed to locate Vulkan loader or MoltenVK. Install Vulkan SDK or set VULKAN_SDK/DYLD_LIBRARY_PATH.");
            return false;
        }
        volkInitializeCustom(proc);
        if (!vkEnumerateInstanceVersion) {
            RL_ERROR("Vulkan loader still unavailable after direct load");
            return false;
        }
#else
        RL_ERROR("Failed to initialize Vulkan loader (VkResult=%s)", string_VkResult(result));
        return false;
#endif
    }

    // Debug create info (used twice)
    VkDebugUtilsMessengerCreateInfoEXT debug_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = debugCallback,
    };

    vkEnumerateInstanceVersion(&context->api_version);
    u32 requested_api = VK_API_VERSION_1_4;
#if defined(PLATFORM_MACOS)
    requested_api = VK_API_VERSION_1_2;
#endif
    RL_INFO("Vulkan loader version: %d.%d", VK_VERSION_MAJOR(context->api_version), VK_VERSION_MINOR(context->api_version));
    if (context->api_version < requested_api) {
        RL_ERROR("Vulkan loader too old. Requires %d.%d", VK_VERSION_MAJOR(requested_api), VK_VERSION_MINOR(requested_api));
        return false;
    }

    u32 layer_count = 0;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);

    VkLayerProperties *layers = rl_arena_push(scratch.arena, sizeof(VkLayerProperties) * layer_count, true);
    vkEnumerateInstanceLayerProperties(&layer_count, layers);

    RL_TRACE("Available vulkan layers for platform (%s): %d", platform_get_info()->platform_name, layer_count);

    const char *validation_layers[] = {"VK_LAYER_KHRONOS_validation"};
    b8 found_validation = false;

    for (u32 i = 0; i < layer_count; i++) {
        RL_TRACE("Available vulkan layer: %s", layers[i].layerName);
        if (cstr_eq("VK_LAYER_KHRONOS_validation", layers[i].layerName)) {
            found_validation = true;
        }
    }
    if (enable_validation && !found_validation) {
#if defined(PLATFORM_MACOS)
        RL_WARN("Vulkan missing debug validation layer; disabling validation");
        enable_validation = false;
#else
        RL_FATAL("Vulkan missing debug validation layer");
#endif
    }

    u32 instance_ext_count = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &instance_ext_count, nullptr);

    VkExtensionProperties *available_extensions = rl_arena_push(scratch.arena, sizeof(VkExtensionProperties) * instance_ext_count, true);
    vkEnumerateInstanceExtensionProperties(nullptr, &instance_ext_count, available_extensions);

    RL_TRACE("Available vulkan extensions for platform (%s): %d", platform_get_info()->platform_name, instance_ext_count);
    for (u32 i = 0; i < instance_ext_count; i++) {
        RL_TRACE("Available vulkan ext: %s", available_extensions[i].extensionName);
    }

    const char **platform_exts = nullptr;
    u32 platform_ext_count = platform_get_required_vulkan_extensions(&platform_exts, enable_validation);

    RL_TRACE("Required vulkan extensions for platform (%s): %d", platform_get_info()->platform_name, platform_ext_count);
    const char **enabled_exts = rl_arena_push(scratch.arena, sizeof(const char *) * platform_ext_count, true);
    u32 enabled_ext_count = 0;
#if defined(PLATFORM_MACOS)
    b8 enable_portability_enumeration = false;
#endif
    for (u32 i = 0; i < platform_ext_count; i++) {
        RL_TRACE("Required vulkan ext: %s", platform_exts[i]);
        b8 found = false;
        for (u32 j = 0; j < instance_ext_count; j++) {
            if (cstr_eq(platform_exts[i], available_extensions[j].extensionName)) {
                found = true;
                break;
            }
        }
        if (!found) {
#if defined(PLATFORM_MACOS)
            b8 is_portability_enum = cstr_contains(platform_exts[i], "portability_enumeration");
            if (is_portability_enum) {
                RL_WARN("Vulkan loader missing %s; continuing without portability enumeration", platform_exts[i]);
                continue;
            }
#endif
            RL_FATAL("Failed to find required vulkan extension: %s", platform_exts[i]);
        }
#if defined(PLATFORM_MACOS)
        if (cstr_contains(platform_exts[i], "portability_enumeration")) {
            enable_portability_enumeration = true;
        }
#endif
        enabled_exts[enabled_ext_count++] = platform_exts[i];
    }

    u32 enabled_layer_count = 0;
    const char **enabled_layers = nullptr;

    if (enable_validation) {
        enabled_layers = validation_layers;
        enabled_layer_count = 1;
    }

    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Realm";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "RealmEngine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = requested_api;

    VkInstanceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    create_info.flags = 0;
#if defined(PLATFORM_MACOS)
    if (enable_portability_enumeration) {
        create_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    }
#endif
    create_info.enabledLayerCount = enabled_layer_count;
    create_info.ppEnabledLayerNames = enabled_layers;
    create_info.enabledExtensionCount = enabled_ext_count;
    create_info.ppEnabledExtensionNames = enabled_exts;
    create_info.pNext = enable_validation ? &debug_create_info : nullptr;

    result = vkCreateInstance(&create_info, nullptr, &context->instance);
    if (result != VK_SUCCESS) {
        RL_ERROR("Failed to create Vulkan instance (VkResult=%s)", string_VkResult(result));
        return false;
    }

    volkLoadInstance(context->instance);

    // Create messenger AFTER instance
    if (enable_validation) {
        VK_CHECK(vkCreateDebugUtilsMessengerEXT(context->instance, &debug_create_info, nullptr, &context->debug_messenger));
        RL_DEBUG("Vulkan validation Layers: ON");
    }

    RL_TRACE("Vulkan instance created successfully");

    ARENA_SCRATCH_RELEASE();
    return true;
}


void vk_instance_destroy(VK_Context *context) {
    if (context->debug_messenger) {
        vkDestroyDebugUtilsMessengerEXT(context->instance, context->debug_messenger, nullptr);
    }
    vkDestroyInstance(context->instance, nullptr);
#if defined(PLATFORM_MACOS)
    if (vk_loader_handle) {
        dlclose(vk_loader_handle);
        vk_loader_handle = nullptr;
    }
#endif
}
