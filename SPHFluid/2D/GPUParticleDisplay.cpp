#include "GPUParticleDisplay.h"
#include <cmath>
#include <iostream>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

GPUParticleDisplay::GPUParticleDisplay(GPUFluidSimulation* sim, Shader* shader)
    : simulation(sim), particleShader(shader), VAO(0), VBO(0), instanceVBO(0), gradientTexture(0), 
      particleMesh(nullptr), particleRenderMesh(nullptr) {
    
    GenerateCircleMesh();
    
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, circleVertices.size() * sizeof(glm::vec2), circleVertices.data(), GL_STATIC_DRAW);
    
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), (void*)0);
    
    glBindVertexArray(0);
    
    InitializeRenderingResources();
}

GPUParticleDisplay::~GPUParticleDisplay() {
    if (VAO) glDeleteVertexArrays(1, &VAO);
    if (VBO) glDeleteBuffers(1, &VBO);
    if (instanceVBO) glDeleteBuffers(1, &instanceVBO);
    if (gradientTexture) glDeleteTextures(1, &gradientTexture);
}

void GPUParticleDisplay::InitializeRenderingResources() {
    CreateGradientTexture();
    
    GLuint simBuffer = simulation->GetParticleBuffer();
    
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, simBuffer);

    GLsizei stride = static_cast<GLsizei>(sizeof(GPUParticle));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(GPUParticle, position));
    glVertexAttribDivisor(1, 1);

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(GPUParticle, velocity));
    glVertexAttribDivisor(2, 1);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void GPUParticleDisplay::GenerateCircleMesh() {
    circleVertices.clear();
    circleVertices.push_back(glm::vec2(-1.0f, -1.0f));
    circleVertices.push_back(glm::vec2( 1.0f, -1.0f));
    circleVertices.push_back(glm::vec2( 1.0f,  1.0f));
    circleVertices.push_back(glm::vec2(-1.0f,  1.0f));
}

void GPUParticleDisplay::CreateGradientTexture() {
    constexpr int gradientSize = 256;
    std::vector<unsigned char> gradientData(gradientSize * 3);
    for (int i = 0; i < gradientSize; ++i) {
        // t is normalized [0,1] along the gradient texture
        float t = static_cast<float>(i) / (gradientSize - 1);
        // Multi-stop gradient:
        // deep blue -> cyan -> green -> yellow -> orange -> red
        glm::vec3 c0 = glm::vec3(0.05f, 0.15f, 0.85f); // deep blue
        glm::vec3 c1 = glm::vec3(0.0f, 0.7f, 1.0f);     // cyan
        glm::vec3 c2 = glm::vec3(0.0f, 1.0f, 0.0f);     // green
        glm::vec3 c3 = glm::vec3(1.0f, 1.0f, 0.0f);     // yellow
        glm::vec3 c4 = glm::vec3(1.0f, 0.6f, 0.0f);     // orange
        glm::vec3 c5 = glm::vec3(1.0f, 0.0f, 0.0f);     // red

        glm::vec3 color;
        if (t <= 0.2f) {
            float localT = t / 0.2f; // between c0 and c1
            color = glm::mix(c0, c1, localT);
        } else if (t <= 0.4f) {
            float localT = (t - 0.2f) / 0.2f; // between c1 and c2
            color = glm::mix(c1, c2, localT);
        } else if (t <= 0.6f) {
            float localT = (t - 0.4f) / 0.2f; // between c2 and c3
            color = glm::mix(c2, c3, localT);
        } else if (t <= 0.8f) {
            float localT = (t - 0.6f) / 0.2f; // between c3 and c4
            color = glm::mix(c3, c4, localT);
        } else {
            float localT = (t - 0.8f) / 0.2f; // between c4 and c5
            color = glm::mix(c4, c5, localT);
        }
        color = glm::clamp(color, 0.0f, 1.0f);

        gradientData[i * 3 + 0] = static_cast<unsigned char>(color.r * 255);
        gradientData[i * 3 + 1] = static_cast<unsigned char>(color.g * 255);
        gradientData[i * 3 + 2] = static_cast<unsigned char>(color.b * 255);
    }

    glGenTextures(1, &gradientTexture);
    glBindTexture(GL_TEXTURE_1D, gradientTexture);
    glTexImage1D(GL_TEXTURE_1D, 0, GL_RGB, gradientSize, 0, GL_RGB, GL_UNSIGNED_BYTE, gradientData.data());

    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
}


void GPUParticleDisplay::Render(const glm::mat4& view, const glm::mat4& projection) {
    particleShader->use();

    particleShader->setMat4("view", view);
    particleShader->setMat4("projection", projection);

    // Bind the gradient texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_1D, gradientTexture);
    particleShader->setInt("ColourMap", 0);

    float particleScale = simulation->GetSettings().smoothingRadius * 0.15f;
    float velocityMax = 8.0f;
    particleShader->setFloat("particleScale", particleScale);
    particleShader->setFloat("velocityMax", velocityMax);
    
    glBindVertexArray(VAO);
    // Draw instanced quads (4 vertices per quad)
    glDrawArraysInstanced(GL_TRIANGLE_FAN, 0, 4, simulation->GetNumParticles());
    glBindVertexArray(0);
}
