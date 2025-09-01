#ifndef GPUPARTICLE_DISPLAY_H
#define GPUPARTICLE_DISPLAY_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>
#include "Shader.h"
#include "GPUFluidSimulation.h"

class GPUParticleDisplay {
public:
    GPUParticleDisplay(GPUFluidSimulation* sim, Shader* shader);
    ~GPUParticleDisplay();

    void Update();
    void Render(const glm::mat4& view, const glm::mat4& projection);

private:
    void InitializeRenderingResources();
    void GenerateCircleMesh();
    void CreateGradientTexture();
    void UpdateInstanceData();

    GPUFluidSimulation* simulation;
    Shader* particleShader;

    GLuint VAO;
    GLuint VBO;
    GLuint instanceVBO;
    GLuint gradientTexture;

    std::vector<glm::vec2> circleVertices;
    const int CIRCLE_SEGMENTS = 24;
};

#endif // GPUPARTICLE_DISPLAY_H
