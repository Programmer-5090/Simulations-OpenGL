#include "GPUFluidSimulation.h"
#include "ComputeHelper.h"
#include <iostream>
#include <random>
#include <cmath>
#include <algorithm>
#include <cstring>
#include <map>
#include <fstream>
#include <iomanip>
#include <sstream>

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

// Update the particle count calculation accordingly
int GPUFluidSimulation::CalculateNumBoundaryParticles() const {
    if (!settings.enableBoundaryParticles || settings.boundaryLayers <= 0) {
        return 0;
    }

    // Base inner bounds (no growth per layer for tangential directions)
    glm::vec3 halfBounds = settings.boundsSize * 0.5f;
    float spacing = settings.boundarySpacing;

    auto steps = [spacing](float dim) {
        // Inclusive sampling along the dimension with given spacing
        return std::max(2, static_cast<int>(std::ceil(dim / spacing)) + 1);
    };

    int nx = steps(2.0f * halfBounds.x);
    int ny = steps(2.0f * halfBounds.y);
    int nz = steps(2.0f * halfBounds.z);

    // Per-layer counts using fixed base grid
    int bottomCount = nx * nz; // full bottom
    int topCount = settings.includeTopFace ? nx * nz : 0;

    // X walls (YZ planes)
    int xWallRows = std::max(0, ny - 1); // exclude bottom row by default
    int xWallsCount = 2 * xWallRows * nz;
    if (settings.coverEdges) {
        // include bottom row edges on X walls to cover seams (duplicates OK)
        xWallsCount += 2 * nz; // add iy=0 row for both X- and X+
    }

    // Z walls (XY planes)
    int zWallCols = settings.coverEdges ? nx : std::max(0, nx - 2); // include edges if coverEdges
    int zWallRows = std::max(0, ny - 1); // exclude bottom row by default
    int zWallsCount = 2 * zWallCols * zWallRows;
    if (settings.coverEdges) {
        // also include the bottom row on Z walls
        zWallsCount += 2 * zWallCols;
    }

    int perLayer = bottomCount + topCount + xWallsCount + zWallsCount;
    
    return perLayer * settings.boundaryLayers;
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

    // Base (inner) bounds used for tangential sampling
    glm::vec3 baseMin = -halfBounds;
    glm::vec3 baseMax = halfBounds;

    auto steps = [spacing](float dim) {
        return std::max(2, static_cast<int>(std::ceil(dim / spacing)) + 1);
    };

    int nx = steps(baseMax.x - baseMin.x);
    int ny = steps(baseMax.y - baseMin.y);
    int nz = steps(baseMax.z - baseMin.z);

    auto interp = [](float minVal, float maxVal, int steps, int i) {
        if (steps <= 1) return minVal;
        return minVal + (maxVal - minVal) * (static_cast<float>(i) / (steps - 1));
    };

    auto xi = [&](int i) { return interp(baseMin.x, baseMax.x, nx, i); };
    auto yi = [&](int i) { return interp(baseMin.y, baseMax.y, ny, i); };
    auto zi = [&](int i) { return interp(baseMin.z, baseMax.z, nz, i); };

    int boundaryIndex = numFluidParticles; // Start placing boundary particles after fluid particles

    for (int layer = 0; layer < layers; layer++) {
        float offset = settings.layerSpacing * static_cast<float>(layer);

        // Bottom face (Y-): full XZ grid, shifted along -Y by offset
        for (int ix = 0; ix < nx; ++ix) {
            for (int iz = 0; iz < nz; ++iz) {
                if (boundaryIndex < numTotalParticles) {
                    particles[boundaryIndex].position = glm::vec3(xi(ix), baseMin.y - offset, zi(iz));
                    particles[boundaryIndex].velocity = glm::vec3(0.0f);
                    particles[boundaryIndex].predictedPosition = particles[boundaryIndex].position;
                    particles[boundaryIndex].density = settings.targetDensity;
                    particles[boundaryIndex].isBoundary = 1;
                    ++boundaryIndex;
                }
            }
        }

        // Optional Top face (Y+): full XZ grid, shifted along +Y by offset
        if (settings.includeTopFace) {
            for (int ix = 0; ix < nx; ++ix) {
                for (int iz = 0; iz < nz; ++iz) {
                    if (boundaryIndex < numTotalParticles) {
                        particles[boundaryIndex].position = glm::vec3(xi(ix), baseMax.y + offset, zi(iz));
                        particles[boundaryIndex].velocity = glm::vec3(0.0f);
                        particles[boundaryIndex].predictedPosition = particles[boundaryIndex].position;
                        particles[boundaryIndex].density = settings.targetDensity;
                        particles[boundaryIndex].isBoundary = 1;
                        ++boundaryIndex;
                    }
                }
            }
        }

        // X walls (left/right): YZ grid; include bottom row if coverEdges, otherwise start at 1
        int yStartX = settings.coverEdges ? 0 : 1;
        for (int iy = yStartX; iy < ny; ++iy) {
            for (int iz = 0; iz < nz; ++iz) {
                if (boundaryIndex < numTotalParticles) {
                    // Left (X-)
                    particles[boundaryIndex].position = glm::vec3(baseMin.x - offset, yi(iy), zi(iz));
                    particles[boundaryIndex].velocity = glm::vec3(0.0f);
                    particles[boundaryIndex].predictedPosition = particles[boundaryIndex].position;
                    particles[boundaryIndex].density = settings.targetDensity;
                    particles[boundaryIndex].isBoundary = 1;
                    ++boundaryIndex;
                }
                if (boundaryIndex < numTotalParticles) {
                    // Right (X+)
                    particles[boundaryIndex].position = glm::vec3(baseMax.x + offset, yi(iy), zi(iz));
                    particles[boundaryIndex].velocity = glm::vec3(0.0f);
                    particles[boundaryIndex].predictedPosition = particles[boundaryIndex].position;
                    particles[boundaryIndex].density = settings.targetDensity;
                    particles[boundaryIndex].isBoundary = 1;
                    ++boundaryIndex;
                }
            }
        }

        // Z walls (front/back): XY grid; include x-edges if coverEdges; include bottom row if coverEdges
        int xStartZ = settings.coverEdges ? 0 : 1;
        int xEndZ = settings.coverEdges ? nx : (nx - 1);
        int yStartZ = settings.coverEdges ? 0 : 1;
        for (int ix = xStartZ; ix < xEndZ; ++ix) {
            for (int iy = yStartZ; iy < ny; ++iy) {
                if (boundaryIndex < numTotalParticles) {
                    // Front (Z-)
                    particles[boundaryIndex].position = glm::vec3(xi(ix), yi(iy), baseMin.z - offset);
                    particles[boundaryIndex].velocity = glm::vec3(0.0f);
                    particles[boundaryIndex].predictedPosition = particles[boundaryIndex].position;
                    particles[boundaryIndex].density = settings.targetDensity;
                    particles[boundaryIndex].isBoundary = 1;
                    ++boundaryIndex;
                }
                if (boundaryIndex < numTotalParticles) {
                    // Back (Z+)
                    particles[boundaryIndex].position = glm::vec3(xi(ix), yi(iy), baseMax.z + offset);
                    particles[boundaryIndex].velocity = glm::vec3(0.0f);
                    particles[boundaryIndex].predictedPosition = particles[boundaryIndex].position;
                    particles[boundaryIndex].density = settings.targetDensity;
                    particles[boundaryIndex].isBoundary = 1;
                    ++boundaryIndex;
                }
            }
        }
    }

    // Update the actual number of boundary particles created
    numBoundaryParticles = boundaryIndex - numFluidParticles;

    std::cout << "Created " << numBoundaryParticles << " boundary particles in " << layers << " layer(s)" << std::endl;
    std::cout << "Container is open at the top (no ceiling)" << std::endl;

    // Write updated particles back to GPU
    ComputeHelper::WriteBuffer(particleBuffer, particles);

    // Debug dump after creation
    // DebugDumpBoundaryLayers("after_init", 'y', 10, 25);
}


