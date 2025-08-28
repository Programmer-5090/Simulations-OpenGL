#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "shader.h"
#include "mesh.h"
#include "geometry/circle.h"
#include <iostream>
#include <random>
#include <vector>
#include <array>
#include <cstdint>
#include <algorithm>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);

// Window settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

// Physics constants
const float GRAVITY = 150.0f;
const float DAMPING = 0.999f;
const float RESTITUTION = 0.7f;

// Sleep parameters
const float SLEEP_THRESHOLD_SQ = 0.001f;
const float SLEEP_TIME = 0.1f;

// World boundaries
const float WORLD_LEFT = -10.0f;
const float WORLD_RIGHT = 10.0f;
const float WORLD_BOTTOM = -7.5f;
const float WORLD_TOP = 7.5f;
const float WORLD_WIDTH = WORLD_RIGHT - WORLD_LEFT;
const float WORLD_HEIGHT = WORLD_TOP - WORLD_BOTTOM;

// Optimized grid settings
const int32_t GRID_WIDTH = 80;
const int32_t GRID_HEIGHT = 60;
const float CELL_SIZE_X = WORLD_WIDTH / GRID_WIDTH;
const float CELL_SIZE_Y = WORLD_HEIGHT / GRID_HEIGHT;

// Mouse interaction
bool mousePressed = false;
glm::vec2 mouseWorldPos(0.0f);
float ballSpawnTimer = 0.0f;
const float BALL_SPAWN_RATE = 0.03f;

// Debug
int selectedBallIndex = -1;
float debugPrintTimer = 0.0f;
const float DEBUG_PRINT_INTERVAL = 1.0f;

// Forward declarations
struct Ball;

// Optimized collision cell with fixed capacity
struct CollisionCell {
    static constexpr uint8_t CELL_CAPACITY = 8;
    uint8_t objects_count = 0;
    uint32_t objects[CELL_CAPACITY];
    
    void addBall(uint32_t id) {
        if (objects_count < CELL_CAPACITY) {
            objects[objects_count++] = id;
        }
    }
    
    void clear() {
        objects_count = 0;
    }
};

// High-performance collision grid
struct CollisionGrid {
    int32_t width, height;
    std::vector<CollisionCell> cells;
    
    CollisionGrid(int32_t w, int32_t h) : width(w), height(h) {
        cells.resize(w * h);
    }
    
    void clear() {
        for (auto& cell : cells) {
            cell.clear();
        }
    }
    
    void addBall(int32_t x, int32_t y, uint32_t ballIndex) {
        if (x >= 0 && x < width && y >= 0 && y < height) {
            cells[y * width + x].addBall(ballIndex);
        }
    }
    
    CollisionCell& getCell(int32_t x, int32_t y) {
        return cells[y * width + x];
    }
    
    bool isValidCell(int32_t x, int32_t y) const {
        return x >= 0 && x < width && y >= 0 && y < height;
    }
};

// Ball structure with optimized physics
struct Ball {
    glm::vec2 position;
    glm::vec2 previous_position;
    glm::vec2 acceleration;
    glm::vec3 color;
    float radius;
    float mass;
    
    // State management
    bool isSleeping = false;
    float sleepTimer = 0.0f;
    
    // Grid position cache
    int32_t gridX = -1;
    int32_t gridY = -1;
    
    Ball() : position(0.0f), previous_position(0.0f), acceleration(0.0f), 
             color(1.0f), radius(0.1f), mass(1.0f) {}
    
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
    
    void stop() {
        previous_position = position;
    }
};

// Global state
std::vector<Ball> allBalls;
CollisionGrid grid(GRID_WIDTH, GRID_HEIGHT);

// Utility functions
glm::vec2 screenToWorld(double xpos, double ypos) {
    float normalizedX = static_cast<float>(xpos) / SCR_WIDTH;
    float normalizedY = static_cast<float>(ypos) / SCR_HEIGHT;
    
    float worldX = WORLD_LEFT + normalizedX * WORLD_WIDTH;
    float worldY = WORLD_TOP - normalizedY * WORLD_HEIGHT;
    
    return glm::vec2(worldX, worldY);
}

float randomFloat(float min, float max) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(min, max);
    return dis(gen);
}

