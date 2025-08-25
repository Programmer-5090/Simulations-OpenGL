#include "polygon.h"
#include <glm/glm.hpp>
#include <vector>
#include <cmath>

Polygon::Polygon(int sides, float radius) : GeometryData(), sides(sides), radius(radius) {
    // Use float math and cosf/sinf to avoid narrowing conversions
    const float TWO_PI = 2 * M_PI;
    const float angleStep = TWO_PI / static_cast<float>(sides);

    std::vector<float> posData;    // flat list: x,y,z, x,y,z, ...
    std::vector<float> uvData;     // flat list: u,v, u,v, ...
    std::vector<float> normData;   // flat list: nx,ny,nz, ...
    std::vector<unsigned int> indices;

    // Center uv and normal
    const float uvCenterU = 0.5f;
    const float uvCenterV = 0.5f;
    const float normX = 0.0f, normY = 0.0f, normZ = 1.0f;

    for (int i = 0; i < sides; ++i) {
        // Triangle fan: center, current, next
        float cx = 0.0f, cy = 0.0f, cz = 0.0f;
        float x1 = radius * std::cosf(i * angleStep);
        float y1 = radius * std::sinf(i * angleStep);
        float z1 = 0.0f;
        float x2 = radius * std::cosf((i + 1) * angleStep);
        float y2 = radius * std::sinf((i + 1) * angleStep);
        float z2 = 0.0f;

        // Push positions (center, v1, v2)
        posData.push_back(cx); posData.push_back(cy); posData.push_back(cz);
        posData.push_back(x1); posData.push_back(y1); posData.push_back(z1);
        posData.push_back(x2); posData.push_back(y2); posData.push_back(z2);

        // Push UVs (center, v1, v2)
        uvData.push_back(uvCenterU); uvData.push_back(uvCenterV);
        uvData.push_back(0.5f + 0.5f * std::cosf(i * angleStep)); uvData.push_back(0.5f + 0.5f * std::sinf(i * angleStep));
        uvData.push_back(0.5f + 0.5f * std::cosf((i + 1) * angleStep)); uvData.push_back(0.5f + 0.5f * std::sinf((i + 1) * angleStep));

        // Push normals (same for all three vertices)
        for (int k = 0; k < 3; ++k) {
            normData.push_back(normX); normData.push_back(normY); normData.push_back(normZ);
        }

        // Indices for this triangle (sequential)
        unsigned int base = static_cast<unsigned int>(i * 3);
        indices.push_back(base + 0);
        indices.push_back(base + 1);
        indices.push_back(base + 2);
    }

    // Add attributes to GeometryData (expects flat vector<float>)
    addAttribute("v_pos", 3, posData);   // Vertex positions (x,y,z)
    addAttribute("v_uv", 2, uvData);     // Texture coordinates (u,v)
    addAttribute("v_norm", 3, normData); // Vertex normals (nx,ny,nz)

    // Set triangle indices
    setIndices(indices);
}

Polygon::~Polygon() {}
