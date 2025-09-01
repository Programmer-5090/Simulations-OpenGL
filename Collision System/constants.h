// In constants.h
#pragma once

// Use a float and a realistic value
const float MAX_PARTICLE_RADIUS = 0.12f;

// Cell size should be based on the largest possible particle diameter
const float CELL_SIZE = MAX_PARTICLE_RADIUS * 2.0f; // Now ~0.24f

// Screen size (pixels)
const unsigned int SCR_WIDTH  = 800;
const unsigned int SCR_HEIGHT = 600;

// World boundaries (simulation space, not pixels)
const float WORLD_LEFT   = -10.0f;
const float WORLD_RIGHT  =  10.0f;
const float WORLD_BOTTOM =  -7.5f;
const float WORLD_TOP    =   7.5f;

const float WORLD_WIDTH  = WORLD_RIGHT - WORLD_LEFT;   // 20.0
const float WORLD_HEIGHT = WORLD_TOP - WORLD_BOTTOM;   // 15.0

// Grid resolution (number of cells along each axis, in world space)
const int GRID_WIDTH  = static_cast<int>(WORLD_WIDTH  / CELL_SIZE);
const int GRID_HEIGHT = static_cast<int>(WORLD_HEIGHT / CELL_SIZE);

// Optional: cell size in world units (if you want exact spacing)
const float CELL_SIZE_X = WORLD_WIDTH  / GRID_WIDTH;
const float CELL_SIZE_Y = WORLD_HEIGHT / GRID_HEIGHT;

const float GRAVITY = 9.81f;
