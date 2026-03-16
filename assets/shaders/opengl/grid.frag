#version 330 core

in vec3 near_point;
in vec3 far_point;

uniform mat4 view;
uniform mat4 projection;
uniform vec3 camera_pos;

out vec4 FragColor;

vec4 grid(vec3 pos, float scale) {
    vec2 coord = pos.xz / scale;
    vec2 deriv = fwidth(coord);
    vec2 grid_lines = abs(fract(coord - 0.5) - 0.5) / deriv;
    float line = min(grid_lines.x, grid_lines.y);
    float alpha = 1.0 - min(line, 1.0);
    return vec4(0.5, 0.5, 0.5, alpha * 0.3);
}

void main() {
    float t = -near_point.y / (far_point.y - near_point.y);
    if (t < 0.0) discard;

    vec3 world_pos = near_point + t * (far_point - near_point);

    float dist = length(world_pos - camera_pos);
    float fade = 1.0 - smoothstep(50.0, 100.0, dist);
    if (fade <= 0.0) discard;

    vec4 minor = grid(world_pos, 0.1);
    vec4 major = grid(world_pos, 1.0);

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

    FragColor = color;

    vec4 clip = projection * view * vec4(world_pos, 1.0);
    gl_FragDepth = (clip.z / clip.w) * 0.5 + 0.5;
}
