#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>
#include "GPUSort.h"

// Note: std430 layout requires vec3 to be aligned to 16 bytes.
// We add padding to ensure the struct size is a multiple of 16 bytes.
struct GPUParticle {
    glm::vec3 position;
    float pad1; // Padding for alignment
    glm::vec3 velocity;
    float pad2; // Padding for alignment
    glm::vec3 predictedPosition;
    float pad3; // Padding for alignment
    float density;
    float nearDensity;
    float pressure;
    float nearPressure;
};

struct SpatialLookup {
    uint32_t particleIndex;
    uint32_t hash;
    uint32_t cellKey;
};

struct GPUSimulationSettings {
    float timeScale = 1.0f;
    int iterationsPerFrame = 1;
    float gravity = -9.81f;
    float collisionDamping = 0.8f;
    float smoothingRadius = 1.0f;
    float targetDensity = 15.0f;
    float pressureMultiplier = 100.0f;
    float nearPressureMultiplier = 20.0f;
    float viscosityStrength = 0.05f;
    glm::vec3 boundsSize = glm::vec3(20, 20, 20);
};

class GPUFluidSimulation {
private:
    int numParticles;
    GPUSimulationSettings settings;

    // Compute shader program
    GLuint fluidComputeProgram;

    // GPU buffers
    GLuint particleBuffer;
    GLuint spatialLookupBuffer;
    GLuint startIndicesBuffer;
    GLuint pressureBuffer;

    // GPU sorting
    GPUSort gpuSort;

    // Precomputed constants for 3D kernels
    float poly6Factor;
    float spikyFactor;
    float spikyDerivativeFactor;
    float viscosityFactor;

    // Kernel constants
    enum KernelType {
        ExternalForcesKernel = 0,
        UpdateSpatialHashKernel = 1,
        CalculateDensitiesKernel = 2,
        CalculatePressureForcesKernel = 3,
        CalculateViscosityKernel = 4,
        UpdatePositionsKernel = 5
    };

public:
    GPUFluidSimulation(int numParticles, const GPUSimulationSettings& settings);
    ~GPUFluidSimulation();

    void Update(float deltaTime);
    void Reset();

    // Data access
    const GPUSimulationSettings& GetSettings() const { return settings; }
    void SetSettings(const GPUSimulationSettings& newSettings);

    GLuint GetParticleBuffer() const { return particleBuffer; }
    int GetNumParticles() const { return numParticles; }

private:
    bool InitializeGPU();
    void InitializeParticles();
    void UpdateConstants();
    void UpdateSpatialHashing();

    void SetComputeUniforms();
    void RunComputeKernel(KernelType kernel);

    void Cleanup();
};
