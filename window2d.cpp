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
#include <unordered_set>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);

// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

// Fixed physics constants for stability
const float GRAVITY = 8.0f;              // Reduced for stability
const float RESTITUTION = 0.6f;          // Reduced for less bouncing
const float BALL_RESTITUTION = 0.7f;

// Improved sleeping parameters
// Threshold is in (m/s)^2 units (velocity squared). Raised slightly so
// very-small residual velocities are considered for sleeping.
const float SLEEP_THRESHOLD_SQ = 0.0004f; // ~0.02 m/s threshold
const float SLEEP_TIME = 0.5f;           // Time (s) a ball must remain below threshold

// World boundaries
const float WORLD_LEFT = -10.0f;
const float WORLD_RIGHT = 10.0f;
const float WORLD_BOTTOM = -7.5f;
const float WORLD_TOP = 7.5f;

const float WORLD_WIDTH = WORLD_RIGHT - WORLD_LEFT;
const float WORLD_HEIGHT = WORLD_TOP - WORLD_BOTTOM;

// Optimized grid settings
const int NUM_COLS = 100;
const int NUM_ROWS = 75;

const float CELL_WIDTH  = WORLD_WIDTH  / NUM_COLS;
const float CELL_HEIGHT = WORLD_HEIGHT / NUM_ROWS;

// Global variables for mouse interaction
bool mousePressed = false;
glm::vec2 mouseWorldPos(0.0f);
float ballSpawnTimer = 0.0f;
const float BALL_SPAWN_RATE = 0.05f; // Slower spawn rate for stability

// Debug selection
int selectedBallIndex = -1;
float debugPrintTimer = 0.0f;
const float DEBUG_PRINT_INTERVAL = 1.0f;

// Forward declaration
struct Ball;

// Optimized grid storage
std::vector<std::vector<std::vector<Ball*>>> grid(
    NUM_ROWS, std::vector<std::vector<Ball*>>(NUM_COLS));

// Global balls vector
std::vector<Ball> allBalls;

// 2D Ball data structure with physics
struct Ball {
    glm::vec2 position;
    glm::vec2 previous_position;
    glm::vec2 acceleration;
    glm::vec3 color;
    float radius;
    float mass;
    bool isSleeping = false;
    float sleepTimer = 0.0f;
    
    int gridRow = -1;
    int gridCol = -1;

    Ball() : position(0.0f), previous_position(0.0f), acceleration(0.0f), 
             color(1.0f), radius(0.1f), mass(1.0f) {}
};

// Convert screen coordinates to world coordinates
glm::vec2 screenToWorld(double xpos, double ypos) {
    float normalizedX = static_cast<float>(xpos) / SCR_WIDTH;
    float normalizedY = static_cast<float>(ypos) / SCR_HEIGHT;
    
    float worldX = WORLD_LEFT + normalizedX * WORLD_WIDTH;
    float worldY = WORLD_TOP - normalizedY * WORLD_HEIGHT;
    
    return glm::vec2(worldX, worldY);
}

// Generate random float between min and max
float randomFloat(float min, float max) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(min, max);
    return dis(gen);
}

// Create a new ball with reduced initial energy for stability
Ball createBall(glm::vec2 position) {
    Ball b;
    b.position = position + glm::vec2(
        randomFloat(-0.1f, 0.1f),
        randomFloat(-0.05f, 0.05f)
    );

    b.color = glm::vec3(
        randomFloat(0.4f, 1.0f),
        randomFloat(0.4f, 1.0f),
        randomFloat(0.4f, 1.0f)
    );

    b.radius = randomFloat(0.08f, 0.12f) * 1.2f;
    b.mass = 1.0f;

    // Reduced initial velocity for less chaotic behavior
    glm::vec2 initialVelocity(
        randomFloat(-2.0f, 2.0f),
        randomFloat(1.0f, 3.0f)
    );
    
    const float fixedDeltaTime = 1.0f / 120.0f;
    b.previous_position = b.position - initialVelocity * fixedDeltaTime;
    b.acceleration = glm::vec2(0.0f);
    b.isSleeping = false;
    b.sleepTimer = 0.0f;

    return b;
}

