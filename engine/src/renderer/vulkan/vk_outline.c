#include "vk_outline.h"

#include "asset/asset.h"
#include "asset/model.h"
#include "core/logger.h"
#include "memory/memory.h"
#include "vk_buffer.h"
#include "vk_descriptor.h"
#include "vk_image.h"
#include "vk_mesh.h"
#include "vk_pipeline.h"
#include "vk_shader.h"
#include "vk_util.h"
#include "renderer/renderer_types.h"
#include <math.h>

// Shorthand for the anonymous outline struct member
#define OL ctx->outline

// -------------------------------------------------------------------------
// Offscreen render pass creation
// -------------------------------------------------------------------------

static b8 create_mask_render_pass(VK_Context *ctx) {
    VkAttachmentDescription attachments[2] = {
        {
            .format = VK_FORMAT_R8G8B8A8_UNORM,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        },
        {
            .format = VK_FORMAT_D32_SFLOAT,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        },
    };

    VkAttachmentReference color_ref = {.attachment = 0, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference depth_ref = {.attachment = 1, .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_ref,
        .pDepthStencilAttachment = &depth_ref,
    };

    VkSubpassDependency deps[2] = {
        {
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            .srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        },
        {
            .srcSubpass = 0,
            .dstSubpass = VK_SUBPASS_EXTERNAL,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        },
    };

    VkRenderPassCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 2,
        .pAttachments = attachments,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 2,
        .pDependencies = deps,
    };

    VK_CHECK_RETURN_FALSE(vkCreateRenderPass(ctx->device, &ci, nullptr, &OL.mask_render_pass),
                          "Failed to create mask render pass");
    return true;
}

static b8 create_jfa_render_pass(VK_Context *ctx) {
    VkAttachmentDescription attachment = {
        .format = VK_FORMAT_R16G16B16A16_SFLOAT,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };

    VkAttachmentReference color_ref = {.attachment = 0, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_ref,
    };

    VkSubpassDependency deps[2] = {
        {
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        },
        {
            .srcSubpass = 0,
            .dstSubpass = VK_SUBPASS_EXTERNAL,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        },
    };

    VkRenderPassCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &attachment,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 2,
        .pDependencies = deps,
    };

    VK_CHECK_RETURN_FALSE(vkCreateRenderPass(ctx->device, &ci, nullptr, &OL.jfa_render_pass),
                          "Failed to create JFA render pass");
    return true;
}

// -------------------------------------------------------------------------
// Offscreen RT helpers
// -------------------------------------------------------------------------

typedef struct { VkImage image; VkDeviceMemory memory; VkImageView view; VkFramebuffer framebuffer; u32 width, height; VkFormat format; } VK_ORT;

static b8 create_ort(VK_Context *ctx, VK_ORT *rt, u32 w, u32 h, VkFormat format,
                      VkRenderPass render_pass, VkImageView extra_attachment) {
    rt->width = w;
    rt->height = h;
    rt->format = format;

    if (!vk_image_create(ctx, w, h, format, VK_IMAGE_TILING_OPTIMAL,
                          VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SAMPLE_COUNT_1_BIT,
                          &rt->image, &rt->memory)) {
        return false;
    }
    if (!vk_image_view_create(ctx, VK_IMAGE_ASPECT_COLOR_BIT, rt->image, format, &rt->view)) {
        return false;
    }

    VkImageView views[2];
    u32 view_count;
    if (extra_attachment) {
        views[0] = rt->view;
        views[1] = extra_attachment;
        view_count = 2;
    } else {
        views[0] = rt->view;
        view_count = 1;
    }

    VkFramebufferCreateInfo fb_ci = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = render_pass,
        .attachmentCount = view_count,
        .pAttachments = views,
        .width = w,
        .height = h,
        .layers = 1,
    };

    VK_CHECK_RETURN_FALSE(vkCreateFramebuffer(ctx->device, &fb_ci, nullptr, &rt->framebuffer),
                          "Failed to create outline framebuffer");
    return true;
}

