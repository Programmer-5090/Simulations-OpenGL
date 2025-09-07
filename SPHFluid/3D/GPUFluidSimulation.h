#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>
#include "GPUSort.h"

// Note: std430 layout requires vec3 to be aligned to 16 bytes.
// We add padding to ensure the struct size is a multiple of 16 bytes.
struct GPUParticle {
    glm::vec3 position;
    float _padding1;        // Explicit padding to align to 16 bytes
    glm::vec3 velocity;
    float _padding2;        // Explicit padding to align to 16 bytes
    glm::vec3 predictedPosition;
    float _padding3;        // Explicit padding to align to 16 bytes
    float density;
    float nearDensity;
    float pressure;
    float nearPressure;

    GPUParticle() : position(0), _padding1(0), velocity(0), _padding2(0), 
                   predictedPosition(0), _padding3(0),
                   density(0), nearDensity(0), pressure(0), nearPressure(0) {}
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
    float collisionDamping = 0.95f;
    float smoothingRadius = 2.0f;
    float targetDensity = 1.0f;
    float pressureMultiplier = 1.0f;
    float nearPressureMultiplier = 0.5f;
    float viscosityStrength = 0.1f;
    glm::vec3 boundsSize = glm::vec3(20, 20, 20);
    float boundaryForceMultiplier = 120.0f;
    float boundaryForceDistance = 1.0f;
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

    // GPU sorting
    GPUSort gpuSort;

    // Precomputed constants for 3D kernels
    float poly6Factor;
    float spikyPow2Factor;
    float spikyPow3Factor;
    float spikyPow2DerivativeFactor;
    float spikyPow3DerivativeFactor;

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
    std::vector<GPUParticle> GetParticles();
    const GPUSimulationSettings& GetSettings() const { return settings; }
    void SetSettings(const GPUSimulationSettings& newSettings);

    GLuint GetParticleBuffer() const { return particleBuffer; }
    int GetNumParticles() const { return numParticles; }

private:
    bool InitializeGPU();
    void InitializeParticles();
    void UpdateConstants();
    void UpdateSpatialHashing();
    void CalculateStartIndices();

    void SetComputeUniforms();
    void RunComputeKernel(KernelType kernel);

    void Cleanup();
};
