#version 330 core
in vec2 TexCoords;
out vec4 FragColor;

uniform sampler2D jfa_tex;
uniform float step_size;
uniform vec2 texel_size; // 1.0 / resolution

void main() {
    float best_dist = 1e10;
    vec2 best_coord = vec2(-1.0);

    for (int y = -1; y <= 1; y++) {
        for (int x = -1; x <= 1; x++) {
            vec2 offset = vec2(float(x), float(y)) * step_size * texel_size;
            vec2 sample_uv = TexCoords + offset;
            vec2 seed = texture(jfa_tex, sample_uv).rg;

            if (seed.x >= 0.0) {
                float d = distance(TexCoords, seed);
                if (d < best_dist) {
                    best_dist = d;
                    best_coord = seed;
                }
            }
        }
    }

    FragColor = vec4(best_coord, 0.0, 1.0);
}
