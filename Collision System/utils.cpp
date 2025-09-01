#include "utils.h"

float randomFloat(float min, float max) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(min, max);
    return dis(gen);
}

// Utility functions
glm::vec2 screenToWorld(double xpos, double ypos) {
    float normalizedX = static_cast<float>(xpos) / SCR_WIDTH;
    float normalizedY = static_cast<float>(ypos) / SCR_HEIGHT;
    
    float worldX = WORLD_LEFT + normalizedX * WORLD_WIDTH;
    float worldY = WORLD_TOP - normalizedY * WORLD_HEIGHT;
    
    return glm::vec2(worldX, worldY);
}
