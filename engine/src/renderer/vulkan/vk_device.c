#include "vk_device.h"

typedef struct VkCandidate {
    VkPhysicalDevice device;
    VkPhysicalDeviceProperties props;
    VkPhysicalDeviceFeatures feats;
    u32 score;
} VkCandidate;


static u32 score_gpu_device(const VkPhysicalDeviceProperties *props,
                            const VkPhysicalDeviceFeatures *feats) {
    u32 score = 0;

    // Prefer discrete GPUs
    if (props->deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        score += 1000;

    // Prefer newer generations
    score += props->limits.maxImageDimension2D / 2048;

    // Example: must support geometry shaders
    if (!feats->geometryShader)
        return 0;

    return score;
}

b8 vk_device_init(VK_Context *context) {
    ARENA_SCRATCH_START();
    u32 physical_device_count = 0;
    vkEnumeratePhysicalDevices(context->instance, &physical_device_count, nullptr);

    RL_TRACE("Vulkan physical devices found: %lu", physical_device_count);
    if (physical_device_count <= 0) {
        RL_FATAL("Vulkan failed to find a supported GPU");
    }

    VkPhysicalDevice *physical_devices = rl_arena_push(scratch.arena, sizeof(VkPhysicalDevice) * physical_device_count, MEM_SUBSYSTEM_RENDERER);
    vkEnumeratePhysicalDevices(context->instance, &physical_device_count, physical_devices);

    VkCandidate best = {0};
    best.score = 0;

    for (u32 i = 0; i < physical_device_count; i++) {
        VkPhysicalDeviceProperties props;
        VkPhysicalDeviceFeatures feats;
        vkGetPhysicalDeviceProperties(physical_devices[i], &props);
        vkGetPhysicalDeviceFeatures(physical_devices[i], &feats);

        // Require Vulkan 1.4+
        if (props.apiVersion < VK_API_VERSION_1_4) {
            RL_TRACE("    Skipped: %s only supports Vulkan %u.%u.%u",
                     props.deviceName,
                     VK_VERSION_MAJOR(props.apiVersion),
                     VK_VERSION_MINOR(props.apiVersion),
                     VK_VERSION_PATCH(props.apiVersion));
            continue;
        }

        RL_TRACE("GPU #%u:", i);
        RL_TRACE("    Name: %s", props.deviceName);
        RL_TRACE("    Vendor ID: 0x%04X", props.vendorID);
        RL_TRACE("    Device ID: 0x%04X", props.deviceID);
        RL_TRACE("    Type: %u", props.deviceType);
        RL_TRACE("    API Version: %u.%u.%u",
                 VK_VERSION_MAJOR(props.apiVersion),
                 VK_VERSION_MINOR(props.apiVersion),
                 VK_VERSION_PATCH(props.apiVersion));

        u32 s = score_gpu_device(&props, &feats);
        RL_TRACE("    Score: %u", s);

        if (s > best.score) {
            best.device = physical_devices[i];
            best.props = props;
            best.feats = feats;
            best.score = s;
        }
    }

    if (best.score == 0) {
        RL_FATAL("Found GPUs, but none satisfied feature requirements.");
        return false;
    }

    context->device = best.device;
    RL_INFO("Selected GPU: %s (score=%u)", best.props.deviceName, best.score);

    ARENA_SCRATCH_RELEASE();
}

void vk_device_destroy(VK_Context *context) {

}