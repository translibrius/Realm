#version 330 core

in vec2 frag_uv;
in vec4 frag_color;
in vec4 frag_rect_info;
in vec4 frag_corner_radii;
out vec4 FragColor;

uniform sampler2D u_font_atlas;
uniform float u_px_range;
uniform float u_weight;

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
        FragColor = vec4(frag_color.rgb, frag_color.a * alpha);
    } else if (frag_uv.x < 0.0) {
        // Plain rect: solid color
        FragColor = frag_color;
    } else {
        // Text mode: MSDF sampling
        vec3 mtsdf = texture(u_font_atlas, frag_uv).rgb;
        float sd = median(mtsdf.r, mtsdf.g, mtsdf.b);

        vec2 unit_range = vec2(u_px_range) / vec2(textureSize(u_font_atlas, 0));
        vec2 screen_tex_size = vec2(1.0) / fwidth(frag_uv);
        float min_range = 1.0 / max(1.0 - 2.0 * u_weight, 0.01);
        float screen_px_range = max(0.5 * dot(unit_range, screen_tex_size), min_range);

        float screen_px_distance = screen_px_range * (sd - (0.5 - u_weight));
        float alpha = clamp(screen_px_distance + 0.5, 0.0, 1.0);

        FragColor = vec4(frag_color.rgb, frag_color.a * alpha);
    }
}
