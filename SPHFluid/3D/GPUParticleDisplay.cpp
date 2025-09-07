#include "GPUParticleDisplay.h"
#include "ComputeHelper.h"
#include "GPUFluidSimulation.h"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>


GPUParticleDisplay::GPUParticleDisplay(GPUFluidSimulation* sim, Shader* shader)
    : simulation(sim), particleShader(shader),
      instanceVBO_positions(0), instanceVBO_velocities(0), gradientTexture(0) {
    
    // Create the CPU-side parametric sphere and convert to a renderable Mesh
    particleMesh = new Sphere(1.0f, 16);
    // Convert generated parametric geometry into a Mesh for rendering
    particleRenderMesh = new Mesh(particleMesh->toMesh());
    InitializeRenderingResources();
}

GPUParticleDisplay::~GPUParticleDisplay() {
    delete particleMesh;
    if (instanceVBO_positions) glDeleteBuffers(1, &instanceVBO_positions);
    if (instanceVBO_velocities) glDeleteBuffers(1, &instanceVBO_velocities);
    if (gradientTexture) glDeleteTextures(1, &gradientTexture);
    if (particleRenderMesh) delete particleRenderMesh;
}

void GPUParticleDisplay::InitializeRenderingResources() {
    CreateGradientTexture();

    // VBO for instance positions
    glGenBuffers(1, &instanceVBO_positions);
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO_positions);
    glBufferData(GL_ARRAY_BUFFER, simulation->GetNumParticles() * sizeof(glm::vec3), nullptr, GL_DYNAMIC_DRAW);
    
    // VBO for instance velocities
    glGenBuffers(1, &instanceVBO_velocities);
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO_velocities);
    glBufferData(GL_ARRAY_BUFFER, simulation->GetNumParticles() * sizeof(glm::vec3), nullptr, GL_DYNAMIC_DRAW);

    // Set up VAO on the converted Mesh
    glBindVertexArray(particleRenderMesh->getVAO());

    // Instance Position attribute (location 2)
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO_positions);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glVertexAttribDivisor(2, 1); // This attribute is per-instance

    // Instance Velocity attribute (location 3)
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO_velocities);
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glVertexAttribDivisor(3, 1); // This attribute is per-instance

    glBindVertexArray(0);
}

void GPUParticleDisplay::CreateGradientTexture() {
    // Create a 1D texture for the color gradient
    // Fixed gradient resolution for the 1D colour lookup texture.
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

void GPUParticleDisplay::Update() {
    UpdateInstanceData();
}

void GPUParticleDisplay::UpdateInstanceData() {
    // Since the simulation buffer contains interleaved GPUParticle structs,
    // we need to copy the data with proper strides, not as contiguous blocks.
    // The safest approach is to read the data and then upload just the positions/velocities.
    
    GLuint simBuffer = simulation->GetParticleBuffer();
    int numParticles = simulation->GetNumParticles();
    if (numParticles <= 0) return;
    
    // Read the particle data from GPU
    std::vector<GPUParticle> particles = simulation->GetParticles();
    
    // Extract positions and velocities into separate arrays
    std::vector<glm::vec3> positions(numParticles);
    std::vector<glm::vec3> velocities(numParticles);
    
    for (int i = 0; i < numParticles; ++i) {
        positions[i] = particles[i].position;
        velocities[i] = particles[i].velocity;
    }

    // Upload positions
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO_positions);
    glBufferSubData(GL_ARRAY_BUFFER, 0, numParticles * sizeof(glm::vec3), positions.data());
    
    // Upload velocities  
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO_velocities);
    glBufferSubData(GL_ARRAY_BUFFER, 0, numParticles * sizeof(glm::vec3), velocities.data());
}

void GPUParticleDisplay::Render(const glm::mat4& view, const glm::mat4& projection) {
    Update(); // Efficiently copy data on GPU before rendering

    particleShader->use();

    // Scale the base sphere mesh to be proportional to the smoothing radius
    // Using a scale factor to make the visual size reasonable relative to the smoothing radius
    float particleRadius = simulation->GetSettings().smoothingRadius * 0.4f; // 40% of smoothing radius for visual appeal
    glm::mat4 model = glm::scale(glm::mat4(1.0f), glm::vec3(particleRadius));
    particleShader->setMat4("model", model);
    particleShader->setMat4("view", view);
    particleShader->setMat4("projection", projection);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_1D, gradientTexture);
    particleShader->setInt("ColourMap", 0);
    particleShader->setFloat("velocityMax", 15.0f);

        // Use the Mesh drawing interface (instanced draw via raw GL call)
        if (particleRenderMesh) {
            glBindVertexArray(particleRenderMesh->getVAO());
            GLsizei indexCount = static_cast<GLsizei>(particleRenderMesh->getIndexCount());
            if (indexCount > 0) {
                glDrawElementsInstanced(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0, static_cast<GLsizei>(simulation->GetNumParticles()));
            }
            glBindVertexArray(0);
        }
}
