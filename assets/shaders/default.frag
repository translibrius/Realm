#version 330 core
out vec4 FragColor;

in vec3 color;
in vec2 tex_coord;

uniform sampler2D our_texture;

void main()
{
    FragColor = texture(our_texture, tex_coord) * vec4(color, 1.0);
}