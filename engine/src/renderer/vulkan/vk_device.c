#include "vk_device.h"

// To gather unique queue indexes
typedef struct QueueRequest {
    u32 family_index;
    u32 count;
} QueueRequest;

typedef struct VkCandidate {
    VkPhysicalDevice device;
    VkPhysicalDeviceProperties2 props;
    VkPhysicalDeviceFeatures2 feats;
    VkPhysicalDeviceVulkan11Features features11;
    VkPhysicalDeviceVulkan12Features features12;
    VkPhysicalDeviceVulkan13Features features13;
    VkPhysicalDeviceVulkan14Features features14;
    VK_QueueFamilyIndices indices;
    u32 score;
} VkCandidate;

static void add_unique_family(u32 family_index, QueueRequest *reqs, u32 *req_count) {
    for (u32 i = 0; i < *req_count; i++) {
        if (reqs[i].family_index == family_index) {
            return; // Already added
        }
    }

    reqs[*req_count].family_index = family_index;
    reqs[*req_count].count = 1;
    (*req_count)++;
}

static u32 score_gpu_device(const VkPhysicalDeviceProperties2 *props,
                            const VkPhysicalDeviceFeatures2 *feats) {
    (void)feats;
    u32 score = 0;

    // Prefer discrete GPUs
    if (props->properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        score += 1000;

    // Prefer newer generations
    score += props->properties.limits.maxImageDimension2D / 2048;

    return score;
}

static VK_QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device,
                                               VkSurfaceKHR surface) {
    ARENA_SCRATCH_START();

    VK_QueueFamilyIndices q = {0};
    u32 count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties2(device, &count, NULL);

    VkQueueFamilyProperties2 *props =
        rl_arena_push(scratch.arena,
                      sizeof(VkQueueFamilyProperties2) * count,
                      true);
    for (u32 i = 0; i < count; i++) {
        props[i].sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
    }

    vkGetPhysicalDeviceQueueFamilyProperties2(device, &count, props);

    i32 dedicated_transfer = -1;
    i32 dedicated_compute = -1;

    for (u32 i = 0; i < count; i++) {
        VkQueueFlags flags = props[i].queueFamilyProperties.queueFlags;

        VkBool32 present_supported = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present_supported);

        /* Graphics */
        if (!q.has_graphics && (flags & VK_QUEUE_GRAPHICS_BIT)) {
            q.graphics_index = i;
            q.has_graphics = true;
        }

        /* Compute */
        if (flags & VK_QUEUE_COMPUTE_BIT) {
            if (!q.has_compute) {
                q.compute_index = i;
                q.has_compute = true;
            }
            if (!(flags & VK_QUEUE_GRAPHICS_BIT))
                dedicated_compute = i;
        }

        /* Transfer */
        if (flags & VK_QUEUE_TRANSFER_BIT) {
            if (!q.has_transfer) {
                q.transfer_index = i;
                q.has_transfer = true;
            }
            if (!(flags & VK_QUEUE_GRAPHICS_BIT) &&
                !(flags & VK_QUEUE_COMPUTE_BIT))
                dedicated_transfer = i;
        }

        /* Present */
        if (present_supported && !q.has_present) {
            q.present_index = i;
            q.has_present = true;
        }

        /* Jackpot: graphics+present same family */
        if (present_supported &&
            (flags & VK_QUEUE_GRAPHICS_BIT)) {
            q.graphics_index = i;
            q.present_index = i;
            q.has_graphics = true;
            q.has_present = true;
        }
    }

    if (dedicated_compute >= 0) {
        q.compute_index = (u32)dedicated_compute;
        q.has_compute = true;
    }

    if (dedicated_transfer >= 0) {
        q.transfer_index = (u32)dedicated_transfer;
        q.has_transfer = true;
    }

    ARENA_SCRATCH_RELEASE();
    return q;
}

