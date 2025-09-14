#include "sphere.h"

const float pi = 3.14159265359f;

Sphere::Sphere(float radius, int rSegments)
    : m_radius(radius), m_uSegments(rSegments), m_vSegments(rSegments) {
    auto sphereFunc = [this](float u, float v) -> glm::vec3 {
        float x = m_radius/2 * sin(u) * cos(v);
        float y = m_radius/2 * cos(u);
        float z = m_radius/2 * sin(u) * sin(v);
        return glm::vec3(x, y, z);
    };
    
    generateMesh(0, 2 * pi, rSegments, -pi/2, pi/2, rSegments, sphereFunc);
}

Sphere::~Sphere() {}
