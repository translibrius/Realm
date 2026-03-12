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

layout (push_constant) uniform PushConstants {
    mat4 model;
    vec4 material_params; // xyz = specular, w = shininess
} push;

layout (location = 0) in vec3 fragNormal;
layout (location = 1) in vec3 fragPos;
layout (location = 2) in vec2 fragTexCoord;

layout (location = 0) out vec4 outColor;

layout (binding = 1) uniform sampler2D tex_sampler;

void main() {
    // Ambient
    vec3 ambient = ubo.light_ambient.xyz * vec3(texture(tex_sampler, fragTexCoord));

    // Diffuse
    vec3 norm = normalize(fragNormal);
    vec3 lightDir = normalize(ubo.light_pos.xyz - fragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = ubo.light_diffuse.xyz * diff * vec3(texture(tex_sampler, fragTexCoord));

    // Specular
    vec3 viewDir = normalize(ubo.camera_pos.xyz - fragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), push.material_params.w);
    vec3 specular = (push.material_params.xyz * spec) * ubo.light_specular.xyz;

    vec3 result = ambient + diffuse + specular;
    outColor = vec4(result, 1.0);
}
