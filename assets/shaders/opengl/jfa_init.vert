#version 330 core
layout(location = 0) in vec2 in_pos;
layout(location = 1) in vec2 in_uv;

out vec2 TexCoords;

void main() {
    TexCoords = in_uv;
    gl_Position = vec4(in_pos, 0.0, 1.0);
}
