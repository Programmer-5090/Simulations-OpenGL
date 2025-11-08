#pragma once
#include <glm/glm.hpp>

const float MAX_PARTICLE_RADIUS = 0.07f;

const float CELL_SIZE = MAX_PARTICLE_RADIUS * 2;

const float MAX_PARTICLE_DENSITY = 100.0f;  // 100 particles per unit² = 30k particles in 300 unit² world
const int MAX_PARTICLES = 17650;

const float AUTO_SPAWN_INTERVAL = 0.03f;
const glm::vec2 CONSTANT_VELOCITY = glm::vec2(2.50f, -1.0f);
const int STREAM_COUNT = 17;
const float STREAM_SPACING = 0.2f;
const float SPAWN_MARGIN_X = 0.15f;
const float TOP_MARGIN = 0.5f;

const unsigned int SCR_WIDTH  = 800;
const unsigned int SCR_HEIGHT = 600;

// World boundaries (simulation space, not pixels)
const float WORLD_LEFT   = -10.0f;
const float WORLD_RIGHT  =  10.0f;
const float WORLD_BOTTOM =  -7.5f;
const float WORLD_TOP    =   7.5f;

const float WORLD_WIDTH  = WORLD_RIGHT - WORLD_LEFT;
const float WORLD_HEIGHT = WORLD_TOP - WORLD_BOTTOM;

const int GRID_WIDTH  = static_cast<int>(WORLD_WIDTH  / CELL_SIZE);
const int GRID_HEIGHT = static_cast<int>(WORLD_HEIGHT / CELL_SIZE);

const float CELL_SIZE_X = WORLD_WIDTH  / GRID_WIDTH;
const float CELL_SIZE_Y = WORLD_HEIGHT / GRID_HEIGHT;

const float GRAVITY = 9.81f;
