#include "CubeStateMachine.h"
#include "CubeConversion.h"
#include "ai rubiks.h"

#include <algorithm>
#include <cmath>
#include <iostream>

const std::array<FacePivot, 9> kFacePivots = {
    FacePivot{{+1, 0, 0}, {1, 0, 0}},  // Right
    FacePivot{{-1, 0, 0}, {-1, 0, 0}}, // Left
    FacePivot{{0, +1, 0}, {0, 1, 0}},  // Up
    FacePivot{{0, -1, 0}, {0, -1, 0}}, // Down
    FacePivot{{0, 0, +1}, {0, 0, 1}},  // Front
    FacePivot{{0, 0, -1}, {0, 0, -1}}, // Back
    FacePivot{{0, 0, 0}, {1, 0, 0}},   // M slice
    FacePivot{{0, 0, 0}, {0, 1, 0}},   // E slice
    FacePivot{{0, 0, 0}, {0, 0, 1}},   // S slice
};

const std::array<glm::vec3, 6> kFaceDirections = {
    glm::vec3(1.0f, 0.0f, 0.0f),
    glm::vec3(-1.0f, 0.0f, 0.0f),
    glm::vec3(0.0f, 1.0f, 0.0f),
    glm::vec3(0.0f, -1.0f, 0.0f),
    glm::vec3(0.0f, 0.0f, 1.0f),
    glm::vec3(0.0f, 0.0f, -1.0f)
};

int directionToFaceIndex(const glm::vec3& dir) {
    auto snapComponent = [](float value) {
        if (value > 0.5f) return 1.0f;
        if (value < -0.5f) return -1.0f;
        return 0.0f;
    };

    glm::vec3 snapped{snapComponent(dir.x), snapComponent(dir.y), snapComponent(dir.z)};
    for (int i = 0; i < static_cast<int>(kFaceDirections.size()); ++i) {
        if (glm::length(snapped - kFaceDirections[i]) < 0.01f) {
            return i;
        }
    }

    float bestDot = -1.0f;
    int bestIdx = 0;
    float len = glm::length(dir);
    glm::vec3 normalized = len > 0.0f ? (dir / len) : glm::vec3(0.0f);
    for (int i = 0; i < static_cast<int>(kFaceDirections.size()); ++i) {
        float dot = glm::dot(normalized, kFaceDirections[i]);
        if (dot > bestDot) {
            bestDot = dot;
            bestIdx = i;
        }
    }
    return bestIdx;
}

CubeStateMachine::CubeStateMachine() {
    buildCubieOffsets();
    initCubieFaceColors();
    logicalCube = CompactCube{};
    rebuildStateFromLogical();
}

void CubeStateMachine::buildCubieOffsets() {
    cubieOffsets.clear();
    cubieOffsets.reserve(26);
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            for (int z = -1; z <= 1; ++z) {
                if (x == 0 && y == 0 && z == 0) {
                    continue;
                }
                cubieOffsets.emplace_back(static_cast<float>(x),
                                          static_cast<float>(y),
                                          static_cast<float>(z));
            }
        }
    }
}

void CubeStateMachine::initCubieFaceColors() {
    cubieFaceColors.resize(cubieOffsets.size());
    for (size_t i = 0; i < cubieOffsets.size(); ++i) {
        const auto& pos = cubieOffsets[i];
        CubieFaces& faces = cubieFaceColors[i];

        faces.colorIndex[0] = (pos.x > 0.5f) ? 0 : -1;
        faces.colorIndex[1] = (pos.x < -0.5f) ? 1 : -1;
        faces.colorIndex[2] = (pos.y > 0.5f) ? 2 : -1;
        faces.colorIndex[3] = (pos.y < -0.5f) ? 3 : -1;
        faces.colorIndex[4] = (pos.z > 0.5f) ? 4 : -1;
        faces.colorIndex[5] = (pos.z < -0.5f) ? 5 : -1;
    }
}

void CubeStateMachine::rotateCubieFaceColors(size_t cubieIndex, const glm::mat4& rotationMatrix) {
    CubieFaces& faces = cubieFaceColors[cubieIndex];
    std::array<int, 6> oldColors = faces.colorIndex;
    glm::mat3 rotation3(rotationMatrix);

    for (int face = 0; face < static_cast<int>(kFaceDirections.size()); ++face) {
        glm::vec3 rotated = rotation3 * kFaceDirections[face];
        int targetIndex = directionToFaceIndex(rotated);
        faces.colorIndex[targetIndex] = oldColors[face];
    }
}

void CubeStateMachine::rotateFace(int faceIndex, float angleDegrees) {
    const auto& pivot = kFacePivots[faceIndex];
    glm::mat4 rotation4 = glm::rotate(glm::mat4(1.0f), glm::radians(angleDegrees), pivot.axis);

    for (size_t i = 0; i < cubieOffsets.size(); ++i) {
        if (cubieOnFace(cubieOffsets[i], faceIndex)) {
            glm::vec4 pos = rotation4 * glm::vec4(cubieOffsets[i], 1.0f);
            cubieOffsets[i] = glm::vec3(pos);

            cubieOffsets[i].x = std::round(cubieOffsets[i].x);
            cubieOffsets[i].y = std::round(cubieOffsets[i].y);
            cubieOffsets[i].z = std::round(cubieOffsets[i].z);

            rotateCubieFaceColors(i, rotation4);
        }
    }

    applyLogicalRotation(faceIndex, angleDegrees);
}

