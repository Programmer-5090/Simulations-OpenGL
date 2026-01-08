#include "parametric.h"
#include <cstdio>
#include <algorithm>

ParametricSurface::ParametricSurface() : GeometryData() {}

ParametricSurface::~ParametricSurface() {}

void ParametricSurface::generateMesh(float uStart, float uEnd, int uSegments, float vStart, float vEnd, int vSegments, std::function<glm::vec3(float, float)> surface_fn) {
    clear();
    
    float uStep = (uEnd - uStart) / static_cast<float>(uSegments);
    float vStep = (vEnd - vStart) / static_cast<float>(vSegments);
    
    auto calculateSurfaceNormal = [&](float u, float v, float scale = 0.01f) -> glm::vec3 {
        float du = scale * uStep;
        float dv = scale * vStep;
        
        glm::vec3 p_center = surface_fn(u, v);
        glm::vec3 p_du = surface_fn(u + du, v);
        glm::vec3 p_dv = surface_fn(u, v + dv);
        
        glm::vec3 tangent_u = (p_du - p_center) / du;
        glm::vec3 tangent_v = (p_dv - p_center) / dv;
        
        glm::vec3 normal = glm::cross(tangent_u, tangent_v);
        float magnitude = glm::length(normal);
        
        if (magnitude < 1e-8f) {
            if (glm::length(p_center) > 1e-8f) {
                normal = glm::normalize(p_center);
            } else {
                normal = glm::vec3(0.0f, 1.0f, 0.0f);
            }
        } else {
            normal = glm::normalize(normal);
            
            if (glm::length(p_center) > 1e-8f) {
                glm::vec3 outwardDirection = glm::normalize(p_center);
                if (glm::dot(normal, outwardDirection) < 0.0f) {
                    normal = -normal;
                }
            }
        }
        
        return normal;
    };
    
    std::vector<float> vertexPositions;
    std::vector<float> vertexUVs;  
    std::vector<float> vertexNormals;
    std::vector<unsigned int> indices;
    
    std::vector<std::vector<glm::vec3>> vertexNormalGrid(uSegments + 1);
    for (int uIdx = 0; uIdx <= uSegments; ++uIdx) {
        vertexNormalGrid[uIdx].resize(vSegments + 1);
        float u = uStart + uIdx * uStep;
        
        for (int vIdx = 0; vIdx <= vSegments; ++vIdx) {
            float v = vStart + vIdx * vStep;
            vertexNormalGrid[uIdx][vIdx] = calculateSurfaceNormal(u, v);
        }
    }
    
    for (int uIdx = 0; uIdx <= uSegments; ++uIdx) {
        float u = uStart + uIdx * uStep;
        
        for (int vIdx = 0; vIdx <= vSegments; ++vIdx) {
            float v = vStart + vIdx * vStep;
            
            glm::vec3 position = surface_fn(u, v);
            vertexPositions.insert(vertexPositions.end(), {position.x, position.y, position.z});
            
            float uvU = static_cast<float>(uIdx) / static_cast<float>(uSegments);
            float uvV = static_cast<float>(vIdx) / static_cast<float>(vSegments);
            vertexUVs.insert(vertexUVs.end(), {uvU, uvV});
            
            glm::vec3 normal = vertexNormalGrid[uIdx][vIdx];
            vertexNormals.insert(vertexNormals.end(), {normal.x, normal.y, normal.z});
        }
    }
    
    for (int uIdx = 0; uIdx < uSegments; ++uIdx) {
        for (int vIdx = 0; vIdx < vSegments; ++vIdx) {
            unsigned int i0 = uIdx * (vSegments + 1) + vIdx;
            unsigned int i1 = (uIdx + 1) * (vSegments + 1) + vIdx;
            unsigned int i2 = (uIdx + 1) * (vSegments + 1) + (vIdx + 1);
            unsigned int i3 = uIdx * (vSegments + 1) + (vIdx + 1);
            
            indices.insert(indices.end(), {i0, i1, i2});
            indices.insert(indices.end(), {i0, i2, i3});
        }
    }
    
    addAttribute("v_pos", 3, vertexPositions);
    addAttribute("v_uv", 2, vertexUVs);
    addAttribute("v_norm", 3, vertexNormals);
    
    setIndices(indices);
    
    size_t vertexCount = vertexPositions.size() / 3;
    size_t triangleCount = indices.size() / 3;
    
    printf("Generated parametric surface: %zu vertices, %zu triangles\n", vertexCount, triangleCount);
}