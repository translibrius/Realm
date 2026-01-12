#pragma once

#include "defines.h"
#include "asset/asset.h"

#include "memory/arena.h"
#include "renderer/renderer_types.h"

typedef struct rl_asset_shader {
    const char *source;
    SHADER_TYPE type;
} rl_asset_shader;

b8 load_shader(rl_arena *arena, rl_asset *asset);