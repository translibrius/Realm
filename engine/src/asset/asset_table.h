#pragma once

#include "asset/asset.h"

#define ASSET_TABLE_TOTAL 11

static rl_asset asset_table[ASSET_TABLE_TOTAL] = {
    (rl_asset){ASSET_FONT, "evil_empire.otf", nullptr},
    (rl_asset){ASSET_FONT, "JetBrainsMono-Regular.ttf", nullptr},
    (rl_asset){ASSET_SHADER, "default.vert", nullptr},
    (rl_asset){ASSET_SHADER, "text.vert", nullptr},
    (rl_asset){ASSET_SHADER, "default.frag", nullptr},
    (rl_asset){ASSET_SHADER, "text.frag", nullptr},
    (rl_asset){ASSET_SHADER, "light.frag", nullptr},
    (rl_asset){ASSET_SHADER, "vulkan_triangle.frag", nullptr},
    (rl_asset){ASSET_SHADER, "vulkan_triangle.vert", nullptr},
    (rl_asset){ASSET_TEXTURE, "wood_container.jpg", nullptr},
    (rl_asset){ASSET_TEXTURE, "face.jpg", nullptr},
};