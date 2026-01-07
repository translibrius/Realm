#include "renderer/vulkan/vk_instance.h"

#include "memory/containers/dynamic_array.h"

DA_DEFINE(Strings, char*);

b8 vk_instance_create() {
#ifdef _DEBUG
    constexpr bool enable_validation = true;
#else
    constexpr bool enable_validation = false;
#endif

    Strings vk_strings;
    da_init(&vk_strings);

    da_append(&vk_strings, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

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

    da_free(&vk_strings);

    return true;
}