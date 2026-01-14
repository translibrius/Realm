#pragma once

#include "defines.h"
#include "vk_types.h"

b8 vk_sync_create_frame(VK_Context *context);
void vk_sync_destroy_frame(VK_Context *context);

b8 vk_sync_create_transfer(VK_Context *context);
void vk_sync_destroy_transfer(VK_Context* context);