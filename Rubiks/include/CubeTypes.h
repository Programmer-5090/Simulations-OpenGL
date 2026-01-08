#pragma once

#include <array>

#include <glm/glm.hpp>

struct CubieFaces {
    std::array<int, 6> colorIndex;
    CubieFaces() : colorIndex{-1, -1, -1, -1, -1, -1} {}
};

struct FacePivot {
    glm::vec3 position;  // Center of the rotating face
    glm::vec3 axis;      // Rotation axis (normal to face)
};

extern const std::array<FacePivot, 9> kFacePivots;
extern const std::array<glm::vec3, 6> kFaceDirections;
int directionToFaceIndex(const glm::vec3& dir);
