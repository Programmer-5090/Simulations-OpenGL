/*Reference: https://github.com/nihaljn/marching-cubes/tree/main*/
#include "CubeMarching.h"
#include "tables.h"
#include <cmath>
#include <iterator>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/norm.hpp>

// Compute face normal (cross of two edges) and normalize.
static glm::vec3 calculateFaceNormal(const Vertex& v0, const Vertex& v1, const Vertex& v2)
{
    glm::vec3 edge1 = v1.Position - v0.Position;
    glm::vec3 edge2 = v2.Position - v0.Position;
    glm::vec3 normal = glm::cross(edge1, edge2);
    float length = glm::length(normal);
    if (length > 1e-8f) normal = glm::normalize(normal);
    return normal;
}

int CubeMarching::calculateCubeIndex(const std::array<float, GridCell::CornerCount>& cubeValues, float isoLevel) const
{
    int cubeIndex = 0;
    for (std::size_t i = 0; i < GridCell::CornerCount; i++)
        if (cubeValues[i] < isoLevel) cubeIndex |= (1 << i);
    return cubeIndex;
}

std::vector<Vertex> CubeMarching::interpolateVertices(const std::array<Vertex, GridCell::CornerCount>& cubeVertices,
                                                        const std::array<float, GridCell::CornerCount>& cubeValues,
                                                        float isoLevel) const
{
    std::array<Vertex, 12> interpolated{};
    const int intersectionsKey = edgeTable[calculateCubeIndex(cubeValues, isoLevel)];

    for (int i = 0; i < 12; ++i)
    {
        if (intersectionsKey & (1 << i))
        {
            const int v1 = edgeToVertices[i].first;
            const int v2 = edgeToVertices[i].second;
            const float val1 = cubeValues[v1];
            const float val2 = cubeValues[v2];
            const float denom = val2 - val1;

            float mu = 0.0f;
            if (std::fabs(denom) > 1e-8f)
                mu = (isoLevel - val1) / denom;
                
            // Linear interpolation between edge vertices using glm::vec3
            const Vertex& vert1 = cubeVertices[v1];
            const Vertex& vert2 = cubeVertices[v2];
            interpolated[i].Position = vert1.Position + mu * (vert2.Position - vert1.Position);
            interpolated[i].Normal = glm::vec3(0.0f);

            // Interpolate texture coordinates
            interpolated[i].TexCoords = vert1.TexCoords + mu * (vert2.TexCoords - vert1.TexCoords);
        }
    }
    return std::vector<Vertex>(interpolated.begin(), interpolated.end());
}

std::vector<std::array<int,3>> CubeMarching::getTriangles(const std::vector<Vertex>& edgeVertices, int cubeIndex) const
{
    std::vector<std::array<int,3>> triangles;
    for (int i = 0; triTable[cubeIndex][i] != -1; i += 3)
    {
        std::array<int,3> triangle;
        for (int j = 0; j < 3; j++)
            triangle[j] = triTable[cubeIndex][i + j];
        triangles.push_back(triangle);
    }
    return triangles;
}

std::vector<std::array<int,3>> CubeMarching::triangulateCube(const std::array<Vertex, GridCell::CornerCount>& cubeVertices,
                                                  const std::array<float, GridCell::CornerCount>& cubeValues,
                                                  float isoLevel) const
{
    std::vector<std::array<int,3>> triangles;
    std::vector<Vertex> edgeVertices = getEdgeVertices(cubeVertices, cubeValues, isoLevel);
    int cubeIndex = calculateCubeIndex(cubeValues, isoLevel);
    triangles = getTriangles(edgeVertices, cubeIndex);
    return triangles;
}

std::vector<Vertex> CubeMarching::getEdgeVertices(const std::array<Vertex, GridCell::CornerCount>& cubeVertices,
                                        const std::array<float, GridCell::CornerCount>& cubeValues,
                                        float isoLevel) const
{
    return interpolateVertices(cubeVertices, cubeValues, isoLevel);
}

