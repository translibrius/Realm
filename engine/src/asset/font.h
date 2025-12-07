#pragma once

#include "defines.h"
#include "util/str.h"

typedef struct rl_asset_font {
    const char *name;
    rl_string path;
} rl_asset_font;

b8 rl_font_init(const char *font_name, rl_asset_font *out_asset);