#version 330 core

in vec2 TexCoord;
in vec3 ParticleColor;

out vec4 FragColor;

void main() {
    // Calculate distance from center to create a circular particle
    // TexCoord is in [-1, 1] range from the vertex shader
    float distSq = dot(TexCoord, TexCoord);

    // Use fwidth for a resolution-independent soft edge, similar to the reference
    float delta = fwidth(sqrt(distSq));
    float alpha = 1.0 - smoothstep(1.0 - delta, 1.0 + delta, distSq);

    // Discard fragments that are fully transparent
    if (alpha < 0.01) {
        discard;
    }

    FragColor = vec4(ParticleColor, alpha);
}