void GPUFluidSimulation::DebugDumpBoundaryLayers(const char* tag, char axis, int maxLayers, int maxPerLayer) const {
    // Read back particles from GPU
    std::vector<GPUParticle> particles = ComputeHelper::ReadBuffer<GPUParticle>(particleBuffer, numTotalParticles);

    float layerStep = (settings.layerSpacing > 0.0f) ? settings.layerSpacing : settings.boundarySpacing;
    if (layerStep <= 0.0f) {
        return;
    }

    glm::vec3 halfBounds = settings.boundsSize * 0.5f;
    auto computeLayerIndex = [&](const GPUParticle& p) -> int {
        float lx = (std::abs(p.position.x) - halfBounds.x) / layerStep;
        float ly = (std::abs(p.position.y) - halfBounds.y) / layerStep;
        float lz = (std::abs(p.position.z) - halfBounds.z) / layerStep;
        int ix = static_cast<int>(std::round(lx));
        int iy = static_cast<int>(std::round(ly));
        int iz = static_cast<int>(std::round(lz));
        int L = std::max(0, std::max(ix, std::max(iy, iz)));
        return L;
    };

    std::map<int, std::vector<int>> layers; // layer index -> particle indices
    for (int i = numFluidParticles; i < numTotalParticles; ++i) {
        if (particles[i].isBoundary != 1u) continue;
        int L = computeLayerIndex(particles[i]);
        layers[L].push_back(i);
    }

    // Build filename
    std::ostringstream name;
    name << "boundary_dump_" << (tag ? tag : "dump") << "_axis_" << axis << ".txt";
    std::ofstream ofs(name.str());
    if (!ofs.is_open()) return;

    ofs << "Boundary Layer Dump\n";
    ofs << "Tag: " << (tag ? tag : "dump") << ", Axis: " << axis << "\n";
    ofs << "Total particles: fluid=" << numFluidParticles << ", boundary=" << numBoundaryParticles
        << ", total=" << numTotalParticles << "\n";
    ofs << "Layer step (spacing): " << layerStep << "\n\n";

    ofs << std::fixed << std::setprecision(6);

    int layerPrinted = 0;
    for (const auto& kv : layers) {
        if (maxLayers > 0 && layerPrinted >= maxLayers) break;
        int L = kv.first;
        const auto& idxs = kv.second;
        ofs << "Layer " << L << " — count=" << idxs.size() << "\n";
        int printed = 0;
        for (int idx : idxs) {
            if (maxPerLayer > 0 && printed >= maxPerLayer) break;
            const auto& p = particles[idx];
            ofs << "  #" << idx << ": pos(" << p.position.x << ", " << p.position.y << ", " << p.position.z << ")";
            ++printed;
        }
        ofs << "\n";
        ++layerPrinted;
    }
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