// Fast inline collision check
inline bool checkBallCollision(const Ball& a, const Ball& b) {
    float dx = a.position.x - b.position.x;
    float dy = a.position.y - b.position.y;
    float distSq = dx * dx + dy * dy;
    float radiusSum = a.radius + b.radius;
    return distSq <= (radiusSum * radiusSum);
}

// Improved collision resolution with mass consideration and stability
void resolveBallCollision(Ball& a, Ball& b) {
    glm::vec2 pos_delta = a.position - b.position;
    float dist = glm::length(pos_delta);
    float combined_radii = a.radius + b.radius;

    // Ignore tiny distances / degenerate cases
    if (dist <= 0.0001f) return;

    float overlap = combined_radii - dist;
    if (overlap <= 0.0f) return;

    // Ignore micro-penetrations to avoid constant tiny corrections
    const float PENETRATION_EPS = 0.001f;
    if (overlap < PENETRATION_EPS) return;

    glm::vec2 normal = pos_delta / dist;

    // Mass-based separation (apply relaxation/percent)
    float total_mass = a.mass + b.mass;
    float a_ratio = b.mass / total_mass;
    float b_ratio = a.mass / total_mass;

    const float percent = 0.7f;     // fraction of penetration to correct
    const float maxCorrection = 0.5f; // clamp per-step correction magnitude

    glm::vec2 rawSeparation = normal * (overlap * percent);
    if (glm::length(rawSeparation) > maxCorrection) {
        rawSeparation = glm::normalize(rawSeparation) * maxCorrection;
    }

    a.position += rawSeparation * a_ratio;
    b.position -= rawSeparation * b_ratio;

    // Wake conservatively: only wake if overlap was significant
    const float WAKE_OVERLAP = 0.01f;
    if (a.isSleeping && overlap > WAKE_OVERLAP) {
        a.isSleeping = false;
        a.sleepTimer = 0.0f;
    }
    if (b.isSleeping && overlap > WAKE_OVERLAP) {
        b.isSleeping = false;
        b.sleepTimer = 0.0f;
    }
}

// Improved wall collision with position-only correction
void handleWallCollisions(Ball& ball) {
    bool collided = false;
    
    if (ball.position.x - ball.radius < WORLD_LEFT) {
        ball.position.x = WORLD_LEFT + ball.radius;
        collided = true;
    }
    if (ball.position.x + ball.radius > WORLD_RIGHT) {
        ball.position.x = WORLD_RIGHT - ball.radius;
        collided = true;
    }
    if (ball.position.y - ball.radius < WORLD_BOTTOM) {
        ball.position.y = WORLD_BOTTOM + ball.radius;
        collided = true;
    }
    if (ball.position.y + ball.radius > WORLD_TOP) {
        ball.position.y = WORLD_TOP - ball.radius;
        collided = true;
    }
    
    if (collided) {
        if (ball.isSleeping) {
            ball.isSleeping = false;
            ball.sleepTimer = 0.0f;
        }
        
        // Apply restitution by modifying the implied velocity
        glm::vec2 velocity = ball.position - ball.previous_position;
        ball.previous_position = ball.position - velocity * RESTITUTION;
    }
}

