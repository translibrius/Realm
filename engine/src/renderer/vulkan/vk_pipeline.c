#include "vk_pipeline.h"

#include "vk_shader.h"
#include <vulkan/vulkan_core.h>

b8 create_shader_stages(VK_Context *context);
VkVertexInputBindingDescription vk_vertex_get_binding_desc();
void vk_vertex_get_attr_desc(VkVertexInputAttributeDescription *out_attrs);

b8 vk_pipeline_create_graphics(VK_Context *ctx, VK_PipelineConfig *cfg, VkPipeline *out_pipeline, VkPipelineLayout *out_layout) {
    VkDynamicState dynamic_states[2] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamic_state_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates = dynamic_states,
    };

    VkPipelineVertexInputStateCreateInfo vertex_input_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = cfg->binding_count,
        .pVertexBindingDescriptions = cfg->bindings,
        .vertexAttributeDescriptionCount = cfg->attribute_count,
        .pVertexAttributeDescriptions = cfg->attributes
    };

    VkPipelineInputAssemblyStateCreateInfo input_assembly_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE
    };

    VkPipelineViewportStateCreateInfo viewport_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };

    VkPipelineRasterizationStateCreateInfo rasterizer_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = cfg->cull_mode,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
        .lineWidth = 1.0f
    };

    VkPipelineMultisampleStateCreateInfo multisample_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable = VK_FALSE,
    };

    VkPipelineDepthStencilStateCreateInfo depth_stencil_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = cfg->depth_test ? VK_TRUE : VK_FALSE,
        .depthWriteEnable = cfg->depth_write ? VK_TRUE : VK_FALSE,
        .depthCompareOp = VK_COMPARE_OP_LESS,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE,
    };

    VkPipelineColorBlendAttachmentState blend_attachment = {
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };

    if (cfg->blend_enable) {
        blend_attachment.blendEnable = VK_TRUE;
        blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
        blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
    }

    VkPipelineColorBlendStateCreateInfo blend_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .attachmentCount = 1,
        .pAttachments = &blend_attachment,
    };

    VkPipelineLayoutCreateInfo layout_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = cfg->set_layout_count,
        .pSetLayouts = cfg->set_layouts,
        .pushConstantRangeCount = cfg->push_constant_count,
        .pPushConstantRanges = cfg->push_constants,
    };

    if (vkCreatePipelineLayout(ctx->device, &layout_ci, nullptr, out_layout) != VK_SUCCESS) {
        RL_ERROR("Failed to create pipeline layout");
        return false;
    }

    VkGraphicsPipelineCreateInfo pipeline_ci = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = cfg->stage_count,
        .pStages = cfg->stages,
        .pVertexInputState = &vertex_input_ci,
        .pInputAssemblyState = &input_assembly_ci,
        .pViewportState = &viewport_ci,
        .pRasterizationState = &rasterizer_ci,
        .pMultisampleState = &multisample_ci,
        .pDepthStencilState = &depth_stencil_ci,
        .pColorBlendState = &blend_ci,
        .pDynamicState = &dynamic_state_ci,
        .layout = *out_layout,
        .renderPass = cfg->render_pass,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1
    };

    if (vkCreateGraphicsPipelines(ctx->device, VK_NULL_HANDLE, 1, &pipeline_ci, nullptr, out_pipeline) != VK_SUCCESS) {
        RL_ERROR("Failed to create graphics pipeline");
        vkDestroyPipelineLayout(ctx->device, *out_layout, nullptr);
        return false;
    }

    return true;
}

b8 vk_pipeline_create(VK_Context *context) {
    if (!vk_shader_module_compile(context, ASSET_ID_SHADER_VULKAN_TRIANGLE_VERT)) {
        return false;
    }

    if (!vk_shader_module_compile(context, ASSET_ID_SHADER_VULKAN_TRIANGLE_FRAG)) {
        return false;
    }

    create_shader_stages(context);

    constexpr u32 attribute_desc_count = 3;
    VkVertexInputBindingDescription binding_description = vk_vertex_get_binding_desc();
    VkVertexInputAttributeDescription *attribute_descriptions = rl_arena_push(&context->arena, sizeof(VkVertexInputAttributeDescription) * attribute_desc_count, true);
    vk_vertex_get_attr_desc(attribute_descriptions);

    VkPushConstantRange push_range = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = sizeof(mat4),
    };

    VK_PipelineConfig cfg = {
        .stages = context->graphics_pipeline.shader_stages,
        .stage_count = context->graphics_pipeline.shader_stage_count,
        .bindings = &binding_description,
        .binding_count = 1,
        .attributes = attribute_descriptions,
        .attribute_count = attribute_desc_count,
        .set_layouts = &context->graphics_pipeline.descriptor_set_layout,
        .set_layout_count = 1,
        .push_constants = &push_range,
        .push_constant_count = 1,
        .depth_test = true,
        .depth_write = true,
        .cull_mode = VK_CULL_MODE_BACK_BIT,
        .render_pass = context->graphics_pipeline.render_pass,
    };

    b8 result = vk_pipeline_create_graphics(context, &cfg, &context->graphics_pipeline.handle, &context->graphics_pipeline.layout);

    vk_shader_modules_destroy(context);

    if (result) {
        RL_TRACE("Successfully created lit pipeline");
    }

    return result;
}

