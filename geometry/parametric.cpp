#include "parametric.h"
#include <cstdio>  // For printf debug output
#include <algorithm>  // For std::min, std::max

ParametricSurface::ParametricSurface() : GeometryData() {}

ParametricSurface::~ParametricSurface() {}

void ParametricSurface::generateMesh(float uStart, float uEnd, int uSegments, float vStart, float vEnd, int vSegments, std::function<glm::vec3(float, float)> surface_fn) {
    /**
     * Generate parametric surface mesh with proper indexing
     * 
     * Process:
     * 1. Calculate step sizes for u and v parameters
     * 2. Generate vertex grid (positions, UVs, normals)
     * 3. Create triangle indices for quads
     * 4. Add all data to GeometryData attributes
     */
    
    // Clear any existing data
    clear();
    
    // Calculate parameter step sizes
    float uStep = (uEnd - uStart) / static_cast<float>(uSegments);
    float vStep = (vEnd - vStart) / static_cast<float>(vSegments);
    
    // Helper function to calculate surface normal at any (u,v) point
    auto calculateSurfaceNormal = [&](float u, float v, float scale = 0.01f) -> glm::vec3 {
        // Use forward differences for calculating partial derivatives
        float du = scale * uStep;
        float dv = scale * vStep;
        
        // Calculate finite differences for partial derivatives
        glm::vec3 p_center = surface_fn(u, v);
        glm::vec3 p_du = surface_fn(u + du, v);
        glm::vec3 p_dv = surface_fn(u, v + dv);
        
        // Calculate tangent vectors
        glm::vec3 tangent_u = (p_du - p_center) / du;
        glm::vec3 tangent_v = (p_dv - p_center) / dv;
        
        // Normal is cross product of tangent vectors
        glm::vec3 normal = glm::cross(tangent_u, tangent_v);
        float magnitude = glm::length(normal);
        
        // Handle degenerate cases
        if (magnitude < 1e-8f) {
            // Fallback to position-based normal for spherical surfaces
            if (glm::length(p_center) > 1e-8f) {
                normal = glm::normalize(p_center);
            } else {
                normal = glm::vec3(0.0f, 1.0f, 0.0f); // Default upward normal
            }
        } else {
            normal = glm::normalize(normal);
            
            // CRITICAL FIX: Ensure normal points outward from the surface
            // For sphere-like surfaces, the outward direction should generally
            // align with the position vector from the origin
            if (glm::length(p_center) > 1e-8f) {
                glm::vec3 outwardDirection = glm::normalize(p_center);
                
                // If the normal is pointing inward (negative dot product), flip it
                if (glm::dot(normal, outwardDirection) < 0.0f) {
                    normal = -normal;
                }
            }
        }
        
        return normal;
    };
    
    // Storage for vertex data
    std::vector<float> vertexPositions;
    std::vector<float> vertexUVs;  
    std::vector<float> vertexNormals;
    std::vector<unsigned int> indices;
    
    // Step 1: Pre-calculate vertex normals in a 2D grid
    std::vector<std::vector<glm::vec3>> vertexNormalGrid(uSegments + 1);
    for (int uIdx = 0; uIdx <= uSegments; ++uIdx) {
        vertexNormalGrid[uIdx].resize(vSegments + 1);
        float u = uStart + uIdx * uStep;
        
        for (int vIdx = 0; vIdx <= vSegments; ++vIdx) {
            float v = vStart + vIdx * vStep;
            vertexNormalGrid[uIdx][vIdx] = calculateSurfaceNormal(u, v);
        }
    }
    
    // Step 2: Generate all vertices and add to flat arrays
    for (int uIdx = 0; uIdx <= uSegments; ++uIdx) {
        float u = uStart + uIdx * uStep;
        
        for (int vIdx = 0; vIdx <= vSegments; ++vIdx) {
            float v = vStart + vIdx * vStep;
            
            // Generate vertex position
            glm::vec3 position = surface_fn(u, v);
            vertexPositions.insert(vertexPositions.end(), {position.x, position.y, position.z});
            
            // Generate UV coordinates (normalized to [0,1])
            float uvU = static_cast<float>(uIdx) / static_cast<float>(uSegments);
            float uvV = static_cast<float>(vIdx) / static_cast<float>(vSegments);
            vertexUVs.insert(vertexUVs.end(), {uvU, uvV});
            
            // Add pre-calculated normal
            glm::vec3 normal = vertexNormalGrid[uIdx][vIdx];
            vertexNormals.insert(vertexNormals.end(), {normal.x, normal.y, normal.z});
        }
    }
    
    // Step 3: Generate triangle indices for each quad in the grid
    for (int uIdx = 0; uIdx < uSegments; ++uIdx) {
        for (int vIdx = 0; vIdx < vSegments; ++vIdx) {
            // Calculate indices of the four corners of this quad
            // Grid layout: each row has (vSegments + 1) vertices
            unsigned int i0 = uIdx * (vSegments + 1) + vIdx;         // Bottom-left
            unsigned int i1 = (uIdx + 1) * (vSegments + 1) + vIdx;   // Top-left  
            unsigned int i2 = (uIdx + 1) * (vSegments + 1) + (vIdx + 1); // Top-right
            unsigned int i3 = uIdx * (vSegments + 1) + (vIdx + 1);   // Bottom-right
            
            // Create two triangles for this quad
            // Triangle 1: (i0, i1, i2) - counter-clockwise winding
            indices.insert(indices.end(), {i0, i1, i2});
            
            // Triangle 2: (i0, i2, i3) - counter-clockwise winding  
            indices.insert(indices.end(), {i0, i2, i3});
        }
    }
    
    // Step 4: Add all attributes to the GeometryData
    addAttribute("v_pos", 3, vertexPositions);   // Vertex positions (x,y,z)
    addAttribute("v_uv", 2, vertexUVs);          // Texture coordinates (u,v)
    addAttribute("v_norm", 3, vertexNormals);    // Vertex normals (nx,ny,nz)
    
    // Set triangle indices
    setIndices(indices);
    
    // Debug output
    size_t vertexCount = vertexPositions.size() / 3;
    size_t triangleCount = indices.size() / 3;
    
    printf("Generated parametric surface: %zu vertices, %zu triangles\n", vertexCount, triangleCount);
}