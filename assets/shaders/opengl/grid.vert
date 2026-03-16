#version 330 core

uniform mat4 view;
uniform mat4 projection;

out vec3 near_point;
out vec3 far_point;

const vec2 positions[6] = vec2[](
    vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(-1.0, 1.0),
    vec2(-1.0, 1.0),  vec2(1.0, -1.0), vec2(1.0, 1.0)
);

void main() {
    vec2 pos = positions[gl_VertexID];
    mat4 inv_vp = inverse(projection * view);
    vec4 near_w = inv_vp * vec4(pos, -1.0, 1.0);
    vec4 far_w  = inv_vp * vec4(pos,  1.0, 1.0);
    near_point = near_w.xyz / near_w.w;
    far_point  = far_w.xyz  / far_w.w;
    gl_Position = vec4(pos, 0.0, 1.0);
}
