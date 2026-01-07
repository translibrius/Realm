#include "renderer/vulkan/vk_renderer.h"
#include "renderer/vulkan/vk_instance.h"

b8 vulkan_initialize(platform_window *window, rl_camera *camera) {
    vk_instance_create();
    return true;
}

void vulkan_destroy() {

}

void vulkan_begin_frame(f64 delta_time) {

}

void vulkan_end_frame() {

}

void vulkan_swap_buffers() {

}

void vulkan_set_view_projection(mat4 view, mat4 projection) {

}