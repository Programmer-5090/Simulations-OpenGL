#include "GPUFluidSimulation.h"
#include "ComputeHelper.h"
#include <iostream>
#include <random>
#include <cmath>
#include <algorithm>
#include <cstring>

// GPUFluidSimulation: manages GPU resources and runs compute shader kernels.

GPUFluidSimulation::GPUFluidSimulation(int numParticles, const GPUSimulationSettings& settings)
    : numParticles(numParticles), settings(settings), 
      fluidComputeProgram(0), particleBuffer(0), spatialLookupBuffer(0), startIndicesBuffer(0) {
    
    if (!InitializeGPU()) {
        std::cerr << "Failed to initialize GPU fluid simulation!" << std::endl;
        return;
    }

    InitializeParticles();
    UpdateConstants();
}

GPUFluidSimulation::~GPUFluidSimulation() {
    Cleanup();
}

bool GPUFluidSimulation::InitializeGPU() {
    fluidComputeProgram = ComputeHelper::LoadComputeShader("SPHFluid/shaders/FluidSim-2D.compute");
    if (fluidComputeProgram == 0) {
        return false;
    }

    particleBuffer = ComputeHelper::CreateBuffer(numParticles * sizeof(GPUParticle));
    spatialLookupBuffer = ComputeHelper::CreateBuffer(numParticles * sizeof(SpatialLookup));
    startIndicesBuffer = ComputeHelper::CreateBuffer(numParticles * sizeof(uint32_t));

    if (particleBuffer == 0 || spatialLookupBuffer == 0 || startIndicesBuffer == 0) {
        return false;
    }

    return true;
}

void GPUFluidSimulation::InitializeParticles() {
    std::vector<GPUParticle> particles(numParticles);
    
    glm::vec2 spawnSize(10.0f, 10.0f);
    glm::vec2 spawnCenter(0.0f, 2.0f);
    
    float aspectRatio = spawnSize.x / spawnSize.y;
    int numX = static_cast<int>(std::ceil(std::sqrt(aspectRatio * numParticles)));
    int numY = static_cast<int>(std::ceil(static_cast<float>(numParticles) / numX));

    // Use a named constexpr seed for clarity and easy change
    constexpr unsigned RNG_SEED = 42u; // Fixed seed for reproducibility
    std::mt19937 gen(RNG_SEED);
    std::uniform_real_distribution<float> dis(-0.5f, 0.5f);
    
    int particleIndex = 0;
    for(int y = 0; y < numY && particleIndex < numParticles; y++) {
        for(int x = 0; x < numX && particleIndex < numParticles; x++) {
            float tx = (numX <= 1) ? 0.5f : static_cast<float>(x) / (numX - 1);
            float ty = (numY <= 1) ? 0.5f : static_cast<float>(y) / (numY - 1);
            
            // Add small random jitter
            glm::vec2 jitter(dis(gen) * 0.1f, dis(gen) * 0.1f);
            
            particles[particleIndex].position = glm::vec2(
                (tx - 0.5f) * spawnSize.x + spawnCenter.x,
                (ty - 0.5f) * spawnSize.y + spawnCenter.y
            ) + jitter;

            particles[particleIndex].velocity = glm::vec2(0.0f, 0.0f);
            particles[particleIndex].predictedPosition = particles[particleIndex].position;
            particles[particleIndex].density = 0.0f;
            particles[particleIndex].nearDensity = 0.0f;
            particles[particleIndex].pressure = 0.0f;
            particles[particleIndex].nearPressure = 0.0f;
            
            particleIndex++;
        }
    }
    
    ComputeHelper::WriteBuffer(particleBuffer, particles);
    gpuSort.SetBuffers(spatialLookupBuffer, startIndicesBuffer);
}

void GPUFluidSimulation::UpdateConstants() {
    const float pi = 3.14159265359f;
    float h = settings.smoothingRadius;

    poly6Factor = 4.0f / (pi * std::pow(h, 8));
    spikyPow2Factor = 6.0f / (pi * std::pow(h, 4));
    spikyPow3Factor = 10.0f / (pi * std::pow(h, 5));
    spikyPow2DerivativeFactor = 12.0f / (pi * std::pow(h, 4));
    spikyPow3DerivativeFactor = 30.0f / (pi * std::pow(h, 5));
}

