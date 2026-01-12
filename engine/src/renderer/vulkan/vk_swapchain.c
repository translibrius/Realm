#include "renderer/vulkan/vk_swapchain.h"

void log_capabilities(VkSurfaceCapabilitiesKHR *caps);
void log_surface_formats(const VkSurfaceFormat2KHR *formats, u32 count);
static const char *present_mode_str(VkPresentModeKHR mode);
void log_present_modes(const VkPresentModeKHR *modes, u32 count);

void vk_swapchain_fetch_support(VK_Context *context, VkPhysicalDevice physical_device) {
    context->swapchain = (VK_Swapchain){0};

    // Query the physical device's capabilities for the given surface.
    const VkPhysicalDeviceSurfaceInfo2KHR surface_info2 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR,
        .surface = context->surface
    };

    VkSurfaceCapabilities2KHR capabilities2 = {
        .sType = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR
    };

    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilities2KHR(physical_device, &surface_info2, &capabilities2));

    context->swapchain.capabilities2 = capabilities2;

    vkGetPhysicalDeviceSurfaceFormats2KHR(physical_device, &surface_info2, &context->swapchain.format_count, nullptr);
    context->swapchain.formats = rl_arena_push(&context->arena, sizeof(VkSurfaceFormat2KHR) * context->swapchain.format_count, true);
    for (u32 i = 0; i < context->swapchain.format_count; ++i) {
        context->swapchain.formats[i].sType =
            VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR;
        context->swapchain.formats[i].pNext = nullptr;
    }
    vkGetPhysicalDeviceSurfaceFormats2KHR(physical_device, &surface_info2, &context->swapchain.format_count, context->swapchain.formats);

    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, context->surface, &context->swapchain.present_mode_count, nullptr);
    context->swapchain.present_modes = rl_arena_push(&context->arena, sizeof(VkPresentModeKHR) * context->swapchain.present_mode_count, true);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, context->surface, &context->swapchain.present_mode_count, context->swapchain.present_modes);
}

b8 vk_swapchain_init(VK_Context *context, b8 vsync) {

    log_capabilities(&context->swapchain.capabilities2.surfaceCapabilities);
    log_surface_formats(context->swapchain.formats, context->swapchain.format_count);
    log_present_modes(context->swapchain.present_modes, context->swapchain.present_mode_count);

    return true;
}

void vk_swapchain_destroy(VK_Context *context) {

}

// Private

void log_capabilities(VkSurfaceCapabilitiesKHR *caps) {
    // General surface properties
    RL_TRACE("---- Vulkan Surface Capabilities ----");
    RL_TRACE("Min Image Count:   %u", caps->minImageCount);
    RL_TRACE("Max Image Count:   %u", caps->maxImageCount == 0 ? 0 : caps->maxImageCount);
    RL_TRACE("Current Extent:    %u x %u", caps->currentExtent.width, caps->currentExtent.height);
    RL_TRACE("Min Extent:        %u x %u", caps->minImageExtent.width, caps->minImageExtent.height);
    RL_TRACE("Max Extent:        %u x %u", caps->maxImageExtent.width, caps->maxImageExtent.height);

    // Swapchain transforms
    RL_TRACE("Supported Transforms: 0x%08X", caps->supportedTransforms);
    RL_TRACE("Current Transform:    0x%08X", caps->currentTransform);

    // Composite alpha (window transparency rules)
    RL_TRACE("Supported Composite Alpha: 0x%08X", caps->supportedCompositeAlpha);

    // Usage flags (what you can do with the swapchain images)
    RL_TRACE("Supported Usage Flags: 0x%08X", caps->supportedUsageFlags);

    // In Vulkan ≥1.1 these sometimes matter for multisampling
    RL_TRACE("Supported Image Array Layers: %u", caps->maxImageArrayLayers);

    RL_TRACE("-----------------------------");
}

void log_surface_formats(const VkSurfaceFormat2KHR *formats, u32 count) {
    RL_TRACE("---- Vulkan Surface Formats ----");
    RL_TRACE("Format Count: %u", count);

    for (u32 i = 0; i < count; i++) {
        const VkSurfaceFormatKHR *f = &formats[i].surfaceFormat;

        RL_TRACE("[%u] Format = %u (%s), Color Space = %u",
                 i,
                 f->format,
                 string_VkFormat(f->format),
                 f->colorSpace);
    }

    RL_TRACE("-------------------------------");
}

static const char *present_mode_str(VkPresentModeKHR mode) {
    switch (mode) {
    case VK_PRESENT_MODE_IMMEDIATE_KHR:
        return "IMMEDIATE (torn frames, no vsync)";
    case VK_PRESENT_MODE_MAILBOX_KHR:
        return "MAILBOX (tearing-free, triple buffer)";
    case VK_PRESENT_MODE_FIFO_KHR:
        return "FIFO (vsync, guaranteed)";
    case VK_PRESENT_MODE_FIFO_RELAXED_KHR:
        return "FIFO_RELAXED (vsync but less strict)";
    default:
        return "UNKNOWN";
    }
}

void log_present_modes(const VkPresentModeKHR *modes, u32 count) {
    RL_TRACE("---- Vulkan Present Modes ----");
    RL_TRACE("Present Mode Count: %u", count);

    for (u32 i = 0; i < count; i++) {
        RL_TRACE("[%u] %u (%s)", i, modes[i], present_mode_str(modes[i]));
    }

    RL_TRACE("--------------------------------");
}

