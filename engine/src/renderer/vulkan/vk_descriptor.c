#include "vk_descriptor.h"

b8 vk_descriptor_create_set_layout(VK_Context *context) {
    VkDescriptorSetLayoutBinding ubo_layout_binding = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .pImmutableSamplers = nullptr // Optional
    };

    VkDescriptorSetLayoutCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &ubo_layout_binding
    };

    VK_CHECK_RETURN_FALSE(
        vkCreateDescriptorSetLayout(context->device, &create_info, nullptr, &context->graphics_pipeline.descriptor_set_layout),
        "Failed to create descriptor set layout"
        );

    return true;
}

void vk_descriptor_destroy_set_layout(VK_Context *context) {
    vkDestroyDescriptorSetLayout(context->device, context->graphics_pipeline.descriptor_set_layout, nullptr);
}

b8 vk_descriptor_create_pool(VK_Context *context) {
    VkDescriptorPoolSize pool_size = {
        .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = context->max_frames_in_flight
    };

    VkDescriptorPoolCreateInfo pool_create_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = context->max_frames_in_flight,
        .poolSizeCount = 1,
        .pPoolSizes = &pool_size
    };

    VK_CHECK_RETURN_FALSE(
        vkCreateDescriptorPool(context->device, &pool_create_info, nullptr, &context->descriptor_pool),
        "Failed to create descriptor pool");

    return true;
}

void vk_descriptor_destroy_pool(VK_Context *context) {
    vkDestroyDescriptorPool(context->device, context->descriptor_pool, nullptr);
}

b8 vk_descriptor_create_sets(VK_Context *context) {
    VkDescriptorSetLayout *layouts = rl_arena_push(&context->arena, sizeof(VkDescriptorSetLayout) * context->max_frames_in_flight, true);
    for (u32 i = 0; i < context->max_frames_in_flight; i++) {
        layouts[i] = context->graphics_pipeline.descriptor_set_layout;
    }

    VkDescriptorSetAllocateInfo allocate_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = context->descriptor_pool,
        .descriptorSetCount = context->max_frames_in_flight,
        .pSetLayouts = layouts
    };

    context->descriptor_sets = rl_arena_push(&context->arena, sizeof(VkDescriptorSet) * context->max_frames_in_flight, true);
    VK_CHECK_RETURN_FALSE(
        vkAllocateDescriptorSets(context->device, &allocate_info, context->descriptor_sets),
        "Failed to allocate descriptor sets"
        );

    for (u32 i = 0; i < context->max_frames_in_flight; i++) {
        VkDescriptorBufferInfo buffer_info = {
            .buffer = context->uniform_buffers[i],
            .offset = 0,
            .range = sizeof(ubo)
        };

        VkWriteDescriptorSet descriptor_write = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = context->descriptor_sets[i],
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pImageInfo = nullptr, // Optional
            .pBufferInfo = &buffer_info,
            .pTexelBufferView = nullptr // Optional
        };

        vkUpdateDescriptorSets(context->device, 1, &descriptor_write, 0, nullptr);
    }

    return true;
}