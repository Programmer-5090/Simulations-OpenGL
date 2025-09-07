#include "GPUFluidSimulation.h"
#include "ComputeHelper.h"
#include <iostream>
#include <random>
#include <cmath>
#include <algorithm>
#include <cstring>

// GPUFluidSimulation: manages GPU resources and runs 3D compute shader kernels.

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
    // IMPORTANT: Load the new 3D compute shader
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
    return true;
}

void GPUFluidSimulation::InitializeParticles() {
    std::vector<GPUParticle> particles(numParticles);

    // Create a very compact spawn area that fits well within bounds
    glm::vec3 spawnSize(1.5f, 2.0f, 1.5f);  // Small, dense spawn area
    glm::vec3 spawnCenter(0.0f, 0.0f, 0.0f);  // Start near bottom for gravity to work

    // Create particles in a tight cubic arrangement
    int particlesPerSide = std::max(1, static_cast<int>(std::cbrt(static_cast<float>(numParticles))) + 1);
    constexpr unsigned RNG_SEED = 42u;
    std::mt19937 gen(RNG_SEED);
    std::uniform_real_distribution<float> dis(-0.02f, 0.02f); // Very small jitter to avoid perfect grid

    int particleIndex = 0;
    for (int x = 0; x < particlesPerSide && particleIndex < numParticles; x++) {
        for (int y = 0; y < particlesPerSide && particleIndex < numParticles; y++) {
            for (int z = 0; z < particlesPerSide && particleIndex < numParticles; z++) {
                float tx = (particlesPerSide <= 1) ? 0.5f : static_cast<float>(x) / (particlesPerSide - 1);
                float ty = (particlesPerSide <= 1) ? 0.5f : static_cast<float>(y) / (particlesPerSide - 1);
                float tz = (particlesPerSide <= 1) ? 0.5f : static_cast<float>(z) / (particlesPerSide - 1);

                glm::vec3 jitter(dis(gen), dis(gen), dis(gen));
                glm::vec3 pos = glm::vec3(
                    (tx - 0.5f) * spawnSize.x + spawnCenter.x,
                    (ty - 0.5f) * spawnSize.y + spawnCenter.y,
                    (tz - 0.5f) * spawnSize.z + spawnCenter.z
                );// + jitter;

                // Initialize particles at rest
                particles[particleIndex].position = pos;
                particles[particleIndex].velocity = glm::vec3(0.0f, 0.0f, 0.0f); // Start at rest
                particles[particleIndex].predictedPosition = pos;
                particles[particleIndex].density = 0.0f;
                particles[particleIndex].nearDensity = 0.0f;
                particles[particleIndex].pressure = 0.0f;
                particles[particleIndex].nearPressure = 0.0f;

                ++particleIndex;
            }
        }
    }

    // Upload initial particle data to the GPU
    ComputeHelper::WriteBuffer(particleBuffer, particles);
    // Ensure the GPU sort helper knows which buffers to use (defensive - already set in InitializeGPU)
    gpuSort.SetBuffers(spatialLookupBuffer, startIndicesBuffer);
}

void GPUFluidSimulation::UpdateConstants() {
    const float pi = 3.14159265359f;
    float h = settings.smoothingRadius;

    // Correct 3D SPH kernel constants from the reference implementation
    poly6Factor = 315.0f / (64.0f * pi * std::powf(h, 9.0f));
    spikyPow2Factor = 15.0f / (2.0f * pi * std::powf(h, 5.0f)); // h^5 power
    spikyPow3Factor = 15.0f / (pi * std::powf(h, 6.0f));
    spikyPow2DerivativeFactor = 15.0f / (pi * std::powf(h, 5.0f)); // h^5 power
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
    // Prepare start indices buffer with sentinel value
    std::vector<uint32_t> startIndices(numParticles, static_cast<uint32_t>(numParticles));
    ComputeHelper::WriteBuffer(startIndicesBuffer, startIndices);

    RunComputeKernel(UpdateSpatialHashKernel);

    // Sort and compute offsets using GPUSort helper
    gpuSort.SortAndCalculateOffsets(spatialLookupBuffer, numParticles);
}

