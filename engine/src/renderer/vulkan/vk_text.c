#include "renderer/vulkan/vk_text.h"

#include "asset/asset_internal.h"
#include "asset/font.h"
#include "core/logger.h"
#include "renderer/renderer_types.h"
#include "vk_buffer.h"
#include "vk_image.h"
#include "vk_shader.h"

#include <string.h>

// Forward declarations
static const rl_glyph *vk_font_find_glyph(const rl_font *font, u32 codepoint);
static VK_Font *vk_find_font(VK_Context *ctx, rl_font *font);
static b8 vk_text_create_font_atlas(VK_Context *ctx, rl_font *font, VK_Font *out_font);
static b8 vk_text_create_pipeline(VK_Context *ctx);
static b8 vk_text_create_vertex_buffers(VK_Context *ctx);
static b8 vk_text_create_descriptors(VK_Context *ctx, VK_Font *font);

// Defined in vk_renderer.c
extern VK_Context *vulkan_get_context_ptr(void);

// ---- Init / Destroy ----

b8 vulkan_text_pipeline_init(VK_Context *ctx) {
    VK_TextPipeline *tp = &ctx->text_pipeline;
    da_init(&tp->fonts);

    // Create text graphics pipeline
    if (!vk_text_create_pipeline(ctx)) {
        RL_ERROR("Failed to create text pipeline");
        return false;
    }

    // Create per-frame vertex buffers (HOST_VISIBLE, persistently mapped)
    if (!vk_text_create_vertex_buffers(ctx)) {
        RL_ERROR("Failed to create text vertex buffers");
        return false;
    }

    // Create font sampler (LINEAR, CLAMP_TO_EDGE)
    VkSamplerCreateInfo sampler_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .anisotropyEnable = VK_FALSE,
        .maxAnisotropy = 1.0f,
        .borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
        .compareEnable = VK_FALSE,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
    };

    if (vkCreateSampler(ctx->device, &sampler_info, nullptr, &tp->font_sampler) != VK_SUCCESS) {
        RL_ERROR("Failed to create font sampler");
        return false;
    }

    // Load all font assets
    Assets *assets = get_assets();
    for (u32 i = 0; i < assets->count; i++) {
        rl_asset *asset = &assets->items[i];
        if (asset->type != ASSET_FONT)
            continue;

        rl_font *font = (rl_font *)asset->handle;
        VK_Font vk_font = {0};

        if (!vk_text_create_font_atlas(ctx, font, &vk_font)) {
            RL_WARN("Failed to create Vulkan font atlas for '%s'", asset->filename);
            continue;
        }

        da_append(&tp->fonts, vk_font);

        if (strcmp(font->name, "evil_empire.otf") == 0) {
            tp->active_font = font;
        }
    }

    // Resolve font pointers after all appends (da_append may realloc)
    if (tp->fonts.count > 0) {
        if (!tp->active_font)
            tp->active_font = tp->fonts.items[0].font;

        // Create descriptor sets for each font's atlas
        for (u32 i = 0; i < tp->fonts.count; i++) {
            if (!vk_text_create_descriptors(ctx, &tp->fonts.items[i])) {
                RL_ERROR("Failed to create text descriptors for font %u", i);
                return false;
            }
        }
    }

    tp->vertex_count = 0;
    return true;
}