b8 create_logical_device(VK_Context *context, VkCandidate *candidate) {
    ARENA_SCRATCH_START();

    QueueRequest reqs[4];
    u32 req_count = 0;

    if (candidate->indices.has_graphics)
        add_unique_family(candidate->indices.graphics_index, reqs, &req_count);
    if (candidate->indices.has_compute)
        add_unique_family(candidate->indices.compute_index, reqs, &req_count);
    if (candidate->indices.has_transfer)
        add_unique_family(candidate->indices.transfer_index, reqs, &req_count);
    if (candidate->indices.has_present)
        add_unique_family(candidate->indices.present_index, reqs, &req_count);

    f32 priority = 1.0f;
    VkDeviceQueueCreateInfo *queue_infos =
        rl_arena_push(scratch.arena,
                      sizeof(VkDeviceQueueCreateInfo) * req_count,
                      true);

    for (u32 i = 0; i < req_count; i++) {
        queue_infos[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_infos[i].queueFamilyIndex = reqs[i].family_index;
        queue_infos[i].queueCount = reqs[i].count; // 1 each for now
        queue_infos[i].pQueuePriorities = &priority;
    }

    // Re-chain candidate feature structs (the copies lost their original pNext)
    candidate->features14.pNext = NULL;
    candidate->features13.pNext = &candidate->features14;
    candidate->features12.pNext = &candidate->features13;
    candidate->features11.pNext = &candidate->features12;

    candidate->feats.pNext = &candidate->features11;

    // Required device extensions (minimal)
    const char *device_extensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    VkDeviceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &candidate->feats, // HEAD of features2 chain
        .flags = 0,
        .queueCreateInfoCount = req_count,
        .pQueueCreateInfos = queue_infos,

        // Device layers are deprecated; leave them empty
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,

        .enabledExtensionCount = (u32)(sizeof(device_extensions) / sizeof(device_extensions[0])),
        .ppEnabledExtensionNames = device_extensions,

        // Must be NULL when using VkPhysicalDeviceFeatures2
        .pEnabledFeatures = nullptr,
    };

    VkResult res = vkCreateDevice(context->physical_device, &create_info, nullptr, &context->device);
    if (res != VK_SUCCESS) {
        RL_ERROR("failed to create logical device (VkResult=%s)", string_VkResult(res));
        ARENA_SCRATCH_RELEASE();
        return false;
    }

    if (candidate->indices.has_graphics) {
        vkGetDeviceQueue(context->device, candidate->indices.graphics_index, 0, &context->graphics_queue);
    }

    if (candidate->indices.has_compute) {
        vkGetDeviceQueue(context->device, candidate->indices.compute_index, 0, &context->compute_queue);
    }

    if (candidate->indices.has_transfer) {
        vkGetDeviceQueue(context->device, candidate->indices.transfer_index, 0, &context->transfer_queue);
    }

    if (candidate->indices.has_present && candidate->indices.present_index != candidate->indices.graphics_index) {
        vkGetDeviceQueue(context->device, candidate->indices.present_index, 0, &context->present_queue);
    } else {
        // Preset == graphics family, reuse graphics_queue
        context->present_queue = context->graphics_queue;
    }
    context->queue_families = candidate->indices;

    ARENA_SCRATCH_RELEASE();
    return true;
}

