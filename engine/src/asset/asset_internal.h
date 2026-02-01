#pragma once

#include "memory/containers/dynamic_array.h"
#include <asset/asset.h>

DA_DEFINE(Assets, rl_asset);

u64 asset_system_size();
b8 asset_system_start(void *system);
void asset_system_shutdown();

b8 asset_system_load_all();
b8 asset_system_load(rl_asset *asset);

Assets *get_assets();

u32 get_asset_count();