void vulkan_text_pipeline_destroy(VK_Context *ctx) {
    VK_TextPipeline *tp = &ctx->text_pipeline;

    // Descriptor pool (frees sets implicitly)
    if (tp->descriptor_pool)
        vkDestroyDescriptorPool(ctx->device, tp->descriptor_pool, nullptr);

    // Font sampler
    if (tp->font_sampler)
        vkDestroySampler(ctx->device, tp->font_sampler, nullptr);

    // Font atlases
    for (u32 i = 0; i < tp->fonts.count; i++) {
        VK_Font *f = &tp->fonts.items[i];
        if (f->atlas_view)
            vkDestroyImageView(ctx->device, f->atlas_view, nullptr);
        if (f->atlas_image)
            vkDestroyImage(ctx->device, f->atlas_image, nullptr);
        if (f->atlas_memory)
            vkFreeMemory(ctx->device, f->atlas_memory, nullptr);
    }

    // Per-frame vertex buffers
    if (tp->vertex_buffers) {
        for (u32 i = 0; i < ctx->max_frames_in_flight; i++) {
            vk_buffer_destroy(ctx, tp->vertex_buffers[i], tp->vertex_buffer_memory[i]);
        }
    }

    // Pipeline
    if (tp->handle)
        vkDestroyPipeline(ctx->device, tp->handle, nullptr);
    if (tp->layout)
        vkDestroyPipelineLayout(ctx->device, tp->layout, nullptr);
    if (tp->descriptor_set_layout)
        vkDestroyDescriptorSetLayout(ctx->device, tp->descriptor_set_layout, nullptr);
}

// ---- Rendering ----

void vulkan_set_active_font(rl_font *font) {
    VK_Context *ctx = vulkan_get_context_ptr();
    if (ctx && font)
        ctx->text_pipeline.active_font = font;
}

void vulkan_render_text(const char *text, f32 size_px, f32 x, f32 y, vec4 color) {
    (void)text;
    (void)size_px;
    (void)x;
    (void)y;
    (void)color;
    // Not used directly — text is batched via vulkan_render_text_batch
}

void vulkan_render_text_batch(VK_Context *ctx, rl_frame_text *texts, u32 text_count) {
    if (!texts || text_count == 0)
        return;

    VK_TextPipeline *tp = &ctx->text_pipeline;
    u32 frame = ctx->current_frame;

    VK_TextVertex *verts = (VK_TextVertex *)tp->vertex_buffer_mapped[frame];
    u32 vert_count = 0;
    tp->batch_font = nullptr;

    for (u32 t = 0; t < text_count; t++) {
        rl_frame_text *entry = &texts[t];
        if (!entry->text)
            continue;

        rl_font *font = entry->font ? entry->font : tp->active_font;
        if (!font)
            continue;

        VK_Font *vk_font = vk_find_font(ctx, font);
        if (!vk_font)
            continue;

        // Track the first font used in this batch for descriptor binding
        if (!tp->batch_font)
            tp->batch_font = vk_font;

        f32 cursor_x = entry->x;
        f32 cursor_y = entry->y;
        f32 size_px = entry->size_px;

        for (const unsigned char *c = (const unsigned char *)entry->text; *c; c++) {
            if (*c == '\n') {
                cursor_x = entry->x;
                cursor_y += (f32)font->line_height * size_px;
                continue;
            }

            if (vert_count + 6 > 6 * MAX_TEXT_GLYPHS)
                break;

            const rl_glyph *g = (*c < 256) ? vk_font->glyph_map[*c] : vk_font_find_glyph(font, (u32)*c);
            if (!g)
                continue;

            const f32 x0 = cursor_x + g->plane_min_x * size_px;
            const f32 x1 = cursor_x + g->plane_max_x * size_px;
            const f32 y0 = cursor_y + g->plane_min_y * size_px;
            const f32 y1 = cursor_y + g->plane_max_y * size_px;

            const f32 u0 = g->uv_min_x;
            const f32 v0 = g->uv_min_y;
            const f32 u1 = g->uv_max_x;
            const f32 v1 = g->uv_max_y;

            const f32 r = entry->color[0];
            const f32 gv = entry->color[1];
            const f32 b = entry->color[2];
            const f32 a = entry->color[3];

            verts[vert_count + 0] = (VK_TextVertex){.pos = {x0, y0}, .uv = {u0, v0}, .color = {r, gv, b, a}};
            verts[vert_count + 1] = (VK_TextVertex){.pos = {x0, y1}, .uv = {u0, v1}, .color = {r, gv, b, a}};
            verts[vert_count + 2] = (VK_TextVertex){.pos = {x1, y1}, .uv = {u1, v1}, .color = {r, gv, b, a}};
            verts[vert_count + 3] = (VK_TextVertex){.pos = {x0, y0}, .uv = {u0, v0}, .color = {r, gv, b, a}};
            verts[vert_count + 4] = (VK_TextVertex){.pos = {x1, y1}, .uv = {u1, v1}, .color = {r, gv, b, a}};
            verts[vert_count + 5] = (VK_TextVertex){.pos = {x1, y0}, .uv = {u1, v0}, .color = {r, gv, b, a}};

            vert_count += 6;
            cursor_x += g->advance * size_px;
        }
    }

    tp->vertex_count = vert_count;
}