// In GPUFluidSimulation.cpp
void GPUFluidSimulation::SetComputeUniforms() {
    glUseProgram(fluidComputeProgram);
    
    // DEBUG: Print uniform values
    static int debugCount = 0;
    if (debugCount < 3) {
        std::cout << "DEBUG Uniforms frame " << debugCount << ":\n";
        std::cout << "  numParticles: " << numParticles << "\n";
        std::cout << "  gravity: " << settings.gravity << "\n";
        std::cout << "  collisionDamping: " << settings.collisionDamping << "\n";
        std::cout << "  boundsSize: (" << settings.boundsSize.x << ", " << settings.boundsSize.y << ", " << settings.boundsSize.z << ")\n";
        debugCount++;
    }
    
    // All your existing uniforms...
    glUniform1i(glGetUniformLocation(fluidComputeProgram, "numParticles"), numParticles);
    glUniform1f(glGetUniformLocation(fluidComputeProgram, "gravity"), settings.gravity);
    glUniform1f(glGetUniformLocation(fluidComputeProgram, "collisionDamping"), settings.collisionDamping);
    glUniform1f(glGetUniformLocation(fluidComputeProgram, "smoothingRadius"), settings.smoothingRadius);
    glUniform1f(glGetUniformLocation(fluidComputeProgram, "targetDensity"), settings.targetDensity);
    glUniform1f(glGetUniformLocation(fluidComputeProgram, "pressureMultiplier"), settings.pressureMultiplier);
    glUniform1f(glGetUniformLocation(fluidComputeProgram, "nearPressureMultiplier"), settings.nearPressureMultiplier);
    glUniform1f(glGetUniformLocation(fluidComputeProgram, "viscosityStrength"), settings.viscosityStrength);
    glUniform3f(glGetUniformLocation(fluidComputeProgram, "boundsSize"), settings.boundsSize.x, settings.boundsSize.y, settings.boundsSize.z);

    // --- NEW UNIFORMS TO MATCH REFERENCE ---
    // Since we don't have a scene transform, we'll use identity matrices and a zero center.
    // This makes the shader logic work while replicating the old behavior.
    glUniform3f(glGetUniformLocation(fluidComputeProgram, "centre"), 0.0f, 0.0f, 0.0f);
    
    // Identity matrix (no rotation or translation)
    const float identityMatrix[16] = {
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        0.0, 0.0, 0.0, 1.0
    };
    glUniformMatrix4fv(glGetUniformLocation(fluidComputeProgram, "localToWorld"), 1, GL_FALSE, identityMatrix);
    glUniformMatrix4fv(glGetUniformLocation(fluidComputeProgram, "worldToLocal"), 1, GL_FALSE, identityMatrix);
    // --- END OF NEW UNIFORMS ---
    
    // Kernel factors
    glUniform1f(glGetUniformLocation(fluidComputeProgram, "poly6Factor"), poly6Factor);
    glUniform1f(glGetUniformLocation(fluidComputeProgram, "spikyPow2Factor"), spikyPow2Factor);
    glUniform1f(glGetUniformLocation(fluidComputeProgram, "spikyPow3Factor"), spikyPow3Factor);
    glUniform1f(glGetUniformLocation(fluidComputeProgram, "spikyPow2DerivativeFactor"), spikyPow2DerivativeFactor);
    glUniform1f(glGetUniformLocation(fluidComputeProgram, "spikyPow3DerivativeFactor"), spikyPow3DerivativeFactor);
}

void GPUFluidSimulation::Update(float deltaTime) {
    // CRITICAL FIX: Clamp deltaTime to prevent huge first-frame jumps
    deltaTime = std::min(deltaTime, 0.016f); // Cap at 60 FPS (16ms) - more conservative
    
    float timeStep = deltaTime / settings.iterationsPerFrame * settings.timeScale;
    
    UpdateConstants();
    SetComputeUniforms();
    
    for (int i = 0; i < settings.iterationsPerFrame; i++) {
        // Set delta time for this iteration
        glUseProgram(fluidComputeProgram);
        glUniform1f(glGetUniformLocation(fluidComputeProgram, "deltaTime"), timeStep);
        
        RunComputeKernel(ExternalForcesKernel);
        
        // STEP 1: Enable spatial hashing ✅
        UpdateSpatialHashing();
        
        // STEP 2: Enable density calculation
        RunComputeKernel(CalculateDensitiesKernel);
        
        // COMMENTED OUT FOR TESTING - Add back gradually
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
