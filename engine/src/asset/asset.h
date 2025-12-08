#pragma once

#include "defines.h"
#include "memory/arena.h"

typedef enum ASSET_TYPE {
    ASSET_FONT,
    ASSET_SHADER,
} ASSET_TYPE;

typedef struct rl_asset {
    ASSET_TYPE type;
    const char *filename;
    void *handle;
} rl_asset;

u64 asset_system_size();
b8 asset_system_start(void *system);
void asset_system_shutdown();

b8 asset_system_load_all();
b8 asset_system_load(rl_asset *asset);

const char *get_assets_dir(ASSET_TYPE asset_type);
rl_asset *get_asset(const char *filename);