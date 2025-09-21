#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>
#include "GPUSort.h"

// Note: std430 layout requires proper alignment for GPU compatibility.
// Each vec3 must be followed by padding to align to 16-byte boundaries.
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
    uint32_t isBoundary;    // 1 if particle is a boundary particle, else 0
    float _padding4;        // Padding to align the entire struct to 16 bytes
    float _padding5;        // Additional padding for alignment
    float _padding6;        // Additional padding for alignment

    GPUParticle() : position(0), _padding1(0), velocity(0), _padding2(0), 
                   predictedPosition(0), _padding3(0),
                   density(0), nearDensity(0), pressure(0), nearPressure(0), 
                   isBoundary(0), _padding4(0), _padding5(0), _padding6(0) {}
};

// std430 layout in the compute shader packs this struct to 80 bytes (5 x 16B).
// Enforce this at compile time so buffer strides always match the GPU side.
static_assert(sizeof(GPUParticle) == 80, "GPUParticle must be 80 bytes to match GLSL std430 layout");

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
    
    // Boundary particle settings
    float boundarySpacing = 0.5f;       // Spacing between boundary particles within a layer
    float layerSpacing = 0.3f;          // Distance between layers (typically particle diameter)
    int boundaryLayers = 2;             // Number of layers of boundary particles
    bool enableBoundaryParticles = true; // Enable/disable boundary particles
    bool coverEdges = true;              // If true, include edge lines on all faces (duplicates allowed at seams)
    bool includeTopFace = false;         // If true, also add a top face (lid)
};

class GPUFluidSimulation {
private:
    int numFluidParticles;
    int numBoundaryParticles;
    int numTotalParticles;
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
    int GetNumFluidParticles() const { return numFluidParticles; }
    int GetNumBoundaryParticles() const { return numBoundaryParticles; }
    int GetNumTotalParticles() const { return numTotalParticles; }

    // Debug: dump boundary particles grouped by layer index (shells offset by layerSpacing) to a txt file.
    // 'axis' is only used as a label in the filename; maxLayers/maxPerLayer limit verbosity.
    void DebugDumpBoundaryLayers(const char* tag, char axis, int maxLayers, int maxPerLayer) const;

private:
    bool InitializeGPU();
    void InitializeParticles();
    void InitializeBoundaryParticles();
    int CalculateNumBoundaryParticles() const;
    void AddCornerParticles(std::vector<GPUParticle>& particles, 
                            int& boundaryIndex, int layer, float offset,
                            const glm::vec3& baseMin, const glm::vec3& baseMax);
    void UpdateConstants();
    void UpdateSpatialHashing();
    void CalculateStartIndices();

    void SetComputeUniforms();
    void RunComputeKernel(KernelType kernel);

    void Cleanup();
};
