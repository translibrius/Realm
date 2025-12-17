#version 330 core

layout (location = 0) in vec2 in_pos; // pixel position
layout (location = 1) in vec2 in_uv;

out vec2 frag_uv;

uniform vec2 u_screen_size;

void main() {
    // Convert from pixel space to NDC
    vec2 ndc;
    ndc.x = (in_pos.x / u_screen_size.x) * 2.0 - 1.0;
    ndc.y = 1.0 - (in_pos.y / u_screen_size.y) * 2.0;

    gl_Position = vec4(ndc, 0.0, 1.0);
    frag_uv = in_uv;
}