void CubeStateMachine::applyMove(Move move) {
    int face = -1;
    float angle = 0.0f;
    switch (move) {
        case Move::U:       face = 2; angle = -90.0f; break;
        case Move::U_PRIME: face = 2; angle = 90.0f; break;
        case Move::U2:      face = 2; angle = 180.0f; break;
        case Move::D:       face = 3; angle = -90.0f; break;
        case Move::D_PRIME: face = 3; angle = 90.0f; break;
        case Move::D2:      face = 3; angle = 180.0f; break;
        case Move::L:       face = 1; angle = -90.0f; break;
        case Move::L_PRIME: face = 1; angle = 90.0f; break;
        case Move::L2:      face = 1; angle = 180.0f; break;
        case Move::R:       face = 0; angle = -90.0f; break;
        case Move::R_PRIME: face = 0; angle = 90.0f; break;
        case Move::R2:      face = 0; angle = 180.0f; break;
        case Move::F:       face = 4; angle = -90.0f; break;
        case Move::F_PRIME: face = 4; angle = 90.0f; break;
        case Move::F2:      face = 4; angle = 180.0f; break;
        case Move::B:       face = 5; angle = -90.0f; break;
        case Move::B_PRIME: face = 5; angle = 90.0f; break;
        case Move::B2:      face = 5; angle = 180.0f; break;
        default: break;
    }

    if (face >= 0) {
        rotateFace(face, angle);
    }
}

void CubeStateMachine::rebuildStateFromLogical() {
    state = CubeConversion::compactToState(logicalCube);
}

void CubeStateMachine::applyLogicalRotation(int faceIndex, float angleDegrees) {
    if (faceIndex < 0 || faceIndex > 5) return;

    char faceChar = '?';
    switch (faceIndex) {
        case 0: faceChar = 'R'; break;
        case 1: faceChar = 'L'; break;
        case 2: faceChar = 'U'; break;
        case 3: faceChar = 'D'; break;
        case 4: faceChar = 'F'; break;
        case 5: faceChar = 'B'; break;
        default: return;
    }

    int turns = static_cast<int>(std::round(std::fabs(angleDegrees) / 90.0f)) % 4;
    if (turns == 0) return;

    int cwTurns = (angleDegrees < 0.0f) ? turns : (4 - turns) % 4;
    if (cwTurns == 0) cwTurns = 2;

    logicalCube.applyMove(faceChar, cwTurns);
    rebuildStateFromLogical();
}

int CubeStateMachine::findCubieAtPosition(const glm::vec3& position) const {
    const float epsilon = 0.5f;
    for (size_t i = 0; i < cubieOffsets.size(); ++i) {
        if (glm::length(cubieOffsets[i] - position) < epsilon) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool CubeStateMachine::cubieOnFace(const glm::vec3& offset, int faceIndex) const {
    const float epsilon = 0.1f;
    switch (faceIndex) {
        case 0: return std::abs(offset.x - 1.0f) < epsilon;
        case 1: return std::abs(offset.x + 1.0f) < epsilon;
        case 2: return std::abs(offset.y - 1.0f) < epsilon;
        case 3: return std::abs(offset.y + 1.0f) < epsilon;
        case 4: return std::abs(offset.z - 1.0f) < epsilon;
        case 5: return std::abs(offset.z + 1.0f) < epsilon;
        case 6: return std::abs(offset.x) < epsilon;
        case 7: return std::abs(offset.y) < epsilon;
        case 8: return std::abs(offset.z) < epsilon;
        default: return false;
    }
}

bool CubeStateMachine::isSolved() const {
    for (int face = 0; face < 6; ++face) {
        int centerColor = state[face * 9 + 4];
        for (int i = 0; i < 9; ++i) {
            if (state[face * 9 + i] != centerColor) {
                return false;
            }
        }
    }
    return true;
}

void CubeStateMachine::printState() const {
    const char* faceNames[] = {"Right", "Left", "Up", "Down", "Front", "Back"};
    const char* colorNames[] = {"R", "O", "W", "Y", "G", "B"};

    std::cout << "\n=== Rubiks Cube State ===" << std::endl;
    for (int face = 0; face < 6; ++face) {
        std::cout << faceNames[face] << ":" << std::endl;
        for (int row = 0; row < 3; ++row) {
            std::cout << "  ";
            for (int col = 0; col < 3; ++col) {
                int colorIdx = state[face * 9 + row * 3 + col];
                if (colorIdx >= 0 && colorIdx < 6) {
                    std::cout << colorNames[colorIdx] << " ";
                } else {
                    std::cout << "? ";
                }
            }
            std::cout << std::endl;
        }
    }
    std::cout << "Solved: " << (isSolved() ? "YES" : "NO") << std::endl;
    std::cout << "=========================\n" << std::endl;
}