// Improved update with better damping and velocity clamping
void updateBall(Ball& ball, float deltaTime) {
    if (ball.isSleeping) {
        ball.acceleration = glm::vec2(0.0f);
        return;
    }

    glm::vec2 velocity = ball.position - ball.previous_position;
    
    // Velocity clamping to prevent excessive speeds
    const float maxSpeed = 12.0f;
    float speed = glm::length(velocity);
    if (speed > maxSpeed) {
        velocity = (velocity / speed) * maxSpeed;
    }
    
    // Progressive damping - stronger for faster objects
    float damping = 0.995f - (speed * 0.0005f);
    damping = std::max(0.985f, std::min(0.999f, damping));
    
    velocity *= damping;

    // Verlet integration
    glm::vec2 current_position = ball.position;
    ball.position = current_position + velocity + ball.acceleration * (deltaTime * deltaTime);
    ball.previous_position = current_position;
    
    ball.acceleration = glm::vec2(0.0f);

    // Improved sleep detection
    // Convert to velocity in units/sec before comparing to threshold
    float speedSq = glm::length2(velocity) / (deltaTime * deltaTime);
    if (speedSq < SLEEP_THRESHOLD_SQ) {
        ball.sleepTimer += deltaTime;
        if (ball.sleepTimer >= SLEEP_TIME) {
            ball.isSleeping = true;
            ball.sleepTimer = SLEEP_TIME;
            // Lock previous_position exactly to current position to avoid tiny drift
            ball.previous_position = ball.position;
        }
        else {
            // If approaching sleep, clamp extremely tiny velocities to zero to damp jitter
            const float VELOCITY_EPS_SQ = 1e-8f;
            if (glm::length2(velocity) < VELOCITY_EPS_SQ) {
                ball.previous_position = ball.position;
            }
        }
    } else {
        ball.sleepTimer = 0.0f;
    }
}

// Apply forces like gravity
void applyForces(Ball& ball) {
    if (!ball.isSleeping) {
        ball.acceleration.y -= GRAVITY;
    }
}

// Optimized grid population
void populateGrid(std::vector<Ball>& balls) {
    for (auto& row : grid) {
        for (auto& cell : row) {
            cell.clear();
        }
    }
    
    for (auto& ball : balls) {
        float relativeX = ball.position.x - WORLD_LEFT;
        float relativeY = ball.position.y - WORLD_BOTTOM;

        int col = static_cast<int>(relativeX / CELL_WIDTH);
        int row = static_cast<int>(relativeY / CELL_HEIGHT);

        col = std::max(0, std::min(col, NUM_COLS - 1));
        row = std::max(0, std::min(row, NUM_ROWS - 1));
        
        ball.gridRow = row;
        ball.gridCol = col;
        
        grid[row][col].push_back(&ball);
    }
}

// Multiple solver iterations with decreasing strength
void handleCollisions() {
    const int SOLVER_ITERATIONS = 8; // fewer iterations to reduce over-correction

    // Temporary per-ball position deltas to accumulate corrections
    std::vector<glm::vec2> positionDeltas;
    positionDeltas.resize(allBalls.size(), glm::vec2(0.0f));

    for (int iter = 0; iter < SOLVER_ITERATIONS; ++iter) {
        float iteration_strength = 1.0f / (iter + 1);

        // Zero deltas each iteration
        std::fill(positionDeltas.begin(), positionDeltas.end(), glm::vec2(0.0f));

        for (int r = 0; r < NUM_ROWS; ++r) {
            for (int c = 0; c < NUM_COLS; ++c) {
                auto& currentCell = grid[r][c];
                if (currentCell.empty()) continue;

                // Within cell collisions
                for (size_t i = 0; i < currentCell.size(); ++i) {
                    for (size_t j = i + 1; j < currentCell.size(); ++j) {
                        Ball* a = currentCell[i];
                        Ball* b = currentCell[j];
                        if (checkBallCollision(*a, *b)) {
                            glm::vec2 orig_a = a->position;
                            glm::vec2 orig_b = b->position;
                            resolveBallCollision(*a, *b);

                            // accumulate delta scaled by iteration strength
                            size_t ia = a - &allBalls[0];
                            size_t ib = b - &allBalls[0];
                            positionDeltas[ia] += (a->position - orig_a) * iteration_strength;
                            positionDeltas[ib] += (b->position - orig_b) * iteration_strength;

                            // restore original now; deltas will be applied later
                            a->position = orig_a;
                            b->position = orig_b;
                        }
                    }
                }

                // Neighboring cell collisions (only +x, +y, +xy to avoid double checks)
                static const int neighbors[][2] = {{0, 1}, {1, 0}, {1, 1}, {1, -1}};
                for (auto& neighbor : neighbors) {
                    int nr = r + neighbor[0];
                    int nc = c + neighbor[1];

                    if (nr < 0 || nr >= NUM_ROWS || nc < 0 || nc >= NUM_COLS) continue;

                    auto& neighborCell = grid[nr][nc];
                    if (neighborCell.empty()) continue;

                    for (Ball* ballA : currentCell) {
                        for (Ball* ballB : neighborCell) {
                            if (checkBallCollision(*ballA, *ballB)) {
                                glm::vec2 orig_a = ballA->position;
                                glm::vec2 orig_b = ballB->position;
                                resolveBallCollision(*ballA, *ballB);

                                size_t ia = ballA - &allBalls[0];
                                size_t ib = ballB - &allBalls[0];
                                positionDeltas[ia] += (ballA->position - orig_a) * iteration_strength;
                                positionDeltas[ib] += (ballB->position - orig_b) * iteration_strength;

                                ballA->position = orig_a;
                                ballB->position = orig_b;
                            }
                        }
                    }
                }
            }
        }

        // Apply accumulated deltas to allBalls (this is the single write phase)
        for (size_t i = 0; i < allBalls.size(); ++i) {
            if (glm::length2(positionDeltas[i]) > 0.0f) {
                allBalls[i].position += positionDeltas[i];
            }
        }
    }
}

