#ifndef GPUPARTICLE_DISPLAY3D_H
#define GPUPARTICLE_DISPLAY3D_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>
#include "Shader.h"
#include "GPUFluidSimulation.h"
#include "geometry/sphere.h"
#include "mesh.h"

class GPUParticleDisplay {
public:
    GPUParticleDisplay(GPUFluidSimulation* sim, Shader* shader);
    ~GPUParticleDisplay();

    void Render(const glm::mat4& view, const glm::mat4& projection);

    // Visually offset particle positions without affecting simulation physics
    void SetWorldOffset(const glm::vec3& offset) { worldOffset = offset; }

private:
    void InitializeRenderingResources();
    void CreateGradientTexture();

    GPUFluidSimulation* simulation;
    Shader* particleShader;
    Sphere* particleMesh;
    Mesh* particleRenderMesh;

    GLuint instanceVBO_positions;
    GLuint instanceVBO_velocities;
    GLuint gradientTexture;

    glm::vec3 worldOffset = glm::vec3(0.0f);
};

#endif // GPUPARTICLE_DISPLAY3D_H
