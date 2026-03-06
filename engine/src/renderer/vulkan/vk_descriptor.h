#pragma once

#include "defines.h"
#include "vk_types.h"

// Generic helpers
b8 vk_descriptor_pool_create(VK_Context *ctx, VkDescriptorPoolSize *sizes, u32 size_count, u32 max_sets, VkDescriptorPool *out);
b8 vk_descriptor_sets_allocate(VK_Context *ctx, VkDescriptorPool pool, VkDescriptorSetLayout layout, u32 count, VkDescriptorSet *out);

// Mesh pipeline-specific (use the generic helpers internally)
b8 vk_descriptor_create_set_layout(VK_Context *context);
void vk_descriptor_destroy_set_layout(VK_Context *context);

b8 vk_descriptor_create_pool(VK_Context *context);
void vk_descriptor_destroy_pool(VK_Context *context);

b8 vk_descriptor_create_sets(VK_Context *context);