void vk_pipeline_destroy(VK_Context *context) {
    vkDestroyPipeline(context->device, context->graphics_pipeline.handle, nullptr);
    vkDestroyPipelineLayout(context->device, context->graphics_pipeline.layout, nullptr);
}

b8 vk_unlit_pipeline_create(VK_Context *context) {
    VkShaderModule vert_module, frag_module;

    if (!vk_shader_compile_to_module(context, ASSET_ID_SHADER_VULKAN_TRIANGLE_VERT, &vert_module)) {
        return false;
    }
    if (!vk_shader_compile_to_module(context, ASSET_ID_SHADER_VULKAN_LIGHT_FRAG, &frag_module)) {
        vkDestroyShaderModule(context->device, vert_module, nullptr);
        return false;
    }

    VkPipelineShaderStageCreateInfo stages[2] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vert_module,
            .pName = "main",
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = frag_module,
            .pName = "main",
        },
    };

    constexpr u32 attribute_desc_count = 3;
    VkVertexInputBindingDescription binding = vk_vertex_get_binding_desc();
    VkVertexInputAttributeDescription attrs[3];
    vk_vertex_get_attr_desc(attrs);

    VkPushConstantRange push_range = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = sizeof(mat4),
    };

    // Reuse the lit pipeline's layout — same descriptor set layout and push constant range.
    // We only need to create the pipeline object itself.
    VkDynamicState dynamic_states[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamic_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates = dynamic_states,
    };
    VkPipelineVertexInputStateCreateInfo vi_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &binding,
        .vertexAttributeDescriptionCount = attribute_desc_count,
        .pVertexAttributeDescriptions = attrs,
    };
    VkPipelineInputAssemblyStateCreateInfo ia_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    };
    VkPipelineViewportStateCreateInfo vp_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };
    VkPipelineRasterizationStateCreateInfo rs_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .lineWidth = 1.0f,
    };
    VkPipelineMultisampleStateCreateInfo ms_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };
    VkPipelineDepthStencilStateCreateInfo ds_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS,
    };
    VkPipelineColorBlendAttachmentState blend_att = {
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };
    VkPipelineColorBlendStateCreateInfo cb_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &blend_att,
    };

    // Create a temporary layout identical to the lit pipeline's (same descriptor + push constant).
    // We could share the layout handle, but vk_pipeline_create_graphics owns the layout lifetime
    // for the lit pipeline, so we create our own here to keep destroy paths simple.
    VkPipelineLayoutCreateInfo layout_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &context->graphics_pipeline.descriptor_set_layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push_range,
    };

    // We reuse the lit pipeline layout directly — the layouts are compatible.
    VkGraphicsPipelineCreateInfo pipeline_ci = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = stages,
        .pVertexInputState = &vi_ci,
        .pInputAssemblyState = &ia_ci,
        .pViewportState = &vp_ci,
        .pRasterizationState = &rs_ci,
        .pMultisampleState = &ms_ci,
        .pDepthStencilState = &ds_ci,
        .pColorBlendState = &cb_ci,
        .pDynamicState = &dynamic_ci,
        .layout = context->graphics_pipeline.layout,
        .renderPass = context->graphics_pipeline.render_pass,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1,
    };

    (void)layout_ci; // layout reused from lit pipeline

    VkResult vk_result = vkCreateGraphicsPipelines(context->device, VK_NULL_HANDLE, 1, &pipeline_ci, nullptr, &context->unlit_pipeline);

    vkDestroyShaderModule(context->device, vert_module, nullptr);
    vkDestroyShaderModule(context->device, frag_module, nullptr);

    if (vk_result != VK_SUCCESS) {
        RL_ERROR("Failed to create unlit pipeline");
        return false;
    }

    RL_TRACE("Successfully created unlit pipeline");
    return true;
}

void vk_unlit_pipeline_destroy(VK_Context *context) {
    vkDestroyPipeline(context->device, context->unlit_pipeline, nullptr);
}

// Private

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
    context->graphics_pipeline.shader_stages = stages;
    context->graphics_pipeline.shader_stage_count = stage_count;
    return true;
}

VkVertexInputBindingDescription vk_vertex_get_binding_desc() {
    return (VkVertexInputBindingDescription){
        .binding = 0,
        .stride = sizeof(vertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };
}

void vk_vertex_get_attr_desc(VkVertexInputAttributeDescription *out_attrs) {
    out_attrs[0].binding = 0;
    out_attrs[0].location = 0;
    out_attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    out_attrs[0].offset = offsetof(vertex, pos);

    out_attrs[1].binding = 0;
    out_attrs[1].location = 1;
    out_attrs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    out_attrs[1].offset = offsetof(vertex, normal);

    out_attrs[2].binding = 0;
    out_attrs[2].location = 2;
    out_attrs[2].format  = VK_FORMAT_R32G32_SFLOAT;
    out_attrs[2].offset = offsetof(vertex, tex_coord);
}
