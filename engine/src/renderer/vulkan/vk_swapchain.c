#include "renderer/vulkan/vk_swapchain.h"

#include "vk_frame_buffers.h"
#include "vk_pipeline.h"
#include "vk_renderpass.h"

void log_capabilities(VkSurfaceCapabilitiesKHR *caps);
void log_surface_formats(const VkSurfaceFormat2KHR *formats, u32 count);
static const char *present_mode_str(VkPresentModeKHR mode);
void log_present_modes(const VkPresentModeKHR *modes, u32 count);
i32 score_format(VkSurfaceFormatKHR *f);

void create_image_views(VK_Context *context);
void destroy_image_views(VK_Context *context);

VkSurfaceFormat2KHR vk_swapchain_choose_format(VK_Context *context, b8 from_recreate);
VkPresentModeKHR vk_swapchain_choose_present_mode(VK_Context *context, b8 from_recreate);
VkExtent2D vk_swapchain_choose_extent(VK_Context *context);

void vk_swapchain_fetch_support(VK_Context *context, VkPhysicalDevice physical_device) {
    if (context->swapchain.formats != nullptr) {
        mem_free(context->swapchain.formats, sizeof(VkSurfaceFormat2KHR) * context->swapchain.format_count, MEM_SUBSYSTEM_RENDERER);
    }

    if (context->swapchain.present_modes != nullptr) {
        mem_free(context->swapchain.present_modes, sizeof(VkPresentModeKHR) * context->swapchain.present_mode_count, MEM_SUBSYSTEM_RENDERER);
    }

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
    context->swapchain.formats = mem_alloc(sizeof(VkSurfaceFormat2KHR) * context->swapchain.format_count, MEM_SUBSYSTEM_RENDERER);
    for (u32 i = 0; i < context->swapchain.format_count; ++i) {
        context->swapchain.formats[i].sType =
            VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR;
        context->swapchain.formats[i].pNext = nullptr;
    }
    vkGetPhysicalDeviceSurfaceFormats2KHR(physical_device, &surface_info2, &context->swapchain.format_count, context->swapchain.formats);

    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, context->surface, &context->swapchain.present_mode_count, nullptr);
    context->swapchain.present_modes = mem_alloc(sizeof(VkPresentModeKHR) * context->swapchain.present_mode_count, MEM_SUBSYSTEM_RENDERER);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, context->surface, &context->swapchain.present_mode_count, context->swapchain.present_modes);
}

b8 vk_swapchain_create(VK_Context *context, b8 vsync, b8 from_recreate, VkSwapchainKHR old_swapchain) {
    if (!from_recreate) {
        vk_swapchain_fetch_support(context, context->physical_device);
        log_capabilities(&context->swapchain.capabilities2.surfaceCapabilities);
        log_surface_formats(context->swapchain.formats, context->swapchain.format_count);
        log_present_modes(context->swapchain.present_modes, context->swapchain.present_mode_count);
    }

    context->swapchain.vsync = vsync;
    context->swapchain.chosen_format = vk_swapchain_choose_format(context, from_recreate);
    context->swapchain.chosen_present_mode = vk_swapchain_choose_present_mode(context, from_recreate);
    context->swapchain.chosen_extent = vk_swapchain_choose_extent(context);

    u32 min_image_count = context->swapchain.capabilities2.surfaceCapabilities.minImageCount;
    u32 preferred_image_count = RL_MAX(min_image_count, 3); // Prefer 3, but respect minimum

    // Handle the maxImageCount case where 0 means "no upper limit"
    u32 max_image_count = 0;
    if (context->swapchain.capabilities2.surfaceCapabilities.maxImageCount == 0) {
        max_image_count = preferred_image_count;
    } else {
        max_image_count = context->swapchain.capabilities2.surfaceCapabilities.maxImageCount;
    }

    context->max_frames_in_flight = RL_CLAMP(preferred_image_count, min_image_count, max_image_count);

    VkSwapchainCreateInfoKHR create_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = nullptr,
        .surface = context->surface,
        .minImageCount = context->max_frames_in_flight,
        .imageFormat = context->swapchain.chosen_format.surfaceFormat.format,
        .imageColorSpace = context->swapchain.chosen_format.surfaceFormat.colorSpace,
        .imageExtent = context->swapchain.chosen_extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, // TODO: | VK_IMAGE_USAGE_TRANSFER_DST_BIT
        .preTransform = context->swapchain.capabilities2.surfaceCapabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = context->swapchain.chosen_present_mode,
        .clipped = VK_TRUE,
        .oldSwapchain = old_swapchain,
    };

    u32 queue_family_indices[2] = {
        context->queue_families.graphics_index,
        context->queue_families.present_index
    };

    if (context->queue_families.graphics_index != context->queue_families.present_index) {
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = queue_family_indices;
    } else {
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        create_info.queueFamilyIndexCount = 0; // Optional
        create_info.pQueueFamilyIndices = nullptr; // Optional
    }

    VkResult result = vkCreateSwapchainKHR(context->device, &create_info, nullptr, &context->swapchain.handle);
    if (result != VK_SUCCESS) {
        RL_ERROR("failed to create swapchain! VkResult=%s", string_VkResult(result));
        return false;
    }

    // Retrieve swapchain images
    context->swapchain.image_count = 0;
    vkGetSwapchainImagesKHR(context->device, context->swapchain.handle, &context->swapchain.image_count, nullptr);
    if (context->swapchain.image_count < context->max_frames_in_flight) {
        RL_ERROR("wrong swapchain setup");
        return false;
    }
    context->max_frames_in_flight = context->swapchain.image_count;

    context->swapchain.images = mem_alloc(sizeof(VkImage) * context->max_frames_in_flight, MEM_SUBSYSTEM_RENDERER);
    vkGetSwapchainImagesKHR(context->device, context->swapchain.handle, &context->swapchain.image_count, context->swapchain.images);

    create_image_views(context);

    if (!from_recreate) {
        RL_INFO("Vulkan swapchain created successfully");
    }
    return true;
}

