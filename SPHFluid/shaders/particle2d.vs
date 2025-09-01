#version 330 core

// Per-vertex attributes for the base mesh (a quad)
layout (location = 0) in vec2 aPos;

// Per-instance attributes
layout (location = 1) in vec2 aInstancePos;
layout (location = 2) in vec2 aInstanceVelocity;

// Outputs to Fragment Shader
out vec2 TexCoord;
out vec3 ParticleColor;

// Uniforms
uniform mat4 view;
uniform mat4 projection;
uniform float particleScale;
uniform float velocityMax;
uniform sampler1D ColourMap; // Matches reference's color gradient texture

void main() {
    // Scale the quad vertex by particle scale and offset by instance position
    vec2 worldPos = aPos * particleScale + aInstancePos;
    
    gl_Position = projection * view * vec4(worldPos, 0.0, 1.0);
    
    // Pass texture coordinates for drawing the circle in the fragment shader
    TexCoord = aPos; // aPos is already in [-1, 1] range for a simple quad
    
    // Calculate color based on velocity, matching the reference implementation
    float speed = length(aInstanceVelocity);
    float speedT = clamp(speed / velocityMax, 0.0, 1.0);
    
    // Sample the color from the gradient texture
    ParticleColor = texture(ColourMap, speedT).rgb;
}