Ball createBall(glm::vec2 position) {
    Ball ball;
    ball.position = position + glm::vec2(
        randomFloat(-0.05f, 0.05f),
        randomFloat(-0.02f, 0.02f)
    );
    
    ball.color = glm::vec3(
        randomFloat(0.5f, 1.0f),
        randomFloat(0.5f, 1.0f),
        randomFloat(0.5f, 1.0f)
    );
    
    ball.radius = randomFloat(0.08f, 0.12f);
    ball.mass = ball.radius * ball.radius; // Mass proportional to area
    
    // Small initial velocity
    glm::vec2 initialVelocity(
        randomFloat(-1.5f, 1.5f),
        randomFloat(0.5f, 2.0f)
    );
    
    const float dt = 1.0f / 120.0f;
    ball.previous_position = ball.position - initialVelocity * dt;
    ball.acceleration = glm::vec2(0.0f);
    
    return ball;
}

// Fast collision detection
inline bool checkCollision(const Ball& a, const Ball& b) {
    glm::vec2 delta = a.position - b.position;
    float distSq = glm::dot(delta, delta);
    float radiusSum = a.radius + b.radius;
    return distSq < (radiusSum * radiusSum);
}

// Optimized collision resolution
void resolveCollision(Ball& a, Ball& b) {
    glm::vec2 delta = a.position - b.position;
    float distSq = glm::dot(delta, delta);
    float radiusSum = a.radius + b.radius;
    float radiusSumSq = radiusSum * radiusSum;
    
    if (distSq >= radiusSumSq || distSq < 1e-6f) return;
    
    float dist = std::sqrt(distSq);
    float overlap = radiusSum - dist;
    
    // Skip micro-penetrations
    if (overlap < 0.001f) return;
    
    glm::vec2 normal = delta / dist;
    
    // Mass-based separation
    float totalMass = a.mass + b.mass;
    float aRatio = b.mass / totalMass;
    float bRatio = a.mass / totalMass;
    
    // Constrained correction
    const float correctionPercent = 0.75f;
    const float maxCorrection = 0.5f;
    
    glm::vec2 correction = normal * (overlap * correctionPercent);
    if (glm::length(correction) > maxCorrection) {
        correction = glm::normalize(correction) * maxCorrection;
    }
    
    a.position += correction * aRatio;
    b.position -= correction * bRatio;
    
    // Wake up sleeping balls on significant collision
    if (overlap > 0.01f) {
        if (a.isSleeping) {
            a.isSleeping = false;
            a.sleepTimer = 0.0f;
        }
        if (b.isSleeping) {
            b.isSleeping = false;
            b.sleepTimer = 0.0f;
        }
    }
}

// ADD THIS NEW FUNCTION
bool isTrapped(uint32_t ballIndex, const std::vector<Ball>& allBalls, const CollisionGrid& grid) {
    const Ball& ball = allBalls[ballIndex];

    // Define 8 directions to check for neighbors
    static const std::array<glm::vec2, 8> directions = {{
        {0, 1}, {0.707f, 0.707f}, {1, 0}, {0.707f, -0.707f},
        {0, -1}, {-0.707f, -0.707f}, {-1, 0}, {-0.707f, 0.707f}
    }};

    std::array<bool, 8> blockedDirections{}; // Initializes all to false
    const float checkRadius = ball.radius * 2.5f; // How far to look for blockers

    // Check the 3x3 grid of cells around the ball
    for (int32_t y = ball.gridY - 1; y <= ball.gridY + 1; ++y) {
        for (int32_t x = ball.gridX - 1; x <= ball.gridX + 1; ++x) {
            if (!grid.isValidCell(x, y)) continue;

            const CollisionCell& cell = grid.cells[y * grid.width + x];
            for (uint8_t i = 0; i < cell.objects_count; ++i) {
                uint32_t otherIndex = cell.objects[i];
                if (otherIndex == ballIndex) continue; // Skip self

                const Ball& otherBall = allBalls[otherIndex];
                glm::vec2 delta = otherBall.position - ball.position;
                
                if (glm::length2(delta) > (checkRadius * checkRadius)) continue;

                glm::vec2 dir = glm::normalize(delta);

                // Check which of the 8 directions this neighbor blocks
                for (size_t j = 0; j < directions.size(); ++j) {
                    if (glm::dot(dir, directions[j]) > 0.75f) { // 0.75 is a good threshold
                        blockedDirections[j] = true;
                    }
                }
            }
        }
    }

    // Count how many directions are blocked
    int blockedCount = 0;
    for (bool blocked : blockedDirections) {
        if (blocked) blockedCount++;
    }

    // If nearly all directions are blocked, the ball is trapped.
    return blockedCount >= 6;
}

