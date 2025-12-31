#pragma once

#include "defines.h"
#include "asset/asset.h"

#include "memory/arena.h"

typedef struct rl_asset_shader {
    const char *source;
} rl_asset_shader;

b8 load_shader(rl_arena *arena, rl_asset *asset);