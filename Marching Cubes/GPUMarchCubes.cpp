#include "GPUMarchCubes.h"
#include "../ComputeHelper.h"
#include "tables.h"
#include <iostream>

GPUMarchCubes::GPUMarchCubes()
    : computeProgram(0), ssboVertices(0), ssboIndices(0),
      maxTriangles(0), settings_(), numGeneratedVertices(0), numGeneratedIndices(0)
{
}

GPUMarchCubes::~GPUMarchCubes()
{
    cleanup();
}

void GPUMarchCubes::cleanup()
{
    if (ssboVertices) {
        glDeleteBuffers(1, &ssboVertices);
        ssboVertices = 0;
    }
    if (ssboIndices) {
        glDeleteBuffers(1, &ssboIndices);
        ssboIndices = 0;
    }
    if (ssboAtomicCounters) {
        glDeleteBuffers(1, &ssboAtomicCounters);
        ssboAtomicCounters = 0;
    }
    if (ssboedgeTable) {
        glDeleteBuffers(1, &ssboedgeTable);
        ssboedgeTable = 0;
    }
    if (ssbotriTable) {
        glDeleteBuffers(1, &ssbotriTable);
        ssbotriTable = 0;
    }
    if (ssboedgeVertexMap) {
        glDeleteBuffers(1, &ssboedgeVertexMap);
        ssboedgeVertexMap = 0;
    }
    if (ssboScalarField) {
        glDeleteBuffers(1, &ssboScalarField);
        ssboScalarField = 0;
    }
    if (computeProgram) {
        glDeleteProgram(computeProgram);
        computeProgram = 0;
    }
}

void GPUMarchCubes::initialize()
{
    // Estimate maximum number of triangles (5 per cube)
    int numCubes = (settings_.gridSizeX - 1) * (settings_.gridSizeY - 1) * (settings_.gridSizeZ - 1);
    maxTriangles = numCubes * 5;
    int maxVertices = maxTriangles * 3;
    size_t vertexBufferSize = maxVertices * sizeof(GPUVertex);
    size_t indexBufferSize = maxTriangles * 3 * sizeof(unsigned int);

    // Create buffers
    ssboScalarField = ComputeHelper::CreateBuffer(
        sizeof(float) * settings_.gridSizeX * settings_.gridSizeY * settings_.gridSizeZ, 
        nullptr, GL_DYNAMIC_DRAW);
    ssboVertices = ComputeHelper::CreateBuffer(vertexBufferSize, nullptr, GL_DYNAMIC_DRAW);
    ssboIndices = ComputeHelper::CreateBuffer(indexBufferSize, nullptr, GL_DYNAMIC_DRAW);
    
    // Create and initialize atomic counters to zero
    uint32_t zeroCounters[2] = {0, 0};
    ssboAtomicCounters = ComputeHelper::CreateBuffer(
        sizeof(uint32_t) * 2, zeroCounters, GL_DYNAMIC_DRAW);
    
    // Upload lookup tables
    ssboedgeTable = ComputeHelper::CreateBuffer(sizeof(edgeTable), edgeTable, GL_STATIC_DRAW);
    ssbotriTable = ComputeHelper::CreateBuffer(sizeof(triTable), triTable, GL_STATIC_DRAW);
    
    // Prepare edgeToVertices data as uvec2 array (matching shader expectation)
    struct EdgePair { uint32_t v1, v2; };
    EdgePair edgeToVertexData[12];
    for (int i = 0; i < 12; i++) {
        edgeToVertexData[i].v1 = static_cast<uint32_t>(edgeToVertices[i].first);
        edgeToVertexData[i].v2 = static_cast<uint32_t>(edgeToVertices[i].second);
    }
    ssboedgeVertexMap = ComputeHelper::CreateBuffer(
        sizeof(edgeToVertexData), edgeToVertexData, GL_STATIC_DRAW);

    if (ssboVertices == 0 || ssboIndices == 0 || ssboAtomicCounters == 0 || 
        ssboedgeTable == 0 || ssbotriTable == 0 || ssboedgeVertexMap == 0 || 
        ssboScalarField == 0) {
        throw std::runtime_error("Failed to create SSBOs for GPUMarchCubes.");
    }

    computeProgram = ComputeHelper::LoadComputeShader("Marching Cubes/CubeMarching.compute");
    if (computeProgram == 0) {
        throw std::runtime_error("Failed to load compute shader for GPUMarchCubes.");
    }

    // Warm up the compute program and GPU to reduce first-dispatch stalls
    glUseProgram(computeProgram);
    glUseProgram(0);
}

