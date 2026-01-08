#pragma once

#include <array>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "CubeTypes.h"
#include "ai rubiks.h"
enum Move : int;

class CubeStateMachine {
public:
    CubeStateMachine();

    const std::vector<glm::vec3>& getCubieOffsets() const { return cubieOffsets; }
    const std::vector<CubieFaces>& getCubieFaceColors() const { return cubieFaceColors; }
    const std::array<int, 54>& getState() const { return state; }

    void rotateFace(int faceIndex, float angleDegrees);
    void applyMove(Move move);

    int findCubieAtPosition(const glm::vec3& position) const;
    bool cubieOnFace(const glm::vec3& offset, int faceIndex) const;

    bool isSolved() const;
    void printState() const;

private:
    void buildCubieOffsets();
    void initCubieFaceColors();
    void rotateCubieFaceColors(size_t cubieIndex, const glm::mat4& rotationMatrix);
    void rebuildStateFromLogical();
    void applyLogicalRotation(int faceIndex, float angleDegrees);

    std::vector<glm::vec3> cubieOffsets;
    std::vector<CubieFaces> cubieFaceColors;
    std::array<int, 54> state;
    CompactCube logicalCube;
};
