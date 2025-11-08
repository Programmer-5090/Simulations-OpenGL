#ifndef NEW_SOLVER_H
#define NEW_SOLVER_H

#include "thread_pool.h"
#include "constants.h"
#include "particle.h"
#include "grid.h"
#include <glm/glm.hpp>
#include <vector>

using Cell = CollisionCell;

class Nsolver {
public:
    Nsolver();
    ~Nsolver();
    

    void update(float dt);
    void updateParticleGrid();
    void updateParticles(float dt);

    void solveCollision(int i, int j);
    void checkPCellCollisions(int idx, Cell &cell);
    void solveCellCollisions(Cell &cell, int idx);
    void solveCollisions();
    void checkCellCollisions(uint32_t cellIndex, uint32_t neighborIndex);
    void processCellRange(uint32_t start, uint32_t end);

    Particle createParticle(glm::vec2 position, glm::vec2 velocity, float r, float dt, bool noColor);
    void addParticle(const Particle& particle);
    void clearParticles();

    float getLastPhysicsTime() const { return lastPhysicsTime; }
    std::vector<Particle>& getParticles() { return particles; }
    size_t getParticleCount() const { return particles.size(); }


private:
    CollisionGrid grid;
    std::vector<Particle> particles;
    TPThreadPool threadPool;
    int iterations = 8;
    float DAMPENING = 0.9f;
    mutable float lastPhysicsTime = 0.0f;
};
#endif // NEW_SOLVER_H

inline void partitionThreads(int count, TPThreadPool &threadPool, std::function<void(int, int)> work);