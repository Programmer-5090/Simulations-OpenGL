#include "solver.h"
#include <iostream>


// PhysicsSolver implementation
PhysicsSolver::PhysicsSolver() : grid(GRID_WIDTH, GRID_HEIGHT, CELL_SIZE) {}

PhysicsSolver::~PhysicsSolver() {}

Particle PhysicsSolver::createBall(glm::vec2 position) {
    Particle particle;
    particle.position = position + glm::vec2(
        randomFloat(-0.05f, 0.05f),
        randomFloat(-0.02f, 0.02f)
    );

    particle.color = glm::vec3(
        randomFloat(0.5f, 1.0f),
        randomFloat(0.5f, 1.0f),
        randomFloat(0.5f, 1.0f)
    );

    particle.radius = 0.12f;
    particle.mass = 1.0f;

    // Small initial velocity
    glm::vec2 initialVelocity(
        randomFloat(-1.5f, 1.5f),
        randomFloat(0.5f, 2.0f)
    );
    
    // Use consistent timestep for physics - should match actual frame time
    const float physics_dt = 1.0f / 120.0f; // Fixed physics timestep
    particle.previous_position = particle.position - initialVelocity * physics_dt;
    particle.acceleration = glm::vec2(0.0f);

    // Set particle ID (this will be updated when added to the particles vector)
    particle.id = static_cast<int>(particles.size());

    // Calculate initial grid position
    particle.gridX = static_cast<int>((particle.position.x - WORLD_LEFT) / CELL_SIZE);
    particle.gridY = static_cast<int>((particle.position.y - WORLD_BOTTOM) / CELL_SIZE);

    return particle;
}

void PhysicsSolver::update(float deltaTime) {
    // Use fixed timestep for stable physics simulation
    const float physics_dt = 1.0f / 120.0f; // Fixed timestep
    
    for (int i = 0; i < iterations; ++i) {
        applyGravity();
        checkCollisions();
        wallCollisions();
        updateParticles(physics_dt); // Use fixed timestep instead of frame deltaTime
        updateParticleGrid();
    }
}

void PhysicsSolver::addParticle(const Particle& particle) {
    // Create a copy and update the ID
    Particle newParticle = particle;
    newParticle.id = static_cast<int>(particles.size());
    
    // Calculate proper grid position
    newParticle.gridX = static_cast<int>((newParticle.position.x - WORLD_LEFT) / CELL_SIZE);
    newParticle.gridY = static_cast<int>((newParticle.position.y - WORLD_BOTTOM) / CELL_SIZE);
    
    // Add to particles vector first
    particles.push_back(newParticle);
    
    // Then add to grid with proper bounds checking
    int num_cells_width = static_cast<int>(WORLD_WIDTH / CELL_SIZE);
    int num_cells_height = static_cast<int>(WORLD_HEIGHT / CELL_SIZE);
    
    if (newParticle.gridX >= 0 && newParticle.gridX < num_cells_width &&
        newParticle.gridY >= 0 && newParticle.gridY < num_cells_height) {
        grid.addParticle(newParticle.gridX, newParticle.gridY, newParticle.id);
    }
}

void PhysicsSolver::updateParticleGrid() {
    grid.clear();

    // Number of cells in x and y axis
    int num_cells_width  = static_cast<int>(std::ceil(WORLD_WIDTH  / CELL_SIZE));
    int num_cells_height = static_cast<int>(std::ceil(WORLD_HEIGHT / CELL_SIZE));

    for (size_t i = 0; i < particles.size(); ++i) {
        const Particle& particle = particles[i];
        
        // Convert world position -> cell index
        int cellX = static_cast<int>((particle.position.x - WORLD_LEFT) / CELL_SIZE);
        int cellY = static_cast<int>((particle.position.y - WORLD_BOTTOM) / CELL_SIZE);

        // Update particle's grid position
        particles[i].gridX = cellX;
        particles[i].gridY = cellY;

        // Only add if inside grid bounds
        if (cellX >= 0 && cellX < num_cells_width &&
            cellY >= 0 && cellY < num_cells_height) 
        {
            grid.addParticle(cellX, cellY, static_cast<int>(i));
        }
    }
}

void PhysicsSolver::wallCollisions() {
    for (size_t i = 0; i < particles.size(); ++i) {
        Particle& p = particles[i];

        // Compute current Verlet "velocity" (displacement)
        glm::vec2 vel = p.position - p.previous_position;

        // Reflected velocity vectors for each axis
        glm::vec2 dy = glm::vec2(vel.x, -vel.y);
        glm::vec2 dx = glm::vec2(-vel.x, vel.y);

        glm::vec2 npos = p.position;

        // Left / Right
        if (p.position.x - p.radius < WORLD_LEFT) {
            npos.x = WORLD_LEFT + p.radius;
            p.position = npos;
            p.setVelocity(dx * DAMPENING, 1.0f);
        }
        else if (p.position.x + p.radius > WORLD_RIGHT) {
            npos.x = WORLD_RIGHT - p.radius;
            p.position = npos;
            p.setVelocity(dx * DAMPENING, 1.0f);
        }

        // Bottom / Top
        if (p.position.y - p.radius < WORLD_BOTTOM) {
            npos.y = WORLD_BOTTOM + p.radius;
            p.position = npos;
            p.setVelocity(dy * DAMPENING, 1.0f);
        }
        else if (p.position.y + p.radius > WORLD_TOP) {
            npos.y = WORLD_TOP - p.radius;
            p.position = npos;
            p.setVelocity(dy * DAMPENING, 1.0f);
        }
    }
}

