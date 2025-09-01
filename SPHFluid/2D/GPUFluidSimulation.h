#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>
#include "GPUSort.h"

struct GPUParticle {
    glm::vec2 position;
    glm::vec2 velocity;
    glm::vec2 predictedPosition;
    float density;
    float nearDensity;
    float pressure;
    float nearPressure;
    
    GPUParticle() : position(0), velocity(0), predictedPosition(0),
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
    glm::vec2 boundsSize = glm::vec2(20, 20);
    glm::vec2 obstacleSize = glm::vec2(4, 4);
    glm::vec2 obstacleCenter = glm::vec2(0, 0);
    float boundaryForceMultiplier = 50.0f;
    float boundaryForceDistance = 0.4f;

    // Interaction settings
    float interactionRadius = 10.0f;
    float interactionStrength = 25.0f;
    glm::vec2 mousePosition = glm::vec2(0, 0);
    bool leftMousePressed = false;
    bool rightMousePressed = false;
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
    
    // Precomputed constants
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

    void ReadBackSample(int count = 10);
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
