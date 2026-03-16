#version 330 core
out vec4 FragColor;

uniform vec3 flat_color = vec3(1.0);

void main()
{
    FragColor = vec4(flat_color, 1.0);
}