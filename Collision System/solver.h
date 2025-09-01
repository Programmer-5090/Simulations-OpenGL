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

// Using Threader from thread.hpp now

class PhysicsSolver {
private:
    CollisionGrid grid;
    std::vector<Particle> particles;
    Threader threader;
    float iterations = 12;
    float UPS = 1.0f / 480.0f;
    float DAMPENING = 0.9f;

public:
    PhysicsSolver();
    ~PhysicsSolver();

    Particle createBall(glm::vec2 position);
    void update(float deltaTime);
    void addParticle(const Particle& particle);
    void updateParticleGrid();
    void wallCollisions();
    bool collision(const Particle& a, const Particle& b);
    void resolveCollision(Particle& a, Particle& b);
    void collideCells(int x1, int y1, int x2, int y2);
    void checkCollisions();
    void checkCollisionsInSlice(int lcol, int rcol);
    void updateParticles(float dt);
    void applyGravity();
    
    // Getter methods
    const std::vector<Particle>& getParticles() const;
    size_t getParticleCount() const;
    void clearParticles();
};