#pragma once

#include "ai rubiks.h"

namespace CubeConversion {

CubeState compactToState(const CompactCube& cube);
CompactCube stateToCompact(const CubeState& state);

}