void vk_swapchain_destroy(VK_Context *context) {
    destroy_image_views(context);
    if (context->swapchain.handle != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(context->device, context->swapchain.handle, nullptr);
        context->swapchain.handle = VK_NULL_HANDLE;
    }
}

b8 vk_swapchain_recreate(VK_Context *context) {
    if (context->window->settings.height <= 0 || context->window->settings.width <= 0) {
        return false;
    }

    vk_swapchain_fetch_support(context, context->physical_device);
    if (context->swapchain.capabilities2.surfaceCapabilities.currentExtent.width <= 0 || context->swapchain.capabilities2.surfaceCapabilities.currentExtent.height <= 0) {
        return false;
    }

    vkDeviceWaitIdle(context->device);
    RL_TRACE("Recreating swapchain...");

    vk_framebuffers_destroy(context);
    destroy_image_views(context);

    VkSwapchainKHR old_swapchain = context->swapchain.handle;

    if (!vk_swapchain_create(context, context->swapchain.vsync, true, old_swapchain)) {
        RL_ERROR("Failed to recreate swapchain");
        return false;
    }

    // Now that new swapchain is live, destroy old one
    vkDestroySwapchainKHR(context->device, old_swapchain, nullptr);

    if (!vk_framebuffers_create(context)) {
        RL_ERROR("Failed to recreate swapchain: framebuffers could not be created");
        return false;
    }

    return true;
}

// Private

void create_image_views(VK_Context *context) {
    context->swapchain.image_views = mem_alloc(sizeof(VkImageView) * context->swapchain.image_count, MEM_SUBSYSTEM_RENDERER);

    VkImageViewCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = context->swapchain.chosen_format.surfaceFormat.format,
        .components = {
            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .a = VK_COMPONENT_SWIZZLE_IDENTITY
        },
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    for (u32 i = 0; i < context->swapchain.image_count; i++) {
        create_info.image = context->swapchain.images[i];
        VK_CHECK(vkCreateImageView(context->device, &create_info, nullptr, &context->swapchain.image_views[i]));
    }

    RL_TRACE("Vulkan image views created successfully!");
}

void destroy_image_views(VK_Context *context) {
    if (context->swapchain.image_views) {
        for (u32 i = 0; i < context->swapchain.image_count; i++) {
            if (context->swapchain.image_views[i]) {
                vkDestroyImageView(context->device, context->swapchain.image_views[i], nullptr);
            }
        }
        mem_free(context->swapchain.image_views,
                 sizeof(VkImageView) * context->swapchain.image_count,
                 MEM_SUBSYSTEM_RENDERER);
        context->swapchain.image_views = nullptr;
    }

    if (context->swapchain.images) {
        mem_free(context->swapchain.images,
                 sizeof(VkImage) * context->max_frames_in_flight,
                 MEM_SUBSYSTEM_RENDERER);
        context->swapchain.images = nullptr;
    }

    context->swapchain.image_count = 0;
    context->max_frames_in_flight = 0;
}

