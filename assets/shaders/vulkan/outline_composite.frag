#version 450

layout (location = 0) in vec2 fragTexCoord;
layout (location = 0) out vec4 outColor;

layout (binding = 0) uniform sampler2D jfa_tex;
layout (binding = 1) uniform sampler2D mask_tex;

layout (push_constant) uniform PushConstants {
    mat4 model;
    vec4 material_params; // xyz = outline_color.rgb, w = outline_color.a
    vec4 obj_center;      // x = outline_width, yz = screen_size, w = debug_mode
} push;

void main() {
    vec4 jfa = texture(jfa_tex, fragTexCoord);
    vec2 seed = jfa.rg;
    float mask = texture(mask_tex, fragTexCoord).r;

    vec4 outline_color = push.material_params;
    float outline_width = push.obj_center.x;
    vec2 screen_size = push.obj_center.yz;
    float debug_mode = push.obj_center.w;

    // Debug visualization modes
    if (debug_mode > 0.5 && debug_mode < 1.5) {
        outColor = vec4(mask, 0.0, 0.0, 1.0);
        return;
    }
    if (debug_mode > 1.5 && debug_mode < 2.5) {
        outColor = vec4(seed.x >= 0.0 ? seed : vec2(0.0), 0.0, 1.0);
        return;
    }
    if (debug_mode > 2.5) {
        if (seed.x < 0.0) discard;
        vec2 diff = (fragTexCoord - seed) * screen_size;
        float d = length(diff) / outline_width;
        outColor = vec4(d, d, d, 1.0);
        return;
    }

    // Normal composite
    if (seed.x < 0.0) discard;

    vec2 diff = (fragTexCoord - seed) * screen_size;
    float dist = length(diff);

    if (mask > 0.5 || dist > outline_width) discard;

    float alpha = smoothstep(outline_width, outline_width - 1.0, dist);
    outColor = vec4(outline_color.rgb, outline_color.a * alpha);
}
