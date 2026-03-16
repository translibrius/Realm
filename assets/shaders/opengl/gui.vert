#version 330 core

layout (location = 0) in vec2 in_pos;
layout (location = 1) in vec2 in_uv;
layout (location = 2) in vec4 in_color;
layout (location = 3) in vec4 in_rect_info;

out vec2 frag_uv;
out vec4 frag_color;
out vec4 frag_rect_info;

uniform vec2 u_screen_size;

void main() {
    vec2 ndc;
    ndc.x = (in_pos.x / u_screen_size.x) * 2.0 - 1.0;
    ndc.y = 1.0 - (in_pos.y / u_screen_size.y) * 2.0;

    gl_Position = vec4(ndc, 0.0, 1.0);
    frag_uv = in_uv;
    frag_color = in_color;
    frag_rect_info = in_rect_info;
}
