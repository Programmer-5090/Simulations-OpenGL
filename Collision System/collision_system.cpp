// Implementation file for collision system
// This ensures all template and inline code gets compiled properly

#include "solver.h"
#include "thread.hpp"
#include "grid.h"
#include "utils.h"
#include "particle.h"
#include "constants.h"

// Force instantiation of commonly used templates
template class std::vector<Particle>;

// This file ensures that all the header-only implementations 
// get compiled into the executable properly
