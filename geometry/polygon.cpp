#include "polygon.h"
#include <glm/glm.hpp>
#include <vector>
#include <cmath>

Polygon::Polygon(int sides, float radius) : GeometryData(), sides(sides), radius(radius) {
    const float TWO_PI = 2 * M_PI;
    const float angleStep = TWO_PI / static_cast<float>(sides);

    std::vector<float> posData;
    std::vector<float> uvData;
    std::vector<float> normData;
    std::vector<unsigned int> indices;

    const float uvCenterU = 0.5f;
    const float uvCenterV = 0.5f;
    const float normX = 0.0f, normY = 0.0f, normZ = 1.0f;

    for (int i = 0; i < sides; ++i) {
        float cx = 0.0f, cy = 0.0f, cz = 0.0f;
        float x1 = radius * std::cosf(i * angleStep);
        float y1 = radius * std::sinf(i * angleStep);
        float z1 = 0.0f;
        float x2 = radius * std::cosf((i + 1) * angleStep);
        float y2 = radius * std::sinf((i + 1) * angleStep);
        float z2 = 0.0f;

        posData.push_back(cx); posData.push_back(cy); posData.push_back(cz);
        posData.push_back(x1); posData.push_back(y1); posData.push_back(z1);
        posData.push_back(x2); posData.push_back(y2); posData.push_back(z2);

        uvData.push_back(uvCenterU); uvData.push_back(uvCenterV);
        uvData.push_back(0.5f + 0.5f * std::cosf(i * angleStep)); uvData.push_back(0.5f + 0.5f * std::sinf(i * angleStep));
        uvData.push_back(0.5f + 0.5f * std::cosf((i + 1) * angleStep)); uvData.push_back(0.5f + 0.5f * std::sinf((i + 1) * angleStep));

        for (int k = 0; k < 3; ++k) {
            normData.push_back(normX); normData.push_back(normY); normData.push_back(normZ);
        }

        unsigned int base = static_cast<unsigned int>(i * 3);
        indices.push_back(base + 0);
        indices.push_back(base + 1);
        indices.push_back(base + 2);
    }

    addAttribute("v_pos", 3, posData);
    addAttribute("v_uv", 2, uvData);
    addAttribute("v_norm", 3, normData);

    setIndices(indices);
}

Polygon::~Polygon() {}
