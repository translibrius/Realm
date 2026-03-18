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
    vec4 material_params; // xyz = outline_color, w = outline_thickness
    vec4 obj_center;      // xyz = object-space mesh center
} push;

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inTexCoord;

void main() {
    mat4 mvp = ubo.proj * ubo.view * push.model;

    // Project vertex and mesh center to clip space
    vec4 clip_pos = mvp * vec4(inPosition, 1.0);
    vec4 clip_center = mvp * vec4(push.obj_center.xyz, 1.0);

    // Push direction = away from mesh center in NDC
    vec2 dir = clip_pos.xy / clip_pos.w - clip_center.xy / clip_center.w;
    float len = length(dir);
    vec2 ndc_dir = len > 0.0001 ? dir / len : vec2(0.0);

    clip_pos.xy += ndc_dir * push.material_params.w * clip_pos.w;
    gl_Position = clip_pos;
}
