#include "renderer/vulkan/vk_renderer.h"

#include "vk_device.h"
#include "vk_frame_buffers.h"
#include "vk_shader.h"
#include "vk_swapchain.h"
#include "vk_pipeline.h"
#include "vk_renderpass.h"
#include "vk_commands.h"
#include "vk_instance.h"
#include "vk_sync.h"

#include "profiler/profiler.h"

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

    if (!vk_swapchain_create(&context, vsync, false)) {
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

    if (!vk_pipeline_create(&context)) {
        RL_ERROR("failed to create graphics pipeline");
        return false;
    }

    if (!vk_framebuffers_create(&context)) {
        RL_ERROR("failed to create framebuffers");
        return false;
    }

    if (!vk_command_pool_create(&context, &context.graphics_pool)) {
        RL_ERROR("failed to create command pool");
        return false;
    }

    if (!vk_command_buffers_create(&context, context.graphics_pool)) {
        RL_ERROR("failed to create command buffer");
        return false;
    }

    if (!vk_sync_objects_create(&context)) {
        RL_ERROR("failed to create sync objects");
        return false;
    }

    return true;
}

void vulkan_destroy() {
    // Wait for logical device to finish operations
    vkDeviceWaitIdle(context.device);

    vk_sync_objects_destroy(&context);
    vk_command_pool_destroy(&context, context.graphics_pool);
    vk_framebuffers_destroy(&context);
    vk_pipeline_destroy(&context);
    vk_renderpass_destroy(&context);
    vk_shader_destroy_compiler(&context);
    vk_swapchain_destroy(&context);
    vk_device_destroy(&context);
    vkDestroySurfaceKHR(context.instance, context.surface, nullptr);
    vk_instance_destroy(&context);
    rl_arena_deinit(&context.arena);
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

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        if (context.window->settings.width == context.swapchain.chosen_extent.width && context.window->settings.height == context.swapchain.chosen_extent.height) {
        } else {
            vk_swapchain_recreate(&context);
        }
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

}