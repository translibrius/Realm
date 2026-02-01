#pragma once

#include "defines.h"

typedef enum ASSET_TYPE {
    ASSET_FONT,
    ASSET_SHADER,
    ASSET_TEXTURE,
} ASSET_TYPE;

typedef struct rl_asset {
    ASSET_TYPE type;
    const char *filename;
    void *handle;
} rl_asset;

REALM_API const char *get_assets_dir(ASSET_TYPE asset_type);
REALM_API rl_asset *get_asset(const char *filename);
