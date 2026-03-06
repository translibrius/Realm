#pragma once

#include "defines.h"
#include "core/camera.h"
#include "platform/platform.h"
#include "renderer/frame_data.h"
#include "renderer/vulkan/vk_types.h"

VK_Context *vulkan_get_context_ptr(void);

b8 vulkan_initialize(platform_window *window, b8 vsync);
void vulkan_destroy();
void vulkan_begin_frame(f64 delta_time);
void vulkan_end_frame();
void vulkan_swap_buffers();
void vulkan_set_vsync(b8 vsync);
void vulkan_set_view_projection(mat4 view, mat4 projection, vec3 pos);

platform_window* vulkan_get_active_window();
void vulkan_set_active_window(platform_window* window);
void vulkan_resize_framebuffer(i32 w, i32 h);
void vulkan_submit_frame_data(rl_frame_data *frame_data);
void vulkan_set_wireframe(b8 enabled);
