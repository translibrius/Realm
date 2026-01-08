#include "renderer/vulkan/vk_instance.h"

#include "core/logger.h"
#include "util/assert.h"

b8 vk_instance_create(VK_Context *context) {
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

    VkExtensionProperties *available_extensions = mem_alloc(sizeof(VkExtensionProperties) * instance_ext_count, MEM_SUBSYSTEM_RENDERER);
    vkEnumerateInstanceExtensionProperties(nullptr, &instance_ext_count, available_extensions);

    RL_TRACE("VK Instance Extension Count: %lu", instance_ext_count);

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
    create_info.enabledLayerCount = 0;
    create_info.enabledExtensionCount = 0;

    VkResult result = vkCreateInstance(&create_info, nullptr, &context->instance);
    if (result != VK_SUCCESS) {
        return false;
    }

    RL_TRACE("Vulkan instance created successfully");

    return true;
}

void vk_instance_destroy(VK_Context *context) {
    vkDestroyInstance(context->instance, nullptr);
}