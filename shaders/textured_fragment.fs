#version 330 core
out vec4 FragColor;

in vec3 Normal;
in vec2 TexCoords;
in vec3 FragPos;

// Texture sampler
uniform sampler2D material_diffuse;

// Simple lighting uniforms
uniform vec3 viewPos;

struct Material {
    vec3 ambient;
    vec3 specular;
    float shininess;
};
uniform Material material;

struct DirLight {
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};
uniform DirLight dirLight;

void main()
{
    // Sample the diffuse texture
    vec3 texColor = texture(material_diffuse, TexCoords).rgb;
    
    // Normalize the normal vector
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 lightDir = normalize(-dirLight.direction);
    
    // Ambient component (using texture color)
    vec3 ambient = dirLight.ambient * material.ambient * texColor;
    
    // Diffuse component (using texture color)
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = dirLight.diffuse * diff * texColor;
    
    // Specular component
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    vec3 specular = dirLight.specular * spec * material.specular;
    
    // Combine all components
    vec3 result = ambient + diffuse + specular;
    FragColor = vec4(result, 1.0);
}