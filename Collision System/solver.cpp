#include "solver.h"
#include <iostream>
#include <algorithm> // For std::max/min
#include <chrono>    // For timing physics updates

// PhysicsSolver implementation
PhysicsSolver::PhysicsSolver() : grid(GRID_WIDTH, GRID_HEIGHT, CELL_SIZE) {}

PhysicsSolver::~PhysicsSolver() {}

Particle PhysicsSolver::createBall(glm::vec2 position, float dt) {
    Particle particle;
    
    // Position still has a slight random offset
    particle.position = position + glm::vec2(
        randomFloat(-0.01f, 0.01f),
        randomFloat(-0.01f, 0.01f)
    );

    particle.color = glm::vec3(
        randomFloat(0.5f, 1.0f),
        randomFloat(0.5f, 1.0f),
        randomFloat(0.5f, 1.0f)
    );

    particle.radius = 0.12f;
    particle.acceleration = glm::vec2(0.0f);
    particle.id = static_cast<int>(particles.size());

    // FIXED: Give the particle a random downward initial velocity
    // The fixed physics timestep from your main loop
    const float sub_dt = dt/iterations;

    // Create a velocity vector:
    // X component has a random horizontal spread.
    // Y component is negative for downward motion.
    glm::vec2 initialVelocity(
        randomFloat(-1.5f, 1.5f),
        randomFloat(0.5f, 2.0f)
    );

    particle.setVelocity(initialVelocity, sub_dt);
    // std::cout << "sub_dt: " << sub_dt << std::endl;
    particle.acceleration = glm::vec2(0.0f);

    return particle;
}

