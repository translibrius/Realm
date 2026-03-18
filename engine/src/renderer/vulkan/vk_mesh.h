#pragma once

#include "vk_types.h"

b8 vk_mesh_create_cube(VK_Context *ctx);
void vk_mesh_destroy_cube(VK_Context *ctx);

// Imported mesh cache — lazily uploads mesh asset vertex/index data to GPU
i32 vk_mesh_cache_find(VK_Context *ctx, asset_id id);
i32 vk_mesh_cache_upload(VK_Context *ctx, asset_id id);
void vk_mesh_cache_destroy_all(VK_Context *ctx);
