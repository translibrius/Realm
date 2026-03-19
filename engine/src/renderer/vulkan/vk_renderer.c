#include "renderer/vulkan/vk_renderer.h"

#include "asset/asset.h"
#include "asset/mesh.h"
#include "core/event.h"
#include "engine.h"
#include "vk_buffer.h"
#include "vk_commands.h"
#include "vk_descriptor.h"
#include "vk_device.h"
#include "vk_frame_buffers.h"
#include "vk_image.h"
#include "vk_instance.h"
#include "vk_mesh.h"
#include "vk_pipeline.h"
#include "vk_renderpass.h"
#include "vk_shader.h"
#include "vk_swapchain.h"
#include "vk_sync.h"
#include "vk_texture.h"
#include "vk_text.h"
#include "vk_gui.h"
#include "vk_util.h"
#include "vk_depth.h"
#include "core/config.h"

static VK_Context context;

static VkSampleCountFlagBits get_max_usable_sample_count(VK_Context *ctx) {
    VkSampleCountFlags counts = ctx->device_properties.properties.limits.framebufferColorSampleCounts
                              & ctx->device_properties.properties.limits.framebufferDepthSampleCounts;
    if (counts & VK_SAMPLE_COUNT_8_BIT)  return VK_SAMPLE_COUNT_8_BIT;
    if (counts & VK_SAMPLE_COUNT_4_BIT)  return VK_SAMPLE_COUNT_4_BIT;
    if (counts & VK_SAMPLE_COUNT_2_BIT)  return VK_SAMPLE_COUNT_2_BIT;
    return VK_SAMPLE_COUNT_1_BIT;
}

static b8 msaa_color_image_create(VK_Context *ctx) {
    VkFormat color_format = ctx->swapchain.chosen_format.surfaceFormat.format;
    if (!vk_image_create(ctx,
            ctx->swapchain.chosen_extent.width,
            ctx->swapchain.chosen_extent.height,
            color_format,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            ctx->msaa_samples,
            &ctx->msaa_color_image,
            &ctx->msaa_color_memory)) {
        RL_ERROR("Failed to create MSAA color image");
        return false;
    }
    if (!vk_image_view_create(ctx, VK_IMAGE_ASPECT_COLOR_BIT, ctx->msaa_color_image, color_format, &ctx->msaa_color_view)) {
        RL_ERROR("Failed to create MSAA color image view");
        return false;
    }
    return true;
}

static void msaa_color_image_destroy(VK_Context *ctx) {
    if (ctx->msaa_color_view)   { vkDestroyImageView(ctx->device, ctx->msaa_color_view, nullptr);   ctx->msaa_color_view = VK_NULL_HANDLE; }
    if (ctx->msaa_color_image)  { vkDestroyImage(ctx->device, ctx->msaa_color_image, nullptr);      ctx->msaa_color_image = VK_NULL_HANDLE; }
    if (ctx->msaa_color_memory) { vkFreeMemory(ctx->device, ctx->msaa_color_memory, nullptr);       ctx->msaa_color_memory = VK_NULL_HANDLE; }
}

VK_Context *vulkan_get_context_ptr(void) {
    return &context;
}

void vulkan_resize_framebuffer(i32 w, i32 h) {
    // Don't trigger swapchain recreation on minimize
    if (w <= 0 || h <= 0) {
        return;
    }

    // Skip if size hasn't actually changed (e.g. window move, not resize)
    if ((u32)w == context.swapchain.chosen_extent.width &&
        (u32)h == context.swapchain.chosen_extent.height) {
        return;
    }

    context.framebuffer_resized = true;
}

