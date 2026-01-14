#pragma once

#include "defines.h"
#include "vk_types.h"

b8 vk_vertex_create_buffer(VK_Context *context, Vertices *vertices);
void vk_vertex_destroy_buffer(VK_Context *context);

VkVertexInputBindingDescription vk_vertex_get_binding_desc();
void vk_vertex_get_attr_desc(VkVertexInputAttributeDescription *out_attrs);