static void destroy_ort(VK_Context *ctx, VK_ORT *rt) {
    if (rt->framebuffer) { vkDestroyFramebuffer(ctx->device, rt->framebuffer, nullptr); rt->framebuffer = VK_NULL_HANDLE; }
    if (rt->view)        { vkDestroyImageView(ctx->device, rt->view, nullptr);          rt->view = VK_NULL_HANDLE; }
    if (rt->image)       { vkDestroyImage(ctx->device, rt->image, nullptr);              rt->image = VK_NULL_HANDLE; }
    if (rt->memory)      { vkFreeMemory(ctx->device, rt->memory, nullptr);               rt->memory = VK_NULL_HANDLE; }
}

// -------------------------------------------------------------------------
// Pipelines
// -------------------------------------------------------------------------

static b8 create_mask_pipeline(VK_Context *ctx) {
    VkShaderModule vert, frag;
    if (!vk_shader_compile_to_module(ctx, asset_find(RL_ASSET_SHADER_VK_OUTLINE_MASK_VERT), &vert)) return false;
    if (!vk_shader_compile_to_module(ctx, asset_find(RL_ASSET_SHADER_VK_OUTLINE_MASK_FRAG), &frag)) {
        vkDestroyShaderModule(ctx->device, vert, nullptr);
        return false;
    }

    VkPipelineShaderStageCreateInfo stages[2] = {
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = vert, .pName = "main"},
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = frag, .pName = "main"},
    };

    VkVertexInputBindingDescription binding = {.binding = 0, .stride = sizeof(vertex), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription attrs[3] = {
        {.binding = 0, .location = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(vertex, pos)},
        {.binding = 0, .location = 1, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(vertex, normal)},
        {.binding = 0, .location = 2, .format = VK_FORMAT_R32G32_SFLOAT,    .offset = offsetof(vertex, tex_coord)},
    };

    VK_PipelineConfig cfg = {
        .stages = stages, .stage_count = 2,
        .bindings = &binding, .binding_count = 1,
        .attributes = attrs, .attribute_count = 3,
        .depth_test = true, .depth_write = true,
        .cull_mode = VK_CULL_MODE_NONE,
        .polygon_mode = VK_POLYGON_MODE_FILL,
        .msaa_samples = VK_SAMPLE_COUNT_1_BIT,
        .render_pass = OL.mask_render_pass,
        .existing_layout = ctx->pipeline_layout,
    };

    VkPipelineLayout reused;
    b8 ok = vk_pipeline_create_graphics(ctx, &cfg, &OL.mask_pipeline, &reused);
    vkDestroyShaderModule(ctx->device, vert, nullptr);
    vkDestroyShaderModule(ctx->device, frag, nullptr);
    return ok;
}

static b8 create_fullscreen_pipeline(VK_Context *ctx, const char *vert_path, const char *frag_path,
                                      VkRenderPass render_pass, b8 blend, VkPipelineLayout layout_override,
                                      VkPipeline *out) {
    VkShaderModule vert, frag;
    if (!vk_shader_compile_to_module(ctx, asset_find(vert_path), &vert)) return false;
    if (!vk_shader_compile_to_module(ctx, asset_find(frag_path), &frag)) {
        vkDestroyShaderModule(ctx->device, vert, nullptr);
        return false;
    }

    VkPipelineShaderStageCreateInfo stages[2] = {
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = vert, .pName = "main"},
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = frag, .pName = "main"},
    };

    VK_PipelineConfig cfg = {
        .stages = stages, .stage_count = 2,
        .binding_count = 0, .attribute_count = 0,
        .depth_test = false, .depth_write = false,
        .cull_mode = VK_CULL_MODE_NONE,
        .blend_enable = blend,
        .polygon_mode = VK_POLYGON_MODE_FILL,
        .msaa_samples = (render_pass == ctx->render_pass) ? ctx->msaa_samples : VK_SAMPLE_COUNT_1_BIT,
        .render_pass = render_pass,
        .existing_layout = layout_override ? layout_override : ctx->pipeline_layout,
    };

    VkPipelineLayout reused;
    b8 ok = vk_pipeline_create_graphics(ctx, &cfg, out, &reused);
    vkDestroyShaderModule(ctx->device, vert, nullptr);
    vkDestroyShaderModule(ctx->device, frag, nullptr);
    return ok;
}

// -------------------------------------------------------------------------
// Descriptors
// -------------------------------------------------------------------------

