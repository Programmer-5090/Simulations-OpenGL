#include "GPUFluidSimulation.h"
#include "ComputeHelper.h"
#include <iostream>
#include <random>
#include <cmath>
#include <algorithm>
#include <cstring>

GPUFluidSimulation::GPUFluidSimulation(int numParticles, const GPUSimulationSettings& settings)
    : numParticles(numParticles), settings(settings),
      fluidComputeProgram(0), particleBuffer(0), spatialLookupBuffer(0), startIndicesBuffer(0){

    if (!InitializeGPU()) {
        std::cerr << "Failed to initialize 3D GPU fluid simulation!" << std::endl;
        return;
    }

    InitializeParticles();
    UpdateConstants();
}

GPUFluidSimulation::~GPUFluidSimulation() {
    Cleanup();
}

bool GPUFluidSimulation::InitializeGPU() {
    fluidComputeProgram = ComputeHelper::LoadComputeShader("SPHFluid/shaders/FluidSim-3D.compute");
    if (fluidComputeProgram == 0) {
        return false;
    }

    particleBuffer = ComputeHelper::CreateBuffer(numParticles * sizeof(GPUParticle));
    spatialLookupBuffer = ComputeHelper::CreateBuffer(numParticles * sizeof(SpatialLookup));
    startIndicesBuffer = ComputeHelper::CreateBuffer(numParticles * sizeof(uint32_t));

    if (particleBuffer == 0 || spatialLookupBuffer == 0 || startIndicesBuffer == 0) {
        return false;
    }
    gpuSort.SetBuffers(spatialLookupBuffer, startIndicesBuffer);
    // Warm up the compute program and GPU to reduce first-dispatch stalls
    // This dispatch runs one small work-group to trigger any driver shader JIT and wake the GPU.
    glUseProgram(fluidComputeProgram);
    ComputeHelper::Dispatch(fluidComputeProgram, 1);
    return true;
}

void GPUFluidSimulation::InitializeParticles() {
    std::vector<GPUParticle> particles(numParticles);

    // Use a grid spawn so we can control spacing between particles.
    // spawnCenter is the center of the initial particle block in simulation space.
    glm::vec3 spawnCenter(0.0f, 0.0f, 0.0f);

    int particlesPerSide = std::max(1, static_cast<int>(std::cbrt(static_cast<float>(numParticles))) + 1);
    constexpr unsigned RNG_SEED = 42u;
    std::mt19937 gen(RNG_SEED);
    std::uniform_real_distribution<float> dis(-0.02f, 0.02f);

    int particleIndex = 0;
    // Determine spacing based on smoothing radius. A smaller spacingFactor makes
    // particles closer together. Default lowered to 0.6 to reduce inter-particle gaps.
    float spacingFactor = 0.6f; // smaller -> particles closer; tweak as needed
    float desiredSpacing = settings.smoothingRadius * spacingFactor;

    // Ensure the particle cube fits inside the simulation bounds. Compute the
    // maximum allowed spacing so the overall grid extent <= 90% of the smallest bound.
    int maxIndex = std::max(0, particlesPerSide - 1);
    float minBound = std::min(std::min(settings.boundsSize.x, settings.boundsSize.y), settings.boundsSize.z);
    float maxAllowedSpacing = 0.0f;
    if (maxIndex > 0) {
        maxAllowedSpacing = (minBound * 0.9f) / static_cast<float>(maxIndex);
    }

    float spacing = desiredSpacing;
    if (maxIndex > 0) {
        spacing = std::min(std::max(0.001f, desiredSpacing), maxAllowedSpacing);
    } else {
        spacing = 0.0f; // single particle case
    }

    // Center the grid around spawnCenter
    glm::vec3 gridCenterOffset = glm::vec3(static_cast<float>(maxIndex) * 0.5f) * spacing;

    for (int x = 0; x < particlesPerSide && particleIndex < numParticles; x++) {
        for (int y = 0; y < particlesPerSide && particleIndex < numParticles; y++) {
            for (int z = 0; z < particlesPerSide && particleIndex < numParticles; z++) {
                // Grid coordinates -> world position
                glm::vec3 basePos = glm::vec3(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)) * spacing;

                // jitter keeps the lattice from perfect alignment (helps break symmetry)
                glm::vec3 jitter(dis(gen), dis(gen), dis(gen));

                glm::vec3 pos = spawnCenter + (basePos - gridCenterOffset) + jitter;

                particles[particleIndex].position = pos;
                particles[particleIndex].velocity = glm::vec3(0.0f, 0.0f, 0.0f);
                particles[particleIndex].predictedPosition = pos;
                particles[particleIndex].density = 0.0f;
                particles[particleIndex].nearDensity = 0.0f;
                particles[particleIndex].pressure = 0.0f;
                particles[particleIndex].nearPressure = 0.0f;

                ++particleIndex;
            }
        }
    }

    ComputeHelper::WriteBuffer(particleBuffer, particles);
    gpuSort.SetBuffers(spatialLookupBuffer, startIndicesBuffer);
}

void GPUFluidSimulation::UpdateConstants() {
    const float pi = 3.14159265359f;
    float h = settings.smoothingRadius;

    poly6Factor = 315.0f / (64.0f * pi * std::powf(h, 9.0f));
    spikyPow2Factor = 15.0f / (2.0f * pi * std::powf(h, 5.0f));
    spikyPow3Factor = 15.0f / (pi * std::powf(h, 6.0f));
    spikyPow2DerivativeFactor = 15.0f / (pi * std::powf(h, 5.0f));
    spikyPow3DerivativeFactor = 45.0f / (pi * std::powf(h, 6.0f));
}

