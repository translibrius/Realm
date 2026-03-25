#include "vk_gui.h"

#include "asset/asset.h"
#include "asset/asset_internal.h"
#include "asset/font.h"
#include "asset/font_atlas.h"
#include "clay.h"
#include "core/logger.h"
#include "renderer/renderer_types.h"
#include "vk_buffer.h"
#include "vk_pipeline.h"
#include "vk_renderer.h"
#include "vk_shader.h"
#include "vk_text.h"

#define GUI_MAX_CLIP_DEPTH 16

#include "util/str.h"

// ---------------------------------------------------------------------------
// Software clip rect stack (identical to gl_gui.c)
// ---------------------------------------------------------------------------

typedef struct {
    f32 x0, y0, x1, y1;
} gui_clip_rect;

static gui_clip_rect clip_stack[GUI_MAX_CLIP_DEPTH];
static i32 clip_depth;

static void clip_push(f32 x, f32 y, f32 w, f32 h) {
    gui_clip_rect r = {x, y, x + w, y + h};
    if (clip_depth > 0) {
        gui_clip_rect *p = &clip_stack[clip_depth - 1];
        if (r.x0 < p->x0) r.x0 = p->x0;
        if (r.y0 < p->y0) r.y0 = p->y0;
        if (r.x1 > p->x1) r.x1 = p->x1;
        if (r.y1 > p->y1) r.y1 = p->y1;
    }
    if (clip_depth < GUI_MAX_CLIP_DEPTH) {
        clip_stack[clip_depth++] = r;
    }
}

static void clip_pop(void) {
    if (clip_depth > 0) clip_depth--;
}

static b8 clip_active(void) {
    return clip_depth > 0;
}

static gui_clip_rect *clip_current(void) {
    return clip_depth > 0 ? &clip_stack[clip_depth - 1] : nullptr;
}

// ---------------------------------------------------------------------------
// Vertex helpers
// ---------------------------------------------------------------------------

static void push_rect(VK_GuiVertex *verts, u32 *count,
                       f32 x, f32 y, f32 w, f32 h,
                       f32 r, f32 g, f32 b, f32 a,
                       Clay_CornerRadius corners) {
    if (*count + 6 > VK_GUI_MAX_VERTS) return;

    b8 has_radius = corners.topLeft > 0 || corners.topRight > 0 ||
                    corners.bottomLeft > 0 || corners.bottomRight > 0;

    f32 x0 = x, y0 = y, x1 = x + w, y1 = y + h;

    if (!has_radius && clip_active()) {
        gui_clip_rect *c = clip_current();
        if (x0 < c->x0) x0 = c->x0;
        if (y0 < c->y0) y0 = c->y0;
        if (x1 > c->x1) x1 = c->x1;
        if (y1 > c->y1) y1 = c->y1;
        if (x0 >= x1 || y0 >= y1) return;
    }

    VK_GuiVertex *v = &verts[*count];

    if (has_radius) {
        f32 hw = (x1 - x0) * 0.5f;
        f32 hh = (y1 - y0) * 0.5f;
        vec4 ri = {hw, hh, 1.0f, 0};
        vec4 cr = {corners.topLeft, corners.topRight, corners.bottomLeft, corners.bottomRight};
        v[0] = (VK_GuiVertex){.pos = {x0, y0}, .uv = {-hw, -hh}, .color = {r, g, b, a}, .rect_info = {ri[0], ri[1], ri[2], ri[3]}, .corner_radii = {cr[0], cr[1], cr[2], cr[3]}};
        v[1] = (VK_GuiVertex){.pos = {x0, y1}, .uv = {-hw,  hh}, .color = {r, g, b, a}, .rect_info = {ri[0], ri[1], ri[2], ri[3]}, .corner_radii = {cr[0], cr[1], cr[2], cr[3]}};
        v[2] = (VK_GuiVertex){.pos = {x1, y1}, .uv = { hw,  hh}, .color = {r, g, b, a}, .rect_info = {ri[0], ri[1], ri[2], ri[3]}, .corner_radii = {cr[0], cr[1], cr[2], cr[3]}};
        v[3] = (VK_GuiVertex){.pos = {x0, y0}, .uv = {-hw, -hh}, .color = {r, g, b, a}, .rect_info = {ri[0], ri[1], ri[2], ri[3]}, .corner_radii = {cr[0], cr[1], cr[2], cr[3]}};
        v[4] = (VK_GuiVertex){.pos = {x1, y1}, .uv = { hw,  hh}, .color = {r, g, b, a}, .rect_info = {ri[0], ri[1], ri[2], ri[3]}, .corner_radii = {cr[0], cr[1], cr[2], cr[3]}};
        v[5] = (VK_GuiVertex){.pos = {x1, y0}, .uv = { hw, -hh}, .color = {r, g, b, a}, .rect_info = {ri[0], ri[1], ri[2], ri[3]}, .corner_radii = {cr[0], cr[1], cr[2], cr[3]}};
    } else {
        v[0] = (VK_GuiVertex){.pos = {x0, y0}, .uv = {-1, -1}, .color = {r, g, b, a}, .rect_info = {0}};
        v[1] = (VK_GuiVertex){.pos = {x0, y1}, .uv = {-1, -1}, .color = {r, g, b, a}, .rect_info = {0}};
        v[2] = (VK_GuiVertex){.pos = {x1, y1}, .uv = {-1, -1}, .color = {r, g, b, a}, .rect_info = {0}};
        v[3] = (VK_GuiVertex){.pos = {x0, y0}, .uv = {-1, -1}, .color = {r, g, b, a}, .rect_info = {0}};
        v[4] = (VK_GuiVertex){.pos = {x1, y1}, .uv = {-1, -1}, .color = {r, g, b, a}, .rect_info = {0}};
        v[5] = (VK_GuiVertex){.pos = {x1, y0}, .uv = {-1, -1}, .color = {r, g, b, a}, .rect_info = {0}};
    }
    *count += 6;
}

