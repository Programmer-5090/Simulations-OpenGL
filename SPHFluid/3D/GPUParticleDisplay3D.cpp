#include "GPUParticleDisplay3D.h"
#include "ComputeHelper.h"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include "GPUParticleDisplay3D.h"
#include "ComputeHelper.h"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

GPUParticleDisplay3D::GPUParticleDisplay3D(GPUFluidSimulation* sim, Shader* shader)
    : simulation(sim), particleShader(shader),
      instanceVBO_positions(0), instanceVBO_velocities(0), gradientTexture(0) {
    
    // Create the CPU-side parametric sphere and convert to a renderable Mesh
    particleMesh = new Sphere(1.0f, 16);
    // Convert generated parametric geometry into a Mesh for rendering
    particleRenderMesh = new Mesh(particleMesh->toMesh());
    InitializeRenderingResources();
}

GPUParticleDisplay3D::~GPUParticleDisplay3D() {
    delete particleMesh;
    if (instanceVBO_positions) glDeleteBuffers(1, &instanceVBO_positions);
    if (instanceVBO_velocities) glDeleteBuffers(1, &instanceVBO_velocities);
    if (gradientTexture) glDeleteTextures(1, &gradientTexture);
    if (particleRenderMesh) delete particleRenderMesh;
}

void GPUParticleDisplay3D::InitializeRenderingResources() {
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

void GPUParticleDisplay3D::CreateGradientTexture() {
    constexpr int gradientSize = 256;
    std::vector<unsigned char> gradientData(gradientSize * 3);
    for (int i = 0; i < gradientSize; ++i) {
        float t = static_cast<float>(i) / (gradientSize - 1);
        glm::vec3 c0 = glm::vec3(0.05f, 0.15f, 0.85f);
        glm::vec3 c1 = glm::vec3(0.0f, 0.8f, 1.0f);
        glm::vec3 c2 = glm::vec3(0.9f, 1.0f, 0.2f);
        glm::vec3 color = (t < 0.5f) ? glm::mix(c0, c1, t * 2.0f) : glm::mix(c1, c2, (t - 0.5f) * 2.0f);
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

void GPUParticleDisplay3D::Update() {
    // Data is already on the GPU. We can use glCopyBufferSubData to move it
    // from the simulation buffer to the instance VBOs without a round trip to the CPU.
    GLuint simBuffer = simulation->GetParticleBuffer();
    int numParticles = simulation->GetNumParticles();
    if (numParticles <= 0) return; // nothing to copy or render
    
    // Copy positions
    glBindBuffer(GL_COPY_READ_BUFFER, simBuffer);
    glBindBuffer(GL_COPY_WRITE_BUFFER, instanceVBO_positions);
    glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, static_cast<GLsizeiptr>(numParticles) * sizeof(glm::vec3));

    // Copy velocities (offset by sizeof(vec3) + padding)
    glBindBuffer(GL_COPY_READ_BUFFER, simBuffer);
    glBindBuffer(GL_COPY_WRITE_BUFFER, instanceVBO_velocities);
    glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, static_cast<GLintptr>(offsetof(GPUParticle, velocity)), 0, static_cast<GLsizeiptr>(numParticles) * sizeof(glm::vec3));
}

void GPUParticleDisplay3D::Render(const glm::mat4& view, const glm::mat4& projection) {
    Update(); // Efficiently copy data on GPU before rendering

    particleShader->use();

    // Scale the base sphere mesh to the desired particle size
    glm::mat4 model = glm::scale(glm::mat4(1.0f), glm::vec3(0.15f));
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
