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
} push;

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inTexCoord;

layout (location = 0) out vec3 fragNormal;
layout (location = 1) out vec3 fragPos;
layout (location = 2) out vec2 fragTexCoord;

void main() {
    vec4 world_pos = push.model * vec4(inPosition, 1.0);
    gl_Position = ubo.proj * ubo.view * world_pos;
    fragPos = world_pos.xyz;
    fragNormal = mat3(transpose(inverse(push.model))) * inNormal;
    fragTexCoord = inTexCoord;
}
