#version 330 core

in vec2 TexCoord;
in vec3 ParticleColor;

out vec4 FragColor;

void main() {
    float distSq = dot(TexCoord, TexCoord);

    float delta = fwidth(sqrt(distSq));
    float alpha = 1.0 - smoothstep(1.0 - delta, 1.0 + delta, distSq);

    if (alpha < 0.01) {
        discard;
    }

    FragColor = vec4(ParticleColor, alpha);
}
