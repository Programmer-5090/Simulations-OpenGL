/**
 * GeometryData - A flexible container for 3D mesh vertex attributes
 * 
 * This class serves as a bridge between raw vertex data and OpenGL-ready Mesh objects.
 * It's inspired by the Python MeshData class and provides similar functionality for C++.
 * 
 * Key Features:
 * - Store vertex attributes (positions, normals, texture coordinates, etc.)
 * - Flexible attribute system (any name, any component count)
 * - Convert to OpenGL-ready Mesh objects
 * - Merge multiple geometry datasets
 * - Extract triangle data for physics/collision
 * 
 * Common Usage Pattern:
 *   GeometryData geometry;
 *   geometry.addAttribute("v_pos", 3, positions);  // x,y,z per vertex
 *   geometry.addAttribute("v_norm", 3, normals);   // nx,ny,nz per vertex
 *   geometry.addAttribute("v_uv", 2, texCoords);   // u,v per vertex
 *   geometry.setIndices(indices);                  // triangle indices
 *   Mesh mesh = geometry.toMesh();                 // convert to renderable mesh
 */

#ifndef GEOMETRY_DATA_H
#define GEOMETRY_DATA_H
#define _USE_MATH_DEFINES

#include <cmath>
#include <array>
#include <vector>
#include <string>
#include <unordered_map>

class Mesh; // Forward declaration to avoid circular includes

/**
 * Simple structure to represent a 3D triangle
 * Much more readable than nested arrays
 */
struct Triangle {
    float vertex1[3];  // First vertex: [x, y, z]
    float vertex2[3];  // Second vertex: [x, y, z]  
    float vertex3[3];  // Third vertex: [x, y, z]
    
    // Constructor for easy initialization
    Triangle(float x1, float y1, float z1,
             float x2, float y2, float z2,
             float x3, float y3, float z3) {
        vertex1[0] = x1; vertex1[1] = y1; vertex1[2] = z1;
        vertex2[0] = x2; vertex2[1] = y2; vertex2[2] = z2;
        vertex3[0] = x3; vertex3[1] = y3; vertex3[2] = z3;
    }
    
    // Default constructor
    Triangle() = default;
};

class GeometryData {
public:
    GeometryData();
    ~GeometryData();

    /**
     * Add a vertex attribute to the geometry
     * 
     * @param name The attribute name (e.g., "v_pos", "v_norm", "v_uv", "color")
     * @param components Number of float components per vertex (3 for positions/normals, 2 for UV)
     * @param data Tightly packed float array: [vertex0_comp0, vertex0_comp1, ..., vertex1_comp0, ...]
     * 
     * Example:
     *   // Add position data for 2 vertices
     *   std::vector<float> positions = {0.0f, 1.0f, 0.0f,  1.0f, 0.0f, 0.0f};
     *   geometry.addAttribute("v_pos", 3, positions);
     */
    void addAttribute(const std::string& name, int components, const std::vector<float>& data);

    /**
     * Set triangle indices for the geometry
     * 
     * @param idx Array of vertex indices forming triangles (every 3 indices = 1 triangle)
     * 
     * Example:
     *   std::vector<unsigned int> indices = {0, 1, 2,  2, 3, 0}; // 2 triangles
     *   geometry.setIndices(indices);
     */
    void setIndices(const std::vector<unsigned int>& idx);

    /**
     * Count the number of vertices in the geometry
     * Uses the "v_pos" attribute if available, otherwise the first available attribute
     */
    size_t countVertices() const;

    /**
     * Count the number of indices (not triangles - divide by 3 for triangle count)
     */
    size_t countIndices() const;

    /**
     * Extract triangles as simple Triangle structures
     * 
     * @return Vector of Triangle objects, each containing 3 vertices with [x,y,z] coordinates
     * 
     * Uses indices if available, otherwise treats vertices as sequential triangles.
     * Only works if "v_pos" attribute exists and has 3 components.
     * 
     * Example usage:
     *   auto triangles = geometry.getTriangles();
     *   for (const auto& triangle : triangles) {
     *       std::cout << "Triangle: (" << triangle.vertex1[0] << "," << triangle.vertex1[1] << "," << triangle.vertex1[2] << ") "
     *                 << "(" << triangle.vertex2[0] << "," << triangle.vertex2[1] << "," << triangle.vertex2[2] << ") "
     *                 << "(" << triangle.vertex3[0] << "," << triangle.vertex3[1] << "," << triangle.vertex3[2] << ")" << std::endl;
     *   }
     */
    std::vector<Triangle> getTriangles() const;

    /**
     * Convert this geometry data into a renderable Mesh object
     * 
     * @return Mesh object with VAO/VBO/EBO set up for OpenGL rendering
     * 
     * Missing attributes are filled with defaults:
     * - Missing normals: (0, 1, 0) - pointing up
     * - Missing UV coords: (0, 0) - bottom-left corner
     * - Missing indices: sequential (0, 1, 2, 3, 4, 5, ...)
     */
    Mesh toMesh() const;

    /**
     * Merge another GeometryData object into this one
     * 
     * @param other The geometry to merge
     * 
     * - Appends vertices from other to this geometry
     * - Adjusts indices from other to point to correct vertices
     * - Aligns attributes (missing attributes filled with zeros)
     * - Useful for combining multiple objects into one mesh
     */
    void merge(const GeometryData& other);

    /**
     * Clear all stored vertex data and indices
     * Resets the object to empty state
     */
    void clear();

private:
    // Storage for vertex attribute data
    // Key: attribute name (e.g., "v_pos"), Value: flat array of floats
    std::unordered_map<std::string, std::vector<float>> m_attributes;
    
    // Number of components per vertex for each attribute
    // Key: attribute name, Value: component count (e.g., 3 for positions)
    std::unordered_map<std::string, int> m_components;
    
    // Triangle indices (every 3 values form one triangle)
    std::vector<unsigned int> m_indices;
};

#endif // GEOMETRY_DATA_H