b8 vk_device_init(VK_Context *context) {
    ARENA_SCRATCH_START();
    u32 physical_device_count = 0;
    vkEnumeratePhysicalDevices(context->instance, &physical_device_count, nullptr);

    RL_TRACE("Vulkan physical devices found: %lu", physical_device_count);
    if (physical_device_count <= 0) {
        RL_ERROR("Vulkan failed to find a supported GPU");
        return false;
    }

    VkPhysicalDevice *physical_devices = rl_arena_push(scratch.arena, sizeof(VkPhysicalDevice) * physical_device_count, MEM_SUBSYSTEM_RENDERER);
    vkEnumeratePhysicalDevices(context->instance, &physical_device_count, physical_devices);

    VkCandidate best = {0};
    best.score = 0;

    for (u32 i = 0; i < physical_device_count; i++) {
        // Modern Vulkan feature chain (1.0 → 1.4)
        VkPhysicalDeviceVulkan11Features features11 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES
        };
        VkPhysicalDeviceVulkan12Features features12 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES
        };
        VkPhysicalDeviceVulkan13Features features13 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES
        };
        VkPhysicalDeviceVulkan14Features features14 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES
        };

        features13.pNext = &features14;
        features12.pNext = &features13;
        features11.pNext = &features12;

        VkPhysicalDeviceProperties2 props = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2
        };

        VkPhysicalDeviceFeatures2 feats = {
            .pNext = &features11,
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2
        };

        vkGetPhysicalDeviceProperties2(physical_devices[i], &props);
        vkGetPhysicalDeviceFeatures2(physical_devices[i], &feats);

        // Require Vulkan 1.4+
        if (props.properties.apiVersion < VK_API_VERSION_1_4) {
            RL_TRACE("    Skipped: %s only supports Vulkan %u.%u.%u",
                     props.properties.deviceName,
                     VK_VERSION_MAJOR(props.properties.apiVersion),
                     VK_VERSION_MINOR(props.properties.apiVersion),
                     VK_VERSION_PATCH(props.properties.apiVersion));
            continue;
        }

        // Require queues
        VK_QueueFamilyIndices indices = findQueueFamilies(physical_devices[i], context->surface);
        if (!(indices.has_graphics && indices.has_present)) {
            RL_TRACE("    Skipped: missing graphics/present queues");
            continue;
        }

        if (!feats.features.geometryShader) {
            RL_TRACE("    Skipped: missing feature - 'geometryShader'");
            continue;
        }

        if (!features13.dynamicRendering) {
            RL_TRACE("    Skipped: missing feature (vulkan 1.3) - 'dynamicRendering'");
            continue;
        }

        if (!features13.maintenance4) {
            RL_TRACE("Skipped: Extension VK_KHR_maintenance4 required, update driver!");
            continue;
        }

        if (!features14.maintenance5) {
            RL_TRACE("Skipped: Extension VK_KHR_maintenance5 required, update driver!");
            continue;
        }

        if (!features14.maintenance6) {
            RL_TRACE("Skipped: Extension VK_KHR_maintenance6 required, update driver!");
            continue;
        }

        RL_TRACE("GPU #%u:", i);
        RL_TRACE("    Name: %s", props.properties.deviceName);
        RL_TRACE("    Vendor ID: 0x%04X", props.properties.vendorID);
        RL_TRACE("    Device ID: 0x%04X", props.properties.deviceID);
        RL_TRACE("    Type: %u", props.properties.deviceType);
        RL_TRACE("    API Version: %u.%u.%u",
                 VK_VERSION_MAJOR(props.properties.apiVersion),
                 VK_VERSION_MINOR(props.properties.apiVersion),
                 VK_VERSION_PATCH(props.properties.apiVersion));

        u32 s = score_gpu_device(&props, &feats);
        RL_TRACE("    Score: %u", s);

        if (s > best.score) {
            best.device = physical_devices[i];
            best.props = props;
            best.feats = feats;
            best.score = s;
            best.features11 = features11;
            best.features12 = features12;
            best.features13 = features13;
            best.features14 = features14;
            best.indices = indices;
        }
    }

    if (best.score == 0) {
        RL_ERROR("Found GPUs, but none satisfied feature requirements.");
        ARENA_SCRATCH_RELEASE();
        return false;
    }

    context->physical_device = best.device;
    RL_INFO("Selected GPU: %s (score=%u) (driver=%d.%d.%d)",
            best.props.properties.deviceName, best.score,
            VK_VERSION_MAJOR(best.props.properties.driverVersion),
            VK_VERSION_MINOR(best.props.properties.driverVersion),
            VK_VERSION_PATCH(best.props.properties.driverVersion));

    RL_TRACE("Graphics=%s index=%d", best.indices.has_graphics ? "YES" : "NO", best.indices.graphics_index);
    RL_TRACE("Compute=%s index=%d", best.indices.has_compute ? "YES" : "NO", best.indices.compute_index);
    RL_TRACE("Transfer=%s index=%d", best.indices.has_transfer ? "YES" : "NO", best.indices.transfer_index);
    RL_TRACE("Present=%s index=%d", best.indices.has_present ? "YES" : "NO", best.indices.present_index);

    if (!create_logical_device(context, &best)) {
        RL_ERROR("Failed to create vulkan logical device");
        ARENA_SCRATCH_RELEASE();
        return false;
    }

    // Load all vulkan device functions
    volkLoadDevice(context->device);

    ARENA_SCRATCH_RELEASE();
    return true;
}

void vk_device_destroy(VK_Context *context) {
    (void)context;
    vkDestroyDevice(context->device, nullptr);
}