void vulkan_text_record_commands(VK_Context *ctx, VkCommandBuffer cmd) {
    VK_TextPipeline *tp = &ctx->text_pipeline;

    if (tp->vertex_count == 0 || !tp->handle || !tp->batch_font)
        return;

    u32 frame = ctx->current_frame;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, tp->handle);

    // Push screen size + pixel range
    struct {
        vec2 screen_size;
        f32 px_range;
    } push_data = {
        .screen_size = {(f32)ctx->swapchain.chosen_extent.width, (f32)ctx->swapchain.chosen_extent.height},
        .px_range = tp->batch_font->font->pixel_range,
    };
    vkCmdPushConstants(cmd, tp->layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push_data), &push_data);

    // Bind descriptor set for the batch font's atlas
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, tp->layout, 0, 1, &tp->batch_font->descriptor_sets[frame], 0, nullptr);

    // Bind vertex buffer
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &tp->vertex_buffers[frame], &offset);

    vkCmdDraw(cmd, tp->vertex_count, 1, 0, 0);
}

// ---- Static helpers ----

static const rl_glyph *vk_font_find_glyph(const rl_font *font, u32 codepoint) {
    for (u32 i = 0; i < font->glyph_count; i++) {
        if ((u32)font->glyphs[i].codepoint == codepoint)
            return &font->glyphs[i];
    }
    return nullptr;
}

static VK_Font *vk_find_font(VK_Context *ctx, rl_font *font) {
    VK_TextPipeline *tp = &ctx->text_pipeline;
    for (u32 i = 0; i < tp->fonts.count; i++) {
        if (tp->fonts.items[i].font == font)
            return &tp->fonts.items[i];
    }
    return nullptr;
}