// Wall collision handling
void handleWallCollisions(Ball& ball) {
    bool collided = false;
    
    if (ball.position.x - ball.radius < WORLD_LEFT) {
        ball.position.x = WORLD_LEFT + ball.radius;
        collided = true;
    } else if (ball.position.x + ball.radius > WORLD_RIGHT) {
        ball.position.x = WORLD_RIGHT - ball.radius;
        collided = true;
    }
    
    if (ball.position.y - ball.radius < WORLD_BOTTOM) {
        ball.position.y = WORLD_BOTTOM + ball.radius;
        collided = true;
    } else if (ball.position.y + ball.radius > WORLD_TOP) {
        ball.position.y = WORLD_TOP - ball.radius;
        collided = true;
    }
    
    if (collided) {
        if (ball.isSleeping) {
            ball.isSleeping = false;
            ball.sleepTimer = 0.0f;
        }
        
        // Apply restitution
        glm::vec2 velocity = ball.position - ball.previous_position;
        ball.previous_position = ball.position - velocity * RESTITUTION;
    }
}

// Verlet integration with improved stability
void updateBall(Ball& ball, uint32_t ballIndex, float dt) {
    if (ball.isSleeping) {
        // Check for external forces that should wake the ball
        if (glm::length(ball.acceleration) > 1.0f) {
            ball.isSleeping = false;
            ball.sleepTimer = 0.0f;
        } else {
            ball.acceleration = glm::vec2(0.0f);
            return;
        }
    }
    
    glm::vec2 velocity = ball.position - ball.previous_position;
    
    // Velocity clamping
    const float maxSpeed = 12.0f;
    float speed = glm::length(velocity);
    if (speed > maxSpeed) {
        velocity = glm::normalize(velocity) * maxSpeed;
    }
    
    // Apply damping
    velocity *= DAMPING;
    
    // Verlet integration
    glm::vec2 currentPos = ball.position;
    ball.position = currentPos + velocity + ball.acceleration * (dt * dt);
    ball.previous_position = currentPos;
    
    ball.acceleration = glm::vec2(0.0f);
    
    // Sleep detection
    float speedSq = glm::length2(velocity) / (dt * dt);
    if (speedSq < SLEEP_THRESHOLD_SQ) {
        ball.sleepTimer += dt;
        if (ball.sleepTimer >= SLEEP_TIME) {
            ball.isSleeping = true;
            ball.previous_position = ball.position;
        }
        // NEW: If moving slowly, check if it's physically trapped
        else if (ball.sleepTimer > (SLEEP_TIME / 2.0f) && isTrapped(ballIndex, allBalls, grid)) {
            ball.isSleeping = true;
            // ball.previous_position = ball.position; // Force it to stop
        }
    } else {
        ball.sleepTimer = 0.0f;
    }
}

// Apply forces
void applyForces(Ball& ball) {
    if (!ball.isSleeping) {
        // Check if ball is resting on ground
        bool isGrounded = (ball.position.y - ball.radius <= WORLD_BOTTOM + 0.01f);
        if (!isGrounded) {
            ball.acceleration.y -= GRAVITY;
        }
    }
}

// Populate collision grid
void populateGrid() {
    grid.clear();
    
    for (size_t i = 0; i < allBalls.size(); ++i) {
        Ball& ball = allBalls[i];
        
        // Convert world to grid coordinates
        float relativeX = ball.position.x - WORLD_LEFT;
        float relativeY = ball.position.y - WORLD_BOTTOM;
        
        int32_t gridX = static_cast<int32_t>(relativeX / CELL_SIZE_X);
        int32_t gridY = static_cast<int32_t>(relativeY / CELL_SIZE_Y);
        
        // Clamp to valid range
        gridX = std::max(0, std::min(gridX, GRID_WIDTH - 1));
        gridY = std::max(0, std::min(gridY, GRID_HEIGHT - 1));
        
        ball.gridX = gridX;
        ball.gridY = gridY;
        
        grid.addBall(gridX, gridY, static_cast<uint32_t>(i));
    }
}

