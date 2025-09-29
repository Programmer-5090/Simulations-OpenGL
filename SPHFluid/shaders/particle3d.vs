#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

// Per-instance attributes
layout (location = 2) in vec3 aInstancePos;
layout (location = 3) in vec3 aInstanceVelocity;

out vec3 FragPos;
out vec3 Normal;
out vec3 ParticleColor;

uniform mat4 model; // The base sphere's model matrix (scaling)
uniform mat4 view;
uniform mat4 projection;

uniform vec3 worldOffset;

uniform float velocityMax;
uniform sampler1D ColourMap;

void main()
{
    // Transform position and normal into world space
    FragPos = vec3(model * vec4(aPos, 1.0)) + aInstancePos + worldOffset;
    Normal = mat3(transpose(inverse(model))) * aNormal;

    gl_Position = projection * view * vec4(FragPos, 1.0);

    // Calculate color based on velocity magnitude
    float speed = length(aInstanceVelocity);
    float speedT = clamp(speed / velocityMax, 0.0, 1.0);
    ParticleColor = texture(ColourMap, speedT).rgb;
}