bool PhysicsSolver::collision(const Particle& a, const Particle& b) {
    glm::vec2 delta = a.position - b.position;
    float distSq = glm::dot(delta, delta); // Use dot product for squared distance
    float sumRadii = a.radius + b.radius;
    return distSq < (sumRadii * sumRadii); // Compare squared distances to avoid sqrt
}

void PhysicsSolver::resolveCollision(Particle& a, Particle& b) {
    glm::vec2 delta = b.position - a.position;
    float distSq = glm::dot(delta, delta);
    float sumRadii = a.radius + b.radius;

    // Check if they are actually overlapping to avoid division by zero
    if (distSq >= sumRadii * sumRadii || distSq == 0.0f) {
        return;
    }

    float dist = std::sqrt(distSq); // Now get the actual distance
    float overlap = sumRadii - dist;

    // A normalized vector pointing from a to b
    glm::vec2 collision_axis = delta / dist;

    // Calculate displacement based on mass ratio
    float totalMass = a.mass + b.mass;
    float displacement_a = (overlap * (b.mass / totalMass));
    float displacement_b = (overlap * (a.mass / totalMass));

    // Move particles apart
    a.position -= collision_axis * displacement_a;
    b.position += collision_axis * displacement_b;
}

// bool PhysicsSolver::collision(const Particle& a, const Particle& b) {
//         glm::vec2 delta = a.position - b.position;
//         float distSq = glm::dot(delta, delta);
//         float minDist = CELL_SIZE;
//         return distSq < (minDist * minDist);
//     }

// void PhysicsSolver::resolveCollision(Particle& a, Particle& b) {
//     glm::vec2 delta = a.position - b.position;
//     float dist = glm::length2(delta);
//     float overlap = 0.25f * (CELL_SIZE - dist);
//     glm::vec2 na = delta / dist * overlap;
//     a.position += overlap * na;
//     b.position -= overlap * na;
// }

void PhysicsSolver::collideCells(int x1, int y1, int x2, int y2) {
    for (int id_1 : grid.getCell(x1, y1).objects) {
        Particle& p1 = this->particles[id_1];
        for (int id_2 : grid.getCell(x2, y2).objects) {
            Particle& p2 = this->particles[id_2];
            if (collision(p1, p2)) {
                resolveCollision(p1, p2);
            }
        }
    }
}

void PhysicsSolver::checkCollisions() {
    int num_cells_width = static_cast<int>(WORLD_WIDTH / CELL_SIZE);
    if (num_cells_width <= 0) return;

    // Use the Threader.parallel over columns
    threader.parallel(num_cells_width, [this](int start, int end){
        checkCollisionsInSlice(start, end);
    });
}

void PhysicsSolver::checkCollisionsInSlice(int lcol, int rcol) {
    int num_cells_width = static_cast<int>(WORLD_WIDTH / CELL_SIZE);
    int num_cells_height = static_cast<int>(WORLD_HEIGHT / CELL_SIZE);
    
    int dx[] = {1, 1, 0, 0, -1};
    int dy[] = {0, 1, 0, 1, 1};
    
    for (int i = lcol; i < rcol; i++) {
        for (int j = 0; j < num_cells_height; j++) {
            if (grid.getCell(i, j).objects.empty()) continue;
            
            for (int k = 0; k < 5; k++) {
                int nx = i + dx[k];
                int ny = j + dy[k];
                
                if (nx < 0 || ny < 0 || nx >= num_cells_width || ny >= num_cells_height) continue;
                
                collideCells(i, j, nx, ny);
            }   
        }
    }
}

void PhysicsSolver::updateParticles(float dt) {
    for (Particle& p : particles) {
        int cellx = p.gridX;
        int celly = p.gridY;
        p.update(dt);
        p.gridX = static_cast<int>((p.position.x - WORLD_LEFT) / CELL_SIZE);
        p.gridY = static_cast<int>((p.position.y - WORLD_BOTTOM) / CELL_SIZE);
        glm::vec2 vel = p.position - p.previous_position;
        if (vel.x * vel.x + vel.y * vel.y > 2 * CELL_SIZE) {
            p.setVelocity({0, 0}, 1.0f);
        }
    }
}

void PhysicsSolver::applyGravity() {
    for (auto& p : particles){
        p.accelerate(glm::vec2(0, -GRAVITY));
    }
}

const std::vector<Particle>& PhysicsSolver::getParticles() const {
    return particles;
}

size_t PhysicsSolver::getParticleCount() const {
    return particles.size();
}

void PhysicsSolver::clearParticles() {
    particles.clear();
}
