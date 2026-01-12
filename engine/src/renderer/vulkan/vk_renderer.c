#include "renderer/vulkan/vk_renderer.h"

#include "vk_device.h"
#include "vk_shader.h"
#include "vk_swapchain.h"
#include "vk_pipeline.h"
#include "renderer/vulkan/vk_instance.h"

static VK_Context context;

b8 vulkan_initialize(platform_window *window, rl_camera *camera, b8 vsync) {
    rl_arena_init(&context.arena, MiB(25), MiB(2), MEM_SUBSYSTEM_RENDERER);

    context.window = window;

    if (!vk_instance_create(&context)) {
        RL_ERROR("failed to create vulkan instance");
        return false;
    }

    if (!platform_create_vulkan_surface(&context)) {
        RL_ERROR("failed to create vulkan surface");
    }

    if (!vk_device_init(&context)) {
        RL_ERROR("failed to find suitable GPU device");
        return false;
    }

    if (!vk_swapchain_create(&context, vsync)) {
        RL_ERROR("failed to initialize swapchain");
        return false;
    }

    if (!vk_shader_init_compiler(&context)) {
        RL_ERROR("failed to initialize shader compiler");
        return false;
    }

    if (!vk_pipeline_create(&context)) {
        RL_ERROR("failed to create graphics pipeline");
        return false;
    }

    return true;
}

void vulkan_destroy() {
    vk_pipeline_destroy(&context);
    vk_shader_destroy_compiler(&context);
    vk_swapchain_destroy(&context);
    vk_device_destroy(&context);
    vkDestroySurfaceKHR(context.instance, context.surface, nullptr);
    vk_instance_destroy(&context);
    rl_arena_deinit(&context.arena);
}

void vulkan_begin_frame(f64 delta_time) {

}

void vulkan_end_frame() {

}

void vulkan_swap_buffers() {

}

void vulkan_set_view_projection(mat4 view, mat4 projection) {

}