// Process collisions for a single cell
void processCollisionCell(int32_t cellX, int32_t cellY) {
    CollisionCell& currentCell = grid.getCell(cellX, cellY);
    
    // Internal collisions
    for (uint8_t i = 0; i < currentCell.objects_count; ++i) {
        for (uint8_t j = i + 1; j < currentCell.objects_count; ++j) {
            Ball& ballA = allBalls[currentCell.objects[i]];
            Ball& ballB = allBalls[currentCell.objects[j]];
            
            if (checkCollision(ballA, ballB)) {
                resolveCollision(ballA, ballB);
            }
        }
    }
    
    // Check adjacent cells (only forward directions to avoid double-checking)
    static const std::array<std::pair<int, int>, 4> neighbors = {{
        {0, 1},   // Up
        {1, 0},   // Right
        {1, 1},   // Up-Right
        {1, -1}   // Down-Right
    }};
    
    for (const auto& [dx, dy] : neighbors) {
        int32_t neighborX = cellX + dx;
        int32_t neighborY = cellY + dy;
        
        if (grid.isValidCell(neighborX, neighborY)) {
            CollisionCell& neighborCell = grid.getCell(neighborX, neighborY);
            
            for (uint8_t i = 0; i < currentCell.objects_count; ++i) {
                for (uint8_t j = 0; j < neighborCell.objects_count; ++j) {
                    Ball& ballA = allBalls[currentCell.objects[i]];
                    Ball& ballB = allBalls[neighborCell.objects[j]];
                    
                    if (checkCollision(ballA, ballB)) {
                        resolveCollision(ballA, ballB);
                    }
                }
            }
        }
    }
}

// Multi-iteration collision solver
void handleCollisions() {
    const int SOLVER_ITERATIONS = 16;
    
    for (int iter = 0; iter < SOLVER_ITERATIONS; ++iter) {
        // Process all cells
        for (int32_t y = 0; y < GRID_HEIGHT; ++y) {
            for (int32_t x = 0; x < GRID_WIDTH; ++x) {
                processCollisionCell(x, y);
            }
        }
        
        // Apply wall constraints after each iteration
        for (auto& ball : allBalls) {
            handleWallCollisions(ball);
        }
    }
}

// Mouse callbacks
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            mousePressed = true;
            ballSpawnTimer = 0.0f;
            
            double xpos, ypos;
            glfwGetCursorPos(window, &xpos, &ypos);
            mouseWorldPos = screenToWorld(xpos, ypos);
            
            Ball newBall = createBall(mouseWorldPos);
            allBalls.push_back(newBall);
            
            std::cout << "Ball stream started | Total balls: " << allBalls.size() << std::endl;
        } else if (action == GLFW_RELEASE) {
            mousePressed = false;
            std::cout << "Ball stream stopped | Total balls: " << allBalls.size() << std::endl;
        }
    } else if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);
        mouseWorldPos = screenToWorld(xpos, ypos);

        const float pickRadius = 0.5f;
        int nearest = -1;
        float bestDistSq = pickRadius * pickRadius;
        
        for (size_t i = 0; i < allBalls.size(); ++i) {
            float distSq = glm::length2(allBalls[i].position - mouseWorldPos);
            if (distSq < bestDistSq) {
                bestDistSq = distSq;
                nearest = static_cast<int>(i);
            }
        }

        selectedBallIndex = nearest;
        if (nearest >= 0) {
            std::cout << "Selected ball: " << selectedBallIndex << std::endl;
        } else {
            std::cout << "No ball selected" << std::endl;
        }
    }
}

