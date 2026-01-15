#include "vk_descriptor.h"

b8 vk_descriptor_create_set_layout(VK_Context *context) {
    VkDescriptorSetLayoutBinding ubo_layout_binding = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .pImmutableSamplers = nullptr // Optional
    };

    VkDescriptorSetLayoutBinding sampler_layout_binding = {
        .binding = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .pImmutableSamplers = nullptr
    };

    VkDescriptorSetLayoutBinding bindings[2] = {ubo_layout_binding, sampler_layout_binding};

    VkDescriptorSetLayoutCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 2,
        .pBindings = bindings
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
    VkDescriptorPoolSize pool_sizes[2] = {0};
    pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    pool_sizes[0].descriptorCount = context->max_frames_in_flight;
    pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pool_sizes[1].descriptorCount = context->max_frames_in_flight;

    VkDescriptorPoolCreateInfo pool_create_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = context->max_frames_in_flight,
        .poolSizeCount = 2,
        .pPoolSizes = pool_sizes
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

        VkDescriptorImageInfo image_info = {
            .sampler = context->texture_sampler,
            .imageView = context->texture_wood.texture_image_view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };

        VkWriteDescriptorSet descriptor_writes[2] = {};
        descriptor_writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_writes[0].dstSet = context->descriptor_sets[i];
        descriptor_writes[0].dstBinding = 0;
        descriptor_writes[0].dstArrayElement = 0;
        descriptor_writes[0].descriptorCount = 1;
        descriptor_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptor_writes[0].pImageInfo = nullptr; // Optional
        descriptor_writes[0].pBufferInfo = &buffer_info;
        descriptor_writes[0].pTexelBufferView = nullptr; // Optional

        descriptor_writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_writes[1].dstSet = context->descriptor_sets[i];
        descriptor_writes[1].dstBinding = 1;
        descriptor_writes[1].dstArrayElement = 0;
        descriptor_writes[1].descriptorCount = 1;
        descriptor_writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptor_writes[1].pImageInfo = &image_info;
        descriptor_writes[1].pBufferInfo = &buffer_info;
        descriptor_writes[1].pTexelBufferView = nullptr; // Optional

        vkUpdateDescriptorSets(context->device, 2, descriptor_writes, 0, nullptr);
    }

    return true;
}