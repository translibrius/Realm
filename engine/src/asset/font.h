#pragma once

#include "asset/asset.h"
#include "defines.h"
#include "util/str.h"
#include "vendor/stb/stb_truetype.h"

typedef struct rl_font {
    void *handle; // To opengl, vulkan ect. specific font structs like GL_Font
    const char *name;
    rl_string path;
    u8 *data;
    u64 data_size;
    u8 *atlas;
    i32 atlas_w;
    i32 atlas_h;
    stbtt_bakedchar *chars;
    i32 first_char;
    i32 char_count;
    f32 baked_px;
} rl_font;

b8 rl_font_init(rl_arena *asset_arena, rl_asset *asset);