#pragma once

#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>
#include <vector>
#include <cmath>
#include <thread>
#include <functional>
#include <queue>
#include <mutex>
#include <condition_variable>
#include "constants.h"
#include "particle.h"
#include "grid.h"
#include "utils.h"
// Threading utilities
#include "thread.hpp"


class PhysicsSolver {
private:
    CollisionGrid grid;
    std::vector<Particle> particles;
    Threader threader;
    int iterations = 8; // substeps per frame
    float UPS = 1.0f / 480.0f;
    float DAMPENING = 0.9f;
    mutable float lastPhysicsTime = 0.0f; // Time spent in last update call
    
    // Object pooling for better memory management
    std::vector<Particle> particlePool;
    std::vector<bool> activeParticles;
    size_t activeCount = 0;

public:
    PhysicsSolver();
    ~PhysicsSolver();

    Particle createBall(glm::vec2 position, float dt);
    Particle createBallWithVelocity(glm::vec2 position, glm::vec2 velocity, float dt);
    void update(float deltaTime);
    float getLastPhysicsTime() const { return lastPhysicsTime; } // Get time spent in last update
    void addParticle(const Particle& particle);
    void compactParticleArray(); // Remove gaps in particle array
    void updateParticleGrid();
    void wallCollisions();
    bool collision(const Particle& a, const Particle& b);
    void resolveCollision(Particle& a, Particle& b);
    void collideCells(int x1, int y1, int x2, int y2);
    void checkCollisions();
    void checkCollisionsInSlice(int leftCol, int rightCol, int& collisionChecks);
    void updateParticles(float dt);
    void applyGravity();
    
    // Getter methods
    const std::vector<Particle>& getParticles() const;
    size_t getParticleCount() const;
    void clearParticles();
};