#pragma once

#include <glm/glm.hpp>
#include "constants.h"

class Particle {

public:
    glm::vec2 position;
    glm::vec2 previous_position;
    glm::vec2 acceleration;
    glm::vec3 color;
    float radius;
    float mass;
    int gridX;
    int gridY;
    int id;
    Particle() 
        : position(0.0f), previous_position(0.0f), acceleration(0.0f), 
          color(1.0f), radius(0.1f), mass(1.0f), gridX(0), gridY(0), id(0) {}

    void setPosition(glm::vec2 pos) {
        position = pos;
        previous_position = pos;
    }

    glm::vec2 getVelocity(float dt) const {
        return (position - previous_position) / dt;
    }

    float getSpeed(float dt) const {
        return glm::length(getVelocity(dt));
    }

    void accelerate(glm::vec2 accel) {
        acceleration += accel;
    }

    void setVelocity(glm::vec2 vel, float dt) {
        previous_position = position - vel * dt;
    }

    void addVelocity(glm::vec2 vel, float dt) {
        previous_position -= vel * dt;
    }

    void update(float dt) {
        glm::vec2 temp = position;
        position = position + (position - previous_position) + acceleration * (dt * dt);
        previous_position = temp;
        acceleration = glm::vec2(0.0f);

        // Convert world coordinates to grid coordinates correctly
        float relativeX = position.x - WORLD_LEFT;
        float relativeY = position.y - WORLD_BOTTOM;
        
        gridX = static_cast<int>(relativeX / CELL_SIZE_X);
        gridY = static_cast<int>(relativeY / CELL_SIZE_Y);
        
        // Clamp to valid range
        gridX = std::max(0, std::min(gridX, GRID_WIDTH - 1));
        gridY = std::max(0, std::min(gridY, GRID_HEIGHT - 1));
    }
};
