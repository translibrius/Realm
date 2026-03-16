#pragma once

#include "defines.h"

#include <volk.h>

#include "cglm.h"
#include "asset/shader.h"
#include "asset/font.h"
#include "memory/containers/dynamic_array.h"
#include "core/logger.h"
#include "platform/platform.h"
#include "renderer/frame_data.h"

#include <shaderc/shaderc.h>

#ifndef _DEBUG
#define VK_CHECK(vkFnc) do { VkResult _r = (vkFnc); if (_r != VK_SUCCESS) RL_ERROR("VK: %s", string_VkResult(_r)); } while(0)
#define VK_CHECK_RETURN_FALSE(vkFnc, msg) do { VkResult _r = (vkFnc); if (_r != VK_SUCCESS) { RL_ERROR("%s VkResult=%s", msg, string_VkResult(_r)); return false; } } while(0)
#else
#define VK_CHECK(vkFnc)                                                 \
{                                                                       \
    const VkResult checkResult = (vkFnc);                               \
    if(checkResult != VK_SUCCESS)                                       \
    {                                                                   \
            const char* errMsg = string_VkResult(checkResult);          \
            RL_ERROR("Vulkan error: %s", errMsg);                       \
            RL_ASSERT_MSG(checkResult == VK_SUCCESS, errMsg);           \
    }                                                                   \
}
#define VK_CHECK_RETURN_FALSE(vkFnc, msg)                               \
{                                                                       \
    const VkResult checkResult = (vkFnc);                               \
    if(checkResult != VK_SUCCESS)                                       \
    {                                                                   \
        const char* errMsg = string_VkResult(checkResult);              \
        RL_ERROR("%s VkResult=%s", msg, errMsg);                        \
        return false;                                                   \
    }                                                                   \
}
#endif

typedef struct VK_QueueFamilyIndices {
    u32 graphics_index;
    u32 compute_index;
    u32 transfer_index;
    u32 present_index;

    b8 has_graphics;
    b8 has_compute;
    b8 has_transfer;
    b8 has_present;

    b8 transfer_is_separate;
} VK_QueueFamilyIndices;

typedef struct VK_Swapchain {
    VkSwapchainKHR handle;
    b8 vsync;

    VkSurfaceCapabilities2KHR capabilities2;

    // Formats
    u32 format_count;
    VkSurfaceFormat2KHR *formats;

    // Present modes
    u32 present_mode_count;
    VkPresentModeKHR *present_modes;

    // Misc settings
    VkSurfaceFormat2KHR chosen_format;
    VkPresentModeKHR chosen_present_mode;
    VkExtent2D chosen_extent;

    // Swapchain images
    u32 image_count;
    VkImage *images;
    VkImageView *image_views;

    // Frames
    u32 frame_buffers_count;
    VkFramebuffer *frame_buffers;

} VK_Swapchain;

typedef struct VK_DeviceProperties {
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceFeatures features;
    VkPhysicalDeviceMemoryProperties memory_properties;
} VK_DeviceProperties;

typedef struct VK_Texture {
    VkImage texture_image;
    VkImageView texture_image_view;
    VkDeviceMemory texture_memory;
    u32 mip_levels;
} VK_Texture;

typedef struct VK_TextVertex {
    vec2 pos;
    vec2 uv;
    vec4 color;
} VK_TextVertex;

typedef struct VK_Font {
    VkImage atlas_image;
    VkImageView atlas_view;
    VkDeviceMemory atlas_memory;
    VkDescriptorSet *descriptor_sets; // per-frame descriptor sets for this font's atlas
    rl_font *font;
    const rl_glyph *glyph_map[256];
} VK_Font;

DA_DEFINE(VK_Fonts, VK_Font);

typedef struct VK_TextPipeline {
    VkDescriptorSetLayout descriptor_set_layout;
    VkDescriptorPool descriptor_pool;
    VkSampler font_sampler;
    VK_Fonts fonts;
    rl_font *active_font;
} VK_TextPipeline;

// Flush segment: records a sub-range of vertices that share a descriptor set (font).
typedef struct VK_GuiSegment {
    u32 start_vertex;
    u32 vertex_count;
    VK_Font *font;
} VK_GuiSegment;

#define GUI_MAX_SEGMENTS 128