static void push_text_glyphs(VK_GuiVertex *verts, u32 *vert_count, u32 max_verts,
                              VK_Font *vk_font, rl_font *font,
                              const char *chars, i32 len,
                              f32 size_px, f32 x, f32 y, vec4 color) {
    gui_clip_rect *clip = clip_current();

    f32 cursor_x = x;
    const char *ptr = chars;
    const char *end = chars + len;
    while (ptr < end) {
        if (*vert_count + 6 > max_verts) break;

        u32 cp = utf8_decode(&ptr);
        const rl_glyph *g = (cp < 256) ? vk_font->glyph_map[cp] : rl_font_find_glyph(font, cp);
        if (!g) continue;

        f32 gx0 = cursor_x + g->plane_min_x * size_px;
        f32 gx1 = cursor_x + g->plane_max_x * size_px;
        f32 gy0 = y - g->plane_max_y * size_px;
        f32 gy1 = y - g->plane_min_y * size_px;

        cursor_x += g->advance * size_px;

        if (clip && (gx1 <= clip->x0 || gx0 >= clip->x1 || gy1 <= clip->y0 || gy0 >= clip->y1)) {
            continue;
        }

        f32 u0 = g->uv_min_x, v0 = g->uv_min_y;
        f32 u1 = g->uv_max_x, v1 = g->uv_max_y;

        if (clip) {
            f32 orig_w = gx1 - gx0;
            f32 orig_h = gy1 - gy0;
            if (gx0 < clip->x0 && orig_w > 0) {
                u0 += (u1 - u0) * (clip->x0 - gx0) / orig_w;
                gx0 = clip->x0;
            }
            if (gx1 > clip->x1 && orig_w > 0) {
                u1 -= (u1 - u0) * (gx1 - clip->x1) / (gx1 - gx0);
                gx1 = clip->x1;
            }
            if (gy0 < clip->y0 && orig_h > 0) {
                v1 -= (v1 - v0) * (clip->y0 - gy0) / orig_h;
                gy0 = clip->y0;
            }
            if (gy1 > clip->y1 && orig_h > 0) {
                v0 += (v1 - v0) * (gy1 - clip->y1) / (gy1 - gy0);
                gy1 = clip->y1;
            }
        }

        VK_GuiVertex *v = &verts[*vert_count];
        v[0] = (VK_GuiVertex){.pos = {gx0, gy0}, .uv = {u0, v1}, .color = {color[0], color[1], color[2], color[3]}, .rect_info = {0}};
        v[1] = (VK_GuiVertex){.pos = {gx0, gy1}, .uv = {u0, v0}, .color = {color[0], color[1], color[2], color[3]}, .rect_info = {0}};
        v[2] = (VK_GuiVertex){.pos = {gx1, gy1}, .uv = {u1, v0}, .color = {color[0], color[1], color[2], color[3]}, .rect_info = {0}};
        v[3] = (VK_GuiVertex){.pos = {gx0, gy0}, .uv = {u0, v1}, .color = {color[0], color[1], color[2], color[3]}, .rect_info = {0}};
        v[4] = (VK_GuiVertex){.pos = {gx1, gy1}, .uv = {u1, v0}, .color = {color[0], color[1], color[2], color[3]}, .rect_info = {0}};
        v[5] = (VK_GuiVertex){.pos = {gx1, gy0}, .uv = {u1, v1}, .color = {color[0], color[1], color[2], color[3]}, .rect_info = {0}};
        *vert_count += 6;
    }
}

