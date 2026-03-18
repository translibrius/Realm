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
    // Space for default sets + per-texture descriptor sets (each needs UBO + sampler)
    u32 max_sets = (1 + VK_MAX_TEXTURES) * context->max_frames_in_flight;
    VkDescriptorPoolSize pool_sizes[2] = {
        { .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = max_sets },
        { .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = max_sets },
    };

    return vk_descriptor_pool_create(context, pool_sizes, 2, max_sets, &context->descriptor_pool);
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

        descriptor_writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_writes[0].dstSet = context->descriptor_sets[i];
        descriptor_writes[0].dstBinding = 0;
        descriptor_writes[0].dstArrayElement = 0;
        descriptor_writes[0].descriptorCount = 1;
        descriptor_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptor_writes[0].pBufferInfo = &buffer_info;

        // Always bind placeholder texture so binding 1 is never uninitialized
        VkDescriptorImageInfo image_info = {
            .sampler = context->texture_sampler,
            .imageView = context->placeholder_texture.texture_image_view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };

        descriptor_writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_writes[1].dstSet = context->descriptor_sets[i];
        descriptor_writes[1].dstBinding = 1;
        descriptor_writes[1].dstArrayElement = 0;
        descriptor_writes[1].descriptorCount = 1;
        descriptor_writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptor_writes[1].pImageInfo = &image_info;

        vkUpdateDescriptorSets(context->device, 2, descriptor_writes, 0, nullptr);
    }

    return true;
}

b8 vk_descriptor_create_texture_sets(VK_Context *context, VkImageView texture_view, VkDescriptorSet *out_sets) {
    if (!vk_descriptor_sets_allocate(context, context->descriptor_pool, context->descriptor_set_layout,
                                     context->max_frames_in_flight, out_sets)) {
        return false;
    }

    for (u32 i = 0; i < context->max_frames_in_flight; i++) {
        VkDescriptorBufferInfo buffer_info = {
            .buffer = context->uniform_buffers[i],
            .offset = 0,
            .range = sizeof(ubo)
        };

        VkDescriptorImageInfo image_info = {
            .sampler = context->texture_sampler,
            .imageView = texture_view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };

        VkWriteDescriptorSet writes[2] = {
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = out_sets[i],
                .dstBinding = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .pBufferInfo = &buffer_info,
            },
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = out_sets[i],
                .dstBinding = 1,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = &image_info,
            },
        };

        vkUpdateDescriptorSets(context->device, 2, writes, 0, nullptr);
    }

    return true;
}
