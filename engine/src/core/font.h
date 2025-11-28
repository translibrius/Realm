#pragma once

#include "defines.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "vendor/stb/stb_truetype.h"

typedef struct rl_font {
    u32 atlas_width;
    u32 atlas_height;
    u8 *pixels;
} rl_font;