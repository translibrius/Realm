#pragma once

#include "defines.h"
#include "vk_types.h"

b8 vk_descriptor_create_set_layout(VK_Context *context);
void vk_descriptor_destroy_set_layout(VK_Context *context);

b8 vk_descriptor_create_pool(VK_Context *context);
void vk_descriptor_destroy_pool(VK_Context *context);

b8 vk_descriptor_create_sets(VK_Context *context);