static b8 create_outline_descriptors(VK_Context *ctx) {
    // 6 UBOs for mask/jfa per-frame sets, 10 samplers (6 existing + 4 for composite), 8 total sets
    VkDescriptorPoolSize sizes[2] = {
        {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 6},
        {.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 10},
    };
    if (!vk_descriptor_pool_create(ctx, sizes, 2, 8, &OL.descriptor_pool)) return false;

    if (!vk_descriptor_sets_allocate(ctx, OL.descriptor_pool, ctx->descriptor_set_layout, 2, OL.mask_ds)) return false;
    if (!vk_descriptor_sets_allocate(ctx, OL.descriptor_pool, ctx->descriptor_set_layout, 2, OL.jfa_a_ds)) return false;
    if (!vk_descriptor_sets_allocate(ctx, OL.descriptor_pool, ctx->descriptor_set_layout, 2, OL.jfa_b_ds)) return false;
    return true;
}

static b8 create_composite_descriptors(VK_Context *ctx) {
    // Composite-specific layout: 2 combined image samplers (JFA result + mask)
    VkDescriptorSetLayoutBinding bindings[2] = {
        {.binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
         .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT},
        {.binding = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
         .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT},
    };
    VkDescriptorSetLayoutCreateInfo layout_ci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 2, .pBindings = bindings,
    };
    VK_CHECK_RETURN_FALSE(vkCreateDescriptorSetLayout(ctx->device, &layout_ci, nullptr, &OL.composite_ds_layout),
                          "Failed to create composite DS layout");

    VkPushConstantRange push = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset = 0, .size = sizeof(VK_MeshPushConstants),
    };
    VkPipelineLayoutCreateInfo pl_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1, .pSetLayouts = &OL.composite_ds_layout,
        .pushConstantRangeCount = 1, .pPushConstantRanges = &push,
    };
    VK_CHECK_RETURN_FALSE(vkCreatePipelineLayout(ctx->device, &pl_ci, nullptr, &OL.composite_pipeline_layout),
                          "Failed to create composite pipeline layout");

    // Allocate 2 descriptor sets from the outline pool using the composite layout
    VkDescriptorSet sets[2];
    if (!vk_descriptor_sets_allocate(ctx, OL.descriptor_pool, OL.composite_ds_layout, 2, sets)) return false;
    OL.composite_from_a_ds = sets[0];
    OL.composite_from_b_ds = sets[1];

    return true;
}