typedef struct VK_GuiPipeline {
    VkPipeline handle;
    VkPipelineLayout layout;
    VkBuffer *vertex_buffers;
    VkDeviceMemory *vertex_buffer_memory;
    void **vertex_buffer_mapped;
    u32 vertex_count;
    VK_Font *current_font;
    VK_GuiSegment segments[GUI_MAX_SEGMENTS];
    u32 segment_count;
} VK_GuiPipeline;

typedef struct VK_Shader {
    rl_asset_shader *asset;
    VkShaderModule module;
} VK_Shader;

DA_DEFINE(Shaders, VK_Shader);

typedef struct {
    shaderc_compiler_t compiler;
    shaderc_compile_options_t options;
    b8 initialized;
} VK_Shader_Compiler;

typedef struct VK_MeshPushConstants {
    mat4 model;           // 64 bytes
    vec4 material_params; // 16 bytes: xyz = specular, w = shininess
} VK_MeshPushConstants;

typedef struct VK_Pipeline {
    VkPipeline handle;
    u32 shader_stage_count;
    VkPipelineShaderStageCreateInfo *shader_stages;
} VK_Pipeline;

typedef struct VK_Context {
    rl_arena arena;
    platform_window *window;
    b8 framebuffer_resized;

    VK_Shader_Compiler shader_compiler;
    Shaders shaders;

    u32 api_version;
    VkInstance instance;
    VkDebugUtilsMessengerEXT debug_messenger;

    VkSurfaceKHR surface;

    VkPhysicalDevice physical_device;
    VkDevice device;
    VK_DeviceProperties device_properties;

    VkQueue graphics_queue;
    VkQueue present_queue;
    VkQueue compute_queue;
    VkQueue transfer_queue;
    VK_QueueFamilyIndices queue_families;

    VK_Swapchain swapchain;

    // Shared across all 3D pipelines
    VkRenderPass render_pass;
    VkDescriptorSetLayout descriptor_set_layout;
    VkPipelineLayout pipeline_layout;

    VK_Pipeline graphics_pipeline;
    VkCommandPool graphics_pool;
    VkCommandPool transfer_pool;
    VkFence transfer_fence; // Waiting for staging buffer to transfer vertex data

    VkImage depth_image;
    VkDeviceMemory depth_image_memory;
    VkImageView depth_image_view;

    // MSAA
    VkSampleCountFlagBits msaa_samples;
    VkImage msaa_color_image;
    VkDeviceMemory msaa_color_memory;
    VkImageView msaa_color_view;

    // Per frame
    u32 current_frame;
    u32 current_image_index;
    b8 frame_acquired;
    u32 max_frames_in_flight;
    VkCommandBuffer *command_buffers;
    VkSemaphore *image_available_semaphores;
    VkSemaphore *render_finished_semaphores; // Per swapchain image (not per frame-in-flight)
    u32 render_finished_semaphore_count;
    VkFence *in_flight_fences;
    VkBuffer *uniform_buffers;
    VkDeviceMemory *uniform_buffers_memory;
    void **uniform_buffers_mapped;
    VkDescriptorSet *descriptor_sets;

    VkDescriptorPool descriptor_pool;

    // Cube mesh (shared geometry for all meshes)
    VkBuffer cube_vertex_buffer;
    VkDeviceMemory cube_vertex_memory;
    u32 cube_vertex_count;

    // Unlit pipeline (light cubes)
    VkPipeline unlit_pipeline;

    // Overlay pipeline (gizmo axes — no depth test)
    VkPipeline overlay_pipeline;
    rl_frame_camera overlay_camera;
    rl_frame_mesh  *overlay_meshes;
    u32             overlay_count;

    // Debug wireframe pipelines
    VkPipeline wireframe_lit_pipeline;
    VkPipeline wireframe_unlit_pipeline;
    b8 has_wireframe_pipelines;
    b8 debug_wireframe;

    // Frame data (set by submit_frame_data, consumed by command recording)
    rl_frame_mesh *frame_meshes;
    u32 frame_mesh_count;
    rl_frame_point_light frame_light;
    vec3 camera_pos;
    rl_viewport_rect scene_viewport;

    // Textures
    VkSampler texture_sampler;
    enum { VK_MAX_TEXTURES = 64 };
    struct { asset_id asset_id; VK_Texture texture; } textures[64];
    u32 texture_count;

    // Text
    VK_TextPipeline text_pipeline;

    // GUI (Clay)
    VK_GuiPipeline gui_pipeline;

    mat4 view;
    mat4 proj;

} VK_Context;