// ---------------------------------------------------------------------------
// Font lookup (same logic as gl_gui.c)
// ---------------------------------------------------------------------------

static rl_font *gui_get_font(u16 font_id) {
    Assets *assets = get_assets();
    u32 font_index = 0;
    for (u32 i = 0; i < assets->count; i++) {
        rl_asset *asset = &assets->items[i];
        if (asset->type == ASSET_FONT && asset->data) {
            if (font_index == font_id) {
                return (rl_font *)asset->data;
            }
            font_index++;
        }
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// Segment tracking — records draw ranges for command recording
// ---------------------------------------------------------------------------

static void gui_segment_flush(VK_GuiPipeline *gp, VK_Font *font, u32 start, u32 count) {
    if (count == 0 || !font) return;
    if (gp->segment_count >= GUI_MAX_SEGMENTS) return;

    gp->segments[gp->segment_count++] = (VK_GuiSegment){
        .start_vertex = start,
        .vertex_count = count,
        .font = font,
    };
}

// ---------------------------------------------------------------------------
// Pipeline init / destroy
// ---------------------------------------------------------------------------

b8 vk_gui_pipeline_init(VK_Context *ctx) {
    VK_GuiPipeline *gp = &ctx->gui_pipeline;
    VK_TextPipeline *tp = &ctx->text_pipeline;

    // --- Compile shaders ---
    VkShaderModule vert_module, frag_module;
    if (!vk_shader_compile_to_module(ctx, asset_find(RL_ASSET_SHADER_VK_GUI_VERT), &vert_module)) {
        RL_ERROR("Failed to compile Vulkan GUI vertex shader");
        return false;
    }
    if (!vk_shader_compile_to_module(ctx, asset_find(RL_ASSET_SHADER_VK_GUI_FRAG), &frag_module)) {
        vkDestroyShaderModule(ctx->device, vert_module, nullptr);
        RL_ERROR("Failed to compile Vulkan GUI fragment shader");
        return false;
    }

    // --- Push constant range: vec2 screen_size + float px_range + float weight = 16 bytes ---
    VkPushConstantRange push_range = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset = 0,
        .size = 16
    };

    VkPipelineShaderStageCreateInfo stages[2] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vert_module,
            .pName = "main"
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = frag_module,
            .pName = "main"
        }
    };

    VkVertexInputBindingDescription binding_desc = {
        .binding = 0,
        .stride = sizeof(VK_GuiVertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };

    VkVertexInputAttributeDescription attr_descs[5] = {
        { .location = 0, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT,       .offset = offsetof(VK_GuiVertex, pos) },
        { .location = 1, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT,       .offset = offsetof(VK_GuiVertex, uv) },
        { .location = 2, .binding = 0, .format = VK_FORMAT_R32G32B32A32_SFLOAT, .offset = offsetof(VK_GuiVertex, color) },
        { .location = 3, .binding = 0, .format = VK_FORMAT_R32G32B32A32_SFLOAT, .offset = offsetof(VK_GuiVertex, rect_info) },
        { .location = 4, .binding = 0, .format = VK_FORMAT_R32G32B32A32_SFLOAT, .offset = offsetof(VK_GuiVertex, corner_radii) },
    };

    // Reuse the text pipeline's descriptor set layout (same: 1 combined image sampler)
    VK_PipelineConfig cfg = {
        .stages = stages,
        .stage_count = 2,
        .bindings = &binding_desc,
        .binding_count = 1,
        .attributes = attr_descs,
        .attribute_count = 5,
        .set_layouts = &tp->descriptor_set_layout,
        .set_layout_count = 1,
        .push_constants = &push_range,
        .push_constant_count = 1,
        .depth_test = false,
        .depth_write = false,
        .cull_mode = VK_CULL_MODE_NONE,
        .blend_enable = true,
        .msaa_samples = ctx->msaa_samples,
        .render_pass = ctx->render_pass,
    };

    b8 ok = vk_pipeline_create_graphics(ctx, &cfg, &gp->handle, &gp->layout);

    vkDestroyShaderModule(ctx->device, vert_module, nullptr);
    vkDestroyShaderModule(ctx->device, frag_module, nullptr);

    if (!ok) {
        RL_ERROR("Failed to create GUI graphics pipeline");
        return false;
    }

    // --- Per-frame vertex buffers ---
    VkDeviceSize vb_size = sizeof(VK_GuiVertex) * VK_GUI_MAX_VERTS;

    gp->vertex_buffers = rl_arena_push(&ctx->arena, sizeof(VkBuffer) * ctx->max_frames_in_flight, alignof(VkBuffer));
    gp->vertex_buffer_memory = rl_arena_push(&ctx->arena, sizeof(VkDeviceMemory) * ctx->max_frames_in_flight, alignof(VkDeviceMemory));
    gp->vertex_buffer_mapped = rl_arena_push(&ctx->arena, sizeof(void *) * ctx->max_frames_in_flight, alignof(void *));

    if (!vk_buffers_create_mapped(ctx, vb_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                  ctx->max_frames_in_flight,
                                  gp->vertex_buffers, gp->vertex_buffer_memory, gp->vertex_buffer_mapped)) {
        return false;
    }

    gp->vertex_count = 0;
    gp->current_font = nullptr;
    gp->segment_count = 0;

    RL_DEBUG("Vulkan GUI pipeline initialized");
    return true;
}

