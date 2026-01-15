#pragma once

#include "defines.h"
#include "vk_types.h"

b8 vk_texture_create(VK_Context *ctx, VK_Texture *vk_texture);
void vk_texture_destroy(VK_Context *ctx, VK_Texture *vk_texture);

b8 vk_texture_create_sampler(VK_Context *ctx);
void vk_texture_destroy_sampler(VK_Context *ctx);