b8 vulkan_initialize(platform_window *window, b8 vsync) {
    context = (VK_Context){0};
    rl_arena_init(&context.arena, MiB(25), MiB(2), MEM_SUBSYSTEM_RENDERER);

    context.max_frames_in_flight = 2;
    context.window = window;
    context.clear_color[0] = RL_CLEAR_COLOR_R;
    context.clear_color[1] = RL_CLEAR_COLOR_G;
    context.clear_color[2] = RL_CLEAR_COLOR_B;
    context.clear_color[3] = RL_CLEAR_COLOR_A;
    glm_mat4_identity(context.view);
    glm_mat4_identity(context.proj);

    if (!vk_instance_create(&context)) {
        RL_ERROR("failed to create vulkan instance");
        return false;
    }

    if (!platform_create_vulkan_surface(&context)) {
        RL_ERROR("failed to create vulkan surface");
        return false;
    }

    if (!vk_device_init(&context)) {
        RL_ERROR("failed to find suitable GPU device");
        return false;
    }

    if (!vk_swapchain_create(&context, vsync, false, VK_NULL_HANDLE)) {
        RL_ERROR("failed to initialize swapchain");
        return false;
    }

    // Determine MSAA sample count from config, clamped to GPU maximum
    {
        rl_config *cfg = config_get();
        VkSampleCountFlagBits requested = cfg ? (VkSampleCountFlagBits)cfg->msaa : VK_SAMPLE_COUNT_1_BIT;
        VkSampleCountFlagBits max_samples = get_max_usable_sample_count(&context);
        context.msaa_samples = (requested <= max_samples) ? requested : max_samples;
        if (context.msaa_samples != requested) {
            RL_WARN("Requested MSAA %dx not supported, using %dx", (i32)requested, (i32)context.msaa_samples);
        }
        RL_INFO("Vulkan MSAA: %dx", (i32)context.msaa_samples);
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

    if (!vk_mesh_create_cube(&context)) {
        RL_ERROR("failed to create cube mesh");
        return false;
    }

    if (!vk_unlit_pipeline_create(&context)) {
        RL_ERROR("failed to create unlit pipeline");
        return false;
    }

    if (!vk_overlay_pipeline_create(&context)) {
        RL_ERROR("failed to create overlay pipeline");
        return false;
    }

    if (!vk_wireframe_pipelines_create(&context)) {
        RL_ERROR("failed to create wireframe pipelines");
        return false;
    }

    if (!vk_grid_pipeline_create(&context)) {
        RL_ERROR("failed to create grid pipeline");
        return false;
    }


    if (!vk_depth_res_create(&context)) {
        RL_ERROR("failed to create depth resources");
        return false;
    }

    if (context.msaa_samples != VK_SAMPLE_COUNT_1_BIT) {
        if (!msaa_color_image_create(&context)) {
            RL_ERROR("failed to create MSAA color image");
            return false;
        }
    }

    if (!vk_framebuffers_create(&context)) {
        RL_ERROR("failed to create framebuffers");
        return false;
    }

    if (!vk_texture_create_sampler(&context)) {
        RL_ERROR("failed to create texture sampler");
        return false;
    }

    if (!vk_placeholder_texture_create(&context)) {
        RL_ERROR("failed to create placeholder texture");
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

    if (!vk_text_pipeline_init(&context)) {
        RL_ERROR("failed to create text pipeline");
        return false;
    }

    if (!vk_gui_pipeline_init(&context)) {
        RL_ERROR("failed to create GUI pipeline");
        return false;
    }

    return true;
}

void vulkan_destroy(void) {
    vkDeviceWaitIdle(context.device);

    vk_gui_pipeline_destroy(&context);
    vk_text_pipeline_destroy(&context);
    vk_sync_destroy_frame(&context);
    vk_descriptor_destroy_pool(&context);
    vk_buffers_destroy_uniform(&context);
    vk_texture_destroy_sampler(&context);
    vk_placeholder_texture_destroy(&context);
    for (u32 i = 0; i < context.texture_count; i++) {
        vk_texture_destroy(&context, &context.textures[i].texture);
    }
    vk_grid_pipeline_destroy(&context);
    vk_wireframe_pipelines_destroy(&context);
    vk_overlay_pipeline_destroy(&context);
    vk_unlit_pipeline_destroy(&context);
    vk_mesh_cache_destroy_all(&context);
    vk_mesh_destroy_cube(&context);
    vk_sync_destroy_transfer(&context);
    if (context.queue_families.has_transfer) {
        vk_command_pool_destroy(&context, context.transfer_pool);
    }
    vk_command_pool_destroy(&context, context.graphics_pool);
    vk_framebuffers_destroy(&context);
    msaa_color_image_destroy(&context);
    vk_pipeline_destroy(&context);
    vk_pipeline_layout_destroy(&context);
    vk_descriptor_destroy_set_layout(&context);
    vk_renderpass_destroy(&context);
    vk_shader_destroy_compiler(&context);
    vk_swapchain_destroy(&context);
    vk_device_destroy(&context);
    vkDestroySurfaceKHR(context.instance, context.surface, nullptr);
    vk_instance_destroy(&context);
    rl_arena_deinit(&context.arena);
}

void update_uniform_buffer(u32 image_index, f64 dt) {
    (void)dt;

    ubo u = {0};
    glm_mat4_copy(context.view, u.view);
    glm_mat4_copy(context.proj, u.proj);

    glm_vec3_copy(context.frame_light.position, u.light_pos);
    glm_vec3_copy(context.frame_light.ambient,  u.light_ambient);
    glm_vec3_copy(context.frame_light.diffuse,  u.light_diffuse);
    glm_vec3_copy(context.frame_light.specular, u.light_specular);
    glm_vec3_copy(context.camera_pos,           u.camera_pos);

    mem_copy(context.uniform_buffers_mapped[image_index], &u, sizeof(ubo));
}

void vulkan_begin_frame(f64 delta_time) {
    (void)delta_time;
    context.frame_acquired = false;

    // Reset GUI pipeline state so stale data from a skipped frame never renders
    context.gui_pipeline.vertex_count = 0;
    context.gui_pipeline.segment_count = 0;
    context.gui_pipeline.current_font = nullptr;

    // Wait for previous frame to finish
    vkWaitForFences(context.device, 1, &context.in_flight_fences[context.current_frame], VK_TRUE, UINT64_MAX);

    // Get image from swapchain and pass image_available semaphore
    VkResult result = vkAcquireNextImageKHR(context.device, context.swapchain.handle, UINT64_MAX, context.image_available_semaphores[context.current_frame], VK_NULL_HANDLE, &context.current_image_index);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        vk_swapchain_recreate(&context);
        context.current_frame = 0;
        return;
    }

    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        RL_FATAL("failed to acquire swap chain image");
    }

    context.frame_acquired = true;

    // Reset fence and command buffer — recording deferred to end_frame
    vkResetFences(context.device, 1, &context.in_flight_fences[context.current_frame]);
    vkResetCommandBuffer(context.command_buffers[context.current_frame], 0);
}

