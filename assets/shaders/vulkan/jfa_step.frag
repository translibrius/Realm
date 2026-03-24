#version 450

layout (location = 0) in vec2 fragTexCoord;
layout (location = 0) out vec4 outColor;

layout (binding = 1) uniform sampler2D jfa_tex;

layout (push_constant) uniform PushConstants {
    mat4 model;
    vec4 material_params; // x = step_size
    vec4 obj_center;      // xy = texel_size
} push;

void main() {
    float step_size = push.material_params.x;
    vec2 texel_size = push.obj_center.xy;

    float best_dist = 1e10;
    vec2 best_coord = vec2(-1.0);

    for (int y = -1; y <= 1; y++) {
        for (int x = -1; x <= 1; x++) {
            vec2 offset = vec2(float(x), float(y)) * step_size * texel_size;
            vec2 sample_uv = fragTexCoord + offset;
            vec2 seed = texture(jfa_tex, sample_uv).rg;

            if (seed.x >= 0.0) {
                float d = distance(fragTexCoord, seed);
                if (d < best_dist) {
                    best_dist = d;
                    best_coord = seed;
                }
            }
        }
    }

    outColor = vec4(best_coord, 0.0, 1.0);
}
