#include "Nsolver.h"
#include <glm/gtx/norm.hpp>
#include <iostream>
#include <algorithm>
#include <future>
#include "utils.h"

Nsolver::Nsolver() : grid(GRID_WIDTH, GRID_HEIGHT, CELL_SIZE),
    threadPool(std::thread::hardware_concurrency()){
}

Nsolver::~Nsolver() {}

void Nsolver::update(float dt){
    float substeps = dt/iterations;
    for (int iter = 0; iter < iterations; ++iter) {
        
        updateParticles(substeps);
        updateParticleGrid();
        solveCollisions();
    }
}

void Nsolver::updateParticleGrid(){
    grid.clear();
    for (size_t i = 0; i < particles.size(); ++i) {
        Particle& p = particles[i];
        
        // Update grid coordinates based on current position
        p.gridX = static_cast<int>((p.position.x - WORLD_LEFT) / CELL_SIZE);
        p.gridY = static_cast<int>((p.position.y - WORLD_BOTTOM) / CELL_SIZE);
        
        // Clamp to grid bounds
        p.gridX = std::max(0, std::min(p.gridX, GRID_WIDTH - 1));
        p.gridY = std::max(0, std::min(p.gridY, GRID_HEIGHT - 1));
        
        grid.addParticle(p.gridX, p.gridY, static_cast<uint32_t>(i));
    }
}

void Nsolver::updateParticles(float dt){
    // Thread the particle updates
    auto work = [this, dt](int start, int end){
        glm::vec2 g(0.0f, -GRAVITY);
        for (int i = start; i < end; ++i) {
            particles[i].accelerate(g);
        }
        
        for (int i = start; i < end; ++i) {
            particles[i].update(dt);
        }
        
        const float restitution = 0.8f;
        for (int i = start; i < end; ++i) {
            Particle& p = particles[i];
            glm::vec2 vel = p.position - p.previous_position;

            // Left/Right walls
            if (p.position.x - p.radius < WORLD_LEFT) {
                p.position.x = WORLD_LEFT + p.radius;
                p.previous_position.x = p.position.x + vel.x * restitution;
            } else if (p.position.x + p.radius > WORLD_RIGHT) {
                p.position.x = WORLD_RIGHT - p.radius;
                p.previous_position.x = p.position.x + vel.x * restitution;
            }

            // Bottom/Top walls
            if (p.position.y - p.radius < WORLD_BOTTOM) {
                p.position.y = WORLD_BOTTOM + p.radius;
                p.previous_position.y = p.position.y + vel.y * restitution;
            } else if (p.position.y + p.radius > WORLD_TOP) {
                p.position.y = WORLD_TOP - p.radius;
                p.previous_position.y = p.position.y + vel.y * restitution;
            }
        }
    };
    partitionThreads((int)particles.size(), threadPool, work);
}


void Nsolver::solveCollision(int i, int j){
    float responseFactor = 1.0f;
    Particle &a = particles[i];
    Particle &b = particles[j];
    glm::vec2 delta = b.position - a.position;
    float distSq = glm::dot(delta, delta);
    float min_dist = a.radius + b.radius;

    if (distSq < min_dist * min_dist && distSq > 1e-9f) {
        float dist = sqrtf(distSq);
        float inv_dist = 1.0f / dist;
        glm::vec2 normal = delta * inv_dist;
        float overlap = 0.5f * (min_dist - dist) * responseFactor;

        a.position -= normal * overlap;
        b.position += normal * overlap;
    }
}

// Helper to check collisions for particles in a cell against another cell
void Nsolver::checkCellCollisions(uint32_t cellIndex, uint32_t neighborIndex) {
    if (neighborIndex >= static_cast<uint32_t>(GRID_WIDTH * GRID_HEIGHT)) return;
    
    Cell& cell = grid.cells[cellIndex];
    Cell& neighborCell = grid.cells[neighborIndex];
    
    for (size_t i = 0; i < cell.size(); ++i) {
        uint32_t idx1 = cell.objects[i];
        for (size_t j = 0; j < neighborCell.size(); ++j) {
            solveCollision(idx1, neighborCell.objects[j]);
        }
    }
}

// Process a range of cells (for a single thread slice)
void Nsolver::processCellRange(uint32_t start, uint32_t end) {
    for (uint32_t cellIdx = start; cellIdx < end; ++cellIdx) {
        int x = cellIdx % GRID_WIDTH;
        int y = cellIdx / GRID_WIDTH;
        
        Cell& cell = grid.cells[cellIdx];
        if (cell.size() == 0) continue;
        
        // Internal collisions within this cell
        for (size_t i = 0; i < cell.size(); ++i) {
            uint32_t idx1 = cell.objects[i];
            for (size_t j = i + 1; j < cell.size(); ++j) {
                solveCollision(idx1, cell.objects[j]);
            }
        }
        
        // Check 9 neighbors using helper function (safer)
        checkCellCollisions(cellIdx, cellIdx - 1);                    // Left
        checkCellCollisions(cellIdx, cellIdx + 1);                    // Right
        checkCellCollisions(cellIdx, cellIdx - GRID_WIDTH);           // Top
        checkCellCollisions(cellIdx, cellIdx + GRID_WIDTH);           // Bottom
        checkCellCollisions(cellIdx, cellIdx - GRID_WIDTH - 1);      // Top-left
        checkCellCollisions(cellIdx, cellIdx - GRID_WIDTH + 1);      // Top-right
        checkCellCollisions(cellIdx, cellIdx + GRID_WIDTH - 1);      // Bottom-left
        checkCellCollisions(cellIdx, cellIdx + GRID_WIDTH + 1);      // Bottom-right
    }
}

