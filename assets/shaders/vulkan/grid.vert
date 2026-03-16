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

layout (location = 0) out vec3 near_point;
layout (location = 1) out vec3 far_point;

const vec2 positions[6] = vec2[](
    vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(-1.0, 1.0),
    vec2(-1.0, 1.0),  vec2(1.0, -1.0), vec2(1.0, 1.0)
);

void main() {
    vec2 pos = positions[gl_VertexIndex];
    mat4 inv_vp = inverse(ubo.proj * ubo.view);
    vec4 near_w = inv_vp * vec4(pos, 0.0, 1.0);
    vec4 far_w  = inv_vp * vec4(pos, 1.0, 1.0);
    near_point = near_w.xyz / near_w.w;
    far_point  = far_w.xyz  / far_w.w;
    gl_Position = vec4(pos, 0.0, 1.0);
}
