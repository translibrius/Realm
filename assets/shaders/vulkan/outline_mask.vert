#version 450

layout (binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
    vec4 light_pos;
    vec4 light_ambient;
    vec4 light_diffuse;
    vec4 light_specular;
    vec4 camera_pos;
} ubo;

layout (push_constant) uniform PushConstants {
    mat4 model;
    vec4 material_params;
    vec4 obj_center;
} push;

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inTexCoord;

void main() {
    gl_Position = ubo.proj * ubo.view * push.model * vec4(inPosition, 1.0);
}
