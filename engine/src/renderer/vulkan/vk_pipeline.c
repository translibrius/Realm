#include "vk_pipeline.h"

#include "vk_shader.h"

b8 create_shader_stages(VK_Context *context);

b8 vk_pipeline_create(VK_Context *context) {
    if (!vk_shader_module_compile(context, "vulkan_triangle.vert")) {
        return false;
    }

    if (!vk_shader_module_compile(context, "vulkan_triangle.frag")) {
        return false;
    }

    create_shader_stages(context);
    return true;
}

void vk_pipeline_destroy(VK_Context *context) {
    vk_shader_modules_destroy(context);
}

b8 create_shader_stages(VK_Context *context) {
    u32 stage_count = context->shaders.count;

    VkPipelineShaderStageCreateInfo *stages =
        rl_arena_push(&context->arena,
                      sizeof(VkPipelineShaderStageCreateInfo) * stage_count,
                      alignof(VkPipelineShaderStageCreateInfo));

    for (u32 i = 0; i < stage_count; i++) {
        VK_Shader *shader = &context->shaders.items[i];

        VkShaderStageFlagBits flag;
        switch (shader->asset->type) {
        case SHADER_TYPE_VERTEX:
            flag = VK_SHADER_STAGE_VERTEX_BIT;
            break;
        case SHADER_TYPE_FRAGMENT:
            flag = VK_SHADER_STAGE_FRAGMENT_BIT;
            break;
        case SHADER_TYPE_COMPUTE:
            flag = VK_SHADER_STAGE_COMPUTE_BIT;
            break;
        default:
            RL_ERROR("Unsupported shader stage in pipeline");
            return false;
        }

        stages[i] = (VkPipelineShaderStageCreateInfo){
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = flag,
            .module = shader->module,
            .pName = "main",
        };
    }

    // Store result in context so pipeline creation can consume it
    context->pipeline.shader_stages = stages;
    context->pipeline.shader_stage_count = stage_count;
    return true;
}