static b8 vk_text_create_font_atlas(VK_Context *ctx, rl_font *font, VK_Font *out_font) {
    rl_texture *atlas = &font->atlas;
    VkDeviceSize image_size = (VkDeviceSize)atlas->width * atlas->height * atlas->channels;

    // Staging buffer
    VkBuffer staging_buffer;
    VkDeviceMemory staging_memory;
    if (!vk_buffer_create(ctx, image_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                          &staging_buffer, &staging_memory)) {
        return false;
    }

    void *data;
    vkMapMemory(ctx->device, staging_memory, 0, image_size, 0, &data);
    mem_copy(atlas->data, data, image_size);
    vkUnmapMemory(ctx->device, staging_memory);

    // Create VkImage
    VkFormat fmt = (atlas->channels == 4) ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_R8G8B8_UNORM;

    if (!vk_image_create(ctx, atlas->width, atlas->height, fmt,
                         VK_IMAGE_TILING_OPTIMAL,
                         VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                         &out_font->atlas_image, &out_font->atlas_memory)) {
        vk_buffer_destroy(ctx, staging_buffer, staging_memory);
        return false;
    }

    // Transition + copy + transition
    vk_image_transition_layout(ctx, out_font->atlas_image, fmt,
                               VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    vk_buffer_copy_to_image(ctx, staging_buffer, out_font->atlas_image, atlas->width, atlas->height);
    vk_image_transition_layout(ctx, out_font->atlas_image, fmt,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vk_buffer_destroy(ctx, staging_buffer, staging_memory);

    // Create image view
    if (!vk_image_view_create(ctx, out_font->atlas_image, fmt, &out_font->atlas_view)) {
        return false;
    }

    // Populate glyph map
    out_font->font = font;
    mem_zero(out_font->glyph_map, sizeof(out_font->glyph_map));
    for (u32 i = 0; i < font->glyph_count; i++) {
        u32 cp = (u32)font->glyphs[i].codepoint;
        if (cp < 256)
            out_font->glyph_map[cp] = &font->glyphs[i];
    }

    RL_DEBUG("Created Vulkan font atlas for '%s' (%dx%d)", font->name, atlas->width, atlas->height);
    return true;
}

static b8 vk_text_create_pipeline(VK_Context *ctx) {
    VK_TextPipeline *tp = &ctx->text_pipeline;

    // Compile shaders
    if (!vk_shader_module_compile(ctx, ASSET_ID_SHADER_VULKAN_TEXT_VERT))
        return false;
    if (!vk_shader_module_compile(ctx, ASSET_ID_SHADER_VULKAN_TEXT_FRAG))
        return false;

    // Shader stages (the last 2 compiled modules are our text shaders)
    u32 text_shader_start = ctx->shaders.count - 2;
    VkPipelineShaderStageCreateInfo shader_stages[2] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = ctx->shaders.items[text_shader_start].module,
            .pName = "main",
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = ctx->shaders.items[text_shader_start + 1].module,
            .pName = "main",
        },
    };

    // Descriptor set layout: binding 0 = combined image sampler (fragment)
    VkDescriptorSetLayoutBinding sampler_binding = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    };

    VkDescriptorSetLayoutCreateInfo set_layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &sampler_binding,
    };

    if (vkCreateDescriptorSetLayout(ctx->device, &set_layout_info, nullptr, &tp->descriptor_set_layout) != VK_SUCCESS) {
        RL_ERROR("Failed to create text descriptor set layout");
        return false;
    }

    // Push constant range for screen_size (vec2) + px_range (float) = 12 bytes
    VkPushConstantRange push_range = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset = 0,
        .size = sizeof(vec2) + sizeof(f32),
    };

    VkPipelineLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &tp->descriptor_set_layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push_range,
    };

    if (vkCreatePipelineLayout(ctx->device, &layout_info, nullptr, &tp->layout) != VK_SUCCESS) {
        RL_ERROR("Failed to create text pipeline layout");
        return false;
    }

    // Vertex input: pos (vec2), uv (vec2), color (vec4)
    VkVertexInputBindingDescription binding = {
        .binding = 0,
        .stride = sizeof(VK_TextVertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };

    VkVertexInputAttributeDescription attrs[3] = {
        {.location = 0, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(VK_TextVertex, pos)},
        {.location = 1, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(VK_TextVertex, uv)},
        {.location = 2, .binding = 0, .format = VK_FORMAT_R32G32B32A32_SFLOAT, .offset = offsetof(VK_TextVertex, color)},
    };

    VkPipelineVertexInputStateCreateInfo vertex_input = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &binding,
        .vertexAttributeDescriptionCount = 3,
        .pVertexAttributeDescriptions = attrs,
    };

    VkPipelineInputAssemblyStateCreateInfo input_assembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE,
    };

    VkDynamicState dynamic_states[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates = dynamic_states,
    };

    VkPipelineViewportStateCreateInfo viewport_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };

    VkPipelineRasterizationStateCreateInfo raster = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
        .lineWidth = 1.0f,
    };

    VkPipelineMultisampleStateCreateInfo multisample = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable = VK_FALSE,
    };

    // Alpha blending
    VkPipelineColorBlendAttachmentState blend_attachment = {
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .alphaBlendOp = VK_BLEND_OP_ADD,
    };

    VkPipelineColorBlendStateCreateInfo color_blend = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .attachmentCount = 1,
        .pAttachments = &blend_attachment,
    };

    VkGraphicsPipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = shader_stages,
        .pVertexInputState = &vertex_input,
        .pInputAssemblyState = &input_assembly,
        .pViewportState = &viewport_state,
        .pRasterizationState = &raster,
        .pMultisampleState = &multisample,
        .pDepthStencilState = nullptr,
        .pColorBlendState = &color_blend,
        .pDynamicState = &dynamic_state,
        .layout = tp->layout,
        .renderPass = ctx->graphics_pipeline.render_pass,
        .subpass = 0,
    };

    VkResult result = vkCreateGraphicsPipelines(ctx->device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &tp->handle);

    // Destroy text shader modules (no longer needed after pipeline creation)
    for (u32 i = text_shader_start; i < ctx->shaders.count; i++) {
        vkDestroyShaderModule(ctx->device, ctx->shaders.items[i].module, nullptr);
    }
    ctx->shaders.count = text_shader_start;

    if (result != VK_SUCCESS) {
        RL_ERROR("Failed to create text graphics pipeline");
        return false;
    }

    RL_TRACE("Created Vulkan text pipeline");
    return true;
}

