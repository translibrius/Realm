#include "vk_vertex.h"

#include "vk_buffer.h"

b8 vk_vertex_create_buffer(VK_Context *context, Vertices *vertices) {
    if (vertices->count <= 0) {
        RL_ERROR("failed to create vertex buffer, number of vertices must be greater than 0");
        return false;
    }

    VkDeviceSize buffer_size = sizeof(vertex) * vertices->count;

    VkBuffer staging_buffer;
    VkDeviceMemory staging_memory;
    vk_buffer_create(
        context,
        buffer_size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &staging_buffer, &staging_memory);

    void *data;
    VK_CHECK(vkMapMemory(context->device, staging_memory, 0, buffer_size, 0, &data));
    mem_copy(vertices->items, data, (u8)buffer_size);
    vkUnmapMemory(context->device, staging_memory);

    vk_buffer_create(
        context,
        buffer_size,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &context->vertex_buffer,
        &context->vertex_buffer_memory);

    // Copy the data from staging buffer to vertex buffer
    VkCommandPool pool = context->queue_families.transfer_is_separate ? context->transfer_pool : context->graphics_pool;
    vk_buffer_copy(context, pool, staging_buffer, context->vertex_buffer, buffer_size);

    // Clean up staging buffer+mem on GPU
    vk_buffer_destroy(context, staging_buffer, staging_memory);
    return true;
}

void vk_vertex_destroy_buffer(VK_Context *context) {
    vk_buffer_destroy(context, context->vertex_buffer, context->vertex_buffer_memory);
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
}