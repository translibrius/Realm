#include "asset/font_atlas.h"

#include "asset/asset.h"
#include "asset/asset_internal.h"
#include "asset/font.h"
#include "core/logger.h"
#include "memory/memory.h"

#include <string.h>

#define FONT_ATLAS_MAX_FONTS 8

static rl_texture combined_atlas;
static b8 combined_built;

b8 rl_font_atlas_build_combined(void) {
    Assets *assets = get_assets();

    rl_font *fonts[FONT_ATLAS_MAX_FONTS];
    u32 font_count = 0;

    for (u32 i = 0; i < assets->count && font_count < FONT_ATLAS_MAX_FONTS; i++) {
        rl_asset *a = &assets->items[i];
        if (a->type == ASSET_FONT && a->data) {
            fonts[font_count++] = (rl_font *)a->data;
        }
    }

    if (font_count < 2) {
        RL_DEBUG("font_atlas: fewer than 2 fonts, skipping combine");
        return false;
    }

    // Compute combined dimensions (vertical stack)
    i32 combined_w = 0;
    i32 combined_h = 0;
    for (u32 i = 0; i < font_count; i++) {
        if (fonts[i]->atlas.width > combined_w)
            combined_w = fonts[i]->atlas.width;
        combined_h += fonts[i]->atlas.height;
    }

    u64 combined_size = (u64)combined_w * combined_h * 4;
    u8 *pixels = (u8 *)mem_alloc(combined_size, MEM_SUBSYSTEM_ASSET);
    memset(pixels, 0, combined_size);

    // Copy each font's atlas into the combined buffer and remap UVs
    i32 y_offset = 0;
    for (u32 f = 0; f < font_count; f++) {
        rl_font *font = fonts[f];
        i32 fw = font->atlas.width;
        i32 fh = font->atlas.height;

        // Copy rows
        for (i32 row = 0; row < fh; row++) {
            u8 *dst = pixels + ((y_offset + row) * combined_w + 0) * 4;
            u8 *src = font->atlas.data + (row * fw) * 4;
            memcpy(dst, src, (u64)fw * 4);
        }

        // Remap glyph UVs from [0,1] relative to individual atlas → combined atlas
        f32 sx = (f32)fw / (f32)combined_w;
        f32 sy = (f32)fh / (f32)combined_h;
        f32 oy = (f32)y_offset / (f32)combined_h;

        for (u32 g = 0; g < font->glyph_count; g++) {
            rl_glyph *glyph = &font->glyphs[g];
            glyph->uv_min_x *= sx;
            glyph->uv_max_x *= sx;
            glyph->uv_min_y = glyph->uv_min_y * sy + oy;
            glyph->uv_max_y = glyph->uv_max_y * sy + oy;
        }

        // Free individual atlas pixel data
        mem_free(font->atlas.data, font->atlas.size, MEM_SUBSYSTEM_ASSET);
        font->atlas.data = nullptr;

        y_offset += fh;
    }

    combined_atlas.width = combined_w;
    combined_atlas.height = combined_h;
    combined_atlas.channels = 4;
    combined_atlas.size = combined_size;
    combined_atlas.data = pixels;
    combined_built = true;

    RL_INFO("font_atlas: combined %u fonts into %dx%d atlas", font_count, combined_w, combined_h);
    return true;
}

const rl_texture *rl_font_atlas_get_combined(void) {
    return combined_built ? &combined_atlas : nullptr;
}

void rl_font_atlas_shutdown(void) {
    if (combined_built && combined_atlas.data) {
        mem_free(combined_atlas.data, combined_atlas.size, MEM_SUBSYSTEM_ASSET);
        combined_atlas.data = nullptr;
    }
    combined_built = false;
}
