#pragma once

#include "asset/asset.h"

#define ASSET_TABLE_TOTAL 3

// ------ FONT
static rl_asset asset_table[ASSET_TABLE_TOTAL] = {
    (rl_asset){ASSET_FONT, "evil_empire.otf", nullptr},
    (rl_asset){ASSET_SHADER, "default.vert", nullptr},
    (rl_asset){ASSET_SHADER, "default.frag", nullptr},
};