void vulkan_end_frame(void) {
    if (!context.frame_acquired)
        return;

    // Update UBO after submit_frame_data has set light/camera for this frame
    update_uniform_buffer(context.current_frame, 0);

    // Record command buffer (text vertex data is now ready from submit_frame_data)
    vk_command_buffer_record(&context, context.command_buffers[context.current_frame], context.current_image_index);

    // Submit
    VkSemaphore wait_semaphores[] = {context.image_available_semaphores[context.current_frame]};
    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSemaphore signal_semaphores[] = {context.render_finished_semaphores[context.current_image_index]};

    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = wait_semaphores,
        .pWaitDstStageMask = wait_stages,
        .commandBufferCount = 1,
        .pCommandBuffers = &context.command_buffers[context.current_frame],
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = signal_semaphores};

    VK_CHECK(vkQueueSubmit(context.graphics_queue, 1, &submit_info, context.in_flight_fences[context.current_frame]));

    // Present
    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = signal_semaphores,
        .swapchainCount = 1,
        .pSwapchains = &context.swapchain.handle,
        .pImageIndices = &context.current_image_index,
        .pResults = nullptr // Optional
    };

    VkResult result = vkQueuePresentKHR(context.present_queue, &present_info);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || context.framebuffer_resized) {
        context.framebuffer_resized = false;
        vk_swapchain_recreate(&context);
        context.current_frame = 0;
        return;
    } else if (result != VK_SUCCESS) {
        RL_FATAL("failed to present swap chain image");
    }

    // advance frame
    context.current_frame = (context.current_frame + 1) % context.max_frames_in_flight;
}

void vulkan_swap_buffers(void) {
}

void vulkan_set_vsync(b8 vsync) {
    if (context.swapchain.vsync == vsync) return;
    context.swapchain.vsync = vsync;
    context.framebuffer_resized = true;
}

// --- Texture cache (mirrors GL's lazy loading) ---

static i32 vk_find_texture_index(asset_id id) {
    for (u32 i = 0; i < context.texture_count; i++) {
        if (context.textures[i].asset_id == id) return (i32)i;
    }
    return -1;
}

static b8 vk_load_texture(asset_id id) {
    if (context.texture_count >= VK_MAX_TEXTURES) {
        RL_ERROR("VK texture table full");
        return false;
    }

    u32 idx = context.texture_count;
    if (!vk_texture_create(&context, id, &context.textures[idx].texture)) {
        return false;
    }

    context.textures[idx].asset_id = id;

    if (!vk_descriptor_create_texture_sets(&context,
            context.textures[idx].texture.texture_image_view,
            context.textures[idx].descriptor_sets)) {
        vk_texture_destroy(&context, &context.textures[idx].texture);
        return false;
    }

    context.texture_count++;
    return true;
}

static void vk_ensure_texture(asset_id id) {
    if (!id) return;
    if (vk_find_texture_index(id) >= 0) return;
    vk_load_texture(id);
}

