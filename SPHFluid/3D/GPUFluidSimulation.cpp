#include "GPUFluidSimulation.h"
#include "ComputeHelper.h"
#include <iostream>
#include <random>
#include <cmath>
#include <algorithm>
#include <cstring>
#include <map>

GPUFluidSimulation::GPUFluidSimulation(int numParticles, const GPUSimulationSettings& settings)
    : numFluidParticles(numParticles), settings(settings), numBoundaryParticles(0), numTotalParticles(numParticles),
      fluidComputeProgram(0), particleBuffer(0), spatialLookupBuffer(0), startIndicesBuffer(0){

    // Debug: Check structure alignment
    std::cout << "GPUParticle structure size: " << sizeof(GPUParticle) << " bytes" << std::endl;
    std::cout << "Expected size for proper alignment: " << (16 * 5) << " bytes (5 x 16-byte slots)" << std::endl;
    
    // Calculate boundary particles if enabled
    if (settings.enableBoundaryParticles) {
        numBoundaryParticles = CalculateNumBoundaryParticles();
        std::cout << "Calculated " << numBoundaryParticles << " boundary particles" << std::endl;
    }
    numTotalParticles = numFluidParticles + numBoundaryParticles;

    if (!InitializeGPU()) {
        std::cerr << "Failed to initialize 3D GPU fluid simulation!" << std::endl;
        return;
    }

    InitializeParticles();
    if (settings.enableBoundaryParticles && numBoundaryParticles > 0) {
        InitializeBoundaryParticles();
    }
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

    particleBuffer = ComputeHelper::CreateBuffer(numTotalParticles * sizeof(GPUParticle));
    spatialLookupBuffer = ComputeHelper::CreateBuffer(numTotalParticles * sizeof(SpatialLookup));
    startIndicesBuffer = ComputeHelper::CreateBuffer(numTotalParticles * sizeof(uint32_t));

    if (particleBuffer == 0 || spatialLookupBuffer == 0 || startIndicesBuffer == 0) {
        return false;
    }
    gpuSort.SetBuffers(spatialLookupBuffer, startIndicesBuffer);
    return true;
}

void GPUFluidSimulation::InitializeParticles() {
    std::vector<GPUParticle> particles(numFluidParticles);

    glm::vec3 spawnSize(1.5f, 2.0f, 1.5f);
    glm::vec3 spawnCenter(0.0f, 0.0f, 0.0f);

    int particlesPerSide = std::max(1, static_cast<int>(std::cbrt(static_cast<float>(numFluidParticles))) + 1);
    constexpr unsigned RNG_SEED = 42u;
    std::mt19937 gen(RNG_SEED);
    std::uniform_real_distribution<float> dis(-0.02f, 0.02f);

    int particleIndex = 0;
    for (int x = 0; x < particlesPerSide && particleIndex < numFluidParticles; x++) {
        for (int y = 0; y < particlesPerSide && particleIndex < numFluidParticles; y++) {
            for (int z = 0; z < particlesPerSide && particleIndex < numFluidParticles; z++) {
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
                particles[particleIndex].isBoundary = 0;

                ++particleIndex;
            }
        }
    }



    ComputeHelper::WriteBuffer(particleBuffer, particles);
    gpuSort.SetBuffers(spatialLookupBuffer, startIndicesBuffer);
}

int GPUFluidSimulation::CalculateNumBoundaryParticles() const {
    if (!settings.enableBoundaryParticles || settings.boundaryLayers <= 0) {
        return 0;
    }

    glm::vec3 halfBounds = settings.boundsSize * 0.5f;
    float spacing = settings.boundarySpacing;
    int layers = settings.boundaryLayers;
    
    // Calculate particles for each face using integer, index-based counts
    int totalBoundaryParticles = 0;
    for (int layer = 0; layer < layers; layer++) {
        // Each layer extends outward by layerSpacing (not boundarySpacing * layer)
        float layerOffset = settings.layerSpacing * static_cast<float>(layer);

        // Expanded extents for this layer
        float xDim = 2.0f * (halfBounds.x + layerOffset);
        float yDim = 2.0f * (halfBounds.y + layerOffset);
        float zDim = 2.0f * (halfBounds.z + layerOffset);

        auto steps = [spacing](float dim) {
            // Inclusive sampling along the dimension with given spacing
            return std::max(1, static_cast<int>(std::floor(dim / spacing)) + 1);
        };
        int nx = steps(xDim);
        int ny = steps(yDim);
        int nz = steps(zDim);

        // +/-X faces (YZ planes)
        totalBoundaryParticles += 2 * (ny * nz);
        // +/-Y faces (XZ planes) excluding edges already counted by X faces
        int nx_in = std::max(0, nx - 2);
        totalBoundaryParticles += 2 * (nx_in * nz);
        // +/-Z faces (XY planes) excluding edges already counted by X and Y faces
        int ny_in = std::max(0, ny - 2);
        totalBoundaryParticles += 2 * (nx_in * ny_in);
    }

    return totalBoundaryParticles;
}