void PhysicsSolver::update(float deltaTime) {
    // Use substeps for stability, like the reference
    const float sub_dt = deltaTime / static_cast<float>(iterations);

    auto start = std::chrono::high_resolution_clock::now();
    
    // Periodic cleanup for better performance
    static int frameCounter = 0;
    if (++frameCounter % 300 == 0) { // Every 5 seconds at 60fps
        removeOffscreenParticles();
        compactParticleArray();
    }
    
    for (int i = 0; i < iterations; ++i) {
        applyGravity();
        checkCollisions();
        wallCollisions();
        updateParticles(sub_dt);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    lastPhysicsTime = std::chrono::duration<float, std::milli>(end - start).count();
}

void PhysicsSolver::addParticle(const Particle& particle) {
    Particle newParticle = particle;
    newParticle.id = static_cast<int>(particles.size());
    particles.push_back(newParticle);
}

void PhysicsSolver::updateParticleGrid() {
    grid.clear();
    
    // Parallel grid update for better performance
    int count = (int)particles.size();
    auto work = [this](int start, int end) {
        for (int i = start; i < end; ++i) {
            Particle& p = particles[i];
            // Compute grid position inline for better cache performance
            float relativeX = p.position.x - WORLD_LEFT;
            float relativeY = p.position.y - WORLD_BOTTOM;
            p.gridX = static_cast<int>(relativeX / CELL_SIZE);
            p.gridY = static_cast<int>(relativeY / CELL_SIZE);
        }
    };
    threader.parallel(count, std::move(work));
    
    // Sequential grid insertion (can't parallelize due to race conditions)
    for (size_t i = 0; i < particles.size(); ++i) {
        const Particle& p = particles[i];
        if (p.gridX >= 0 && p.gridX < GRID_WIDTH &&
            p.gridY >= 0 && p.gridY < GRID_HEIGHT) {
            grid.addParticle(p.gridX, p.gridY, static_cast<int>(i));
        }
    }
}

void PhysicsSolver::wallCollisions() {
    // Parallel wall collision checks
    int count = (int)particles.size();
    auto work = [this](int start, int end) {
        const float restitution = 0.8f; // Dampening factor
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
    threader.parallel(count, std::move(work));
}

// Optimized collision resolution with early exit and reduced overlap factor
void PhysicsSolver::resolveCollision(Particle& a, Particle& b) {
    glm::vec2 delta = a.position - b.position;
    float distSq = glm::dot(delta, delta);
    float min_dist = a.radius + b.radius;
    float min_dist_sq = min_dist * min_dist;

    if (distSq < min_dist_sq && distSq > 1e-9f) {
        float dist = sqrtf(distSq); // Use sqrtf for better performance
        float inv_dist = 1.0f / dist;
        glm::vec2 normal = delta * inv_dist;
        float overlap = 0.2f * (min_dist - dist); // Reduced overlap factor for stability
        
        a.position += normal * overlap;
        b.position -= normal * overlap;
    }
}

void PhysicsSolver::checkCollisions() {
    updateParticleGrid(); // Update grid before checking collisions

    // Optimized: Skip empty cells early and use better work distribution
    std::vector<int> nonEmptyCells;
    nonEmptyCells.reserve(GRID_WIDTH * GRID_HEIGHT / 4); // Estimate
    
    for (int cellIndex = 0; cellIndex < GRID_WIDTH * GRID_HEIGHT; ++cellIndex) {
        int x = cellIndex % GRID_WIDTH;
        int y = cellIndex / GRID_WIDTH;
        if (grid.getCell(x, y).objects.size() > 0) {
            nonEmptyCells.push_back(cellIndex);
        }
    }
    
    // Only process non-empty cells in parallel
    int cellCount = (int)nonEmptyCells.size();
    auto work = [this, &nonEmptyCells](int start, int end) {
        for (int i = start; i < end; ++i) {
            int cellIndex = nonEmptyCells[i];
            int x = cellIndex % GRID_WIDTH;
            int y = cellIndex / GRID_WIDTH;
            
            const auto& cell_particles_indices = grid.getCell(x, y).objects;
            // Check within the same cell
            for (size_t j = 0; j < cell_particles_indices.size(); ++j) {
                for (size_t k = j + 1; k < cell_particles_indices.size(); ++k) {
                    resolveCollision(particles[cell_particles_indices[j]], particles[cell_particles_indices[k]]);
                }
            }

            // Check with neighboring cells (avoids double-checking)
            int neighbors[4][2] = {{1, 0}, {0, 1}, {1, 1}, {-1, 1}};
            for (auto& offset : neighbors) {
                int nx = x + offset[0];
                int ny = y + offset[1];
                if (nx >= 0 && nx < GRID_WIDTH && ny >= 0 && ny < GRID_HEIGHT) {
                    const auto& neighbor_cell_indices = grid.getCell(nx, ny).objects;
                    for (int p_idx : cell_particles_indices) {
                        for (int n_idx : neighbor_cell_indices) {
                            resolveCollision(particles[p_idx], particles[n_idx]);
                        }
                    }
                }
            }
        }
    };
    threader.parallel(cellCount, std::move(work));
}

void PhysicsSolver::updateParticles(float dt) {
    // Parallel update of particles (Verlet integration + velocity cap)
    int count = (int)particles.size();
    const float max_vel = CELL_SIZE * 0.8f; // Pre-calculate max velocity
    const float max_vel_sq = max_vel * max_vel;
    
    auto work = [this, dt, max_vel_sq, max_vel](int start, int end){
        for (int i = start; i < end; ++i) {
            Particle &p = particles[i];
            p.update(dt);
            
            // Optimized velocity capping with early exit
            glm::vec2 vel = p.position - p.previous_position;
            float vel_sq = glm::dot(vel, vel);
            if (vel_sq > max_vel_sq) {
                float inv_vel_mag = max_vel / sqrtf(vel_sq);
                p.previous_position = p.position - vel * inv_vel_mag;
            }
        }
    };
    threader.parallel(count, std::move(work));
}

void PhysicsSolver::applyGravity() {
    glm::vec2 g(0.0f, -GRAVITY);
    int count = (int)particles.size();
    auto work = [this, g](int start, int end){
        for (int i = start; i < end; ++i) {
            particles[i].accelerate(g);
        }
    };
    threader.parallel(count, std::move(work));
}

const std::vector<Particle>& PhysicsSolver::getParticles() const {
    return particles;
}

size_t PhysicsSolver::getParticleCount() const {
    return particles.size();
}

void PhysicsSolver::clearParticles() {
    particles.clear();
    grid.clear();
}

// Memory and performance optimizations
void PhysicsSolver::removeOffscreenParticles() {
    const float margin = 2.0f; // Remove particles well outside world bounds
    particles.erase(
        std::remove_if(particles.begin(), particles.end(),
            [margin](const Particle& p) {
                return p.position.x < WORLD_LEFT - margin || p.position.x > WORLD_RIGHT + margin ||
                       p.position.y < WORLD_BOTTOM - margin || p.position.y > WORLD_TOP + margin;
            }),
        particles.end()
    );
}

void PhysicsSolver::compactParticleArray() {
    // Remove dead particles and compact array for better cache performance
    if (particles.size() > 1000 && particles.capacity() > particles.size() * 2) {
        std::vector<Particle> compacted;
        compacted.reserve(particles.size());
        for (const auto& p : particles) {
            compacted.push_back(p);
        }
        particles = std::move(compacted);
    }
}

Particle PhysicsSolver::createBallWithVelocity(glm::vec2 position, glm::vec2 velocity, float dt) {
    Particle particle;
    particle.position = position;
    particle.color = glm::vec3(
        randomFloat(0.5f, 1.0f),
        randomFloat(0.5f, 1.0f),
        randomFloat(0.5f, 1.0f)
    );
    particle.radius = 0.12f;
    particle.acceleration = glm::vec2(0.0f);
    particle.id = static_cast<int>(particles.size());
    const float sub_dt = dt/iterations;
    particle.setVelocity(velocity, sub_dt);
    return particle;
}