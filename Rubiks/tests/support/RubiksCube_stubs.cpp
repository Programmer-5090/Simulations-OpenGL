#include "RubiksCube.h"

// This avoids linking the full renderer into RubiksIdTest.

void RubiksCube::queueFaceRotation(int /*faceIndex*/, float /*targetAngle*/, float /*duration*/) {
    // no-op in headless test
}