int main() {
    // GLFW initialization
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }
    
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Optimized Physics Engine", NULL, NULL);
    if (window == NULL) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);

    // GLAD initialization
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // OpenGL setup
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Shader and mesh setup
    Shader shader2D("shaders/vertex.vs", "shaders/simple_fragment.fs");
    Circle baseCircle(16, 1.0f);
    Mesh circleMesh = baseCircle.toMesh();

    // Texture setup
    unsigned int texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    unsigned char whitePixel[] = {255, 255, 255, 255};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, whitePixel);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Initialize simulation
    allBalls.reserve(3000);

    // Timing
    const float fixedDeltaTime = 1.0f / 120.0f;
    float deltaTime = 0.0f;
    float lastFrame = 0.0f;
    float accumulator = 0.0f;
    
    // Performance monitoring
    int frameCount = 0;
    float fpsTimer = 0.0f;

    std::cout << "Optimized physics engine ready! Hold left mouse to spawn balls." << std::endl;

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        processInput(window);

        // Timing
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // Prevent spiral of death
        if (deltaTime > 0.0167f) deltaTime = 0.0167f;
        accumulator += deltaTime;

        // Handle continuous ball spawning
        if (mousePressed) {
            ballSpawnTimer += deltaTime;
            
            double xpos, ypos;
            glfwGetCursorPos(window, &xpos, &ypos);
            mouseWorldPos = screenToWorld(xpos, ypos);
            
            while (ballSpawnTimer >= BALL_SPAWN_RATE) {
                if (allBalls.size() < 3000) {  // Limit for performance
                    Ball newBall = createBall(mouseWorldPos);
                    allBalls.push_back(newBall);
                }
                ballSpawnTimer -= BALL_SPAWN_RATE;
            }
        }

        // Fixed timestep physics
        while (accumulator >= fixedDeltaTime && !allBalls.empty()) {
            // Apply forces and update
            for (size_t i = 0; i < allBalls.size(); ++i) {
                Ball& ball = allBalls[i];
                applyForces(ball);
                updateBall(ball, static_cast<uint32_t>(i), fixedDeltaTime); // Pass the index
            }

            // Handle collisions
            populateGrid();
            handleCollisions();
            
            accumulator -= fixedDeltaTime;
        }

        // Performance monitoring
        frameCount++;
        fpsTimer += deltaTime;
        debugPrintTimer += deltaTime;
        
        if (fpsTimer >= 2.0f) {
            std::cout << "FPS: " << static_cast<int>(frameCount / 2.0f) 
                      << " | Balls: " << allBalls.size() << std::endl;
            frameCount = 0;
            fpsTimer = 0.0f;
        }

        // Rendering
        glClearColor(0.05f, 0.05f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        if (!allBalls.empty()) {
            glm::mat4 projection = glm::ortho(WORLD_LEFT, WORLD_RIGHT, WORLD_BOTTOM, WORLD_TOP, -1.0f, 1.0f);
            glm::mat4 view = glm::mat4(1.0f);

            shader2D.use();
            shader2D.setMat4("projection", projection);
            shader2D.setMat4("view", view);

            // Lighting
            shader2D.setVec3("lightColor", glm::vec3(1.0f));
            shader2D.setVec3("viewPos", glm::vec3(0.0f, 0.0f, 1.0f));
            shader2D.setVec3("dirLight.direction", glm::vec3(0.0f, 0.0f, -1.0f));
            shader2D.setVec3("dirLight.ambient", glm::vec3(0.7f));
            shader2D.setVec3("dirLight.diffuse", glm::vec3(0.3f));
            shader2D.setVec3("dirLight.specular", glm::vec3(0.1f));

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texture);
            shader2D.setInt("texture1", 0);

            // Render balls
            for (size_t i = 0; i < allBalls.size(); ++i) {
                const Ball& ball = allBalls[i];
                glm::mat4 model = glm::mat4(1.0f);
                model = glm::translate(model, glm::vec3(ball.position, 0.0f));
                model = glm::scale(model, glm::vec3(ball.radius));

                shader2D.setMat4("model", model);

                glm::vec3 renderColor = (static_cast<int>(i) == selectedBallIndex) ? 
                    glm::vec3(1.0f) : ball.color;

                shader2D.setVec3("material.ambient", renderColor * 0.5f);
                shader2D.setVec3("material.diffuse", renderColor);
                shader2D.setVec3("material.specular", glm::vec3(0.1f));
                shader2D.setFloat("material.shininess", 16.0f);

                circleMesh.Draw(shader2D);
            }

            // Debug selected ball
            if (selectedBallIndex >= 0 && selectedBallIndex < static_cast<int>(allBalls.size()) && 
                debugPrintTimer >= DEBUG_PRINT_INTERVAL) {
                const Ball& ball = allBalls[selectedBallIndex];
                glm::vec2 vel = ball.getVelocity(fixedDeltaTime);
                std::cout << "[DEBUG] Ball " << selectedBallIndex
                          << " | pos=(" << ball.position.x << "," << ball.position.y << ")"
                          << " | vel=(" << vel.x << "," << vel.y << ")"
                          << " | sleeping=" << (ball.isSleeping ? "yes" : "no")
                          << std::endl;
            }
        }

        if (debugPrintTimer >= DEBUG_PRINT_INTERVAL) debugPrintTimer = 0.0f;

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup
    glDeleteTextures(1, &texture);
    glfwTerminate();
    return 0;
}

void processInput(GLFWwindow *window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
    
    static bool cKeyPressed = false;
    if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS) {
        if (!cKeyPressed) {
            allBalls.clear();
            std::cout << "All balls cleared!" << std::endl;
            selectedBallIndex = -1;
            cKeyPressed = true;
        }
    } else {
        cKeyPressed = false;
    }
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}