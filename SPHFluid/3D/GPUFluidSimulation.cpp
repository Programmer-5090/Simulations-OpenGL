#include "GPUFluidSimulation3D.h"
#include "ComputeHelper.h"
#include <iostream>
#include <random>
#include <cmath>
#include <algorithm>
#include <cstring>

// GPUFluidSimulation: manages GPU resources and runs 3D compute shader kernels.

GPUFluidSimulation::GPUFluidSimulation(int numParticles, const GPUSimulationSettings& settings)
    : numParticles(numParticles), settings(settings),
      fluidComputeProgram(0), particleBuffer(0), spatialLookupBuffer(0), startIndicesBuffer(0), pressureBuffer(0) {

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
    // Pressure buffer stores 2 floats (pressure, nearPressure) per particle
    pressureBuffer = ComputeHelper::CreateBuffer(numParticles * 2 * sizeof(float));


    if (particleBuffer == 0 || spatialLookupBuffer == 0 || startIndicesBuffer == 0 || pressureBuffer == 0) {
        return false;
    }

    gpuSort.SetBuffers(spatialLookupBuffer, startIndicesBuffer);
    return true;
}

void GPUFluidSimulation::InitializeParticles() {
    std::vector<GPUParticle> particles(numParticles);

    glm::vec3 spawnSize(8.0f, 12.0f, 8.0f);
    glm::vec3 spawnCenter(0.0f, 6.0f, 0.0f);

    // Create a cubic grid filled to approximately numParticles. Use a fixed RNG for reproducibility.
    int particlesPerSide = std::max(1, static_cast<int>(std::cbrt(static_cast<float>(numParticles))));
    constexpr unsigned RNG_SEED = 42u;
    std::mt19937 gen(RNG_SEED);
    std::uniform_real_distribution<float> dis(-0.08f, 0.08f);

    int particleIndex = 0;
    for (int z = 0; z < particlesPerSide && particleIndex < numParticles; ++z) {
        for (int y = 0; y < particlesPerSide && particleIndex < numParticles; ++y) {
            for (int x = 0; x < particlesPerSide && particleIndex < numParticles; ++x) {
                float tx = (particlesPerSide <= 1) ? 0.5f : static_cast<float>(x) / (particlesPerSide - 1);
                float ty = (particlesPerSide <= 1) ? 0.5f : static_cast<float>(y) / (particlesPerSide - 1);
                float tz = (particlesPerSide <= 1) ? 0.5f : static_cast<float>(z) / (particlesPerSide - 1);

                glm::vec3 jitter(dis(gen), dis(gen), dis(gen));
                glm::vec3 pos = glm::vec3(
                    (tx - 0.5f) * spawnSize.x + spawnCenter.x,
                    (ty - 0.5f) * spawnSize.y + spawnCenter.y,
                    (tz - 0.5f) * spawnSize.z + spawnCenter.z
                ) + jitter;

                // Initialize all particle fields explicitly to match 2D initializer patterns and avoid garbage.
                particles[particleIndex].position = pos;
                particles[particleIndex].pad1 = 0.0f;
                particles[particleIndex].velocity = glm::vec3(0.0f);
                particles[particleIndex].pad2 = 0.0f;
                particles[particleIndex].predictedPosition = pos;
                particles[particleIndex].pad3 = 0.0f;
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
    std::vector<GPUParticle> particles(numParticles);

    glm::vec3 spawnSize(8.0f, 12.0f, 8.0f);
    glm::vec3 spawnCenter(0.0f, 6.0f, 0.0f);

    // Create a cubic grid filled to approximately numParticles. Use a fixed RNG for reproducibility.
    int particlesPerSide = std::max(1, static_cast<int>(std::cbrt(static_cast<float>(numParticles))));
    constexpr unsigned RNG_SEED = 42u;
    std::mt19937 gen(RNG_SEED);
    std::uniform_real_distribution<float> dis(-0.08f, 0.08f);

    int particleIndex = 0;
    for (int z = 0; z < particlesPerSide && particleIndex < numParticles; ++z) {
        for (int y = 0; y < particlesPerSide && particleIndex < numParticles; ++y) {
            for (int x = 0; x < particlesPerSide && particleIndex < numParticles; ++x) {
                float tx = (particlesPerSide <= 1) ? 0.5f : static_cast<float>(x) / (particlesPerSide - 1);
                float ty = (particlesPerSide <= 1) ? 0.5f : static_cast<float>(y) / (particlesPerSide - 1);
                float tz = (particlesPerSide <= 1) ? 0.5f : static_cast<float>(z) / (particlesPerSide - 1);

                glm::vec3 jitter(dis(gen), dis(gen), dis(gen));
                glm::vec3 pos = glm::vec3(
                    (tx - 0.5f) * spawnSize.x + spawnCenter.x,
                    (ty - 0.5f) * spawnSize.y + spawnCenter.y,
                    (tz - 0.5f) * spawnSize.z + spawnCenter.z
                ) + jitter;

                // Initialize all particle fields explicitly to match 2D initializer patterns and avoid garbage.
                particles[particleIndex].position = pos;
                particles[particleIndex].pad1 = 0.0f;
                particles[particleIndex].velocity = glm::vec3(0.0f);
                particles[particleIndex].pad2 = 0.0f;
                particles[particleIndex].predictedPosition = pos;
                particles[particleIndex].pad3 = 0.0f;
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
    glUniform1f(glGetUniformLocation(fluidComputeProgram, "gravity"), settings.gravity);
    glUniform1f(glGetUniformLocation(fluidComputeProgram, "collisionDamping"), settings.collisionDamping);
    glUniform1f(glGetUniformLocation(fluidComputeProgram, "smoothingRadius"), settings.smoothingRadius);
    glUniform1f(glGetUniformLocation(fluidComputeProgram, "targetDensity"), settings.targetDensity);
    glUniform1f(glGetUniformLocation(fluidComputeProgram, "pressureMultiplier"), settings.pressureMultiplier);
    glUniform1f(glGetUniformLocation(fluidComputeProgram, "nearPressureMultiplier"), settings.nearPressureMultiplier);
    glUniform1f(glGetUniformLocation(fluidComputeProgram, "viscosityStrength"), settings.viscosityStrength);
    glUniform3f(glGetUniformLocation(fluidComputeProgram, "boundsSize"), settings.boundsSize.x, settings.boundsSize.y, settings.boundsSize.z);

    // 3D Kernel factors
    glUniform1f(glGetUniformLocation(fluidComputeProgram, "poly6Factor"), poly6Factor);
    glUniform1f(glGetUniformLocation(fluidComputeProgram, "spikyFactor"), spikyFactor);
    glUniform1f(glGetUniformLocation(fluidComputeProgram, "spikyDerivativeFactor"), spikyDerivativeFactor);
    glUniform1f(glGetUniformLocation(fluidComputeProgram, "viscosityFactor"), viscosityFactor);
}

void GPUFluidSimulation::RunComputeKernel(KernelType kernel) {
    glUseProgram(fluidComputeProgram);

    // Bind buffers
    ComputeHelper::BindBuffer(particleBuffer, 0);
    ComputeHelper::BindBuffer(spatialLookupBuffer, 1);
    ComputeHelper::BindBuffer(startIndicesBuffer, 2);
    ComputeHelper::BindBuffer(pressureBuffer, 3);

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

void GPUFluidSimulation::SetComputeUniforms() {
    glUseProgram(fluidComputeProgram);

    glUniform1i(glGetUniformLocation(fluidComputeProgram, "numParticles"), numParticles);
    glUniform1f(glGetUniformLocation(fluidComputeProgram, "gravity"), settings.gravity);
    glUniform1f(glGetUniformLocation(fluidComputeProgram, "collisionDamping"), settings.collisionDamping);
    glUniform1f(glGetUniformLocation(fluidComputeProgram, "smoothingRadius"), settings.smoothingRadius);
    glUniform1f(glGetUniformLocation(fluidComputeProgram, "targetDensity"), settings.targetDensity);
    glUniform1f(glGetUniformLocation(fluidComputeProgram, "pressureMultiplier"), settings.pressureMultiplier);
    glUniform1f(glGetUniformLocation(fluidComputeProgram, "nearPressureMultiplier"), settings.nearPressureMultiplier);
    glUniform1f(glGetUniformLocation(fluidComputeProgram, "viscosityStrength"), settings.viscosityStrength);
    glUniform3f(glGetUniformLocation(fluidComputeProgram, "boundsSize"), settings.boundsSize.x, settings.boundsSize.y, settings.boundsSize.z);

    // Kernel factors
    glUniform1f(glGetUniformLocation(fluidComputeProgram, "poly6Factor"), poly6Factor);
    glUniform1f(glGetUniformLocation(fluidComputeProgram, "spikyFactor"), spikyFactor);
    glUniform1f(glGetUniformLocation(fluidComputeProgram, "spikyDerivativeFactor"), spikyDerivativeFactor);
    glUniform1f(glGetUniformLocation(fluidComputeProgram, "viscosityFactor"), viscosityFactor);
}

void GPUFluidSimulation::Update(float deltaTime) {
    float timeStep = deltaTime / settings.iterationsPerFrame * settings.timeScale;

    UpdateConstants();
    SetComputeUniforms();

    for (int i = 0; i < settings.iterationsPerFrame; ++i) {
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
    ComputeHelper::Release(pressureBuffer);
    ComputeHelper::ReleaseProgram(fluidComputeProgram);
}
