#pragma once

#include "defines.h"
#include "vk_types.h"

// Create a GPU buffer
b8 vk_buffer_create(VK_Context *context, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags mem_props, VkBuffer *buffer, VkDeviceMemory *memory);

// Destroy a GPU buffer
void vk_buffer_destroy(VK_Context *context, VkBuffer buffer, VkDeviceMemory memory);

// Copy data from GPU buffer src to GPU buffer dst
b8 vk_buffer_copy(VK_Context *context, VkCommandPool cmd_pool, VkBuffer src, VkBuffer dst, VkDeviceSize size);

b8 vk_buffer_create_vertex(VK_Context *context, Vertices *vertices);
void vk_buffer_destroy_vertex(VK_Context *context);

b8 vk_buffer_create_index(VK_Context *context, Indices *indices);
void vk_buffer_destroy_index(VK_Context *context);

b8 vk_buffers_create_uniform(VK_Context *context);
void vk_buffers_destroy_uniform(VK_Context *context);