#include "vk_text.h"

#include "asset/asset_internal.h"
#include "asset/font.h"
#include "core/logger.h"
#include "memory/arena.h"
#include "vk_descriptor.h"
#include "vk_gui.h"
#include "vk_renderer.h"
#include "vk_texture.h"
#include "vk_util.h"

#include <string.h>

VK_Font *vk_find_font(VK_Context *ctx, rl_font *font) {
    for (u32 i = 0; i < ctx->text_pipeline.fonts.count; i++) {
        if (ctx->text_pipeline.fonts.items[i].font == font)
            return &ctx->text_pipeline.fonts.items[i];
    }
    return nullptr;
}

static b8 vk_font_create(rl_font *font, VK_Context *ctx) {
    VK_TextPipeline *tp = &ctx->text_pipeline;

    u32 atlas_w = font->atlas.width;
    u32 atlas_h = font->atlas.height;
    VkDeviceSize image_size = (VkDeviceSize)atlas_w * atlas_h * 4;

    // Prepare RGBA pixel data (convert RGB->RGBA if needed)
    ARENA_SCRATCH_START();
    void *pixels;
    if (font->atlas.channels == 4) {
        pixels = font->atlas.data;
    } else {
        u8 *rgba = rl_arena_push(scratch.arena, image_size, true);
        u8 *src = font->atlas.data;
        for (u32 i = 0; i < atlas_w * atlas_h; i++) {
            rgba[i * 4 + 0] = src[i * 3 + 0];
            rgba[i * 4 + 1] = src[i * 3 + 1];
            rgba[i * 4 + 2] = src[i * 3 + 2];
            rgba[i * 4 + 3] = 255;
        }
        pixels = rgba;
    }

    VK_Font vk_font = {0};

    b8 ok = vk_texture_upload(ctx, atlas_w, atlas_h, 1, VK_FORMAT_R8G8B8A8_UNORM,
                              pixels, image_size,
                              &vk_font.atlas_image, &vk_font.atlas_memory, &vk_font.atlas_view);

    ARENA_SCRATCH_RELEASE();

    if (!ok) {
        RL_ERROR("Failed to upload font atlas");
        return false;
    }

    // Per-frame descriptor sets
    vk_font.descriptor_sets = rl_arena_push(&ctx->arena,
                                            sizeof(VkDescriptorSet) * ctx->max_frames_in_flight,
                                            alignof(VkDescriptorSet));

    if (!vk_descriptor_sets_allocate(ctx, tp->descriptor_pool, tp->descriptor_set_layout,
                                     ctx->max_frames_in_flight, vk_font.descriptor_sets)) {
        return false;
    }

    for (u32 i = 0; i < ctx->max_frames_in_flight; i++) {
        VkDescriptorImageInfo img_info = {
            .sampler = tp->font_sampler,
            .imageView = vk_font.atlas_view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };

        VkWriteDescriptorSet write = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = vk_font.descriptor_sets[i],
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &img_info
        };

        vkUpdateDescriptorSets(ctx->device, 1, &write, 0, nullptr);
    }

    // Glyph map
    vk_font.font = font;
    for (u32 i = 0; i < font->glyph_count; i++) {
        u32 cp = (u32)font->glyphs[i].codepoint;
        if (cp < 256)
            vk_font.glyph_map[cp] = &font->glyphs[i];
    }

    da_append(&tp->fonts, vk_font);
    return true;
}

b8 vk_text_pipeline_init(VK_Context *ctx) {
    VK_TextPipeline *tp = &ctx->text_pipeline;

    // --- Descriptor set layout: 1 combined image sampler at binding 0 ---
    VkDescriptorSetLayoutBinding sampler_binding = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    };

    VkDescriptorSetLayoutCreateInfo ds_layout_ci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &sampler_binding
    };

    VK_CHECK_RETURN_FALSE(
        vkCreateDescriptorSetLayout(ctx->device, &ds_layout_ci, nullptr, &tp->descriptor_set_layout),
        "Failed to create text descriptor set layout");

    // --- Font sampler via shared helper ---
    if (!vk_sampler_create(ctx, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 0.0f, &tp->font_sampler)) {
        return false;
    }

    // --- Descriptor pool via shared helper ---
    VkDescriptorPoolSize pool_size = {
        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 8 * ctx->max_frames_in_flight
    };

    if (!vk_descriptor_pool_create(ctx, &pool_size, 1, 8 * ctx->max_frames_in_flight, &tp->descriptor_pool)) {
        return false;
    }

    // --- Load all font assets ---
    da_init_with_cap(&tp->fonts, 4);

    Assets *assets = get_assets();
    for (u32 i = 0; i < assets->count; i++) {
        rl_asset *asset = &assets->items[i];
        if (asset->type == ASSET_FONT) {
            rl_font *font = (rl_font *)asset->data;
            RL_DEBUG("loading vk font %s", font->name);
            if (!vk_font_create(font, ctx)) {
                RL_WARN("vk_font_create() failed for '%s'", asset->filename);
            }

            if (strcmp(font->name, "JetBrainsMono-Regular.ttf") == 0) {
                vulkan_set_active_font(font);
            }
        }
    }

    RL_DEBUG("Vulkan font manager initialized");
    return true;
}

