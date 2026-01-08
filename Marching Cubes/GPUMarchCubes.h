#ifndef GPUMARCHCUBES_H
#define GPUMARCHCUBES_H

#include <glad/glad.h>
#include <glm/glm/glm.hpp>
#include <vector>

// Vertex structure matching the shader layout (std430)
// include padding so the CPU-side struct size/layout matches the GPU std430 struct.
// Layout: vec3 position + pad -> 16 bytes, vec3 normal + pad -> 16 bytes, vec2 texcoord + pad(2) -> 16 bytes -> total 48 bytes
struct GPUVertex {
    float position[3];
    float pad0; // padding to align to 16 bytes
    float normal[3];
    float pad1;
    float texcoord[2];
    float pad2[2]; // ensure struct is 12 floats = 48 bytes
};

struct GPUTriangle {
    unsigned int vertexIndices[3];
};

struct GridCell {
    GPUVertex corners[8];
    float values[8];
};

struct CMarchSettings {
    int gridSizeX;
    int gridSizeY;
    int gridSizeZ;
    float isoLevel;
    glm::vec3 boundsMin;
    glm::vec3 boundsMax;
};

class GPUMarchCubes {
public:
    GPUMarchCubes();
    ~GPUMarchCubes();

    void initialize();
    
    void uploadScalarField(const std::vector<float>& scalarField);
    
    void execute();
    
    void setSettings(const CMarchSettings& settings);
    CMarchSettings getSettings() const;
    
    std::vector<float> getVertices();
    std::vector<unsigned int> getIndices();
    
    int getVertexCount() const { return numGeneratedVertices; }
    int getIndexCount() const { return numGeneratedIndices; }
    int getTriangleCount() const { return numGeneratedIndices / 3; }

private:
    GLuint computeProgram;
    GLuint ssboVertices;
    GLuint ssboIndices;
    GLuint ssboAtomicCounters;
    GLuint ssboedgeTable;
    GLuint ssbotriTable;
    GLuint ssboedgeVertexMap;
    GLuint ssboScalarField;
    
    int maxTriangles;
    int numGeneratedVertices;
    int numGeneratedIndices;

    CMarchSettings settings_;

    void setUniforms();
    void cleanup();
};

#endif // GPUMARCHCUBES_H