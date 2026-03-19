#include "vk_commands.h"
#include "vk_gui.h"
#include "vk_mesh.h"
#include "vk_util.h"
#include "asset/asset.h"
#include "asset/mesh.h"
#include "asset/model.h"
#include "renderer/renderer_backend.h"
#include "renderer/renderer_types.h"
#include "memory/memory.h"

// Find the descriptor sets for a given texture, or nullptr for placeholder
static VkDescriptorSet *vk_cmd_find_texture_sets(VK_Context *ctx, asset_id diffuse_id) {
    if (!diffuse_id) return nullptr;
    for (u32 i = 0; i < ctx->texture_count; i++) {
        if (ctx->textures[i].asset_id == diffuse_id) return ctx->textures[i].descriptor_sets;
    }
    return nullptr;
}

// Resolve effective diffuse texture for a frame mesh
static asset_id vk_cmd_resolve_diffuse(rl_frame_mesh *fm) {
    if (fm->material.diffuse_map) return fm->material.diffuse_map;
    if (!fm->model_asset) return 0;

    rl_asset *a = asset_get(fm->model_asset);
    if (!a || !a->data) return 0;

    if (a->type == ASSET_MODEL) {
        rl_model *m = (rl_model *)a->data;
        if (fm->mesh_index < m->mesh_count) {
            u32 mat_idx = m->meshes[fm->mesh_index].material_index;
            if (mat_idx < m->material_count) return m->materials[mat_idx].base_color_texture;
        }
    } else if (a->type == ASSET_MESH) {
        rl_mesh *m = (rl_mesh *)a->data;
        if (m->material_count > 0) return m->materials[0].base_color_texture;
    }
    return 0;
}

