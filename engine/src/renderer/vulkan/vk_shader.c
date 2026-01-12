#include "renderer/vulkan/vk_shader.h"

#include "asset/shader.h"

#include <shaderc/shaderc.h>

b8 vk_shader_init_compiler(VK_Context *context) {
    if (context->shader_compiler.initialized) {
        return true;
    }

    context->shader_compiler.compiler = shaderc_compiler_initialize();
    context->shader_compiler.options = shaderc_compile_options_initialize();

    if (!context->shader_compiler.compiler || !context->shader_compiler.options) {
        RL_ERROR("Failed to initialize shader compiler");
        return false;
    }

    shaderc_compile_options_set_optimization_level(context->shader_compiler.options, shaderc_optimization_level_performance);

    da_init_with_cap(&context->shaders, 10);

    context->shader_compiler.initialized = true;
    RL_DEBUG("Spir-V Shader compiler initialized");
    return true;
}

void vk_shader_destroy_compiler(VK_Context *context) {
    if (!context->shader_compiler.initialized)
        return;

    shaderc_compile_options_release(context->shader_compiler.options);
    shaderc_compiler_release(context->shader_compiler.compiler);

    context->shader_compiler.compiler = nullptr;
    context->shader_compiler.options = nullptr;
    context->shader_compiler.initialized = false;

    RL_DEBUG("Shader compiler destroyed");
}

b8 vk_shader_compile(VK_Context *context, const char *filename) {
    if (!context->shader_compiler.initialized) {
        RL_ERROR("Shader compiler not initialized before compile!");
        return false;
    }

    rl_asset_shader *asset_shader = get_asset(filename)->handle;

    shaderc_shader_kind kind;
    switch (asset_shader->type) {
    case SHADER_TYPE_VERTEX:
        kind = shaderc_glsl_vertex_shader;
        break;
    case SHADER_TYPE_FRAGMENT:
        kind = shaderc_glsl_fragment_shader;
        break;
    case SHADER_TYPE_COMPUTE:
        kind = shaderc_glsl_compute_shader;
        break;
    default:
        RL_ERROR("Unknown shader type for '%s'", filename);
        return false;
    }

    shaderc_compilation_result_t result = shaderc_compile_into_spv(
        context->shader_compiler.compiler,
        asset_shader->source,
        cstr_len(asset_shader->source),
        kind,
        filename,
        "main",
        context->shader_compiler.options);

    if (shaderc_result_get_compilation_status(result) != shaderc_compilation_status_success) {
        RL_ERROR("Shader compile failed for '%s':\n%s",
                 filename,
                 shaderc_result_get_error_message(result));
        shaderc_result_release(result);
        return false;
    }

    size_t codeSize = shaderc_result_get_length(result);
    const uint32_t *codePtr = (const uint32_t *)shaderc_result_get_bytes(result);

    VkShaderModuleCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = codeSize,
        .pCode = codePtr
    };

    VkShaderModule *module = rl_arena_push(&context->arena, sizeof(VkShaderModule), alignof(VkShaderModule));
    if (vkCreateShaderModule(context->device, &createInfo, NULL, module) != VK_SUCCESS) {
        RL_ERROR("Failed to create VkShaderModule for '%s'", filename);
        shaderc_result_release(result);
        return false;
    }

    da_append(&context->shaders, *module);

    shaderc_result_release(result);
    return true;
}