void GPUFluidSimulation::Update(float deltaTime) {
    float timeStep = deltaTime / settings.iterationsPerFrame * settings.timeScale;
    
    UpdateConstants();
    SetComputeUniforms();
    
    for (int i = 0; i < settings.iterationsPerFrame; i++) {
        // Set delta time for this iteration
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

void GPUFluidSimulation::ReadBackSample(int count) {
    if (count <= 0) return;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, particleBuffer);
    size_t mapSize = sizeof(GPUParticle) * std::min(numParticles, count);
    void* ptr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, mapSize, GL_MAP_READ_BIT);
    if (!ptr) return;
    GPUParticle* particles = reinterpret_cast<GPUParticle*>(ptr);
    std::ostringstream oss;
    oss << "Sample Particles (index: density nearDensity pressure nearPressure):\n";
    for (int i = 0; i < std::min(numParticles, count); ++i) {
        oss << i << ": " << particles[i].density << " " << particles[i].nearDensity
            << " " << particles[i].pressure << " " << particles[i].nearPressure << "\n";
    }
    glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    const std::string out = oss.str();
    std::cout << out << std::endl;
}

void GPUFluidSimulation::UpdateSpatialHashing() {
    CalculateStartIndices();

    RunComputeKernel(UpdateSpatialHashKernel);

    gpuSort.SortAndCalculateOffsets(spatialLookupBuffer, numParticles);
}

void GPUFluidSimulation::CalculateStartIndices() {
    // Initialize start index buffer with sentinel value (numParticles) meaning
    // "empty cell". Use explicit cast to make intent clear.
    std::vector<uint32_t> startIndices(numParticles, static_cast<uint32_t>(numParticles));
    ComputeHelper::WriteBuffer(startIndicesBuffer, startIndices);
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
    glUniform2f(glGetUniformLocation(fluidComputeProgram, "boundsSize"), settings.boundsSize.x, settings.boundsSize.y);
    glUniform2f(glGetUniformLocation(fluidComputeProgram, "obstacleSize"), settings.obstacleSize.x, settings.obstacleSize.y);
    glUniform2f(glGetUniformLocation(fluidComputeProgram, "obstacleCenter"), settings.obstacleCenter.x, settings.obstacleCenter.y);

    glUniform1f(glGetUniformLocation(fluidComputeProgram, "boundaryForceMultiplier"), settings.boundaryForceMultiplier);
    glUniform1f(glGetUniformLocation(fluidComputeProgram, "boundaryForceDistance"), settings.boundaryForceDistance);
    
    // Interaction uniforms
    glUniform1f(glGetUniformLocation(fluidComputeProgram, "interactionRadius"), settings.interactionRadius);
    glUniform1f(glGetUniformLocation(fluidComputeProgram, "interactionStrength"), settings.interactionStrength);
    glUniform2f(glGetUniformLocation(fluidComputeProgram, "mousePosition"), settings.mousePosition.x, settings.mousePosition.y);
    glUniform1i(glGetUniformLocation(fluidComputeProgram, "leftMousePressed"), settings.leftMousePressed ? 1 : 0);
    glUniform1i(glGetUniformLocation(fluidComputeProgram, "rightMousePressed"), settings.rightMousePressed ? 1 : 0);
    
    // Kernel factors
    glUniform1f(glGetUniformLocation(fluidComputeProgram, "poly6Factor"), poly6Factor);
    glUniform1f(glGetUniformLocation(fluidComputeProgram, "spikyPow2Factor"), spikyPow2Factor);
    glUniform1f(glGetUniformLocation(fluidComputeProgram, "spikyPow3Factor"), spikyPow3Factor);
    glUniform1f(glGetUniformLocation(fluidComputeProgram, "spikyPow2DerivativeFactor"), spikyPow2DerivativeFactor);
    glUniform1f(glGetUniformLocation(fluidComputeProgram, "spikyPow3DerivativeFactor"), spikyPow3DerivativeFactor);
}

void GPUFluidSimulation::RunComputeKernel(KernelType kernel) {
    glUseProgram(fluidComputeProgram);
    
    // Bind buffers
    ComputeHelper::BindBuffer(particleBuffer, 0);
    ComputeHelper::BindBuffer(spatialLookupBuffer, 1);
    ComputeHelper::BindBuffer(startIndicesBuffer, 2);
    
    // Set current kernel
    glUniform1i(glGetUniformLocation(fluidComputeProgram, "currentKernel"), static_cast<int>(kernel));
    
    // Dispatch
    int numGroups = ComputeHelper::GetThreadGroupSizes(numParticles, 64);
    ComputeHelper::Dispatch(fluidComputeProgram, numGroups);
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
