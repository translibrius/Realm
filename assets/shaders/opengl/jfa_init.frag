#version 330 core
in vec2 TexCoords;
out vec4 FragColor;

uniform sampler2D mask_tex;

void main() {
    float mask = texture(mask_tex, TexCoords).r;
    if (mask > 0.5) {
        FragColor = vec4(TexCoords, 0.0, 1.0);
    } else {
        FragColor = vec4(-1.0, -1.0, 0.0, 1.0);
    }
}
