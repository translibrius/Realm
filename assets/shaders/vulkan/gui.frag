#version 450

layout (location = 0) centroid in vec2 frag_uv;
layout (location = 1) in vec4 frag_color;
layout (location = 2) in vec4 frag_rect_info;
layout (location = 3) in vec4 frag_corner_radii;

layout (location = 0) out vec4 out_color;

layout (binding = 0) uniform sampler2D u_font_atlas;

layout (push_constant) uniform PushConstants {
    layout (offset = 0) vec2 screen_size;
    layout (offset = 8) float px_range;
    layout (offset = 12) float weight;
} pc;

float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

void main() {
    if (frag_rect_info.z > 0.0) {
        // Rounded rect — per-corner SDF
        vec2 half_size = frag_rect_info.xy;
        // Select radius for this fragment's quadrant
        // corner_radii = (topLeft, topRight, bottomLeft, bottomRight)
        // UV: (-hw,-hh)=top-left, (+hw,+hh)=bottom-right
        float radius = (frag_uv.x < 0.0)
            ? (frag_uv.y < 0.0 ? frag_corner_radii.x : frag_corner_radii.z)
            : (frag_uv.y < 0.0 ? frag_corner_radii.y : frag_corner_radii.w);
        vec2 p = abs(frag_uv) - half_size + vec2(radius);
        float d = length(max(p, 0.0)) + min(max(p.x, p.y), 0.0) - radius;
        float alpha = 1.0 - smoothstep(-0.75, 0.75, d);
        out_color = vec4(frag_color.rgb, frag_color.a * alpha);
    } else if (frag_uv.x < 0.0) {
        // Plain rect: solid color
        out_color = frag_color;
    } else {
        // Text mode: MSDF sampling
        vec3 mtsdf = texture(u_font_atlas, frag_uv).rgb;
        float sd = median(mtsdf.r, mtsdf.g, mtsdf.b);

        vec2 unit_range = vec2(pc.px_range) / vec2(textureSize(u_font_atlas, 0));
        vec2 screen_tex_size = vec2(1.0) / fwidth(frag_uv);
        float min_range = 1.0 / max(1.0 - 2.0 * pc.weight, 0.01);
        float screen_px_range = max(0.5 * dot(unit_range, screen_tex_size), min_range);

        float screen_px_distance = screen_px_range * (sd - (0.5 - pc.weight));
        float alpha = clamp(screen_px_distance + 0.5, 0.0, 1.0);

        out_color = vec4(frag_color.rgb, frag_color.a * alpha);
    }
}
