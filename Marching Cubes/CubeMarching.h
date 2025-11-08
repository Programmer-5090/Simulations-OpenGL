/*
    Tables and conventions from
    http://paulbourke.net/geometry/polygonise/
*/
#ifndef CUBEMARCHING_H
#define CUBEMARCHING_H

#include <vector>
#include <array>
#include <cstddef>
#include "../mesh.h"

// Grid cell: 8 corner vertices and their scalar values
struct GridCell {
    static constexpr std::size_t CornerCount = 8;
    std::array<Vertex, CornerCount> vertex{}; // corner positions
    std::array<float, CornerCount> value{};   // corner scalar values
};

// CubeMarching: compute cube index, interpolate edge intersections and
// produce triangle lists for a single cube or a full scalar field.
class CubeMarching {
public:
    CubeMarching() = default;
    ~CubeMarching() = default;

    CubeMarching(const CubeMarching&) = default;
    CubeMarching& operator=(const CubeMarching&) = default;
    CubeMarching(CubeMarching&&) = default;
    CubeMarching& operator=(CubeMarching&&) = default;

    // Calculate 8-bit cube index (0..255) from 8 corner scalar values.
    int calculateCubeIndex(const std::array<float, GridCell::CornerCount>& cubeValues, float isoLevel) const;

    // Interpolate intersection points on edges; up to 12 edge vertices returned.
    std::vector<Vertex> interpolateVertices(const std::array<Vertex, GridCell::CornerCount>& cubeVertices,
                                            const std::array<float, GridCell::CornerCount>& cubeValues,
                                            float isoLevel) const;

    // Given edge intersections and a cube index, return triangle index triples
    std::vector<std::array<int,3>> getTriangles(const std::vector<Vertex>& edgeVertices, int cubeIndex) const;

    // Convenience: compute edge vertices directly from cube corners/values.
    std::vector<Vertex> getEdgeVertices(const std::array<Vertex, GridCell::CornerCount>& cubeVertices,
                                        const std::array<float, GridCell::CornerCount>& cubeValues,
                                        float isoLevel) const;

    // Triangulate a single cube and return triangle index triples (indices into edge vertices).
    std::vector<std::array<int,3>> triangulateCube(const std::array<Vertex, GridCell::CornerCount>& cubeVertices,
                                                  const std::array<float, GridCell::CornerCount>& cubeValues,
                                                  float isoLevel) const;

    // Triangulate a full 3D scalar field (field[z][y][x]). Returns triangle triples.
    std::vector<std::array<int,3>> triangulateField(const std::vector<std::vector<std::vector<float>>>& scalarField,
                                                    float isoLevel) const;

    // Generate mesh (vertices + indices) for the field and store internally.
    void generateMesh(const std::vector<std::vector<std::vector<float>>>& scalarField, float isoLevel);

    // Stepwise processing helpers
    void clearMesh();
    void processSingleCube(const std::vector<std::vector<std::vector<float>>>& scalarField,
                          int x, int y, int z, float isoLevel);
    void processUpToCell(const std::vector<std::vector<std::vector<float>>>& scalarField,
                        int maxX, int maxY, int maxZ, float isoLevel);

    void setIsoLevel(float isoLevel) { isoLevel_ = isoLevel; }
    float getIsoLevel() const { return isoLevel_; }

    // Access generated mesh data
    const std::vector<Vertex>& getVertices() const { return vertices_; }
    const std::vector<int>& getIndices() const { return indices_; }
    std::size_t getTriangleCount() const { return indices_.size() / 3; }

private:
    float isoLevel_{0.0f};
    std::vector<Vertex> vertices_{}; // generated vertices with normals built-in
    std::vector<int> indices_{};     // flattened triangle indices (triples)
};
#endif // CUBEMARCHING_H
