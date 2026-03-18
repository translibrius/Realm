#version 330 core
layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 aTexCoords;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform float outline_thickness;
uniform vec3 obj_center; // object-space AABB center (0,0,0 for primitives)

void main() {
    mat4 mvp = projection * view * model;

    // Project vertex and mesh center to clip space
    vec4 clip_pos = mvp * vec4(in_pos, 1.0);
    vec4 clip_center = mvp * vec4(obj_center, 1.0);

    // Push direction = away from mesh center in NDC
    vec2 dir = clip_pos.xy / clip_pos.w - clip_center.xy / clip_center.w;
    float len = length(dir);
    vec2 ndc_dir = len > 0.0001 ? dir / len : vec2(0.0);

    clip_pos.xy += ndc_dir * outline_thickness * clip_pos.w;
    gl_Position = clip_pos;
}
