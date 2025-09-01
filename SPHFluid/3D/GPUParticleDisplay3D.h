#ifndef GPUPARTICLE_DISPLAY3D_H
#define GPUPARTICLE_DISPLAY3D_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>
#include "Shader.h"
#include "GPUFluidSimulation3D.h"
#include "geometry/sphere.h" // Use the sphere class for rendering
#include "mesh.h"   // For converting parametric geometry to a renderable mesh (VAO/EBO)

class GPUParticleDisplay3D {
public:
    GPUParticleDisplay3D(GPUFluidSimulation* sim, Shader* shader);
    ~GPUParticleDisplay3D();

    void Update(); // This might not be needed if all data is on GPU
    void Render(const glm::mat4& view, const glm::mat4& projection);

private:
    void InitializeRenderingResources();
    void CreateGradientTexture();

    GPUFluidSimulation* simulation;
    Shader* particleShader;
    Sphere* particleMesh;
    Mesh* particleRenderMesh; // Mesh created from the Sphere's GeometryData

    GLuint instanceVBO_positions; // VBO for instance positions
    GLuint instanceVBO_velocities; // VBO for instance velocities
    GLuint gradientTexture;
};

#endif // GPUPARTICLE_DISPLAY3D_H
