#version 330 core
in vec2 TexCoords;
out vec4 FragColor;

uniform sampler2D jfa_tex;
uniform sampler2D mask_tex;
uniform vec4 outline_color;
uniform float outline_width;
uniform vec2 screen_size;

void main() {
    vec2 seed = texture(jfa_tex, TexCoords).rg;
    float mask = texture(mask_tex, TexCoords).r;

    if (seed.x < 0.0) {
        discard;
    }

    // Distance in pixels
    vec2 diff = (TexCoords - seed) * screen_size;
    float dist = length(diff);

    // Outline: outside the mask, within outline_width pixels
    if (mask < 0.5 && dist <= outline_width) {
        // Soft anti-aliased edge
        float alpha = smoothstep(outline_width, outline_width - 1.0, dist);
        FragColor = vec4(outline_color.rgb, outline_color.a * alpha);
    } else {
        discard;
    }
}
