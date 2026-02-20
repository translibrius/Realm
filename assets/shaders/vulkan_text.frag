#version 450

layout (location = 0) in vec2 frag_uv;
layout (location = 1) in vec4 frag_color;

layout (location = 0) out vec4 out_color;

layout (binding = 0) uniform sampler2D u_font_atlas;

void main() {
    float sd = texture(u_font_atlas, frag_uv).a - 0.5;
    float w = fwidth(sd);
    float alpha = smoothstep(-w, w, sd);

    out_color = vec4(frag_color.rgb, frag_color.a * alpha);
}
