#version 330 core

in vec2 frag_uv;
out vec4 FragColor;

uniform sampler2D u_font_atlas;
uniform vec4 u_color;

void main() {
    float sd = texture(u_font_atlas, frag_uv).a - 0.5;
    float w = fwidth(sd);
    float alpha = smoothstep(-w, w, sd);

    FragColor = vec4(u_color.rgb, u_color.a * alpha);
}