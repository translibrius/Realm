#version 450

layout (location = 0) in vec2 frag_uv;
layout (location = 1) in vec4 frag_color;

layout (location = 0) out vec4 out_color;

layout (binding = 0) uniform sampler2D u_font_atlas;

layout (push_constant) uniform PushConstants {
    layout (offset = 0) vec2 screen_size;
    layout (offset = 8) float px_range;
} pc;

float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

void main() {
    vec3 mtsdf = texture(u_font_atlas, frag_uv).rgb;
    float sd = median(mtsdf.r, mtsdf.g, mtsdf.b);

    vec2 unit_range = vec2(pc.px_range) / vec2(textureSize(u_font_atlas, 0));
    vec2 screen_tex_size = vec2(1.0) / fwidth(frag_uv);
    float screen_px_range = max(0.5 * dot(unit_range, screen_tex_size), 1.0);

    float screen_px_distance = screen_px_range * (sd - 0.5);
    float alpha = clamp(screen_px_distance + 0.5, 0.0, 1.0);

    out_color = vec4(frag_color.rgb, frag_color.a * alpha);
}
