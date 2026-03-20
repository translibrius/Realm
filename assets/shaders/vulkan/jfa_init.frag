#version 450

layout (location = 0) in vec2 fragTexCoord;
layout (location = 0) out vec4 outColor;

layout (binding = 1) uniform sampler2D mask_tex;

void main() {
    float mask = texture(mask_tex, fragTexCoord).r;
    if (mask > 0.5) {
        // RG = seed coords, B = mask flag (carried through JFA steps)
        outColor = vec4(fragTexCoord, 1.0, 1.0);
    } else {
        outColor = vec4(-1.0, -1.0, 0.0, 1.0);
    }
}
