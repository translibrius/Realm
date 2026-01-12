#include "renderer/vulkan/vk_renderer.h"

#include "vk_device.h"
#include "renderer/vulkan/vk_instance.h"

static VK_Context context;

b8 vulkan_initialize(platform_window *window, rl_camera *camera) {
    if (!vk_instance_create(&context)) {
        RL_ERROR("failed to create vulkan instance");
        return false;
    }

    if (!platform_create_vulkan_surface(window, &context)) {
        RL_ERROR("failed to create vulkan surface");
    }

    if (!vk_device_init(&context)) {
        RL_ERROR("failed to find suitable GPU device");
        return false;
    }

    return true;
}

void vulkan_destroy() {
    vk_device_destroy(&context);
    vkDestroySurfaceKHR(context.instance, context.surface, nullptr);
    vk_instance_destroy(&context);
}

void vulkan_begin_frame(f64 delta_time) {

}

void vulkan_end_frame() {

}

void vulkan_swap_buffers() {

}

void vulkan_set_view_projection(mat4 view, mat4 projection) {

}