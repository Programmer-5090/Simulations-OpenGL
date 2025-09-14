#version 330 core

layout (location = 0) in vec2 aPos;

layout (location = 1) in vec2 aInstancePos;
layout (location = 2) in vec2 aInstanceVelocity;

out vec2 TexCoord;
out vec3 ParticleColor;

uniform mat4 view;
uniform mat4 projection;
uniform float particleScale;
uniform float velocityMax;
uniform sampler1D ColourMap;

void main() {
    vec2 worldPos = aPos * particleScale + aInstancePos;
    
    gl_Position = projection * view * vec4(worldPos, 0.0, 1.0);
    
    TexCoord = aPos;
    
    float speed = length(aInstanceVelocity);
    float speedT = clamp(speed / velocityMax, 0.0, 1.0);
    
    ParticleColor = texture(ColourMap, speedT).rgb;
}
