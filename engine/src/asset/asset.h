#pragma once

#include "defines.h"

typedef enum ASSET_TYPE {
    ASSET_FONT,
} ASSET_TYPE;

const char *get_assets_dir(ASSET_TYPE asset_type);