void vk_gui_pipeline_destroy(VK_Context *ctx) {
    VK_GuiPipeline *gp = &ctx->gui_pipeline;

    if (gp->vertex_buffers) {
        for (u32 i = 0; i < ctx->max_frames_in_flight; i++) {
            vk_buffer_destroy(ctx, gp->vertex_buffers[i], gp->vertex_buffer_memory[i]);
        }
    }

    if (gp->handle) {
        vkDestroyPipeline(ctx->device, gp->handle, nullptr);
    }

    if (gp->layout) {
        vkDestroyPipelineLayout(ctx->device, gp->layout, nullptr);
    }
}

// ---------------------------------------------------------------------------
// Public helpers — let vk_text.c write into the GUI vertex buffer
// ---------------------------------------------------------------------------

VK_GuiVertex *vk_gui_pipeline_get_write_ptr(VK_Context *ctx, u32 *out_remaining) {
    VK_GuiPipeline *gp = &ctx->gui_pipeline;
    if (gp->vertex_count >= VK_GUI_MAX_VERTS) {
        *out_remaining = 0;
        return nullptr;
    }
    *out_remaining = VK_GUI_MAX_VERTS - gp->vertex_count;
    VK_GuiVertex *base = gp->vertex_buffer_mapped[ctx->current_frame];
    return &base[gp->vertex_count];
}

