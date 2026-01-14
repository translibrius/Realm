#include "renderer/vulkan/vk_renderer.h"

#include "vk_buffer.h"
#include "vk_device.h"
#include "vk_frame_buffers.h"
#include "vk_shader.h"
#include "vk_swapchain.h"
#include "vk_pipeline.h"
#include "vk_renderpass.h"
#include "vk_commands.h"
#include "vk_descriptor.h"
#include "vk_instance.h"
#include "vk_sync.h"
#include "core/event.h"

#include "profiler/profiler.h"

static VK_Context context;

b8 vk_resize_callback(void *event, void *data) {
    platform_window *window = event;
    if (window->id == context.window->id) {
        /*
        RL_DEBUG("Window #%d resized | POS: %d;%d | Size: %dx%d",
                 window->id,
                 window->settings.x, window->settings.y,
                 window->settings.width, window->settings.height); */

        context.window = window;
        // Don't trigger swapchain recreation on minimize
        if (window->settings.width <= 0 || window->settings.height <= 0) {
            return false;
        }
        context.framebuffer_resized = true;
    }
    return false;
}

b8 vulkan_initialize(platform_window *window, rl_camera *camera, b8 vsync) {
    rl_arena_init(&context.arena, MiB(25), MiB(2), MEM_SUBSYSTEM_RENDERER);

    // Listen to window size
    event_register(EVENT_WINDOW_RESIZE, vk_resize_callback, nullptr);

    context.window = window;

    // Rectangle
    da_append(&context.vertices, ((vertex) {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}}));
    da_append(&context.vertices, ((vertex) {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}}));
    da_append(&context.vertices, ((vertex) {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}));
    da_append(&context.vertices, ((vertex) {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}));

    da_append(&context.indices, 0);
    da_append(&context.indices, 1);
    da_append(&context.indices, 2);
    da_append(&context.indices, 2);
    da_append(&context.indices, 3);
    da_append(&context.indices, 0);

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

    if (!vk_swapchain_create(&context, vsync, false, VK_NULL_HANDLE)) {
        RL_ERROR("failed to initialize swapchain");
        return false;
    }

    if (!vk_shader_init_compiler(&context)) {
        RL_ERROR("failed to initialize shader compiler");
        return false;
    }

    if (!vk_renderpass_create(&context)) {
        RL_ERROR("failed to create render pass");
        return false;
    }

    if (!vk_descriptor_create_set_layout(&context)) {
        RL_ERROR("failed to create descriptor set layout");
    }

    if (!vk_pipeline_create(&context)) {
        RL_ERROR("failed to create graphics pipeline");
        return false;
    }

    if (!vk_framebuffers_create(&context)) {
        RL_ERROR("failed to create framebuffers");
        return false;
    }

    if (!vk_command_pool_create(&context, &context.graphics_pool, context.queue_families.graphics_index)) {
        RL_ERROR("failed to create command pool");
        return false;
    }

    if (context.queue_families.has_transfer) {
        if (!vk_command_pool_create(&context, &context.transfer_pool, context.queue_families.transfer_index)) {
            RL_ERROR("failed to create transfer pool");
            return false;
        }
    }

    if (!vk_sync_create_transfer(&context)) {
        RL_ERROR("failed to create transfer sync objects");
        return false;
    }

    if (!vk_buffer_create_vertex(&context, &context.vertices)) {
        RL_ERROR("failed to create vertex buffer");
        return false;
    }

    if (!vk_buffer_create_index(&context, &context.indices)) {
        RL_ERROR("failed to create index buffer");
        return false;
    }

    if (!vk_buffers_create_uniform(&context)) {
        RL_ERROR("failed to create uniform buffers");
        return false;
    }

    if (!vk_descriptor_create_pool(&context)) {
        RL_ERROR("failed to create descriptor pool");
        return false;
    }

    if (!vk_descriptor_create_sets(&context)) {
        return false;
    }

    if (!vk_command_buffers_create(&context, context.graphics_pool)) {
        RL_ERROR("failed to create command buffer");
        return false;
    }

    if (!vk_sync_create_frame(&context)) {
        RL_ERROR("failed to create sync objects");
        return false;
    }

    return true;
}