static void update_composite_descriptors(VK_Context *ctx) {
    VkDescriptorImageInfo jfa_a_info = {.sampler = OL.sampler, .imageView = OL.jfa_a.view, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkDescriptorImageInfo jfa_b_info = {.sampler = OL.sampler, .imageView = OL.jfa_b.view, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkDescriptorImageInfo mask_info  = {.sampler = OL.sampler, .imageView = OL.mask_rt.view, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

    // composite_from_a: binding 0 = jfa_a, binding 1 = mask
    VkWriteDescriptorSet writes_a[2] = {
        {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = OL.composite_from_a_ds, .dstBinding = 0,
         .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &jfa_a_info},
        {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = OL.composite_from_a_ds, .dstBinding = 1,
         .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &mask_info},
    };
    vkUpdateDescriptorSets(ctx->device, 2, writes_a, 0, nullptr);

    // composite_from_b: binding 0 = jfa_b, binding 1 = mask
    VkWriteDescriptorSet writes_b[2] = {
        {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = OL.composite_from_b_ds, .dstBinding = 0,
         .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &jfa_b_info},
        {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = OL.composite_from_b_ds, .dstBinding = 1,
         .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &mask_info},
    };
    vkUpdateDescriptorSets(ctx->device, 2, writes_b, 0, nullptr);
}

static void update_rt_descriptor(VK_Context *ctx, VkDescriptorSet *ds, VkImageView view) {
    for (u32 i = 0; i < ctx->max_frames_in_flight; i++) {
        VkDescriptorBufferInfo buf_info = {.buffer = ctx->uniform_buffers[i], .offset = 0, .range = sizeof(ubo)};
        VkDescriptorImageInfo img_info = {.sampler = OL.sampler, .imageView = view, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkWriteDescriptorSet writes[2] = {
            {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = ds[i], .dstBinding = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .pBufferInfo = &buf_info},
            {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = ds[i], .dstBinding = 1, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &img_info},
        };
        vkUpdateDescriptorSets(ctx->device, 2, writes, 0, nullptr);
    }
}

// -------------------------------------------------------------------------
// Public API
// -------------------------------------------------------------------------

b8 vk_outline_init(VK_Context *ctx) {
    mem_zero(&OL, sizeof(OL));

    u32 w = ctx->swapchain.chosen_extent.width;
    u32 h = ctx->swapchain.chosen_extent.height;

    // Sampler
    VkSamplerCreateInfo sampler_ci = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_NEAREST,
        .minFilter = VK_FILTER_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .maxAnisotropy = 1.0f,
        .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
    };
    VK_CHECK_RETURN_FALSE(vkCreateSampler(ctx->device, &sampler_ci, nullptr, &OL.sampler), "Failed to create outline sampler");

    if (!create_mask_render_pass(ctx)) return false;
    if (!create_jfa_render_pass(ctx)) return false;

    // Mask depth
    if (!vk_image_create(ctx, w, h, VK_FORMAT_D32_SFLOAT, VK_IMAGE_TILING_OPTIMAL,
                          VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                          VK_SAMPLE_COUNT_1_BIT, &OL.mask_depth_image, &OL.mask_depth_memory))
        return false;
    if (!vk_image_view_create(ctx, VK_IMAGE_ASPECT_DEPTH_BIT, OL.mask_depth_image, VK_FORMAT_D32_SFLOAT, &OL.mask_depth_view))
        return false;

    // RTs (cast to VK_ORT* — same layout as the anonymous struct members)
    if (!create_ort(ctx, (VK_ORT *)&OL.mask_rt, w, h, VK_FORMAT_R8G8B8A8_UNORM, OL.mask_render_pass, OL.mask_depth_view)) return false;
    if (!create_ort(ctx, (VK_ORT *)&OL.jfa_a, w, h, VK_FORMAT_R16G16B16A16_SFLOAT, OL.jfa_render_pass, VK_NULL_HANDLE)) return false;
    if (!create_ort(ctx, (VK_ORT *)&OL.jfa_b, w, h, VK_FORMAT_R16G16B16A16_SFLOAT, OL.jfa_render_pass, VK_NULL_HANDLE)) return false;

    // Descriptors
    if (!create_outline_descriptors(ctx)) return false;
    if (!create_composite_descriptors(ctx)) return false;
    update_rt_descriptor(ctx, OL.mask_ds, OL.mask_rt.view);
    update_rt_descriptor(ctx, OL.jfa_a_ds, OL.jfa_a.view);
    update_rt_descriptor(ctx, OL.jfa_b_ds, OL.jfa_b.view);
    update_composite_descriptors(ctx);

    // Pipelines
    if (!create_mask_pipeline(ctx)) return false;
    if (!create_fullscreen_pipeline(ctx, RL_ASSET_SHADER_VK_JFA_INIT_VERT, RL_ASSET_SHADER_VK_JFA_INIT_FRAG, OL.jfa_render_pass, false, VK_NULL_HANDLE, &OL.jfa_init_pipeline)) return false;
    if (!create_fullscreen_pipeline(ctx, RL_ASSET_SHADER_VK_JFA_STEP_VERT, RL_ASSET_SHADER_VK_JFA_STEP_FRAG, OL.jfa_render_pass, false, VK_NULL_HANDLE, &OL.jfa_step_pipeline)) return false;
    if (!create_fullscreen_pipeline(ctx, RL_ASSET_SHADER_VK_OUTLINE_COMP_VERT, RL_ASSET_SHADER_VK_OUTLINE_COMP_FRAG, ctx->render_pass, true, OL.composite_pipeline_layout, &OL.composite_pipeline)) return false;

    OL.ready = true;
    RL_TRACE("Outline (JFA) pipeline initialized");
    return true;
}

void vk_outline_destroy(VK_Context *ctx) {
    if (!OL.ready) return;

    if (OL.composite_pipeline)        vkDestroyPipeline(ctx->device, OL.composite_pipeline, nullptr);
    if (OL.jfa_step_pipeline)         vkDestroyPipeline(ctx->device, OL.jfa_step_pipeline, nullptr);
    if (OL.jfa_init_pipeline)         vkDestroyPipeline(ctx->device, OL.jfa_init_pipeline, nullptr);
    if (OL.mask_pipeline)             vkDestroyPipeline(ctx->device, OL.mask_pipeline, nullptr);
    if (OL.composite_pipeline_layout) vkDestroyPipelineLayout(ctx->device, OL.composite_pipeline_layout, nullptr);
    if (OL.composite_ds_layout)       vkDestroyDescriptorSetLayout(ctx->device, OL.composite_ds_layout, nullptr);
    if (OL.descriptor_pool)           vkDestroyDescriptorPool(ctx->device, OL.descriptor_pool, nullptr);

    destroy_ort(ctx, (VK_ORT *)&OL.jfa_b);
    destroy_ort(ctx, (VK_ORT *)&OL.jfa_a);
    destroy_ort(ctx, (VK_ORT *)&OL.mask_rt);

    if (OL.mask_depth_view)   vkDestroyImageView(ctx->device, OL.mask_depth_view, nullptr);
    if (OL.mask_depth_image)  vkDestroyImage(ctx->device, OL.mask_depth_image, nullptr);
    if (OL.mask_depth_memory) vkFreeMemory(ctx->device, OL.mask_depth_memory, nullptr);

    if (OL.jfa_render_pass)   vkDestroyRenderPass(ctx->device, OL.jfa_render_pass, nullptr);
    if (OL.mask_render_pass)  vkDestroyRenderPass(ctx->device, OL.mask_render_pass, nullptr);
    if (OL.sampler)           vkDestroySampler(ctx->device, OL.sampler, nullptr);

    mem_zero(&OL, sizeof(OL));
}

void vk_outline_resize(VK_Context *ctx, u32 w, u32 h) {
    if (!OL.ready) return;
    if (w == OL.mask_rt.width && h == OL.mask_rt.height) return;

    vkDeviceWaitIdle(ctx->device);

    destroy_ort(ctx, (VK_ORT *)&OL.jfa_b);
    destroy_ort(ctx, (VK_ORT *)&OL.jfa_a);
    destroy_ort(ctx, (VK_ORT *)&OL.mask_rt);

    if (OL.mask_depth_view)   { vkDestroyImageView(ctx->device, OL.mask_depth_view, nullptr);   OL.mask_depth_view = VK_NULL_HANDLE; }
    if (OL.mask_depth_image)  { vkDestroyImage(ctx->device, OL.mask_depth_image, nullptr);       OL.mask_depth_image = VK_NULL_HANDLE; }
    if (OL.mask_depth_memory) { vkFreeMemory(ctx->device, OL.mask_depth_memory, nullptr);        OL.mask_depth_memory = VK_NULL_HANDLE; }

    vk_image_create(ctx, w, h, VK_FORMAT_D32_SFLOAT, VK_IMAGE_TILING_OPTIMAL,
                     VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                     VK_SAMPLE_COUNT_1_BIT, &OL.mask_depth_image, &OL.mask_depth_memory);
    vk_image_view_create(ctx, VK_IMAGE_ASPECT_DEPTH_BIT, OL.mask_depth_image, VK_FORMAT_D32_SFLOAT, &OL.mask_depth_view);

    create_ort(ctx, (VK_ORT *)&OL.mask_rt, w, h, VK_FORMAT_R8G8B8A8_UNORM, OL.mask_render_pass, OL.mask_depth_view);
    create_ort(ctx, (VK_ORT *)&OL.jfa_a, w, h, VK_FORMAT_R16G16B16A16_SFLOAT, OL.jfa_render_pass, VK_NULL_HANDLE);
    create_ort(ctx, (VK_ORT *)&OL.jfa_b, w, h, VK_FORMAT_R16G16B16A16_SFLOAT, OL.jfa_render_pass, VK_NULL_HANDLE);

    update_rt_descriptor(ctx, OL.mask_ds, OL.mask_rt.view);
    update_rt_descriptor(ctx, OL.jfa_a_ds, OL.jfa_a.view);
    update_rt_descriptor(ctx, OL.jfa_b_ds, OL.jfa_b.view);
    update_composite_descriptors(ctx);
}

// -------------------------------------------------------------------------
// Command recording — offscreen passes (before main render pass)
// -------------------------------------------------------------------------

void vk_outline_record_offscreen(VK_Context *ctx, VkCommandBuffer cmd) {
    if (!OL.ready || OL.outline_count == 0 || !OL.outlines) return;

    // Use viewport dimensions (matching the scene projection's aspect ratio),
    // falling back to swapchain size when no viewport rect is specified.
    rl_viewport_rect vr = ctx->scene_viewport;
    u32 rt_w = (vr.w > 0 && vr.h > 0) ? (u32)vr.w : ctx->swapchain.chosen_extent.width;
    u32 rt_h = (vr.w > 0 && vr.h > 0) ? (u32)vr.h : ctx->swapchain.chosen_extent.height;
    vk_outline_resize(ctx, rt_w, rt_h);

    u32 w = OL.mask_rt.width;
    u32 h = OL.mask_rt.height;

    // Barrier: ensure any previous frame's outline reads/writes are complete before
    // we start writing. The RTs are shared between frames-in-flight.
    // Use per-image barriers so MoltenVK can emit correct Metal resource hazard tracking.
    VkImageSubresourceRange color_range = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1};
    VkImageMemoryBarrier img_barriers[3] = {
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = OL.mask_rt.image,
            .subresourceRange = color_range,
        },
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = OL.jfa_a.image,
            .subresourceRange = color_range,
        },
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = OL.jfa_b.image,
            .subresourceRange = color_range,
        },
    };
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        0, 0, nullptr, 0, nullptr, 3, img_barriers);

    // === 1. Mask pass ===
    {
        VkClearValue clears[2] = {{.color = {{0, 0, 0, 0}}}, {.depthStencil = {1.0f, 0}}};
        VkRenderPassBeginInfo rp = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = OL.mask_render_pass,
            .framebuffer = OL.mask_rt.framebuffer,
            .renderArea = {.extent = {w, h}},
            .clearValueCount = 2, .pClearValues = clears,
        };
        vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport vp = {.width = (f32)w, .height = (f32)h, .maxDepth = 1};
        VkRect2D sc = {.extent = {w, h}};
        vkCmdSetViewport(cmd, 0, 1, &vp);
        vkCmdSetScissor(cmd, 0, 1, &sc);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, OL.mask_pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx->pipeline_layout,
                                 0, 1, &ctx->descriptor_sets[ctx->current_frame], 0, nullptr);

        for (u32 oi = 0; oi < OL.outline_count; oi++) {
            rl_frame_outline *ol = &OL.outlines[oi];
            for (u32 mi = 0; mi < ctx->frame_mesh_count; mi++) {
                rl_frame_mesh *fm = &ctx->frame_meshes[mi];
                if (fm->source_entity != ol->entity) continue;

                VK_MeshPushConstants pc = {0};
                glm_mat4_copy(fm->model, pc.model);
                vkCmdPushConstants(cmd, ctx->pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);

                VkDeviceSize off = 0;
                if (fm->model_asset) {
                    i32 idx = vk_model_cache_find(ctx, fm->model_asset);
                    if (idx < 0) continue;
                    if (fm->mesh_index >= ctx->model_cache[idx].mesh_count) continue;
                    VK_CachedMesh *cm = &ctx->model_cache[idx].meshes[fm->mesh_index];
                    vkCmdBindVertexBuffers(cmd, 0, 1, &cm->vertex_buffer, &off);
                    if (cm->index_count > 0) {
                        vkCmdBindIndexBuffer(cmd, cm->index_buffer, 0, VK_INDEX_TYPE_UINT32);
                        vkCmdDrawIndexed(cmd, cm->index_count, 1, 0, 0, 0);
                    } else {
                        vkCmdDraw(cmd, cm->vertex_count, 1, 0, 0);
                    }
                } else {
                    vkCmdBindVertexBuffers(cmd, 0, 1, &ctx->cube_vertex_buffer, &off);
                    vkCmdDraw(cmd, ctx->cube_vertex_count, 1, 0, 0);
                }
            }
        }
        vkCmdEndRenderPass(cmd);
    }

    // === 2. JFA init ===
    {
        VkClearValue clear = {.color = {{-1, -1, 0, 1}}};
        VkRenderPassBeginInfo rp = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = OL.jfa_render_pass,
            .framebuffer = OL.jfa_a.framebuffer,
            .renderArea = {.extent = {w, h}},
            .clearValueCount = 1, .pClearValues = &clear,
        };
        vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport vp = {.width = (f32)w, .height = (f32)h, .maxDepth = 1};
        VkRect2D sc = {.extent = {w, h}};
        vkCmdSetViewport(cmd, 0, 1, &vp);
        vkCmdSetScissor(cmd, 0, 1, &sc);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, OL.jfa_init_pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx->pipeline_layout,
                                 0, 1, &OL.mask_ds[ctx->current_frame], 0, nullptr);
        vkCmdDraw(cmd, 3, 1, 0, 0);
        vkCmdEndRenderPass(cmd);
    }

    // === 3. JFA flood passes (ping-pong) ===
    u32 max_dim = w > h ? w : h;
    i32 passes = (i32)ceilf(log2f((f32)max_dim));
    f32 texel_w = 1.0f / (f32)w;
    f32 texel_h = 1.0f / (f32)h;

    VkFramebuffer src_fb = OL.jfa_a.framebuffer, dst_fb = OL.jfa_b.framebuffer;
    VkDescriptorSet *src_ds = OL.jfa_a_ds, *dst_ds = OL.jfa_b_ds;

    for (i32 i = passes - 1; i >= 0; i--) {
        f32 step = (f32)(1 << i);

        VkClearValue clear = {.color = {{0, 0, 0, 1}}};
        VkRenderPassBeginInfo rp = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = OL.jfa_render_pass,
            .framebuffer = dst_fb,
            .renderArea = {.extent = {w, h}},
            .clearValueCount = 1, .pClearValues = &clear,
        };
        vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport vp = {.width = (f32)w, .height = (f32)h, .maxDepth = 1};
        VkRect2D sc = {.extent = {w, h}};
        vkCmdSetViewport(cmd, 0, 1, &vp);
        vkCmdSetScissor(cmd, 0, 1, &sc);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, OL.jfa_step_pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx->pipeline_layout,
                                 0, 1, &src_ds[ctx->current_frame], 0, nullptr);

        VK_MeshPushConstants pc = {0};
        pc.material_params[0] = step;
        pc.obj_center[0] = texel_w;
        pc.obj_center[1] = texel_h;
        vkCmdPushConstants(cmd, ctx->pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
        vkCmdDraw(cmd, 3, 1, 0, 0);
        vkCmdEndRenderPass(cmd);

        // Swap
        VkFramebuffer tmp_fb = src_fb; src_fb = dst_fb; dst_fb = tmp_fb;
        VkDescriptorSet *tmp_ds = src_ds; src_ds = dst_ds; dst_ds = tmp_ds;
    }

    // Pick the composite descriptor set that references the final JFA buffer
    VkImage final_jfa_image = (src_ds == OL.jfa_a_ds) ? OL.jfa_a.image : OL.jfa_b.image;
    OL._final_composite_ds = (src_ds == OL.jfa_a_ds) ? OL.composite_from_a_ds : OL.composite_from_b_ds;

    // Barrier: ensure mask + final JFA writes are fully visible to the main render
    // pass composite read. Explicit image barriers help MoltenVK emit correct
    // Metal resource hazard tracking between render command encoders.
    VkImageSubresourceRange cr = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1};
    VkImageMemoryBarrier read_barriers[2] = {
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = OL.mask_rt.image,
            .subresourceRange = cr,
        },
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = final_jfa_image,
            .subresourceRange = cr,
        },
    };
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 2, read_barriers);
}

// -------------------------------------------------------------------------
// Command recording — composite (inside main render pass)
// -------------------------------------------------------------------------

void vk_outline_record_composite(VK_Context *ctx, VkCommandBuffer cmd) {
    if (!OL.ready || OL.outline_count == 0 || !OL.outlines || !OL._final_composite_ds) return;

    rl_frame_outline *first = &OL.outlines[0];
    u32 w = OL.mask_rt.width;
    u32 h = OL.mask_rt.height;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, OL.composite_pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, OL.composite_pipeline_layout,
                             0, 1, &OL._final_composite_ds, 0, nullptr);

    VK_MeshPushConstants pc = {0};
    glm_vec3_copy(first->color, pc.material_params);
    pc.material_params[3] = first->color[3];
    pc.obj_center[0] = first->width;
    pc.obj_center[1] = (f32)w;
    pc.obj_center[2] = (f32)h;
    vkCmdPushConstants(cmd, OL.composite_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
    vkCmdDraw(cmd, 3, 1, 0, 0);

    // Rebind scene descriptor set and pipeline layout for subsequent draws
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx->pipeline_layout,
                             0, 1, &ctx->descriptor_sets[ctx->current_frame], 0, nullptr);
}

#undef OL
