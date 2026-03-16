#version 450

layout (push_constant) uniform PushConstants {
    mat4 model;
    vec4 material_params; // xyz = flat_color, w unused
} pc;

layout (location = 0) out vec4 outColor;

void main() {
    vec3 c = pc.material_params.xyz;
    // If color is all zero (legacy unlit cubes), default to white
    float len = dot(c, c);
    outColor = vec4(len > 0.0 ? c : vec3(1.0), 1.0);
}
