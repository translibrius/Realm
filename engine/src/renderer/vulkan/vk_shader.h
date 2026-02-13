#pragma once

#include "defines.h"
#include "vk_types.h"

b8 vk_shader_init_compiler(VK_Context *context);
void vk_shader_destroy_compiler(VK_Context *context);

b8 vk_shader_module_compile(VK_Context *context, ASSET_ID asset_id);
void vk_shader_modules_destroy(VK_Context *context);
