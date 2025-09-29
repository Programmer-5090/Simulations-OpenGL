#include "GPUParticleDisplay.h"
#include "ComputeHelper.h"
#include "GPUFluidSimulation.h"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <cstddef> // for offsetof


GPUParticleDisplay::GPUParticleDisplay(GPUFluidSimulation* sim, Shader* shader)
    : simulation(sim), particleShader(shader),
      instanceVBO_positions(0), instanceVBO_velocities(0), gradientTexture(0) {
    
    particleMesh = new Sphere(1.0f, 16);
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
    // Instead of copying particle data back to CPU every frame, bind the
    // GPU particle buffer directly as the source for instanced attributes.
    // This avoids glMapBuffer/glGetBuffer stalls and eliminates CPU-GPU sync.

    glBindVertexArray(particleRenderMesh->getVAO());

    GLuint simBuffer = simulation->GetParticleBuffer();
    // Bind the same buffer as ARRAY_BUFFER to describe vertex attrib layout
    glBindBuffer(GL_ARRAY_BUFFER, simBuffer);

    GLsizei stride = static_cast<GLsizei>(sizeof(GPUParticle));

    // position attribute (vec3) at offset offsetof(GPUParticle, position)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(GPUParticle, position));
    glVertexAttribDivisor(2, 1);

    // velocity attribute (vec3) at offset offsetof(GPUParticle, velocity)
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(GPUParticle, velocity));
    glVertexAttribDivisor(3, 1);

    // Unbind to avoid accidental state leaks. The VAO stores the attribute bindings.
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
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
    // No CPU-side readback required. Instance attributes read directly from
    // the GPU particle buffer bound to the VAO. This function remains as a
    // placeholder in case future CPU-side work is needed.
}

void GPUParticleDisplay::Render(const glm::mat4& view, const glm::mat4& projection) {
    Update(); // No-op now; retained for compatibility

    particleShader->use();

    // Scale the base sphere mesh to be proportional to the smoothing radius
    float particleRadius = 0.07f; // Adjust as needed for visual size
    glm::mat4 model = glm::scale(glm::mat4(1.0f), glm::vec3(particleRadius));
    particleShader->setMat4("model", model);
    particleShader->setMat4("view", view);
    particleShader->setMat4("projection", projection);

    // Pass world offset into the shader so GPU-side instance positions can
    // be offset without changing simulation coordinates.
    particleShader->setVec3("worldOffset", worldOffset);

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