void GPUFluidSimulation::InitializeBoundaryParticles() {
    if (!settings.enableBoundaryParticles || numBoundaryParticles <= 0) {
        return;
    }

    // Read existing particles (fluid particles should already be initialized)
    std::vector<GPUParticle> particles = ComputeHelper::ReadBuffer<GPUParticle>(particleBuffer, numTotalParticles);
    
    glm::vec3 halfBounds = settings.boundsSize * 0.5f;
    float spacing = settings.boundarySpacing;
    int layers = settings.boundaryLayers;
    
    int boundaryIndex = numFluidParticles; // Start placing boundary particles after fluid particles
    
    for (int layer = 0; layer < layers; layer++) {
        // Use consistent layer spacing - each layer is offset by layerSpacing
        float layerOffset = settings.layerSpacing * static_cast<float>(layer);

        glm::vec3 minP = -halfBounds - glm::vec3(layerOffset);
        glm::vec3 maxP = halfBounds + glm::vec3(layerOffset);

        auto steps = [spacing](float dim) {
            return std::max(1, static_cast<int>(std::floor(dim / spacing)) + 1);
        };
        int nx = steps(maxP.x - minP.x);
        int ny = steps(maxP.y - minP.y);
        int nz = steps(maxP.z - minP.z);

        auto xi = [&](int i) { return minP.x + i * spacing; };
        auto yi = [&](int i) { return minP.y + i * spacing; };
        auto zi = [&](int i) { return minP.z + i * spacing; };

        // BOTTOM FACE ONLY (minP.y) - Full coverage
        for (int ix = 0; ix < nx; ++ix) {
            for (int iz = 0; iz < nz; ++iz) {
                if (boundaryIndex < numTotalParticles) {
                    particles[boundaryIndex].position = glm::vec3(xi(ix), minP.y, zi(iz));
                    particles[boundaryIndex].velocity = glm::vec3(0.0f);
                    particles[boundaryIndex].predictedPosition = particles[boundaryIndex].position;
                    particles[boundaryIndex].density = settings.targetDensity;
                    particles[boundaryIndex].isBoundary = 1;
                    ++boundaryIndex;
                }
            }
        }

        // LEFT AND RIGHT WALLS (X-faces) - Exclude bottom row (already covered by bottom face)
        int yStart = 1; // Start from y=1 to avoid overlap with bottom
        for (int iy = yStart; iy < ny; ++iy) {
            for (int iz = 0; iz < nz; ++iz) {
                // Left wall (minP.x)
                if (boundaryIndex < numTotalParticles) {
                    particles[boundaryIndex].position = glm::vec3(minP.x, yi(iy), zi(iz));
                    particles[boundaryIndex].velocity = glm::vec3(0.0f);
                    particles[boundaryIndex].predictedPosition = particles[boundaryIndex].position;
                    particles[boundaryIndex].density = settings.targetDensity;
                    particles[boundaryIndex].isBoundary = 1;
                    ++boundaryIndex;
                }
                // Right wall (maxP.x)
                if (boundaryIndex < numTotalParticles) {
                    particles[boundaryIndex].position = glm::vec3(maxP.x, yi(iy), zi(iz));
                    particles[boundaryIndex].velocity = glm::vec3(0.0f);
                    particles[boundaryIndex].predictedPosition = particles[boundaryIndex].position;
                    particles[boundaryIndex].density = settings.targetDensity;
                    particles[boundaryIndex].isBoundary = 1;
                    ++boundaryIndex;
                }
            }
        }

        // FRONT AND BACK WALLS (Z-faces) - Exclude edges already covered by X-faces and bottom
        int xStart = 1; // Exclude left/right edges
        int xEnd = nx - 1; // Exclude left/right edges
        for (int ix = xStart; ix < xEnd; ++ix) {
            for (int iy = yStart; iy < ny; ++iy) { // Start from y=1 to exclude bottom
                // Front wall (minP.z)
                if (boundaryIndex < numTotalParticles) {
                    particles[boundaryIndex].position = glm::vec3(xi(ix), yi(iy), minP.z);
                    particles[boundaryIndex].velocity = glm::vec3(0.0f);
                    particles[boundaryIndex].predictedPosition = particles[boundaryIndex].position;
                    particles[boundaryIndex].density = settings.targetDensity;
                    particles[boundaryIndex].isBoundary = 1;
                    ++boundaryIndex;
                }
                // Back wall (maxP.z)
                if (boundaryIndex < numTotalParticles) {
                    particles[boundaryIndex].position = glm::vec3(xi(ix), yi(iy), maxP.z);
                    particles[boundaryIndex].velocity = glm::vec3(0.0f);
                    particles[boundaryIndex].predictedPosition = particles[boundaryIndex].position;
                    particles[boundaryIndex].density = settings.targetDensity;
                    particles[boundaryIndex].isBoundary = 1;
                    ++boundaryIndex;
                }
            }
        }

        // NOTE: No top face (maxP.y) - leaving it open like a container
    }
    
    // Update the actual number of boundary particles created
    numBoundaryParticles = boundaryIndex - numFluidParticles;
    
    std::cout << "Created " << numBoundaryParticles << " boundary particles in " << layers << " layer(s)" << std::endl;
    std::cout << "Container is open at the top (no ceiling)" << std::endl;
    
    // Debug: Show first few boundary particle positions
    if (numBoundaryParticles > 0) {
        std::cout << "First few boundary particles:" << std::endl;
        for (int i = numFluidParticles; i < std::min(numFluidParticles + 10, numTotalParticles); i++) {
            glm::vec3 pos = particles[i].position;
            std::cout << "  Particle " << i << ": (" << pos.x << ", " << pos.y << ", " << pos.z 
                      << ") isBoundary=" << particles[i].isBoundary << std::endl;
        }
        
        // Show samples from different layers
        std::cout << "Layer distribution analysis:" << std::endl;
        std::map<float, std::vector<int>> layerMap;
        for (int i = numFluidParticles; i < numTotalParticles; i++) {
            if (particles[i].isBoundary == 1) {
                float z = particles[i].position.z;
                layerMap[z].push_back(i);
            }
        }
        
        int layerCount = 0;
        for (auto& layer : layerMap) {
            std::cout << "  Z=" << layer.first << " has " << layer.second.size() << " particles" << std::endl;
            layerCount++;
            if (layerCount >= 5) break; // Show first 5 distinct Z values
        }
    }
    
    // Write updated particles back to GPU
    ComputeHelper::WriteBuffer(particleBuffer, particles);
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

    int numGroups = ComputeHelper::GetThreadGroupSizes(numTotalParticles, 64);
    ComputeHelper::Dispatch(fluidComputeProgram, numGroups);
}

