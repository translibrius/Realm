#pragma once

#include "defines.h"
#include "vk_types.h"

b8 vk_sync_objects_create(VK_Context *context);
void vk_sync_objects_destroy(VK_Context *context);