#include "solver.h"
#include <iostream>
#include <algorithm> // For std::max/min
#include <chrono>    // For timing physics updates

// Performance timing variables
static int frameCounter = 0;
static const int TIMING_INTERVAL = 300; // Print timing every 300 frames (5 seconds at 60fps)
static float totalGravityTime = 0.0f;
static float totalCollisionTime = 0.0f;
static float totalWallCollisionTime = 0.0f;
static float totalUpdateTime = 0.0f;
static float totalGridUpdateTime = 0.0f;
static float totalGridClearTime = 0.0f;
static float totalGridInsertTime = 0.0f;
static int totalCollisionChecks = 0;
static int totalActiveCells = 0;

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

    particle.radius = 0.06f;
    particle.acceleration = glm::vec2(0.0f);
    particle.id = static_cast<int>(particles.size());

    // Create a velocity vector:
    // X component has a random horizontal spread.
    // Y component is negative for downward motion.
    glm::vec2 initialVelocity(
        randomFloat(-1.5f, 1.5f),
        randomFloat(0.5f, 2.0f)
    );

    particle.setVelocity(initialVelocity, dt);
    // std::cout << "sub_dt: " << sub_dt << std::endl;
    particle.acceleration = glm::vec2(0.0f);

    return particle;
}

void PhysicsSolver::update(float deltaTime) {
    // Use substeps for stability, like the reference
    const float sub_dt = deltaTime / static_cast<float>(iterations);

    auto start = std::chrono::high_resolution_clock::now();
    
    // Periodic cleanup for better performance
    frameCounter++;
    if (frameCounter % 300 == 0) { // Every 5 seconds at 60fps
        compactParticleArray();
    }
    
    for (int i = 0; i < iterations; ++i) {
        auto gravityStart = std::chrono::high_resolution_clock::now();
        applyGravity();
        auto gravityEnd = std::chrono::high_resolution_clock::now();
        totalGravityTime += std::chrono::duration<float, std::milli>(gravityEnd - gravityStart).count();
        
        auto collisionStart = std::chrono::high_resolution_clock::now();
        checkCollisions();
        auto collisionEnd = std::chrono::high_resolution_clock::now();
        totalCollisionTime += std::chrono::duration<float, std::milli>(collisionEnd - collisionStart).count();
        
        auto wallStart = std::chrono::high_resolution_clock::now();
        wallCollisions();
        auto wallEnd = std::chrono::high_resolution_clock::now();
        totalWallCollisionTime += std::chrono::duration<float, std::milli>(wallEnd - wallStart).count();
        
        auto updateStart = std::chrono::high_resolution_clock::now();
        updateParticles(sub_dt);
        auto updateEnd = std::chrono::high_resolution_clock::now();
        totalUpdateTime += std::chrono::duration<float, std::milli>(updateEnd - updateStart).count();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    lastPhysicsTime = std::chrono::duration<float, std::milli>(end - start).count();
    
    // Print detailed timing every TIMING_INTERVAL frames
    if (frameCounter % TIMING_INTERVAL == 0) {
        float avgGravity = totalGravityTime / TIMING_INTERVAL / iterations;
        float avgCollision = totalCollisionTime / TIMING_INTERVAL / iterations;
        float avgWall = totalWallCollisionTime / TIMING_INTERVAL / iterations;
        float avgUpdate = totalUpdateTime / TIMING_INTERVAL / iterations;
        float avgGrid = totalGridUpdateTime / TIMING_INTERVAL / iterations;
        float avgGridClear = totalGridClearTime / TIMING_INTERVAL / iterations;
        float avgGridInsert = totalGridInsertTime / TIMING_INTERVAL / iterations;
        float avgTotal = lastPhysicsTime;
        int avgCollisionChecks = totalCollisionChecks / TIMING_INTERVAL / iterations;
        int avgActiveCells = totalActiveCells / TIMING_INTERVAL / iterations;
        
        std::cout << "=== Performance Timing (Particles: " << particles.size() << ") ===" << std::endl;
        std::cout << "Threading: " << std::thread::hardware_concurrency() << " cores detected" << std::endl;
        std::cout << "Average per substep (ms):" << std::endl;
        std::cout << "  Gravity:        " << avgGravity << std::endl;
        std::cout << "  Grid Update:    " << avgGrid << " (clear: " << avgGridClear << ", insert: " << avgGridInsert << ")" << std::endl;
        std::cout << "  Collisions:     " << avgCollision << " (" << avgCollisionChecks << " checks, " << avgActiveCells << " active cells)" << std::endl;
        std::cout << "  Wall Check:     " << avgWall << std::endl;
        std::cout << "  Update:         " << avgUpdate << std::endl;
        std::cout << "Total frame:      " << avgTotal << " ms" << std::endl;
        std::cout << "FPS estimate:     " << (1000.0f / avgTotal) << std::endl;
        
        // Calculate efficiency metrics
        float collisionWorkPerParticle = (avgCollision / particles.size()) * 1000.0f; // microseconds per particle
        float gridWorkPerParticle = (avgGrid / particles.size()) * 1000.0f;
        std::cout << "Efficiency (μs per particle): Grid=" << gridWorkPerParticle << ", Collisions=" << collisionWorkPerParticle << std::endl;
        std::cout << "=========================================" << std::endl;
        
        // Reset counters
        totalGravityTime = 0.0f;
        totalCollisionTime = 0.0f;
        totalWallCollisionTime = 0.0f;
        totalUpdateTime = 0.0f;
        totalGridUpdateTime = 0.0f;
        totalGridClearTime = 0.0f;
        totalGridInsertTime = 0.0f;
        totalCollisionChecks = 0;
        totalActiveCells = 0;
    }
}

void PhysicsSolver::addParticle(const Particle& particle) {
    Particle newParticle = particle;
    newParticle.id = static_cast<int>(particles.size());
    particles.push_back(newParticle);
}

void PhysicsSolver::updateParticleGrid() {
    auto gridStart = std::chrono::high_resolution_clock::now();
    
    // Clear the grid (just clear vectors, don't deallocate)
    auto clearStart = std::chrono::high_resolution_clock::now();
    grid.clear();
    auto clearEnd = std::chrono::high_resolution_clock::now();
    totalGridClearTime += std::chrono::duration<float, std::milli>(clearEnd - clearStart).count();
    
    // Grid positions are now updated during updateParticles() for better cache locality
    // Just need to insert particles into grid cells
    auto insertStart = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < particles.size(); ++i) {
        const Particle& p = particles[i];
        if (p.gridX >= 0 && p.gridX < GRID_WIDTH &&
            p.gridY >= 0 && p.gridY < GRID_HEIGHT) {
            grid.addParticle(p.gridX, p.gridY, static_cast<uint32_t>(i));
        }
    }
    auto insertEnd = std::chrono::high_resolution_clock::now();
    totalGridInsertTime += std::chrono::duration<float, std::milli>(insertEnd - insertStart).count();
    
    auto gridEnd = std::chrono::high_resolution_clock::now();
    totalGridUpdateTime += std::chrono::duration<float, std::milli>(gridEnd - gridStart).count();
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

// Helper method for collision detection within cell slices
void PhysicsSolver::checkCollisionsInSlice(int leftCol, int rightCol, int& collisionChecks) {
    // Direction vectors for neighbor checking (avoids double-checking pairs)
    int dx[] = {1, 1, 0, 0, -1};
    int dy[] = {0, 1, 0, 1, 1};
    
    for (int x = leftCol; x < rightCol; x++) {
        for (int y = 0; y < GRID_HEIGHT; y++) {
            const auto& cell_particles = grid.getCell(x, y).objects;
            if (cell_particles.empty()) continue;
            
            totalActiveCells++;
            
            // Check within the same cell
            for (size_t j = 0; j < cell_particles.size(); ++j) {
                for (size_t k = j + 1; k < cell_particles.size(); ++k) {
                    resolveCollision(particles[cell_particles[j]], particles[cell_particles[k]]);
                    collisionChecks++;
                }
            }
            
            // Check with neighboring cells (using optimized neighbor pattern)
            for (int i = 0; i < 5; i++) {
                int nx = x + dx[i];
                int ny = y + dy[i];
                
                if (nx >= 0 && ny >= 0 && nx < GRID_WIDTH && ny < GRID_HEIGHT) {
                    const auto& neighbor_particles = grid.getCell(nx, ny).objects;
                    
                    for (uint32_t p_idx : cell_particles) {
                        for (uint32_t n_idx : neighbor_particles) {
                            resolveCollision(particles[p_idx], particles[n_idx]);
                            collisionChecks++;
                        }
                    }
                }
            }
        }
    }
}

void PhysicsSolver::checkCollisions() {
    updateParticleGrid(); // Update grid before checking collisions
    
    // Track collision statistics
    int collisionChecks = 0;
    
    // Two-pass collision detection for better parallelization and reduced redundancy
    int slice_count = threader.num_threads * 2;
    int slice_size = GRID_WIDTH / slice_count;
    
    // LEFT PASS - Process even-indexed slices
    for (int i = 0; i < threader.num_threads; i++) {
        int start = 2 * i * slice_size;
        int end = std::min(start + slice_size, GRID_WIDTH);
        if (start >= GRID_WIDTH) break;
        
        threader.t_queue.addTask([this, start, end, &collisionChecks]() {
            int localChecks = 0;
            checkCollisionsInSlice(start, end, localChecks);
            collisionChecks += localChecks; // Note: Not thread-safe but gives rough estimate
        });
    }
    
    // Handle remaining columns if slice_count * slice_size < GRID_WIDTH
    if (slice_count * slice_size < GRID_WIDTH) {
        threader.t_queue.addTask([this, slice_count, slice_size, &collisionChecks]() {
            int localChecks = 0;
            checkCollisionsInSlice(slice_count * slice_size, GRID_WIDTH, localChecks);
            collisionChecks += localChecks;
        });
    }
    
    threader.t_queue.waitUntilDone();
    
    // RIGHT PASS - Process odd-indexed slices
    for (int i = 0; i < threader.num_threads; i++) {
        int start = (2 * i + 1) * slice_size;
        int end = std::min(start + slice_size, GRID_WIDTH);
        if (start >= GRID_WIDTH) break;
        
        threader.t_queue.addTask([this, start, end, &collisionChecks]() {
            int localChecks = 0;
            checkCollisionsInSlice(start, end, localChecks);
            collisionChecks += localChecks;
        });
    }
    
    threader.t_queue.waitUntilDone();
    
    totalCollisionChecks += collisionChecks;
}

void PhysicsSolver::updateParticles(float dt) {
    // Parallel update of particles (Verlet integration + velocity cap + grid position update)
    int count = (int)particles.size();
    const float max_vel = CELL_SIZE * 0.8f; // Pre-calculate max velocity
    const float max_vel_sq = max_vel * max_vel;
    
    auto work = [this, dt, max_vel_sq, max_vel](int start, int end){
        for (int i = start; i < end; ++i) {
            Particle &p = particles[i];
            p.update(dt);
            
            // Update grid position during particle update for better cache performance
            float relativeX = p.position.x - WORLD_LEFT;
            float relativeY = p.position.y - WORLD_BOTTOM;
            p.gridX = static_cast<int>(relativeX / CELL_SIZE);
            p.gridY = static_cast<int>(relativeY / CELL_SIZE);
            
            // Optimized velocity capping with early exit
            glm::vec2 vel = p.position - p.previous_position;
            float vel_sq = glm::dot(vel, vel);
            if (vel_sq > max_vel_sq) {
                float inv_vel_mag = max_vel / sqrtf(vel_sq);
                p.previous_position = p.position - vel * inv_vel_mag;
                
                // Velocity capping can reset to zero if too fast
                if (vel_sq > 4.0f * max_vel_sq) { // If velocity is extremely high
                    p.previous_position = p.position; // Stop the particle
                }
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
    particle.radius = 0.06f;
    particle.acceleration = glm::vec2(0.0f);
    particle.id = static_cast<int>(particles.size());
    const float sub_dt = dt/iterations;
    particle.setVelocity(velocity, sub_dt);
    return particle;
}