void GPUMarchCubes::setUniforms()
{
    glUseProgram(computeProgram);
    glUniform1f(glGetUniformLocation(computeProgram, "isolevel"), settings_.isoLevel);
    glUniform1ui(glGetUniformLocation(computeProgram, "sizeX"), settings_.gridSizeX);
    glUniform1ui(glGetUniformLocation(computeProgram, "sizeY"), settings_.gridSizeY);
    glUniform1ui(glGetUniformLocation(computeProgram, "sizeZ"), settings_.gridSizeZ);
    glUniform3f(glGetUniformLocation(computeProgram, "boundsMin"), 
                settings_.boundsMin.x, settings_.boundsMin.y, settings_.boundsMin.z);
    glUniform3f(glGetUniformLocation(computeProgram, "boundsMax"), 
                settings_.boundsMax.x, settings_.boundsMax.y, settings_.boundsMax.z);
    
    // Debug output
    std::cout << "GPU Shader uniforms: isolevel=" << settings_.isoLevel 
              << ", grid=(" << settings_.gridSizeX << "," << settings_.gridSizeY << "," << settings_.gridSizeZ << ")"
              << ", boundsMin=(" << settings_.boundsMin.x << "," << settings_.boundsMin.y << "," << settings_.boundsMin.z << ")"
              << ", boundsMax=(" << settings_.boundsMax.x << "," << settings_.boundsMax.y << "," << settings_.boundsMax.z << ")"
              << std::endl;
}

void GPUMarchCubes::uploadScalarField(const std::vector<float>& scalarField)
{
    size_t expectedSize = settings_.gridSizeX * settings_.gridSizeY * settings_.gridSizeZ;
    if (scalarField.size() != expectedSize) {
        throw std::runtime_error("Scalar field size mismatch. Expected " + 
                                 std::to_string(expectedSize) + " but got " + 
                                 std::to_string(scalarField.size()));
    }
    
    ComputeHelper::WriteBuffer(ssboScalarField, scalarField);
}

void GPUMarchCubes::execute()
{
    // Bind SSBOs in correct order matching shader bindings
    ComputeHelper::BindBuffer(ssboScalarField, 0);      // binding 0: ScalarField
    ComputeHelper::BindBuffer(ssboVertices, 1);         // binding 1: VertexBuffer
    ComputeHelper::BindBuffer(ssboIndices, 2);          // binding 2: IndexBuffer
    ComputeHelper::BindBuffer(ssboedgeTable, 3);        // binding 3: edgeTableBuffer
    ComputeHelper::BindBuffer(ssbotriTable, 4);         // binding 4: triTableBuffer
    ComputeHelper::BindBuffer(ssboedgeVertexMap, 5);    // binding 5: edgeToVertexBuffer
    ComputeHelper::BindBuffer(ssboAtomicCounters, 6);   // binding 6: AtomicCounters

    // Set uniforms
    setUniforms();

    // Reset atomic counters to zero
    uint32_t zeroCounters[2] = {0, 0};
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboAtomicCounters);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(uint32_t) * 2, zeroCounters);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // Calculate number of work groups
    // Each cube is at coordinates (x,y,z) and needs neighbors at (x+1, y+1, z+1)
    // So we process (gridSize - 1) cubes in each dimension
    GLuint numCubesX = settings_.gridSizeX - 1;
    GLuint numCubesY = settings_.gridSizeY - 1;
    GLuint numCubesZ = settings_.gridSizeZ - 1;
    
    // Work group size is 8x4x1 in the shader
    GLuint numGroupsX = (numCubesX + 7) / 8;
    GLuint numGroupsY = (numCubesY + 3) / 4;
    GLuint numGroupsZ = (numCubesZ + 0) / 1;
    
    // Dispatch compute shader
    ComputeHelper::Dispatch(computeProgram, numGroupsX, numGroupsY, numGroupsZ);

    // Read back the number of generated vertices and indices
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboAtomicCounters);
    uint32_t* counterData = (uint32_t*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
    if (counterData) {
        numGeneratedVertices = counterData[0];
        numGeneratedIndices = counterData[1];
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        
        std::cout << "Generated " << numGeneratedVertices << " vertices and " 
                  << numGeneratedIndices << " indices (" 
                  << (numGeneratedIndices / 3) << " triangles)" << std::endl;
    } else {
        std::cerr << "Failed to read atomic counters" << std::endl;
        numGeneratedVertices = 0;
        numGeneratedIndices = 0;
    }
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

std::vector<float> GPUMarchCubes::getVertices()
{
    if (numGeneratedVertices == 0) return {};
    
    std::vector<float> result;
    result.reserve(numGeneratedVertices * 8); // 3 position + 3 normal + 2 texcoord
    
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboVertices);
    GPUVertex* vertData = (GPUVertex*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
    
    if (vertData) {
        for (int i = 0; i < numGeneratedVertices; i++) {
            // Position
            result.push_back(vertData[i].position[0]);
            result.push_back(vertData[i].position[1]);
            result.push_back(vertData[i].position[2]);
            // Normal
            result.push_back(vertData[i].normal[0]);
            result.push_back(vertData[i].normal[1]);
            result.push_back(vertData[i].normal[2]);
            // TexCoord
            result.push_back(vertData[i].texcoord[0]);
            result.push_back(vertData[i].texcoord[1]);
        }
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    } else {
        std::cerr << "Failed to map vertex buffer" << std::endl;
    }
    
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    return result;
}

std::vector<unsigned int> GPUMarchCubes::getIndices()
{
    if (numGeneratedIndices == 0) return {};
    return ComputeHelper::ReadBuffer<unsigned int>(ssboIndices, numGeneratedIndices);
}

void GPUMarchCubes::setSettings(const CMarchSettings& settings)
{
    settings_ = settings;
}

CMarchSettings GPUMarchCubes::getSettings() const
{
    return settings_;
}