VkExtent2D vk_swapchain_choose_extent(VK_Context *context) {
    VkSurfaceCapabilitiesKHR *caps = &context->swapchain.capabilities2.surfaceCapabilities;

    // If Vulkan gives us the exact size, trust it.
    if (caps->currentExtent.width != UINT32_MAX) {
        RL_TRACE("Swap extent chosen: %ux%u (fixed by surface)",
                 caps->currentExtent.width, caps->currentExtent.height);
        return caps->currentExtent;
    }

    // Otherwise, compute it ourselves.
    // You should already track framebuffer size in pixels.
    u32 fb_width = context->window->settings.width;
    u32 fb_height = context->window->settings.height;

    VkExtent2D actual = {
        .width = fb_width,
        .height = fb_height
    };

    // Clamp to allowed range.
    actual.width = RL_CLAMP(actual.width,
                            caps->minImageExtent.width,
                            caps->maxImageExtent.width);
    actual.height = RL_CLAMP(actual.height,
                             caps->minImageExtent.height,
                             caps->maxImageExtent.height);

    RL_TRACE("Swap extent chosen: %ux%u (from window DPI / clamped)", actual.width, actual.height);

    return actual;
}


VkSurfaceFormat2KHR vk_swapchain_choose_format(VK_Context *context, b8 from_recreate) {
    u32 format_count = context->swapchain.format_count;
    VkSurfaceFormat2KHR *formats = context->swapchain.formats;

    VkSurfaceFormat2KHR best = formats[0];
    i32 best_score = score_format(&formats[0].surfaceFormat);

    for (u32 i = 0; i < format_count; ++i) {
        i32 score = score_format(&formats[i].surfaceFormat);
        if (!from_recreate) {
            RL_TRACE("Swapchain format: %s Score=%d", string_VkFormat(formats[i].surfaceFormat.format), score);
        }
        if (score > best_score) {
            best = formats[i];
            best_score = score;
        }
    }

    return best;
}

VkPresentModeKHR vk_swapchain_choose_present_mode(VK_Context *context, b8 from_recreate) {
    u32 count = context->swapchain.present_mode_count;
    VkPresentModeKHR *modes = context->swapchain.present_modes;

    // If vsync requested, always use FIFO (guaranteed by spec).
    if (context->swapchain.vsync) {
        if (!from_recreate) {
            RL_TRACE("Present mode chosen: FIFO (vsync ON)");
        }
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    b8 has_mailbox = false;
    b8 has_immediate = false;
    b8 has_fifo_relaxed = false;

    // Scan once to see what's available.
    for (u32 i = 0; i < count; ++i) {
        VkPresentModeKHR pm = modes[i];
        if (pm == VK_PRESENT_MODE_MAILBOX_KHR) {
            has_mailbox = true;
        } else if (pm == VK_PRESENT_MODE_IMMEDIATE_KHR) {
            has_immediate = true;
        } else if (pm == VK_PRESENT_MODE_FIFO_RELAXED_KHR) {
            has_fifo_relaxed = true;
        }
    }

    // Best choices based on intent (vsync off).
    if (has_mailbox) {
        if (!from_recreate) {
            RL_TRACE("Present mode chosen: MAILBOX (vsync OFF)");
        }
        return VK_PRESENT_MODE_MAILBOX_KHR;
    }

    if (has_immediate) {
        if (!from_recreate) {
            RL_TRACE("Present mode chosen: IMMEDIATE (vsync OFF)");
        }
        return VK_PRESENT_MODE_IMMEDIATE_KHR;
    }

    if (has_fifo_relaxed) {
        if (!from_recreate) {
            RL_TRACE("Present mode chosen: FIFO_RELAXED (vsync OFF fallback)");
        }
        return VK_PRESENT_MODE_FIFO_RELAXED_KHR;
    }

    if (!from_recreate) {
        RL_TRACE("Present mode chosen: FIFO (vsync OFF fallback)");
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

i32 score_format(VkSurfaceFormatKHR *f) {
    int s = 0;

    // Color space
    if (f->colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        s += 100;
    else
        s -= 50;

    switch (f->format) {
    case VK_FORMAT_B8G8R8A8_SRGB:
        s += 400;
        break;
    case VK_FORMAT_R8G8B8A8_SRGB:
        s += 350;
        break;
    case VK_FORMAT_B8G8R8A8_UNORM:
        s += 200;
        break;
    case VK_FORMAT_R8G8B8A8_UNORM:
        s += 150;
        break;
    // 8-bit BGRA/RGBA with some unusual space
    case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
    case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
        s += 50;
        break;
    default:
        // Unknown / weird? probably slow, maybe unsupported blending, etc.
        s -= 100;
        break;
    }

    return s;
}

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

