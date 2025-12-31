#pragma once

#include "asset/asset.h"
#include "defines.h"
#include "util/str.h"
#include "asset/texture.h"

typedef struct rl_glyph {
    u32 codepoint;
    f32 advance;

    f32 plane_min_x, plane_min_y;
    f32 plane_max_x, plane_max_y;

    f32 uv_min_x, uv_min_y;
    f32 uv_max_x, uv_max_y;
} rl_glyph;

typedef struct rl_font {
    const char *name;
    const char *path;

    rl_glyph *glyphs;
    u32 glyph_count;

    f32 ascender;
    f32 descender;
    f64 line_height;
    f32 scale;
    f32 pixel_range;

    rl_texture atlas;
} rl_font;

b8 rl_font_load(rl_arena *asset_arena, rl_asset *asset);