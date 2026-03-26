#pragma once

#include "defines.h"
#include "vk_types.h"

typedef struct VK_PipelineConfig {
    VkPipelineShaderStageCreateInfo *stages;
    u32 stage_count;

    VkVertexInputBindingDescription *bindings;
    u32 binding_count;
    VkVertexInputAttributeDescription *attributes;
    u32 attribute_count;

    VkDescriptorSetLayout *set_layouts;
    u32 set_layout_count;
    VkPushConstantRange *push_constants;
    u32 push_constant_count;

    VkPrimitiveTopology topology; // 0 = default (TRIANGLE_LIST)
    b8 depth_test;
    b8 depth_write;
    VkCompareOp depth_compare_op; // 0 (VK_COMPARE_OP_NEVER) = default to VK_COMPARE_OP_LESS
    VkCullModeFlags cull_mode;
    b8 blend_enable;
    VkPolygonMode polygon_mode;
    VkSampleCountFlagBits msaa_samples;

    // Stencil (disabled by default)
    b8 stencil_test;
    VkStencilOpState stencil_op; // applied to front and back

    // Color write mask override
    b8 color_write_disable; // if true, mask off all color writes

    VkRenderPass render_pass;
    VkPipelineLayout existing_layout; // non-null = reuse this layout instead of creating one
} VK_PipelineConfig;

b8 vk_pipeline_create_graphics(VK_Context *ctx, VK_PipelineConfig *cfg, VkPipeline *out_pipeline, VkPipelineLayout *out_layout);

b8 vk_pipeline_create(VK_Context *context);
void vk_pipeline_destroy(VK_Context *context);
void vk_pipeline_layout_destroy(VK_Context *context);

b8 vk_unlit_pipeline_create(VK_Context *context);
void vk_unlit_pipeline_destroy(VK_Context *context);

b8 vk_overlay_pipeline_create(VK_Context *context);
void vk_overlay_pipeline_destroy(VK_Context *context);

b8 vk_wireframe_pipelines_create(VK_Context *context);
void vk_wireframe_pipelines_destroy(VK_Context *context);

b8 vk_grid_pipeline_create(VK_Context *context);
void vk_grid_pipeline_destroy(VK_Context *context);

b8 vk_line_pipeline_create(VK_Context *context);
void vk_line_pipeline_destroy(VK_Context *context);

