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
    return true;
}

void GPUFluidSimulation::InitializeParticles() {
    std::vector<GPUParticle> particles(numParticles);

    glm::vec3 spawnSize(1.5f, 2.0f, 1.5f);
    glm::vec3 spawnCenter(0.0f, 0.0f, 0.0f);

    int particlesPerSide = std::max(1, static_cast<int>(std::cbrt(static_cast<float>(numParticles))) + 1);
    constexpr unsigned RNG_SEED = 42u;
    std::mt19937 gen(RNG_SEED);
    std::uniform_real_distribution<float> dis(-0.02f, 0.02f);

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
                );

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
