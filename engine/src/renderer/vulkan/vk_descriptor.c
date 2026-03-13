#include "vk_descriptor.h"
#include "vk_util.h"
#include "memory/arena.h"

// --- Generic helpers ---

b8 vk_descriptor_pool_create(VK_Context *ctx, VkDescriptorPoolSize *sizes, u32 size_count, u32 max_sets, VkDescriptorPool *out) {
    VkDescriptorPoolCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = max_sets,
        .poolSizeCount = size_count,
        .pPoolSizes = sizes
    };

    VK_CHECK_RETURN_FALSE(
        vkCreateDescriptorPool(ctx->device, &ci, nullptr, out),
        "Failed to create descriptor pool");

    return true;
}

b8 vk_descriptor_sets_allocate(VK_Context *ctx, VkDescriptorPool pool, VkDescriptorSetLayout layout, u32 count, VkDescriptorSet *out) {
    ARENA_SCRATCH_START();

    VkDescriptorSetLayout *layouts = rl_arena_push(scratch.arena, sizeof(VkDescriptorSetLayout) * count, alignof(VkDescriptorSetLayout));
    for (u32 i = 0; i < count; i++) {
        layouts[i] = layout;
    }

    VkDescriptorSetAllocateInfo ai = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = pool,
        .descriptorSetCount = count,
        .pSetLayouts = layouts
    };

    VkResult result = vkAllocateDescriptorSets(ctx->device, &ai, out);
    ARENA_SCRATCH_RELEASE();

    if (result != VK_SUCCESS) {
        RL_ERROR("Failed to allocate descriptor sets: %s", string_VkResult(result));
        return false;
    }

    return true;
}

// --- Mesh pipeline-specific wrappers ---

b8 vk_descriptor_create_set_layout(VK_Context *context) {
    VkDescriptorSetLayoutBinding ubo_layout_binding = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .pImmutableSamplers = nullptr
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
        vkCreateDescriptorSetLayout(context->device, &create_info, nullptr, &context->descriptor_set_layout),
        "Failed to create descriptor set layout"
        );

    return true;
}

void vk_descriptor_destroy_set_layout(VK_Context *context) {
    vkDestroyDescriptorSetLayout(context->device, context->descriptor_set_layout, nullptr);
}

b8 vk_descriptor_create_pool(VK_Context *context) {
    VkDescriptorPoolSize pool_sizes[2] = {
        { .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = context->max_frames_in_flight },
        { .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = context->max_frames_in_flight },
    };

    return vk_descriptor_pool_create(context, pool_sizes, 2, context->max_frames_in_flight, &context->descriptor_pool);
}

void vk_descriptor_destroy_pool(VK_Context *context) {
    vkDestroyDescriptorPool(context->device, context->descriptor_pool, nullptr);
}

b8 vk_descriptor_create_sets(VK_Context *context) {
    context->descriptor_sets = rl_arena_push(&context->arena, sizeof(VkDescriptorSet) * context->max_frames_in_flight, true);

    if (!vk_descriptor_sets_allocate(context, context->descriptor_pool, context->descriptor_set_layout, context->max_frames_in_flight, context->descriptor_sets)) {
        return false;
    }

    for (u32 i = 0; i < context->max_frames_in_flight; i++) {
        VkDescriptorBufferInfo buffer_info = {
            .buffer = context->uniform_buffers[i],
            .offset = 0,
            .range = sizeof(ubo)
        };

        VkWriteDescriptorSet descriptor_writes[2] = {};
        u32 write_count = 1;

        descriptor_writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_writes[0].dstSet = context->descriptor_sets[i];
        descriptor_writes[0].dstBinding = 0;
        descriptor_writes[0].dstArrayElement = 0;
        descriptor_writes[0].descriptorCount = 1;
        descriptor_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptor_writes[0].pBufferInfo = &buffer_info;

        VkDescriptorImageInfo image_info = {0};
        if (context->texture_count > 0) {
            image_info.sampler = context->texture_sampler;
            image_info.imageView = context->textures[0].texture.texture_image_view;
            image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            descriptor_writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptor_writes[1].dstSet = context->descriptor_sets[i];
            descriptor_writes[1].dstBinding = 1;
            descriptor_writes[1].dstArrayElement = 0;
            descriptor_writes[1].descriptorCount = 1;
            descriptor_writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptor_writes[1].pImageInfo = &image_info;
            write_count = 2;
        }

        vkUpdateDescriptorSets(context->device, write_count, descriptor_writes, 0, nullptr);
    }

    return true;
}