void vk_gui_pipeline_commit_verts(VK_Context *ctx, VK_Font *font, u32 count) {
    if (count == 0) return;
    VK_GuiPipeline *gp = &ctx->gui_pipeline;
    gui_segment_flush(gp, font, gp->vertex_count, count);
    gp->vertex_count += count;
}

// ---------------------------------------------------------------------------
// Render (called from renderer_submit_gui_data — fills vertex buffer)
// ---------------------------------------------------------------------------

void vulkan_render_gui(void *commands, i32 command_count) {
    if (!commands || command_count <= 0) return;

    VK_Context *ctx = vulkan_get_context_ptr();
    if (!ctx || !ctx->frame_acquired) return;

    VK_GuiPipeline *gp = &ctx->gui_pipeline;
    Clay_RenderCommand *cmds = (Clay_RenderCommand *)commands;

    VK_GuiVertex *verts = gp->vertex_buffer_mapped[ctx->current_frame];
    u32 vert_count = gp->vertex_count;       // continue from where text left off
    VK_Font *current_font = nullptr;
    u32 segment_start = vert_count;

    clip_depth = 0;

    for (i32 i = 0; i < command_count; i++) {
        Clay_RenderCommand *cmd = &cmds[i];
        Clay_BoundingBox bb = cmd->boundingBox;

        switch (cmd->commandType) {
        case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
            Clay_RectangleRenderData *rect = &cmd->renderData.rectangle;
            f32 r = rect->backgroundColor.r / 255.0f;
            f32 g = rect->backgroundColor.g / 255.0f;
            f32 b = rect->backgroundColor.b / 255.0f;
            f32 a = rect->backgroundColor.a / 255.0f;
            push_rect(verts, &vert_count, bb.x, bb.y, bb.width, bb.height, r, g, b, a, rect->cornerRadius);
            break;
        }

        case CLAY_RENDER_COMMAND_TYPE_BORDER: {
            Clay_BorderRenderData *border = &cmd->renderData.border;
            f32 r = border->color.r / 255.0f;
            f32 g = border->color.g / 255.0f;
            f32 b = border->color.b / 255.0f;
            f32 a = border->color.a / 255.0f;

            if (border->width.top > 0)
                push_rect(verts, &vert_count, bb.x, bb.y, bb.width, (f32)border->width.top, r, g, b, a, (Clay_CornerRadius){0});
            if (border->width.bottom > 0)
                push_rect(verts, &vert_count, bb.x, bb.y + bb.height - (f32)border->width.bottom, bb.width, (f32)border->width.bottom, r, g, b, a, (Clay_CornerRadius){0});
            if (border->width.left > 0)
                push_rect(verts, &vert_count, bb.x, bb.y, (f32)border->width.left, bb.height, r, g, b, a, (Clay_CornerRadius){0});
            if (border->width.right > 0)
                push_rect(verts, &vert_count, bb.x + bb.width - (f32)border->width.right, bb.y, (f32)border->width.right, bb.height, r, g, b, a, (Clay_CornerRadius){0});
            break;
        }

        case CLAY_RENDER_COMMAND_TYPE_TEXT: {
            Clay_TextRenderData *text = &cmd->renderData.text;
            if (!text->stringContents.chars || text->stringContents.length <= 0) break;

            rl_font *font = gui_get_font(text->fontId);
            if (!font) break;

            VK_Font *vk_font = vk_find_font(ctx, font);
            if (!vk_font) break;

            // Flush segment if font changed (only needed without combined atlas)
            if (!ctx->text_pipeline.has_combined_atlas && current_font && current_font != vk_font) {
                gui_segment_flush(gp, current_font, segment_start, vert_count - segment_start);
                segment_start = vert_count;
            }
            current_font = vk_font;

            vec4 color = {
                text->textColor.r / 255.0f,
                text->textColor.g / 255.0f,
                text->textColor.b / 255.0f,
                text->textColor.a / 255.0f,
            };

            f32 text_y = bb.y + font->ascender * (f32)text->fontSize;

            push_text_glyphs(verts, &vert_count, VK_GUI_MAX_VERTS,
                             vk_font, font,
                             text->stringContents.chars, text->stringContents.length,
                             (f32)text->fontSize, bb.x, text_y, color);
            break;
        }

        case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START: {
            clip_push(bb.x, bb.y, bb.width, bb.height);
            break;
        }

        case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END: {
            clip_pop();
            break;
        }

        case CLAY_RENDER_COMMAND_TYPE_IMAGE: {
            push_rect(verts, &vert_count, bb.x, bb.y, bb.width, bb.height,
                      0.3f, 0.3f, 0.3f, 1.0f, (Clay_CornerRadius){0});
            break;
        }

        default:
            break;
        }
    }

    // Final segment
    if (vert_count > segment_start) {
        gui_segment_flush(gp, current_font, segment_start, vert_count - segment_start);
    }

    gp->vertex_count = vert_count;
}

