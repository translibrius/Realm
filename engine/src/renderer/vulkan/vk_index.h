#pragma once

#include "defines.h"
#include "vk_types.h"

b8 vk_index_create_buffer(VK_Context *context, Indices *indices);
void vk_index_destroy_buffer(VK_Context *context);