void vulkan_destroy() {
    // Wait for logical device to finish operations
    vkDeviceWaitIdle(context.device);

    vk_sync_destroy_frame(&context);
    vk_descriptor_destroy_pool(&context);
    vk_buffers_destroy_uniform(&context);
    vk_buffer_destroy_index(&context);
    vk_buffer_destroy_vertex(&context);
    vk_sync_destroy_transfer(&context);
    if (context.queue_families.has_transfer) {
        vk_command_pool_destroy(&context, context.transfer_pool);
    }
    vk_command_pool_destroy(&context, context.graphics_pool);
    vk_framebuffers_destroy(&context);
    vk_pipeline_destroy(&context);
    vk_renderpass_destroy(&context);
    vk_shader_destroy_compiler(&context);
    vk_swapchain_destroy(&context);
    vk_descriptor_destroy_set_layout(&context);
    vk_device_destroy(&context);
    vkDestroySurfaceKHR(context.instance, context.surface, nullptr);
    vk_instance_destroy(&context);
    rl_arena_deinit(&context.arena);
}

void update_uniform_buffer(u32 image_index, f64 dt) {
    mat4 model;
    mat4 view;
    mat4 proj;

    glm_mat4_identity(model);
    glm_mat4_identity(view);
    glm_mat4_identity(proj);

    glm_rotate(model, glm_rad(90.0f), (vec3){0.0f, 0.0f, 1.0f});

    ubo u = {0};
    glm_mat4_copy(model, u.model);
    glm_mat4_copy(context.view, u.view);
    glm_mat4_copy(context.proj, u.proj);

    mem_copy(&u, context.uniform_buffers_mapped[image_index], sizeof(ubo));
}

void vulkan_begin_frame(f64 delta_time) {
    TracyCZoneN(fence, "vkWaitForFences", true);
    // Wait for previous frame to finish
    vkWaitForFences(context.device, 1, &context.in_flight_fences[context.current_frame], VK_TRUE, UINT64_MAX);

    TracyCZoneEnd(fence);

    // Get image from swapchain and pass image_available semaphore
    u32 image_index;
    TracyCZoneN(aquire, "vkAcquireNextImageKHR", true);
    VkResult result = vkAcquireNextImageKHR(context.device, context.swapchain.handle, UINT64_MAX, context.image_available_semaphores[context.current_frame], VK_NULL_HANDLE, &image_index);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        vk_swapchain_recreate(&context);
        return;
    }

    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        RL_FATAL("failed to acquire swap chain image");
    }
    TracyCZoneEnd(aquire);

    update_uniform_buffer(image_index, delta_time);

    TracyCZoneN(record, "Reset + Record Command Buffer", true);
    // Only reset the fence if we are submitting work
    vkResetFences(context.device, 1, &context.in_flight_fences[context.current_frame]);

    // Reset, record and submit command buffer
    vkResetCommandBuffer(context.command_buffers[context.current_frame], 0);
    vk_command_buffer_record(&context, context.command_buffers[context.current_frame], image_index);
    TracyCZoneEnd(record);

    TracyCZoneN(submit, "vkQueueSubmit", true);
    VkSemaphore wait_semaphores[] = {context.image_available_semaphores[context.current_frame]};
    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSemaphore signal_semaphores[] = {context.render_finished_semaphores[context.current_frame]};

    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = wait_semaphores,
        .pWaitDstStageMask = wait_stages,
        .commandBufferCount = 1,
        .pCommandBuffers = &context.command_buffers[context.current_frame],
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = signal_semaphores
    };

    VK_CHECK(vkQueueSubmit(context.graphics_queue, 1, &submit_info, context.in_flight_fences[context.current_frame]));

    TracyCZoneEnd(submit);

    TracyCZoneN(present, "vkQueuePresentKHR", true);
    // Present the result back to the swapchain to have it eventually show up on screen
    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = signal_semaphores,
        .swapchainCount = 1,
        .pSwapchains = &context.swapchain.handle,
        .pImageIndices = &image_index,
        .pResults = nullptr // Optional
    };

    result = vkQueuePresentKHR(context.present_queue, &present_info);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || context.framebuffer_resized) {
        context.framebuffer_resized = false;
        vk_swapchain_recreate(&context);
    } else if (result != VK_SUCCESS) {
        RL_FATAL("failed to present swap chain image");
    }

    TracyCZoneEnd(present);

    // advance frame
    context.current_frame = (context.current_frame + 1) % context.max_frames_in_flight;
}

void vulkan_end_frame() {

}

void vulkan_swap_buffers() {

}

void vulkan_set_view_projection(mat4 view, mat4 projection) {
    projection[1][1] *= -1;

    glm_mat4_copy(view, context.view);
    glm_mat4_copy(projection, context.proj);
}