void GPUFluidSimulation::RunComputeKernel(KernelType kernel) {
    glUseProgram(fluidComputeProgram);

    // Bind buffers
    ComputeHelper::BindBuffer(particleBuffer, 0);
    ComputeHelper::BindBuffer(spatialLookupBuffer, 1);
    ComputeHelper::BindBuffer(startIndicesBuffer, 2);

    glUniform1i(glGetUniformLocation(fluidComputeProgram, "currentKernel"), static_cast<int>(kernel));

    int numGroups = ComputeHelper::GetThreadGroupSizes(numParticles, 64);
    ComputeHelper::Dispatch(fluidComputeProgram, numGroups);
}

void GPUFluidSimulation::UpdateSpatialHashing() {
    std::vector<uint32_t> startIndices(numParticles, static_cast<uint32_t>(numParticles));
    ComputeHelper::WriteBuffer(startIndicesBuffer, startIndices);

    RunComputeKernel(UpdateSpatialHashKernel);

    gpuSort.SortAndCalculateOffsets(spatialLookupBuffer, numParticles);
}

void GPUFluidSimulation::SetComputeUniforms() {
    glUseProgram(fluidComputeProgram);
    
    static int debugCount = 0;
    if (debugCount < 3) {
        std::cout << "DEBUG Uniforms frame " << debugCount << ":\n";
        std::cout << "  numParticles: " << numParticles << "\n";
        std::cout << "  gravity: " << settings.gravity << "\n";
        std::cout << "  collisionDamping: " << settings.collisionDamping << "\n";
        std::cout << "  boundsSize: (" << settings.boundsSize.x << ", " << settings.boundsSize.y << ", " << settings.boundsSize.z << ")\n";
        debugCount++;
    }
    
    glUniform1i(glGetUniformLocation(fluidComputeProgram, "numParticles"), numParticles);
    glUniform1f(glGetUniformLocation(fluidComputeProgram, "gravity"), settings.gravity);
    glUniform1f(glGetUniformLocation(fluidComputeProgram, "collisionDamping"), settings.collisionDamping);
    glUniform1f(glGetUniformLocation(fluidComputeProgram, "smoothingRadius"), settings.smoothingRadius);
    glUniform1f(glGetUniformLocation(fluidComputeProgram, "targetDensity"), settings.targetDensity);
    glUniform1f(glGetUniformLocation(fluidComputeProgram, "pressureMultiplier"), settings.pressureMultiplier);
    glUniform1f(glGetUniformLocation(fluidComputeProgram, "nearPressureMultiplier"), settings.nearPressureMultiplier);
    glUniform1f(glGetUniformLocation(fluidComputeProgram, "viscosityStrength"), settings.viscosityStrength);
    glUniform3f(glGetUniformLocation(fluidComputeProgram, "boundsSize"), settings.boundsSize.x, settings.boundsSize.y, settings.boundsSize.z);

    glUniform3f(glGetUniformLocation(fluidComputeProgram, "centre"), 0.0f, 0.0f, 0.0f);
    
    const float identityMatrix[16] = {
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        0.0, 0.0, 0.0, 1.0
    };
    glUniformMatrix4fv(glGetUniformLocation(fluidComputeProgram, "localToWorld"), 1, GL_FALSE, identityMatrix);
    glUniformMatrix4fv(glGetUniformLocation(fluidComputeProgram, "worldToLocal"), 1, GL_FALSE, identityMatrix);
    
    glUniform1f(glGetUniformLocation(fluidComputeProgram, "poly6Factor"), poly6Factor);
    glUniform1f(glGetUniformLocation(fluidComputeProgram, "spikyPow2Factor"), spikyPow2Factor);
    glUniform1f(glGetUniformLocation(fluidComputeProgram, "spikyPow3Factor"), spikyPow3Factor);
    glUniform1f(glGetUniformLocation(fluidComputeProgram, "spikyPow2DerivativeFactor"), spikyPow2DerivativeFactor);
    glUniform1f(glGetUniformLocation(fluidComputeProgram, "spikyPow3DerivativeFactor"), spikyPow3DerivativeFactor);
}

void GPUFluidSimulation::Update(float deltaTime) {
    deltaTime = std::min(deltaTime, 0.016f);
    
    float timeStep = deltaTime / settings.iterationsPerFrame * settings.timeScale;
    
    UpdateConstants();
    SetComputeUniforms();
    
    for (int i = 0; i < settings.iterationsPerFrame; i++) {
        glUseProgram(fluidComputeProgram);
        glUniform1f(glGetUniformLocation(fluidComputeProgram, "deltaTime"), timeStep);
        
        RunComputeKernel(ExternalForcesKernel);
        
        UpdateSpatialHashing();
        
        RunComputeKernel(CalculateDensitiesKernel);
        
        RunComputeKernel(CalculatePressureForcesKernel);
        RunComputeKernel(CalculateViscosityKernel);
        
        RunComputeKernel(UpdatePositionsKernel);
    }
}

std::vector<GPUParticle> GPUFluidSimulation::GetParticles() {
    return ComputeHelper::ReadBuffer<GPUParticle>(particleBuffer, numParticles);
}

void GPUFluidSimulation::SetSettings(const GPUSimulationSettings& newSettings) {
    settings = newSettings;
    UpdateConstants();
}

void GPUFluidSimulation::Reset() {
    InitializeParticles();
}

void GPUFluidSimulation::Cleanup() {
    ComputeHelper::Release(particleBuffer);
    ComputeHelper::Release(spatialLookupBuffer);
    ComputeHelper::Release(startIndicesBuffer);
    ComputeHelper::ReleaseProgram(fluidComputeProgram);
}