// Mouse button callback function
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
    }
    else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        if (action == GLFW_PRESS) {
            double xpos, ypos;
            glfwGetCursorPos(window, &xpos, &ypos);
            mouseWorldPos = screenToWorld(xpos, ypos);

            const float pickRadius = 0.5f;
            int nearest = -1;
            float bestSq = pickRadius * pickRadius;
            for (size_t i = 0; i < allBalls.size(); ++i) {
                float d2 = glm::length2(allBalls[i].position - mouseWorldPos);
                if (d2 <= bestSq) {
                    bestSq = d2;
                    nearest = static_cast<int>(i);
                }
            }

            if (nearest >= 0) {
                selectedBallIndex = nearest;
                std::cout << "Selected ball: " << selectedBallIndex << std::endl;
            } else {
                selectedBallIndex = -1;
                std::cout << "No ball selected" << std::endl;
            }
        }
    }
}

int main()
{
    // GLFW initialization
    if (!glfwInit()) {
        std::cout << "Failed to initialize GLFW" << std::endl;
        return -1;
    }
    
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // Window creation
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Fixed Stable Verlet Physics", NULL, NULL);
    if (window == NULL) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);

    // GLAD initialization
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // OpenGL setup
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Shader loading
    std::cout << "Loading shaders..." << std::endl;
    Shader shader2D("shaders/vertex.vs", "shaders/simple_fragment.fs");
    std::cout << "Shaders loaded successfully" << std::endl;

    // Circle mesh creation
    std::cout << "Generating circle mesh..." << std::endl;
    Circle baseCircle(16, 1.0f);
    Mesh circleMesh = baseCircle.toMesh();
    std::cout << "Circle mesh generated successfully" << std::endl;

    // Initialize with empty balls vector
    allBalls.clear();
    allBalls.reserve(2000);

    std::cout << "Hold left mouse button to spawn stable ball streams!" << std::endl;

    // Texture setup
    unsigned int texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    
    unsigned char whitePixel[] = {255, 255, 255, 255};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, whitePixel);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Timing setup
    float deltaTime = 0.0f;
    float lastFrame = 0.0f;
    float accumulator = 0.0f;
    const float fixedDeltaTime = 1.0f / 120.0f;

    std::cout << "Starting stable physics simulation..." << std::endl;

    // Performance monitoring
    int frameCount = 0;
    float fpsTimer = 0.0f;

    // Main render loop
    while (!glfwWindowShouldClose(window))
    {
        processInput(window);

        // Timing
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // Prevent spiral of death
        if (deltaTime > 0.0167f) deltaTime = 0.0167f;
        accumulator += deltaTime;

        // Handle continuous ball spawning while mouse is held
        if (mousePressed) {
            ballSpawnTimer += deltaTime;
            
            double xpos, ypos;
            glfwGetCursorPos(window, &xpos, &ypos);
            mouseWorldPos = screenToWorld(xpos, ypos);
            
            while (ballSpawnTimer >= BALL_SPAWN_RATE) {
                Ball newBall = createBall(mouseWorldPos);
                allBalls.push_back(newBall);
                ballSpawnTimer -= BALL_SPAWN_RATE;
            }
        }

        // Fixed timestep physics
        if (!allBalls.empty()) {
            while (accumulator >= fixedDeltaTime) {
                // Apply forces and update positions
                for (auto& ball : allBalls) {
                    applyForces(ball);
                    updateBall(ball, fixedDeltaTime);
                    handleWallCollisions(ball);
                }

                // Handle collisions
                populateGrid(allBalls);
                handleCollisions();
                
                accumulator -= fixedDeltaTime;
            }
        } else {
            accumulator = 0.0f;
        }

        // FPS monitoring
        frameCount++;
        fpsTimer += deltaTime;
        debugPrintTimer += deltaTime;
        
        if (fpsTimer >= 2.0f) {
            std::cout << "FPS: " << frameCount / 2 << " | Balls: " << allBalls.size() << std::endl;
            frameCount = 0;
            fpsTimer = 0.0f;
        }

        // Rendering
        glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glm::mat4 projection = glm::ortho(WORLD_LEFT, WORLD_RIGHT, WORLD_BOTTOM, WORLD_TOP, -1.0f, 1.0f);
        glm::mat4 view = glm::mat4(1.0f);

        shader2D.use();
        shader2D.setMat4("projection", projection);
        shader2D.setMat4("view", view);

        // Lighting setup
        shader2D.setVec3("lightColor", glm::vec3(1.0f));
        shader2D.setVec3("viewPos", glm::vec3(0.0f, 0.0f, 1.0f));
        shader2D.setVec3("dirLight.direction", glm::vec3(0.0f, 0.0f, -1.0f));
        shader2D.setVec3("dirLight.ambient", glm::vec3(0.6f));
        shader2D.setVec3("dirLight.diffuse", glm::vec3(0.4f));
        shader2D.setVec3("dirLight.specular", glm::vec3(0.1f));

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        shader2D.setInt("texture1", 0);

        // Render all balls
        for (size_t i = 0; i < allBalls.size(); ++i) {
            const auto& ball = allBalls[i];
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(ball.position.x, ball.position.y, 0.0f));
            model = glm::scale(model, glm::vec3(ball.radius));

            shader2D.setMat4("model", model);

            glm::vec3 renderColor = (static_cast<int>(i) == selectedBallIndex) ? glm::vec3(1.0f) : ball.color;

            shader2D.setVec3("material.ambient", renderColor * 0.4f);
            shader2D.setVec3("material.diffuse", renderColor);
            shader2D.setVec3("material.specular", glm::vec3(0.1f));
            shader2D.setFloat("material.shininess", 8.0f);

            circleMesh.Draw(shader2D);

            // Debug selected ball
            if (static_cast<int>(i) == selectedBallIndex && debugPrintTimer >= DEBUG_PRINT_INTERVAL) {
                glm::vec2 vel = (ball.position - ball.previous_position) / fixedDeltaTime;
                std::cout << "[DEBUG] Ball " << selectedBallIndex
                          << " | pos=" << ball.position.x << "," << ball.position.y
                          << " | vel=" << vel.x << "," << vel.y
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