// Bind the correct vertex/index buffer and issue the draw call for a frame mesh.
// Returns false if the mesh can't be drawn (missing asset).
static b8 vk_cmd_bind_and_draw(VK_Context *ctx, VkCommandBuffer buf, rl_frame_mesh *fm) {
    VkDeviceSize offset = 0;

    if (fm->model_asset) {
        i32 idx = vk_model_cache_find(ctx, fm->model_asset);
        if (idx < 0) return false;
        if (fm->mesh_index >= ctx->model_cache[idx].mesh_count) return false;

        VK_CachedMesh *cm = &ctx->model_cache[idx].meshes[fm->mesh_index];
        vkCmdBindVertexBuffers(buf, 0, 1, &cm->vertex_buffer, &offset);
        if (cm->index_count > 0) {
            vkCmdBindIndexBuffer(buf, cm->index_buffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(buf, cm->index_count, 1, 0, 0, 0);
        } else {
            vkCmdDraw(buf, cm->vertex_count, 1, 0, 0);
        }
    } else {
        vkCmdBindVertexBuffers(buf, 0, 1, &ctx->cube_vertex_buffer, &offset);
        vkCmdDraw(buf, ctx->cube_vertex_count, 1, 0, 0);
    }
    return true;
}

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
        (VkClearValue) {.color = {context->clear_color[0], context->clear_color[1], context->clear_color[2], context->clear_color[3]}},
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
        rl_viewport_rect vr = context->scene_viewport;
        b8 has_vp = (vr.w > 0 && vr.h > 0);

        // Negative height flips Y to match OpenGL conventions without
        // affecting winding order (unlike proj[1][1] *= -1).
        VkViewport viewport;
        VkRect2D scissor;
        if (has_vp) {
            viewport = (VkViewport){
                .x = vr.x,
                .y = vr.y + vr.h,
                .width = vr.w,
                .height = -vr.h,
                .minDepth = 0.0f,
                .maxDepth = 1.0f,
            };
            scissor = (VkRect2D){
                .offset = {(i32)vr.x, (i32)vr.y},
                .extent = {(u32)vr.w, (u32)vr.h},
            };
        } else {
            f32 vp_h = (f32)context->swapchain.chosen_extent.height;
            viewport = (VkViewport){
                .x = 0.0f,
                .y = vp_h,
                .width = (f32)context->swapchain.chosen_extent.width,
                .height = -vp_h,
                .minDepth = 0.0f,
                .maxDepth = 1.0f,
            };
            scissor = (VkRect2D){
                .offset = {0, 0},
                .extent = context->swapchain.chosen_extent,
            };
        }
        vkCmdSetViewport(buffer, 0, 1, &viewport);
        vkCmdSetScissor(buffer, 0, 1, &scissor);

        // Shared descriptors for all 3D passes
        vkCmdBindDescriptorSets(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, context->pipeline_layout, 0, 1, &context->descriptor_sets[context->current_frame], 0, nullptr);

        // --- Grid pass (no vertex buffer — generated in shader) ---
        if (context->show_grid && context->grid_pipeline) {
            vkCmdBindPipeline(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, context->grid_pipeline);
            vkCmdDraw(buffer, 6, 1, 0, 0);
        }

        // --- Lit pass (per-mesh texture + geometry binding) ---
        VkPipeline lit_pipe = context->debug_wireframe ? context->wireframe_lit_pipeline : context->graphics_pipeline.handle;
        vkCmdBindPipeline(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, lit_pipe);
        for (u32 i = 0; i < context->frame_mesh_count; i++) {
            rl_frame_mesh *fm = &context->frame_meshes[i];
            if (fm->kind != RL_FRAME_MESH_KIND_LIT) continue;

            // Bind per-mesh texture descriptor set (or default placeholder)
            asset_id diffuse_id = vk_cmd_resolve_diffuse(fm);
            VkDescriptorSet *tex_sets = vk_cmd_find_texture_sets(context, diffuse_id);
            VkDescriptorSet ds = tex_sets ? tex_sets[context->current_frame]
                                          : context->descriptor_sets[context->current_frame];
            vkCmdBindDescriptorSets(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                context->pipeline_layout, 0, 1, &ds, 0, nullptr);

            VK_MeshPushConstants pc;
            glm_mat4_copy(fm->model, pc.model);
            glm_vec3_copy(fm->material.specular, pc.material_params);
            pc.material_params[3] = fm->material.shininess;
            vkCmdPushConstants(buffer, context->pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(VK_MeshPushConstants), &pc);
            vk_cmd_bind_and_draw(context, buffer, fm);
        }

        // --- Unlit pass ---
        VkPipeline unlit_pipe = context->debug_wireframe ? context->wireframe_unlit_pipeline : context->unlit_pipeline;
        vkCmdBindPipeline(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, unlit_pipe);
        for (u32 i = 0; i < context->frame_mesh_count; i++) {
            rl_frame_mesh *fm = &context->frame_meshes[i];
            if (fm->kind != RL_FRAME_MESH_KIND_UNLIT) continue;
            VK_MeshPushConstants pc;
            glm_mat4_copy(fm->model, pc.model);
            glm_vec4_zero(pc.material_params);
            vkCmdPushConstants(buffer, context->pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(VK_MeshPushConstants), &pc);
            vk_cmd_bind_and_draw(context, buffer, fm);
        }

        // --- World-space overlays (transform gizmos — main camera, no depth test) ---
        if (context->world_overlay_count > 0 && context->world_overlays) {
            // Rebind cube vertex buffer (may have been changed by per-mesh draws above)
            VkDeviceSize vb_offset = 0;
            vkCmdBindVertexBuffers(buffer, 0, 1, &context->cube_vertex_buffer, &vb_offset);
            // Rebind default descriptor set for overlays (no texture needed)
            vkCmdBindDescriptorSets(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, context->pipeline_layout,
                0, 1, &context->descriptor_sets[context->current_frame], 0, nullptr);
            vkCmdBindPipeline(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, context->overlay_pipeline);
            for (u32 i = 0; i < context->world_overlay_count; i++) {
                VK_MeshPushConstants pc;
                glm_mat4_copy(context->world_overlays[i].model, pc.model);
                glm_vec3_copy(context->world_overlays[i].material.specular, pc.material_params);
                pc.material_params[3] = 0.0f;
                vkCmdPushConstants(buffer, context->pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(VK_MeshPushConstants), &pc);
                vkCmdDraw(buffer, context->cube_vertex_count, 1, 0, 0);
            }
            // Rebind scene UBO (overlay pipeline uses same UBO, which is already scene camera)
        }

        // --- Overlay pass (gizmo axes — no depth test) ---
        if (context->overlay_count > 0 && context->overlay_meshes && context->overlay_camera.valid) {
            // Compute gizmo sub-viewport: 100x100 in bottom-left of viewport rect, 10px margin
            f32 gx, gy, gw, gh;
            gw = 100.0f;
            gh = 100.0f;
            if (has_vp) {
                gx = vr.x + 10.0f;
                gy = vr.y + vr.h - gh - 10.0f;
            } else {
                gx = 10.0f;
                gy = (f32)context->swapchain.chosen_extent.height - gh - 10.0f;
            }

            // Negative-height convention for Y-flip
            VkViewport gizmo_vp = {
                .x = gx,
                .y = gy + gh,
                .width = gw,
                .height = -gh,
                .minDepth = 0.0f,
                .maxDepth = 1.0f,
            };
            VkRect2D gizmo_scissor = {
                .offset = {(i32)gx, (i32)gy},
                .extent = {(u32)gw, (u32)gh},
            };
            vkCmdSetViewport(buffer, 0, 1, &gizmo_vp);
            vkCmdSetScissor(buffer, 0, 1, &gizmo_scissor);

            // Bind cube VB, overlay descriptor set, and overlay pipeline
            VkDeviceSize gizmo_vb_offset = 0;
            vkCmdBindVertexBuffers(buffer, 0, 1, &context->cube_vertex_buffer, &gizmo_vb_offset);
            vkCmdBindPipeline(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, context->overlay_pipeline);

            // Write overlay camera to dedicated overlay UBO (not the scene UBO)
            ubo overlay_ubo = {0};
            glm_mat4_copy(context->overlay_camera.view, overlay_ubo.view);
            glm_mat4_copy(context->overlay_camera.projection, overlay_ubo.proj);
            mem_copy(context->overlay_uniform_buffers_mapped[context->current_frame], &overlay_ubo, sizeof(ubo));

            // Bind overlay descriptor set so this pass reads the overlay UBO
            vkCmdBindDescriptorSets(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, context->pipeline_layout,
                0, 1, &context->overlay_descriptor_sets[context->current_frame], 0, nullptr);

            for (u32 i = 0; i < context->overlay_count; i++) {
                VK_MeshPushConstants pc;
                glm_mat4_copy(context->overlay_meshes[i].model, pc.model);
                glm_vec3_copy(context->overlay_meshes[i].material.specular, pc.material_params);
                pc.material_params[3] = 0.0f;
                vkCmdPushConstants(buffer, context->pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(VK_MeshPushConstants), &pc);
                vkCmdDraw(buffer, context->cube_vertex_count, 1, 0, 0);
            }
        }

        // Restore full-swapchain viewport/scissor for GUI overlay
        if (has_vp) {
            f32 full_h = (f32)context->swapchain.chosen_extent.height;
            VkViewport full_viewport = {
                .x = 0.0f,
                .y = full_h,
                .width = (f32)context->swapchain.chosen_extent.width,
                .height = -full_h,
                .minDepth = 0.0f,
                .maxDepth = 1.0f,
            };
            VkRect2D full_scissor = {
                .offset = {0, 0},
                .extent = context->swapchain.chosen_extent,
            };
            vkCmdSetViewport(buffer, 0, 1, &full_viewport);
            vkCmdSetScissor(buffer, 0, 1, &full_scissor);
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
