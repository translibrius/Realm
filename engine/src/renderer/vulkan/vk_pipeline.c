#include "vk_pipeline.h"

#include "vk_shader.h"

b8 create_shader_stages(VK_Context *context);
VkVertexInputBindingDescription vk_vertex_get_binding_desc();
void vk_vertex_get_attr_desc(VkVertexInputAttributeDescription *out_attrs);

b8 vk_pipeline_create(VK_Context *context) {
    if (!vk_shader_module_compile(context, "vulkan_triangle.vert")) {
        return false;
    }

    if (!vk_shader_module_compile(context, "vulkan_triangle.frag")) {
        return false;
    }

    create_shader_stages(context);

    // Dynamic state
    VkDynamicState dynamic_states[2] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamic_state_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates = dynamic_states,
    };

    constexpr u32 attribute_desc_count = 3;
    VkVertexInputBindingDescription binding_description = vk_vertex_get_binding_desc();
    VkVertexInputAttributeDescription *attribute_descriptions = rl_arena_push(&context->arena, sizeof(VkVertexInputAttributeDescription) * attribute_desc_count, true);
    vk_vertex_get_attr_desc(attribute_descriptions);

    // Vertex input
    VkPipelineVertexInputStateCreateInfo vertex_input_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &binding_description,
        .vertexAttributeDescriptionCount = attribute_desc_count,
        .pVertexAttributeDescriptions = attribute_descriptions
    };

    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo input_assembly_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE
    };

    VkPipelineViewportStateCreateInfo viewport_state_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };

    // Rasterizer
    VkPipelineRasterizationStateCreateInfo rasterization_state_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        // If depthClampEnable is set to VK_TRUE
        // then fragments that are beyond the near and far planes are clamped to them as opposed to discarding them.
        .depthClampEnable = VK_FALSE, //
        .rasterizerDiscardEnable = VK_FALSE, // VK_TRUE disables rasterizer
        .polygonMode = VK_POLYGON_MODE_FILL, // VK_POLYGON_MODE_LINE (Edges as lines) | VK_POLYGON_MODE_POINT
        .cullMode = VK_CULL_MODE_BACK_BIT, // Cull back faces
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE, // C-Clockwise order
        .depthBiasEnable = VK_FALSE, // Can be used for shadow mapping
        .depthBiasConstantFactor = 0.0f, // Optional
        .depthBiasClamp = 0.0f, // Optional
        .depthBiasSlopeFactor = 0.0f, // Optional
        .lineWidth = 1.0f // Width in fragments (>1.0 requires wideLines GPU feature
    };

    // Multisampling (one of the ways to perform anti-aliasing)
    VkPipelineMultisampleStateCreateInfo multisample_state_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable = VK_FALSE,
        .minSampleShading = 1.0f, // Optional
        .pSampleMask = nullptr, // Optional
        .alphaToCoverageEnable = VK_FALSE, // Optional
        .alphaToOneEnable = VK_FALSE // Optional
    };

    // TODO: Depth and stencil testing (VkPipelineDepthStencilStateCreateInfo)

    // Color blending
    VkPipelineColorBlendAttachmentState color_blend_attachment_state = {
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        .blendEnable = VK_FALSE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_ONE, // Optional
        .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO, // Optional
        .colorBlendOp = VK_BLEND_OP_ADD, // Optional
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE, // Optional
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO, // Optional
        .alphaBlendOp = VK_BLEND_OP_ADD, // Optional
    };

    VkPipelineColorBlendStateCreateInfo color_blend_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY, // Optional
        .attachmentCount = 1,
        .pAttachments = &color_blend_attachment_state,
        .blendConstants[0] = 0.0f, // Optional
        .blendConstants[1] = 0.0f, // Optional
        .blendConstants[2] = 0.0f, // Optional
        .blendConstants[3] = 0.0f, // Optional
    };

    // Pipeline layout
    VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1, // Optional
        .pSetLayouts = &context->graphics_pipeline.descriptor_set_layout, // Optional
        .pushConstantRangeCount = 0, // Optional
        .pPushConstantRanges = nullptr, // Optional
    };

    if (vkCreatePipelineLayout(context->device, &pipeline_layout_create_info, nullptr, &context->graphics_pipeline.layout) != VK_SUCCESS) {
        RL_ERROR("Failed to create pipeline layout");
        vk_shader_modules_destroy(context);
        return false;
    }

    VkGraphicsPipelineCreateInfo pipeline_create_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = context->graphics_pipeline.shader_stage_count,
        .pStages = context->graphics_pipeline.shader_stages,
        .pVertexInputState = &vertex_input_create_info,
        .pInputAssemblyState = &input_assembly_create_info,
        .pTessellationState = nullptr, // Optional
        .pViewportState = &viewport_state_create_info,
        .pRasterizationState = &rasterization_state_create_info,
        .pMultisampleState = &multisample_state_create_info,
        .pDepthStencilState = nullptr, // Optional
        .pColorBlendState = &color_blend_create_info,
        .pDynamicState = &dynamic_state_create_info,
        .layout = context->graphics_pipeline.layout,
        .renderPass = context->graphics_pipeline.render_pass,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE, // Optional
        .basePipelineIndex = -1 // Optional
    };

    if (VK_SUCCESS != vkCreateGraphicsPipelines(context->device, VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr, &context->graphics_pipeline.handle)) {
        RL_ERROR("Failed to create graphics pipeline");
        vk_shader_modules_destroy(context);
        return false;
    }

    RL_TRACE("Successfully created pipeline");
    vk_shader_modules_destroy(context);
    return true;
}

void vk_pipeline_destroy(VK_Context *context) {
    vkDestroyPipeline(context->device, context->graphics_pipeline.handle, nullptr);
    vkDestroyPipelineLayout(context->device, context->graphics_pipeline.layout, nullptr);
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
    out_attrs[0].format = VK_FORMAT_R32G32_SFLOAT;
    out_attrs[0].offset = offsetof(vertex, pos);

    out_attrs[1].binding = 0;
    out_attrs[1].location = 1;
    out_attrs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    out_attrs[1].offset = offsetof(vertex, color);

    out_attrs[2].binding = 0;
    out_attrs[2].location = 2;
    out_attrs[2].format  = VK_FORMAT_R32G32_SFLOAT;
    out_attrs[2].offset = offsetof(vertex, tex_coord);
}