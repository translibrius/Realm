#version 330 core

in vec2 frag_uv;
in vec4 frag_color;
out vec4 FragColor;

uniform sampler2D u_font_atlas;
uniform float u_px_range;

float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

void main() {
    if (frag_uv.x < 0.0) {
        // Rect mode: solid color
        FragColor = frag_color;
    } else {
        // Text mode: MSDF sampling
        vec3 mtsdf = texture(u_font_atlas, frag_uv).rgb;
        float sd = median(mtsdf.r, mtsdf.g, mtsdf.b);

        vec2 unit_range = vec2(u_px_range) / vec2(textureSize(u_font_atlas, 0));
        vec2 screen_tex_size = vec2(1.0) / fwidth(frag_uv);
        float screen_px_range = max(0.5 * dot(unit_range, screen_tex_size), 1.0);

        float screen_px_distance = screen_px_range * (sd - 0.5);
        float alpha = clamp(screen_px_distance + 0.5, 0.0, 1.0);

        FragColor = vec4(frag_color.rgb, frag_color.a * alpha);
    }
}