void GPUFluidSimulation::UpdateSpatialHashing() {
    std::vector<uint32_t> startIndices(numTotalParticles, static_cast<uint32_t>(numTotalParticles));
    ComputeHelper::WriteBuffer(startIndicesBuffer, startIndices);

    RunComputeKernel(UpdateSpatialHashKernel);

    gpuSort.SortAndCalculateOffsets(spatialLookupBuffer, numTotalParticles);
}

void GPUFluidSimulation::SetComputeUniforms() {
    glUseProgram(fluidComputeProgram);
    
    static int debugCount = 0;
    if (debugCount < 3) {
        std::cout << "DEBUG Uniforms frame " << debugCount << ":\n";
        std::cout << "  numTotalParticles: " << numTotalParticles << "\n";
        std::cout << "  gravity: " << settings.gravity << "\n";
        std::cout << "  collisionDamping: " << settings.collisionDamping << "\n";
        std::cout << "  boundsSize: (" << settings.boundsSize.x << ", " << settings.boundsSize.y << ", " << settings.boundsSize.z << ")\n";
        debugCount++;
    }
    
    glUniform1i(glGetUniformLocation(fluidComputeProgram, "numParticles"), numTotalParticles);
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
        RunComputeKernel(UpdateSpatialHashKernel);
        gpuSort.SortAndCalculateOffsets(spatialLookupBuffer, numTotalParticles);
        
        RunComputeKernel(CalculateDensitiesKernel);
        
        RunComputeKernel(CalculatePressureForcesKernel);
        RunComputeKernel(CalculateViscosityKernel);
        
        RunComputeKernel(UpdatePositionsKernel);
    }
}

std::vector<GPUParticle> GPUFluidSimulation::GetParticles() {
    return ComputeHelper::ReadBuffer<GPUParticle>(particleBuffer, numTotalParticles);
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