// THREADED collision solving with spatial slicing (TWO-PASS to avoid race conditions)
void Nsolver::solveCollisions(){
    const uint32_t thread_count = static_cast<uint32_t>(threadPool.getThreadCount());
    if (thread_count == 0) return;
    
    const uint32_t total_cells = GRID_WIDTH * GRID_HEIGHT;
    const uint32_t slice_count = thread_count * 2;
    const uint32_t slice_size = (GRID_WIDTH / slice_count) * GRID_HEIGHT;
    const uint32_t last_slice_start = (2 * (thread_count - 1) + 2) * slice_size;
    
    // FIRST PASS: Process even slices (0, 2, 4, ...)
    {
        std::vector<std::future<void>> futures;
        for (uint32_t i = 0; i < thread_count; ++i) {
            futures.push_back(threadPool.enqueue([this, i, slice_size]() {
                const uint32_t start = 2 * i * slice_size;
                const uint32_t end = start + slice_size;
                processCellRange(start, end);
            }));
        }
        
        // Handle remainder if grid not evenly divisible
        if (last_slice_start < total_cells) {
            futures.push_back(threadPool.enqueue([this, last_slice_start, total_cells]() {
                processCellRange(last_slice_start, total_cells);
            }));
        }
        
        // Wait for first pass to complete
        for (auto& f : futures) {
            f.wait();
        }
    }
    
    // SECOND PASS: Process odd slices (1, 3, 5, ...)
    {
        std::vector<std::future<void>> futures;
        for (uint32_t i = 0; i < thread_count; ++i) {
            futures.push_back(threadPool.enqueue([this, i, slice_size]() {
                const uint32_t start = (2 * i + 1) * slice_size;
                const uint32_t end = start + slice_size;
                processCellRange(start, end);
            }));
        }
        
        // Wait for second pass to complete
        for (auto& f : futures) {
            f.wait();
        }
    }
}

Particle Nsolver::createParticle(glm::vec2 position, glm::vec2 velocity, float r, float dt, bool noColor = false){
    Particle particle;
    
    particle.position = position;
    if (noColor) {
        particle.color = glm::vec3(1.0f, 1.0f, 1.0f);
        } else{
        particle.color = glm::vec3(
            randomFloat(0.5f, 1.0f),
            randomFloat(0.5f, 1.0f),
            randomFloat(0.5f, 1.0f)
        );
    }

    particle.radius = r;
    particle.acceleration = glm::vec2(0.0f);
    particle.id = static_cast<int>(particles.size());

    // Initialize grid coordinates
    particle.gridX = static_cast<int>((particle.position.x - WORLD_LEFT) / CELL_SIZE);
    particle.gridY = static_cast<int>((particle.position.y - WORLD_BOTTOM) / CELL_SIZE);
    particle.gridX = std::max(0, std::min(particle.gridX, GRID_WIDTH - 1));
    particle.gridY = std::max(0, std::min(particle.gridY, GRID_HEIGHT - 1));

    particle.setVelocity(velocity, dt);
    particle.acceleration = glm::vec2(0.0f);

    return particle;
}

void Nsolver::addParticle(const Particle& particle){
    if (std::find_if(particles.begin(), particles.end(),
        [&particle](const Particle& p) { return p.id == particle.id; }) != particles.end()) {
        return;
    }
    particles.push_back(particle);

    if (particle.gridX >= 0 && particle.gridX < GRID_WIDTH &&
        particle.gridY >= 0 && particle.gridY < GRID_HEIGHT) {
        if (!grid.cellContainsParticle(particle.gridX, particle.gridY, particle.id)){
            grid.addParticle(particle.gridX, particle.gridY, static_cast<uint32_t>(particle.id));
        }
    }
}

void Nsolver::clearParticles(){
    particles.clear();
    grid.clear();
}

inline void partitionThreads(int count, TPThreadPool &threadPool, std::function<void(int, int)> work) {
    int numThreads = static_cast<int>(threadPool.getThreadCount());
    if (numThreads <= 0) numThreads = 1;
    int slice = (numThreads > 0) ? (count / numThreads) : count;
    if (slice == 0) {
        work(0, count);
    } else {
        std::vector<std::future<void>> futures;
        futures.reserve(numThreads + 1);
        for (int t = 0; t < numThreads; ++t) {
            int start = t * slice;
            int end = (t == numThreads - 1) ? count : (start + slice);
            auto workCopy = work;
            futures.push_back(threadPool.enqueue([workCopy, start, end]() { workCopy(start, end); }));
        }
        for (auto &f : futures) f.wait();
    }
}