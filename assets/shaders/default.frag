#version 330 core
out vec4 FragColor;

in vec3 normal;
in vec3 frag_pos;
uniform vec3 view_pos;

struct Material {
    sampler2D diffuse;
    vec3 specular;
    float shininess;
};
uniform Material material;
in vec2 TexCoords;

struct Light {
    vec3 position;

    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};
uniform Light light;

void main()
{
    vec3 ambient = light.ambient * vec3(texture(material.diffuse, TexCoords));

    vec3 norm = normalize(normal);
    vec3 lightDir = normalize(light.position - frag_pos);

    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = light.diffuse * diff * vec3(texture(material.diffuse, TexCoords));

    vec3 viewDir = normalize(view_pos - frag_pos);
    vec3 reflectDir = reflect(-lightDir, norm);

    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    vec3 specular = (material.specular * spec) * light.specular;

    vec3 result = (ambient + diffuse + specular);
    FragColor = vec4(result, 1.0);
}
