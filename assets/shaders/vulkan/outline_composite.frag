#version 450

layout (location = 0) in vec2 fragTexCoord;
layout (location = 0) out vec4 outColor;

layout (binding = 1) uniform sampler2D jfa_tex;

layout (push_constant) uniform PushConstants {
    mat4 model;
    vec4 material_params; // xyz = outline_color.rgb, w = outline_color.a
    vec4 obj_center;      // x = outline_width, yz = screen_size
} push;

void main() {
    vec2 seed = texture(jfa_tex, fragTexCoord).rg;

    if (seed.x < 0.0) discard; // no seed nearby

    vec4 outline_color = push.material_params;
    float outline_width = push.obj_center.x;
    vec2 screen_size = push.obj_center.yz;

    vec2 diff = (fragTexCoord - seed) * screen_size;
    float dist = length(diff);

    // dist ~0 = inside mask (seed pixel), dist > 0 = outside mask
    if (dist < 0.5 || dist > outline_width) discard;

    float alpha = smoothstep(outline_width, outline_width - 1.0, dist);
    outColor = vec4(outline_color.rgb, outline_color.a * alpha);
}
