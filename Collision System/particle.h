#pragma once

#include <glm/glm.hpp>
#include <algorithm> // For std::max/min
#include "constants.h"

class Particle {
public:
    glm::vec2 position;
    glm::vec2 previous_position;
    glm::vec2 acceleration;
    glm::vec3 color;
    float radius;
    int gridX;
    int gridY;
    int id;

    Particle() 
        : position(0.0f), previous_position(0.0f), acceleration(0.0f), 
          color(1.0f), radius(0.1f), gridX(0), gridY(0), id(0) {}

    void accelerate(glm::vec2 accel) {
        acceleration += accel;
    }

    void setVelocity(glm::vec2 vel, float dt) {
        previous_position = position - vel * dt;
    }

    void addVelocity(glm::vec2 vel, float dt) {
        previous_position -= vel * dt;
    }

    // This function should only be called once per frame/substep
    void update(float dt) {
        // Verlet integration
        glm::vec2 velocity = position - previous_position;
        glm::vec2 temp_pos = position;
        position = position + velocity + acceleration * (dt * dt);
        previous_position = temp_pos;
        acceleration = glm::vec2(0.0f);
    }

    // Separate function to update grid position when needed
    void updateGridPosition() {
        gridX = static_cast<int>((position.x - WORLD_LEFT) / CELL_SIZE);
        gridY = static_cast<int>((position.y - WORLD_BOTTOM) / CELL_SIZE);
    }
};