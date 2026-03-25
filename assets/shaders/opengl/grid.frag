#version 330 core

in vec3 near_point;
in vec3 far_point;

uniform mat4 view;
uniform mat4 projection;
uniform vec3 camera_pos;

out vec4 FragColor;

// Pristine Grid (Ben Golus, 2023)
float pristineGrid(vec2 uv, vec2 lineWidth) {
    vec4 uvDDXY = vec4(dFdx(uv), dFdy(uv));
    vec2 uvDeriv = vec2(length(uvDDXY.xz), length(uvDDXY.yw));

    bvec2 invertLine = greaterThan(lineWidth, vec2(0.5));
    vec2 targetWidth = mix(lineWidth, 1.0 - lineWidth, invertLine);
    vec2 drawWidth = clamp(targetWidth, uvDeriv, vec2(0.5));
    vec2 lineAA = uvDeriv * 1.5;

    vec2 gridUV = abs(fract(uv) * 2.0 - 1.0);
    gridUV = mix(1.0 - gridUV, gridUV, invertLine);

    vec2 grid2 = smoothstep(drawWidth + lineAA, drawWidth - lineAA, gridUV);
    grid2 *= clamp(targetWidth / drawWidth, 0.0, 1.0);
    grid2 = mix(grid2, targetWidth, clamp(uvDeriv * 2.0 - 1.0, 0.0, 1.0));
    grid2 = mix(grid2, 1.0 - grid2, invertLine);

    return mix(grid2.x, 1.0, grid2.y);
}

void main() {
    float t = -near_point.y / (far_point.y - near_point.y);
    if (t < 0.0) discard;

    vec3 world_pos = near_point + t * (far_point - near_point);

    float minor = pristineGrid(world_pos.xz, vec2(0.02));
    float major = pristineGrid(world_pos.xz / 10.0, vec2(0.02));

    vec4 color = vec4(vec3(0.5), minor * 0.3);
    color = mix(color, vec4(vec3(0.5), 0.6), major);

    // Axis lines (constant pixel width, capped to prevent horizon bleed)
    vec4 axisDDXY = vec4(dFdx(world_pos.xz), dFdy(world_pos.xz));
    vec2 axisDeriv = vec2(length(axisDDXY.xz), length(axisDDXY.yw));
    vec2 axisLineWidth = min(axisDeriv * 1.5, vec2(2.0));
    float axisX = smoothstep(axisLineWidth.x, 0.0, abs(world_pos.x));
    float axisZ = smoothstep(axisLineWidth.y, 0.0, abs(world_pos.z));
    color = mix(color, vec4(0.9, 0.2, 0.2, 1.0), axisZ);
    color = mix(color, vec4(0.2, 0.4, 0.9, 1.0), axisX);

    FragColor = color;
    if (FragColor.a < 0.001) discard;

    vec4 clip = projection * view * vec4(world_pos, 1.0);
    gl_FragDepth = (clip.z / clip.w) * 0.5 + 0.5;
}
