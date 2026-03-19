#version 450

layout (binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
    vec4 light_pos;
    vec4 light_ambient;
    vec4 light_diffuse;
    vec4 light_specular;
    vec4 camera_pos;
} ubo;

layout (location = 0) in vec3 near_point;
layout (location = 1) in vec3 far_point;

layout (location = 0) out vec4 outColor;

vec4 grid(vec3 pos, float scale) {
    vec2 coord = pos.xz / scale;
    vec2 deriv = fwidth(coord);
    vec2 grid_lines = abs(fract(coord - 0.5) - 0.5) / deriv;
    float line = min(grid_lines.x, grid_lines.y);
    float alpha = 1.0 - min(line, 1.0);
    return vec4(0.5, 0.5, 0.5, alpha * 0.4);
}

void main() {
    float t = -near_point.y / (far_point.y - near_point.y);
    if (t < 0.0) discard;

    vec3 world_pos = near_point + t * (far_point - near_point);

    float dist = length(world_pos - ubo.camera_pos.xyz);

    // Adaptive multi-level grid: pick two adjacent power-of-10 scales
    // based on camera height above the grid plane
    float cam_height = max(abs(ubo.camera_pos.y), 0.1);
    float log_level = log2(cam_height) / log2(10.0);
    float level = floor(log_level);
    float blend = fract(log_level);

    float minor_scale = pow(10.0, level - 1.0);
    float major_scale = pow(10.0, level);

    // Early discard before expensive grid computation
    float fade_start = major_scale * 150.0;
    float fade_end   = major_scale * 400.0;
    float fade = 1.0 - smoothstep(fade_start, fade_end, dist);
    if (fade <= 0.0) discard;

    vec4 minor = grid(world_pos, minor_scale);
    vec4 major = grid(world_pos, major_scale);

    // Fade out minor lines as we approach the next level
    minor.a *= 1.0 - blend;

    vec4 color = minor;
    color = mix(color, major, major.a);

    // Axis lines
    vec2 deriv = fwidth(world_pos.xz);
    if (abs(world_pos.z) < deriv.y * 0.5)
        color = vec4(0.9, 0.2, 0.2, 1.0);
    if (abs(world_pos.x) < deriv.x * 0.5)
        color = vec4(0.2, 0.4, 0.9, 1.0);

    color.a *= fade;
    if (color.a < 0.01) discard;

    outColor = color;

    vec4 clip = ubo.proj * ubo.view * vec4(world_pos, 1.0);
    gl_FragDepth = clip.z / clip.w;
}