std::vector<std::array<int,3>> CubeMarching::triangulateField(const std::vector<std::vector<std::vector<float>>>& scalarField,
                                                    float isoLevel) const
{
    std::vector<std::array<int,3>> triangles;
    
    for (std::size_t z = 0; z + 1 < scalarField.size(); ++z)
    {
        for (std::size_t y = 0; y + 1 < scalarField[z].size(); ++y)
        {
            for (std::size_t x = 0; x + 1 < scalarField[z][y].size(); ++x)
            {
                // Marching cubes vertex ordering: bottom face (z), then top face (z+1)
                // Bottom: (0,0,0), (1,0,0), (1,1,0), (0,1,0)
                // Top:    (0,0,1), (1,0,1), (1,1,1), (0,1,1)
                const std::array<float, GridCell::CornerCount> cubeValues = {
                    scalarField[z][y][x],         // vertex 0: (x,   y,   z)
                    scalarField[z][y][x + 1],     // vertex 1: (x+1, y,   z)
                    scalarField[z][y + 1][x + 1], // vertex 2: (x+1, y+1, z)
                    scalarField[z][y + 1][x],     // vertex 3: (x,   y+1, z)
                    scalarField[z + 1][y][x],     // vertex 4: (x,   y,   z+1)
                    scalarField[z + 1][y][x + 1], // vertex 5: (x+1, y,   z+1)
                    scalarField[z + 1][y + 1][x + 1], // vertex 6: (x+1, y+1, z+1)
                    scalarField[z + 1][y + 1][x]  // vertex 7: (x,   y+1, z+1)
                };
                
                std::array<Vertex, GridCell::CornerCount> cubeVertices;
                // Generate normalized texture coordinates (0.0 to 1.0 across entire grid)
                float gridSizeX = static_cast<float>(scalarField[0][0].size() - 1);
                float gridSizeY = static_cast<float>(scalarField[0].size() - 1);
                cubeVertices[0] = {glm::vec3(x,     y,     z),     glm::vec3(0.0f), glm::vec2(x / gridSizeX,         y / gridSizeY)};
                cubeVertices[1] = {glm::vec3(x + 1, y,     z),     glm::vec3(0.0f), glm::vec2((x + 1) / gridSizeX,   y / gridSizeY)};
                cubeVertices[2] = {glm::vec3(x + 1, y + 1, z),     glm::vec3(0.0f), glm::vec2((x + 1) / gridSizeX,   (y + 1) / gridSizeY)};
                cubeVertices[3] = {glm::vec3(x,     y + 1, z),     glm::vec3(0.0f), glm::vec2(x / gridSizeX,         (y + 1) / gridSizeY)};
                cubeVertices[4] = {glm::vec3(x,     y,     z + 1), glm::vec3(0.0f), glm::vec2(x / gridSizeX,         y / gridSizeY)};
                cubeVertices[5] = {glm::vec3(x + 1, y,     z + 1), glm::vec3(0.0f), glm::vec2((x + 1) / gridSizeX,   y / gridSizeY)};
                cubeVertices[6] = {glm::vec3(x + 1, y + 1, z + 1), glm::vec3(0.0f), glm::vec2((x + 1) / gridSizeX,   (y + 1) / gridSizeY)};
                cubeVertices[7] = {glm::vec3(x,     y + 1, z + 1), glm::vec3(0.0f), glm::vec2(x / gridSizeX,         (y + 1) / gridSizeY)};
                
                // Triangulate the cube and append results
                const auto cubeTriangles = triangulateCube(cubeVertices, cubeValues, isoLevel);
                triangles.insert(triangles.end(), cubeTriangles.begin(), cubeTriangles.end());
            }
        }
    }
    return triangles;
}

void CubeMarching::generateMesh(const std::vector<std::vector<std::vector<float>>>& scalarField, float isoLevel)
{
    isoLevel_ = isoLevel;
    vertices_.clear();
    indices_.clear();
    
    // Build vertex list from all cube edge intersections
    for (std::size_t z = 0; z + 1 < scalarField.size(); ++z)
    {
        for (std::size_t y = 0; y + 1 < scalarField[z].size(); ++y)
        {
            for (std::size_t x = 0; x + 1 < scalarField[z][y].size(); ++x)
            {
                processSingleCube(scalarField, static_cast<int>(x), static_cast<int>(y), static_cast<int>(z), isoLevel);
            }
        }
    }
}

void CubeMarching::clearMesh()
{
    vertices_.clear();
    indices_.clear();
}