void vk_text_pipeline_destroy(VK_Context *ctx) {
    VK_TextPipeline *tp = &ctx->text_pipeline;

    if (tp->descriptor_pool) {
        vkDestroyDescriptorPool(ctx->device, tp->descriptor_pool, nullptr);
    }

    for (u32 i = 0; i < tp->fonts.count; i++) {
        VK_Font *f = &tp->fonts.items[i];
        vkDestroyImageView(ctx->device, f->atlas_view, nullptr);
        vkDestroyImage(ctx->device, f->atlas_image, nullptr);
        vkFreeMemory(ctx->device, f->atlas_memory, nullptr);
    }

    if (tp->font_sampler) {
        vkDestroySampler(ctx->device, tp->font_sampler, nullptr);
    }

    if (tp->descriptor_set_layout) {
        vkDestroyDescriptorSetLayout(ctx->device, tp->descriptor_set_layout, nullptr);
    }

    da_free(&tp->fonts);
}

void vulkan_render_text(const char *text, f32 size_px, f32 x, f32 y, vec4 color) {
    VK_Context *ctx = vulkan_get_context_ptr();
    if (!ctx || !ctx->frame_acquired || !text)
        return;

    VK_TextPipeline *tp = &ctx->text_pipeline;

    if (!tp->active_font)
        return;

    VK_Font *vk_font = vk_find_font(ctx, tp->active_font);
    if (!vk_font)
        return;

    rl_font *font = vk_font->font;

    u32 remaining = 0;
    VK_TextVertex *verts = vk_gui_pipeline_get_write_ptr(ctx, &remaining);
    if (!verts || remaining < 6) return;

    u32 vert_count = 0;
    f32 cursor_x = x;
    f32 cursor_y = y;

    for (const unsigned char *c = (const unsigned char *)text; *c; c++) {
        if (*c == '\n') {
            cursor_x = x;
            cursor_y += (f32)(font->line_height * size_px);
            continue;
        }

        if (vert_count + 6 > remaining)
            break;

        u32 cp = *c;
        const rl_glyph *g = (cp < 256) ? vk_font->glyph_map[cp] : rl_font_find_glyph(font, cp);
        if (!g)
            continue;

        f32 x0 = cursor_x + g->plane_min_x * size_px;
        f32 x1 = cursor_x + g->plane_max_x * size_px;
        f32 y0 = cursor_y + g->plane_min_y * size_px;
        f32 y1 = cursor_y + g->plane_max_y * size_px;

        f32 u0 = g->uv_min_x, v0 = g->uv_min_y;
        f32 u1 = g->uv_max_x, v1 = g->uv_max_y;
        f32 r = color[0], g_ = color[1], b = color[2], a = color[3];

        verts[vert_count + 0] = (VK_TextVertex){.pos = {x0, y0}, .uv = {u0, v0}, .color = {r, g_, b, a}};
        verts[vert_count + 1] = (VK_TextVertex){.pos = {x0, y1}, .uv = {u0, v1}, .color = {r, g_, b, a}};
        verts[vert_count + 2] = (VK_TextVertex){.pos = {x1, y1}, .uv = {u1, v1}, .color = {r, g_, b, a}};
        verts[vert_count + 3] = (VK_TextVertex){.pos = {x0, y0}, .uv = {u0, v0}, .color = {r, g_, b, a}};
        verts[vert_count + 4] = (VK_TextVertex){.pos = {x1, y1}, .uv = {u1, v1}, .color = {r, g_, b, a}};
        verts[vert_count + 5] = (VK_TextVertex){.pos = {x1, y0}, .uv = {u1, v0}, .color = {r, g_, b, a}};

        vert_count += 6;
        cursor_x += g->advance * size_px;
    }

    if (vert_count > 0) {
        vk_gui_pipeline_commit_verts(ctx, vk_font, vert_count);
    }
}

void vulkan_set_active_font(rl_font *font) {
    VK_Context *ctx = vulkan_get_context_ptr();
    ctx->text_pipeline.active_font = font;
}
