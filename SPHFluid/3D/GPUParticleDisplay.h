#ifndef GPUPARTICLE_DISPLAY3D_H
#define GPUPARTICLE_DISPLAY3D_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>
#include "Shader.h"
#include "GPUFluidSimulation.h"
#include "geometry/sphere.h" // Use the sphere class for rendering
#include "mesh.h"   // For converting parametric geometry to a renderable mesh (VAO/EBO)

class GPUParticleDisplay {
public:
    GPUParticleDisplay(GPUFluidSimulation* sim, Shader* shader);
    ~GPUParticleDisplay();

    void Update();
    void Render(const glm::mat4& view, const glm::mat4& projection);

    // Visually offset particle positions without affecting simulation physics
    void SetWorldOffset(const glm::vec3& offset) { worldOffset = offset; }
    
    // Toggle ghost particle visibility
    void SetShowGhostParticles(bool show) { showGhostParticles = show; }

private:
    void InitializeRenderingResources();
    void CreateGradientTexture();
    void UpdateInstanceData();

    GPUFluidSimulation* simulation;
    Shader* particleShader;
    Sphere* particleMesh;
    Mesh* particleRenderMesh; // Mesh created from the Sphere's GeometryData

    GLuint instanceVBO_positions; // VBO for instance positions
    GLuint instanceVBO_velocities; // VBO for instance velocities
    GLuint instanceVBO_isBoundary; // VBO for boundary particle flags
    GLuint gradientTexture;

    glm::vec3 worldOffset = glm::vec3(0.0f);
    bool showGhostParticles = true;  // Toggle for ghost particle visibility
};

#endif // GPUPARTICLE_DISPLAY3D_H
