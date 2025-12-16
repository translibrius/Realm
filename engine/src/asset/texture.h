#pragma once

#include "defines.h"
#include "asset/asset.h"

typedef struct rl_texture {
    i32 width, height, channels;
    u64 size;
    u8 *data;
} rl_texture;

b8 load_texture(rl_arena *asset_arena, rl_asset *asset);