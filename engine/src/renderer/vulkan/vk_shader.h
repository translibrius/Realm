#pragma once

#include "defines.h"
#include "vk_types.h"

b8 vk_shader_init_compiler(VK_Context *context);
void vk_shader_destroy_compiler(VK_Context *context);

b8 vk_shader_compile(VK_Context *context, const char *filename);