// ---------------------------------------------------------------------------
// Record commands (called during command buffer recording)
// ---------------------------------------------------------------------------

void vulkan_gui_record_commands(VK_Context *ctx, VkCommandBuffer cmd) {
    VK_GuiPipeline *gp = &ctx->gui_pipeline;

    if (gp->vertex_count == 0 || gp->segment_count == 0)
        return;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, gp->handle);

    VkViewport viewport = {
        .x = 0.0f,
        .y = 0.0f,
        .width = (f32)ctx->swapchain.chosen_extent.width,
        .height = (f32)ctx->swapchain.chosen_extent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor = {
        .offset = {0, 0},
        .extent = ctx->swapchain.chosen_extent
    };
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // Bind vertex buffer (shared across all segments)
    VkBuffer buffers[] = {gp->vertex_buffers[ctx->current_frame]};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, buffers, offsets);

    VK_TextPipeline *tp = &ctx->text_pipeline;

    if (tp->has_combined_atlas) {
        // Single push + bind + draw for the entire GUI pass
        VK_GuiSegment *seg = &gp->segments[0];

        struct {
            f32 screen_w;
            f32 screen_h;
            f32 px_range;
            f32 weight;
        } push = {
            .screen_w = (f32)ctx->swapchain.chosen_extent.width,
            .screen_h = (f32)ctx->swapchain.chosen_extent.height,
            .px_range = seg->font->font->pixel_range,
            .weight = 0.12f
        };
        vkCmdPushConstants(cmd, gp->layout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(push), &push);

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                gp->layout, 0, 1,
                                &tp->combined_descriptor_sets[ctx->current_frame],
                                0, nullptr);

        vkCmdDraw(cmd, gp->vertex_count, 1, 0, 0);
    } else {
        // Legacy path: per-segment draw calls
        for (u32 i = 0; i < gp->segment_count; i++) {
            VK_GuiSegment *seg = &gp->segments[i];

            struct {
                f32 screen_w;
                f32 screen_h;
                f32 px_range;
                f32 weight;
            } push = {
                .screen_w = (f32)ctx->swapchain.chosen_extent.width,
                .screen_h = (f32)ctx->swapchain.chosen_extent.height,
                .px_range = seg->font->font->pixel_range,
                .weight = 0.12f
            };
            vkCmdPushConstants(cmd, gp->layout,
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               0, sizeof(push), &push);

            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    gp->layout, 0, 1,
                                    &seg->font->descriptor_sets[ctx->current_frame],
                                    0, nullptr);

            vkCmdDraw(cmd, seg->vertex_count, 1, seg->start_vertex, 0);
        }
    }

    // Reset for next frame
    gp->vertex_count = 0;
    gp->segment_count = 0;
    gp->current_font = nullptr;
}