// Resolve the effective diffuse texture for a frame mesh
static asset_id vk_resolve_diffuse(rl_frame_mesh *fm) {
    if (fm->material.diffuse_map) return fm->material.diffuse_map;
    if (fm->mesh_asset) {
        rl_asset *a = asset_get(fm->mesh_asset);
        if (a && a->data) {
            rl_mesh *m = (rl_mesh *)a->data;
            if (m->material_count > 0)
                return m->materials[0].base_color_texture;
        }
    }
    return 0;
}

void vulkan_submit_frame_data(rl_frame_data *frame_data) {
    if (!frame_data) {
        return;
    }

    if (frame_data->camera.valid) {
        vulkan_set_view_projection(frame_data->camera.view, frame_data->camera.projection, frame_data->camera.position);
    }

    // Copy mesh list into frame arena — the source is on the game module's
    // stack and will be gone by the time vulkan_end_frame records commands.
    context.frame_mesh_count = frame_data->mesh_count;
    if (frame_data->mesh_count > 0 && frame_data->meshes) {
        rl_arena *fa = rl_engine_get_frame_arena();
        u64 sz = (u64)frame_data->mesh_count * sizeof(rl_frame_mesh);
        context.frame_meshes = rl_arena_push(fa, sz, alignof(rl_frame_mesh));
        mem_copy(context.frame_meshes, frame_data->meshes, sz);
    } else {
        context.frame_meshes = nullptr;
    }

    // Ensure all referenced textures and meshes are uploaded to VK before command recording
    for (u32 i = 0; i < frame_data->mesh_count; i++) {
        vk_ensure_texture(vk_resolve_diffuse(&frame_data->meshes[i]));
        if (frame_data->meshes[i].mesh_asset) {
            vk_mesh_cache_upload(&context, frame_data->meshes[i].mesh_asset);
        }
    }

    // Copy world overlay data (transform gizmos)
    context.world_overlay_count = frame_data->world_overlay_count;
    if (frame_data->world_overlay_count > 0 && frame_data->world_overlays) {
        rl_arena *fa2 = rl_engine_get_frame_arena();
        u64 wsz = (u64)frame_data->world_overlay_count * sizeof(rl_frame_mesh);
        context.world_overlays = rl_arena_push(fa2, wsz, alignof(rl_frame_mesh));
        mem_copy(context.world_overlays, frame_data->world_overlays, wsz);
    } else {
        context.world_overlays = nullptr;
    }

    // Copy overlay data
    context.overlay_camera = frame_data->overlay_camera;
    context.overlay_count = frame_data->overlay_count;
    if (frame_data->overlay_count > 0 && frame_data->overlay_meshes) {
        rl_arena *fa = rl_engine_get_frame_arena();
        u64 osz = (u64)frame_data->overlay_count * sizeof(rl_frame_mesh);
        context.overlay_meshes = rl_arena_push(fa, osz, alignof(rl_frame_mesh));
        mem_copy(context.overlay_meshes, frame_data->overlay_meshes, osz);
    } else {
        context.overlay_meshes = nullptr;
    }

    // Store grid + viewport state for command recording
    context.show_grid = frame_data->show_grid;
    context.scene_viewport = frame_data->viewport_rect;

    // Store first point light (or sensible default)
    if (frame_data->point_light_count > 0 && frame_data->point_lights) {
        context.frame_light = frame_data->point_lights[0];
    } else {
        context.frame_light = RL_DEFAULT_POINT_LIGHT;
    }

    if (frame_data->text_count > 0 && frame_data->texts) {
        for (u32 i = 0; i < frame_data->text_count; i++) {
            rl_frame_text *entry = &frame_data->texts[i];
            if (!entry->text) continue;

            if (entry->font) {
                vulkan_set_active_font(entry->font);
            }
            vulkan_render_text(entry->text, entry->size_px, entry->x, entry->y, entry->color);
        }
    }
}

void vulkan_set_view_projection(mat4 view, mat4 projection, vec3 pos) {
    glm_mat4_copy(view, context.view);
    glm_mat4_copy(projection, context.proj);
    glm_vec3_copy(pos, context.camera_pos);
}

platform_window *vulkan_get_active_window(void) {
    return context.window;
}

void vulkan_set_active_window(platform_window *window) {
    context.window = window;
}

void vulkan_set_wireframe(b8 enabled) {
    context.debug_wireframe = enabled && context.has_wireframe_pipelines;
}

void vulkan_set_clear_color(f32 r, f32 g, f32 b, f32 a) {
    context.clear_color[0] = r;
    context.clear_color[1] = g;
    context.clear_color[2] = b;
    context.clear_color[3] = a;
}
