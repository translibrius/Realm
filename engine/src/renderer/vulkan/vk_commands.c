#include "vk_commands.h"
#include "vk_gui.h"
#include "vk_util.h"
#include "renderer/renderer_backend.h"

b8 vk_command_pool_create(VK_Context *context, VkCommandPool *out_pool, u32 family_index) {

    /*
    There are two possible flags for command pools:
        VK_COMMAND_POOL_CREATE_TRANSIENT_BIT: Hint that command buffers are rerecorded with new commands very often (may change memory allocation behavior)
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT: Allow command buffers to be rerecorded individually, without this flag they all have to be reset together
    */

    VkCommandPoolCreateInfo pool_create_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = family_index
    };

    VkResult result = vkCreateCommandPool(context->device, &pool_create_info, nullptr, out_pool);
    if (result != VK_SUCCESS) {
        RL_ERROR("failed to create command pool. VkResult=%s", string_VkResult(result));
        return false;
    }

    return true;
}

void vk_command_pool_destroy(VK_Context *context, VkCommandPool pool) {
    vkDestroyCommandPool(context->device, pool, nullptr);
}

b8 vk_command_buffers_create(VK_Context *context, VkCommandPool pool) {
    context->command_buffers = rl_arena_push(&context->arena, sizeof(VkCommandBuffer) * context->max_frames_in_flight, true);

    VkCommandBufferAllocateInfo allocate_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = context->max_frames_in_flight
    };

    VkResult result = vkAllocateCommandBuffers(context->device, &allocate_info, context->command_buffers);
    if (result != VK_SUCCESS) {
        RL_ERROR("Failed to allocate command buffer. VkResult=%s", string_VkResult(result));
        return false;
    }

    return true;
}

b8 vk_command_buffer_record(VK_Context *context, VkCommandBuffer buffer, u32 image_index) {
    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = 0,
        .pInheritanceInfo = nullptr
    };

    VkResult result = vkBeginCommandBuffer(buffer, &begin_info);
    if (result != VK_SUCCESS) {
        RL_ERROR("Failed to begin recording a command buffer");
        return false;
    }

    VkClearValue clear_values[2] = {
        (VkClearValue) {.color = {RL_CLEAR_COLOR_R, RL_CLEAR_COLOR_G, RL_CLEAR_COLOR_B, RL_CLEAR_COLOR_A}},
        (VkClearValue) {.depthStencil = {1.0f, 0.0f}}
    };

    VkRenderPassBeginInfo render_pass_begin_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = context->render_pass,
        .framebuffer = context->swapchain.frame_buffers[image_index],
        .renderArea = {
            .offset = {0, 0},
            .extent = context->swapchain.chosen_extent
        },
        .clearValueCount = 2,
        .pClearValues = clear_values
    };

    vkCmdBeginRenderPass(buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
    {
        // Negative height flips Y to match OpenGL conventions without
        // affecting winding order (unlike proj[1][1] *= -1).
        f32 vp_h = (f32)context->swapchain.chosen_extent.height;
        VkViewport viewport = {
            .x = 0.0f,
            .y = vp_h,
            .width = (f32)context->swapchain.chosen_extent.width,
            .height = -vp_h,
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };
        vkCmdSetViewport(buffer, 0, 1, &viewport);

        VkRect2D scissor = {
            .offset = {0, 0},
            .extent = context->swapchain.chosen_extent
        };
        vkCmdSetScissor(buffer, 0, 1, &scissor);

        // Shared vertex buffer + descriptors for all 3D meshes
        VkBuffer vbufs[] = {context->cube_vertex_buffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(buffer, 0, 1, vbufs, offsets);
        vkCmdBindDescriptorSets(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, context->pipeline_layout, 0, 1, &context->descriptor_sets[context->current_frame], 0, nullptr);

        // --- Lit pass ---
        VkPipeline lit_pipe = context->debug_wireframe ? context->wireframe_lit_pipeline : context->graphics_pipeline.handle;
        vkCmdBindPipeline(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, lit_pipe);
        for (u32 i = 0; i < context->frame_mesh_count; i++) {
            if (context->frame_meshes[i].kind != RL_FRAME_MESH_KIND_LIT) continue;
            VK_MeshPushConstants pc;
            glm_mat4_copy(context->frame_meshes[i].model, pc.model);
            glm_vec3_copy(context->frame_meshes[i].material.specular, pc.material_params);
            pc.material_params[3] = context->frame_meshes[i].material.shininess;
            vkCmdPushConstants(buffer, context->pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(VK_MeshPushConstants), &pc);
            vkCmdDraw(buffer, context->cube_vertex_count, 1, 0, 0);
        }

        // --- Unlit pass ---
        VkPipeline unlit_pipe = context->debug_wireframe ? context->wireframe_unlit_pipeline : context->unlit_pipeline;
        vkCmdBindPipeline(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, unlit_pipe);
        for (u32 i = 0; i < context->frame_mesh_count; i++) {
            if (context->frame_meshes[i].kind != RL_FRAME_MESH_KIND_UNLIT) continue;
            VK_MeshPushConstants pc;
            glm_mat4_copy(context->frame_meshes[i].model, pc.model);
            glm_vec4_zero(pc.material_params);
            vkCmdPushConstants(buffer, context->pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(VK_MeshPushConstants), &pc);
            vkCmdDraw(buffer, context->cube_vertex_count, 1, 0, 0);
        }

        // GUI + text overlay (unified pipeline)
        vulkan_gui_record_commands(context, buffer);
    }
    vkCmdEndRenderPass(buffer);

    result = vkEndCommandBuffer(buffer);
    if (result != VK_SUCCESS) {
        RL_ERROR("failed to record command buffer");
        return false;
    }

    return true;
}