void CubeMarching::processSingleCube(const std::vector<std::vector<std::vector<float>>>& scalarField, 
                                    int x, int y, int z, float isoLevel)
{
    // Bounds check
    if (z + 1 >= static_cast<int>(scalarField.size()) || 
        y + 1 >= static_cast<int>(scalarField[z].size()) || 
        x + 1 >= static_cast<int>(scalarField[z][y].size()) ||
        x < 0 || y < 0 || z < 0) {
        return; // Out of bounds
    }
    
    // Set up cube vertices and values
    const std::array<float, GridCell::CornerCount> cubeValues = {
        scalarField[z][y][x],             scalarField[z][y][x + 1],
        scalarField[z][y + 1][x + 1],     scalarField[z][y + 1][x],
        scalarField[z + 1][y][x],         scalarField[z + 1][y][x + 1],
        scalarField[z + 1][y + 1][x + 1], scalarField[z + 1][y + 1][x]
    };
    
    std::array<Vertex, GridCell::CornerCount> cubeVertices;
    // Normalized texture coordinates across grid
    float gridSizeX = static_cast<float>(scalarField[0][0].size() - 1);
    float gridSizeY = static_cast<float>(scalarField[0].size() - 1);
    cubeVertices[0] = {glm::vec3(x,     y,     z),     glm::vec3(0.0f), glm::vec2(x / gridSizeX,         y / gridSizeY)};
    cubeVertices[1] = {glm::vec3(x + 1, y,     z),     glm::vec3(0.0f), glm::vec2((x + 1) / gridSizeX,   y / gridSizeY)};
    cubeVertices[2] = {glm::vec3(x + 1, y + 1, z),     glm::vec3(0.0f), glm::vec2((x + 1) / gridSizeX,   (y + 1) / gridSizeY)};
    cubeVertices[3] = {glm::vec3(x,     y + 1, z),     glm::vec3(0.0f), glm::vec2(x / gridSizeX,         (y + 1) / gridSizeY)};
    cubeVertices[4] = {glm::vec3(x,     y,     z + 1), glm::vec3(0.0f), glm::vec2(x / gridSizeX,         y / gridSizeY)};
    cubeVertices[5] = {glm::vec3(x + 1, y,     z + 1), glm::vec3(0.0f), glm::vec2((x + 1) / gridSizeX,   y / gridSizeY)};
    cubeVertices[6] = {glm::vec3(x + 1, y + 1, z + 1), glm::vec3(0.0f), glm::vec2((x + 1) / gridSizeX,   (y + 1) / gridSizeY)};
    cubeVertices[7] = {glm::vec3(x,     y + 1, z + 1), glm::vec3(0.0f), glm::vec2(x / gridSizeX,         (y + 1) / gridSizeY)};
    
    const auto edgeVertices = getEdgeVertices(cubeVertices, cubeValues, isoLevel);
    const auto triangles = getTriangles(edgeVertices, calculateCubeIndex(cubeValues, isoLevel));

    // Add triangle vertices and flat normals
    for (const auto& triangle : triangles)
    {
        // Validate triangle indices
        if (triangle[0] >= 0 && triangle[0] < static_cast<int>(edgeVertices.size()) &&
            triangle[1] >= 0 && triangle[1] < static_cast<int>(edgeVertices.size()) &&
            triangle[2] >= 0 && triangle[2] < static_cast<int>(edgeVertices.size()))
        {
            const Vertex& v0 = edgeVertices[triangle[0]];
            const Vertex& v1 = edgeVertices[triangle[1]];
            const Vertex& v2 = edgeVertices[triangle[2]];

            const glm::vec3 faceNormal = calculateFaceNormal(v0, v1, v2);

            Vertex vert0 = v0, vert1 = v1, vert2 = v2;
            vert0.Normal = faceNormal;
            vert1.Normal = faceNormal;
            vert2.Normal = faceNormal;

            const std::size_t baseVertexIndex = vertices_.size();
            vertices_.push_back(vert0);
            vertices_.push_back(vert1);
            vertices_.push_back(vert2);

            indices_.push_back(static_cast<int>(baseVertexIndex));
            indices_.push_back(static_cast<int>(baseVertexIndex + 1));
            indices_.push_back(static_cast<int>(baseVertexIndex + 2));
        }
    }
}

void CubeMarching::processUpToCell(const std::vector<std::vector<std::vector<float>>>& scalarField,
                                  int maxX, int maxY, int maxZ, float isoLevel)
{
    isoLevel_ = isoLevel;
    
    // Process all cells from (0,0,0) up to (maxX, maxY, maxZ) inclusive
    for (int z = 0; z <= maxZ && z + 1 < static_cast<int>(scalarField.size()); ++z)
    {
        for (int y = 0; y <= maxY && y + 1 < static_cast<int>(scalarField[z].size()); ++y)
        {
            for (int x = 0; x <= maxX && x + 1 < static_cast<int>(scalarField[z][y].size()); ++x)
            {
                // Stop if we've reached the target cell
                if (x == maxX && y == maxY && z == maxZ) {
                    processSingleCube(scalarField, x, y, z, isoLevel);
                    return;
                }
                processSingleCube(scalarField, x, y, z, isoLevel);
            }
        }
    }
}

