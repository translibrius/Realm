#version 330 core

in vec2 frag_uv;
in vec4 frag_color;
out vec4 FragColor;

uniform sampler2D u_font_atlas;

void main() {
    float sd = texture(u_font_atlas, frag_uv).a - 0.5;
    float w = fwidth(sd);
    float alpha = smoothstep(-w, w, sd);

    FragColor = vec4(frag_color.rgb, frag_color.a * alpha);
}
