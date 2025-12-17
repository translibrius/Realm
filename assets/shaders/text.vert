#version 330 core

layout (location = 0) in vec2 in_pos; // pixel position
layout (location = 1) in vec2 in_uv;

out vec2 frag_uv;

uniform vec2 u_screen_size;
uniform float u_scale;

void main() {
    // apply scale in pixel space
    vec2 scaled = in_pos * u_scale;

    // pixel -> NDC
    vec2 ndc;
    ndc.x = (scaled.x / u_screen_size.x) * 2.0 - 1.0;
    ndc.y = 1.0 - (scaled.y / u_screen_size.y) * 2.0;

    gl_Position = vec4(ndc, 0.0, 1.0);
    frag_uv = in_uv;
}