static b8 vk_text_create_vertex_buffers(VK_Context *ctx) {
    VK_TextPipeline *tp = &ctx->text_pipeline;
    VkDeviceSize buf_size = sizeof(VK_TextVertex) * 6 * MAX_TEXT_GLYPHS;

    tp->vertex_buffers = rl_arena_push(&ctx->arena, sizeof(VkBuffer) * ctx->max_frames_in_flight, true);
    tp->vertex_buffer_memory = rl_arena_push(&ctx->arena, sizeof(VkDeviceMemory) * ctx->max_frames_in_flight, true);
    tp->vertex_buffer_mapped = rl_arena_push(&ctx->arena, sizeof(void *) * ctx->max_frames_in_flight, true);

    for (u32 i = 0; i < ctx->max_frames_in_flight; i++) {
        if (!vk_buffer_create(ctx, buf_size,
                              VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                              &tp->vertex_buffers[i], &tp->vertex_buffer_memory[i])) {
            RL_ERROR("Failed to create text vertex buffer %d", i);
            return false;
        }

        if (vkMapMemory(ctx->device, tp->vertex_buffer_memory[i], 0, buf_size, 0, &tp->vertex_buffer_mapped[i]) != VK_SUCCESS) {
            RL_ERROR("Failed to map text vertex buffer %d", i);
            return false;
        }
    }

    return true;
}

static b8 vk_text_create_descriptors(VK_Context *ctx, VK_Font *font) {
    VK_TextPipeline *tp = &ctx->text_pipeline;

    // Create pool on first call (sized for all fonts)
    if (!tp->descriptor_pool) {
        u32 total_sets = tp->fonts.count * ctx->max_frames_in_flight;
        VkDescriptorPoolSize pool_size = {
            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = total_sets,
        };

        VkDescriptorPoolCreateInfo pool_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .maxSets = total_sets,
            .poolSizeCount = 1,
            .pPoolSizes = &pool_size,
        };

        if (vkCreateDescriptorPool(ctx->device, &pool_info, nullptr, &tp->descriptor_pool) != VK_SUCCESS) {
            RL_ERROR("Failed to create text descriptor pool");
            return false;
        }
    }

    // Allocate per-frame descriptor sets for this font
    VkDescriptorSetLayout *layouts = rl_arena_push(&ctx->arena, sizeof(VkDescriptorSetLayout) * ctx->max_frames_in_flight, true);
    for (u32 i = 0; i < ctx->max_frames_in_flight; i++) {
        layouts[i] = tp->descriptor_set_layout;
    }

    font->descriptor_sets = rl_arena_push(&ctx->arena, sizeof(VkDescriptorSet) * ctx->max_frames_in_flight, true);

    VkDescriptorSetAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = tp->descriptor_pool,
        .descriptorSetCount = ctx->max_frames_in_flight,
        .pSetLayouts = layouts,
    };

    if (vkAllocateDescriptorSets(ctx->device, &alloc_info, font->descriptor_sets) != VK_SUCCESS) {
        RL_ERROR("Failed to allocate text descriptor sets for font '%s'", font->font->name);
        return false;
    }

    // Write this font's atlas to each set
    for (u32 i = 0; i < ctx->max_frames_in_flight; i++) {
        VkDescriptorImageInfo image_info = {
            .sampler = tp->font_sampler,
            .imageView = font->atlas_view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };

        VkWriteDescriptorSet write = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = font->descriptor_sets[i],
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &image_info,
        };

        vkUpdateDescriptorSets(ctx->device, 1, &write, 0, nullptr);
    }

    return true;
}
