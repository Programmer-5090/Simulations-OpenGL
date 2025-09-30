#ifndef CUBEMARCHING_H
#define CUBEMARCHING_H

#include <vector>
#include <array>
#include <cmath>
#include <limits>
#include <numeric>

#include "../mesh.h"

class CubeMarching {
public:
    CubeMarching();
    ~CubeMarching();

    struct GridCell {
        std::array<glm::vec3, 8> positions; // Positions of the 8 corners
        std::array<float, 8> values;       // Scalar values at the corners
    };

    
    void ClearMesh();

    void setDimensions(int width, int height, int depth);
    
    void GenerateMesh(float isolevel);
    
    void SetInputVertices(const std::vector<glm::vec3>& vertices);

    void GenerateMeshFromVertices(float isolevel, float radius);
    
    Mesh CreateMesh();

    void TriangulateCellAt(int gx, int gy, int gz, float isolevel);

    // Grid dimension getters
    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }
    int getDepth() const { return m_depth; }

    void SetNoiseParameters(float resolution, float scale);

    void SetVisualizeNoise(bool visualize);

    void SetInfluenceRadius(float radius);

    // If input vertices were provided, compute the inclusive cell range (minX..maxX, etc.)
    // that covers the region of interest in grid cell coordinates.
    void GetInputGridBounds(int &minX, int &minY, int &minZ, int &maxX, int &maxY, int &maxZ) const;

    const std::vector<Vertex>& GetVertices() const;

private:

    int calculateCubeIndex(const GridCell& cell, float isolevel) const;
    
    Vertex VertexInterp(float isolevel, const Vertex& p1, const Vertex& p2, float valp1, float valp2);
    
    void TriangulateCell(const GridCell& cell, float isolevel);
    
    glm::vec3 CalculateGradient(const glm::vec3& p) const;
    glm::vec3 CalculateSurfaceNormal(const glm::vec3& p) const;
    
    float scalarField(float x, float y, float z) const;


    std::vector<Vertex> m_vertices;
    int m_width, m_height, m_depth;
    std::vector<float> m_voxel_grid; // 3D scalar field stored as a 1D array

    // Noise parameters
    float m_noiseResolution = 0.05f;
    float m_noiseScale = 1.0f;
    bool m_visualizeNoise = false;
    
    // Parameters for generating mesh from input vertices
    std::vector<glm::vec3> m_inputVertices;
    bool m_useInputVertices = false;
    float m_influenceRadius = 1.0f;
    glm::vec3 m_gridMin = {0, 0, 0};
    glm::vec3 m_gridMax = {0, 0, 0};
};

#endif // CUBEMARCHING_H
