#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../shader.h"
#include "../mesh.h"
#include "../geometry/circle.h"
#include "constants.h"
#include "solver.h"
#include "utils.h"
#include <iostream>
#include <random>
#include <vector>
#include <iomanip> // For std::fixed, std::setprecision

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods); // (unused now)

// Automatic spawning configuration
float autoSpawnTimer = 0.0f;
const float AUTO_SPAWN_INTERVAL = 0.0125f; // seconds between spawn waves
const glm::vec2 CONSTANT_VELOCITY = glm::vec2(3.0f, 0.0f); // constant initial velocity for all balls
const int STREAM_COUNT = 4;            // number of parallel streams
const float STREAM_SPACING = 0.30f;    // vertical spacing between streams
const float SPAWN_MARGIN_X = 0.25f;    // offset from WORLD_LEFT
const float TOP_MARGIN = 0.4f;         // distance below WORLD_TOP for first stream
bool spawnEnabled = true;                 // can be disabled on slowdown
const float PERF_CHECK_INTERVAL = 2.0f;   // seconds between performance checks
const float PHYSICS_TIME_THRESHOLD = 100.0f; // if physics takes >30ms per frame, stop spawning
const float MAX_PARTICLE_DENSITY = 1.0f; // max particles per world unit area (stops spawning when full)
float perfTimer = 0.0f;                   // accumulates time for performance window
float maxPhysicsTimeInWindow = 0.0f;      // peak physics time in current window

// Timing
const float fixedDeltaTime = 1.0f / 120.0f;
float deltaTime = 0.0f;
float lastFrame = 0.0f;
float accumulator = 0.0f;

// Global physics solver
PhysicsSolver solver;

// Mouse callback retained but disabled (no spawning)
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
        solver.clearParticles();
        std::cout << "Particles cleared" << std::endl;
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

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Collision System", NULL, NULL);
    if (window == NULL) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    // Mouse spawning disabled – only right-click clear retained
    glfwSetMouseButtonCallback(window, mouse_button_callback);

    // GLAD initialization
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        glfwTerminate();
        return -1;
    }

    // Check OpenGL version
    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "GLSL Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;

    // OpenGL setup
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Shader and mesh setup
    std::cout << "Loading shaders..." << std::endl;
    Shader shader2D("shaders/vertex.vs", "shaders/simple_fragment.fs");
    std::cout << "Shaders loaded successfully" << std::endl;
    
    std::cout << "Creating circle mesh..." << std::endl;
    Circle baseCircle(16, 1.0f);
    Mesh circleMesh = baseCircle.toMesh();
    std::cout << "Circle mesh created successfully" << std::endl;

    // Texture setup
    unsigned int texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    unsigned char whitePixel[] = {255, 255, 255, 255};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, whitePixel);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Add some initial particles for testing
    std::cout << "Adding initial test particles..." << std::endl;
    for (int i = 0; i < 10; ++i) {
        glm::vec2 testPos(randomFloat(-5.0f, 5.0f), randomFloat(2.0f, 5.0f));
        Particle testParticle = solver.createBall(testPos, fixedDeltaTime);
        solver.addParticle(testParticle);
    }
    std::cout << "Initial particles added: " << solver.getParticleCount() << std::endl;

    
    
    // Performance monitoring
    int frameCount = 0;
    float fpsTimer = 0.0f;

    std::cout << "Physics engine ready! Auto-spawning from left edge. Right-click or press C to clear." << std::endl;
    std::cout << "Spawning stops automatically if physics time exceeds " << PHYSICS_TIME_THRESHOLD << "ms per frame" << std::endl;
    std::cout << "Or if particle density exceeds " << MAX_PARTICLE_DENSITY << " particles per world unit area" << std::endl;

    std::cout << "Entering main loop..." << std::endl;
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        processInput(window);

        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        if (deltaTime > 0.0167f) deltaTime = 0.0167f;
        accumulator += deltaTime;

        if (spawnEnabled) {
            autoSpawnTimer += deltaTime;
            while (autoSpawnTimer >= AUTO_SPAWN_INTERVAL) {
                float baseY = WORLD_TOP - TOP_MARGIN;
                float x = WORLD_LEFT + SPAWN_MARGIN_X;
                for (int i = 0; i < STREAM_COUNT; ++i) {
                    float y = baseY - i * STREAM_SPACING;
                    if (y - 0.2f < WORLD_BOTTOM) break;
                    glm::vec2 spawnPos(x, y);
                    Particle p = solver.createBallWithVelocity(spawnPos, CONSTANT_VELOCITY, fixedDeltaTime);
                    solver.addParticle(p);
                }
                autoSpawnTimer -= AUTO_SPAWN_INTERVAL;
            }
        }

        try {
            while (accumulator >= fixedDeltaTime) {
                solver.update(fixedDeltaTime);
                accumulator -= fixedDeltaTime;
                
                float physicsTime = solver.getLastPhysicsTime();
                maxPhysicsTimeInWindow = std::max(maxPhysicsTimeInWindow, physicsTime);
            }
        } catch (const std::exception& e) {
            std::cerr << "Solver update error: " << e.what() << std::endl;
            break;
        }

        // Performance monitoring based on physics timing
        perfTimer += deltaTime;
        
        // Debug first few frames
        static int debugFrameCount = 0;
        debugFrameCount++;
        if (debugFrameCount < 5) {
            std::cout << "Frame " << debugFrameCount << " - Particles: " << solver.getParticleCount() << std::endl;
        }
        
        if (perfTimer >= PERF_CHECK_INTERVAL) {
            float worldArea = (WORLD_RIGHT - WORLD_LEFT) * (WORLD_TOP - WORLD_BOTTOM);
            float currentDensity = solver.getParticleCount() / worldArea;
            
            // std::cout << "Peak Physics Time: " << maxPhysicsTimeInWindow << "ms"
            //           << " | Particles: " << solver.getParticleCount()
            //           << " | Density: " << std::fixed << std::setprecision(3) << currentDensity
            //           << (spawnEnabled ? " | Spawning ON" : " | Spawning OFF")
            //           << std::endl;
            if (spawnEnabled && maxPhysicsTimeInWindow > PHYSICS_TIME_THRESHOLD) {
                spawnEnabled = false;
                std::cout << "Auto-spawning disabled due to slow physics (" << maxPhysicsTimeInWindow << "ms)" << std::endl;
            }
            maxPhysicsTimeInWindow = 0.0f;
            perfTimer = 0.0f;
        }        // Rendering
        glClearColor(0.05f, 0.05f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        const std::vector<Particle>& particles = solver.getParticles();
        static bool debugOnce = false;
        if (!debugOnce) {
            std::cout << "Got particles vector, size: " << particles.size() << std::endl;
            debugOnce = true;
        }
        
        if (!particles.empty()) {
            glm::mat4 projection = glm::ortho(WORLD_LEFT, WORLD_RIGHT, WORLD_BOTTOM, WORLD_TOP, -1.0f, 1.0f);
            glm::mat4 view = glm::mat4(1.0f);

            shader2D.use();
            shader2D.setMat4("projection", projection);
            shader2D.setMat4("view", view);

            // Optimized: Set lighting once per frame, not per particle
            shader2D.setVec3("lightColor", glm::vec3(1.0f));
            shader2D.setVec3("viewPos", glm::vec3(0.0f, 0.0f, 1.0f));
            shader2D.setVec3("dirLight.direction", glm::vec3(0.0f, 0.0f, -1.0f));
            shader2D.setVec3("dirLight.ambient", glm::vec3(0.7f));
            shader2D.setVec3("dirLight.diffuse", glm::vec3(0.3f));
            shader2D.setVec3("dirLight.specular", glm::vec3(0.1f));

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texture);
            shader2D.setInt("texture1", 0);

            size_t renderedCount = 0;
            for (const Particle& particle : particles) {
                
                glm::mat4 model = glm::mat4(1.0f);
                model = glm::translate(model, glm::vec3(particle.position.x, particle.position.y, 0.0f));
                model = glm::scale(model, glm::vec3(particle.radius));

                shader2D.setMat4("model", model);

                // Optimized: Simpler material setup
                shader2D.setVec3("material.ambient", particle.color * 0.6f);
                shader2D.setVec3("material.diffuse", particle.color);
                shader2D.setVec3("material.specular", glm::vec3(0.05f)); // Reduced specular
                shader2D.setFloat("material.shininess", 8.0f); // Reduced shininess

                circleMesh.Draw(shader2D);
                renderedCount++;
            }
            
            // Debug: Show render stats occasionally
            static int renderDebugCounter = 0;
            if (++renderDebugCounter % 120 == 0) { // Every 2 seconds at 60fps
                float cullingRatio = (float)renderedCount / particles.size();
                if (cullingRatio < 0.95f) {
                    std::cout << "Culled " << (particles.size() - renderedCount) 
                              << " particles (" << (int)((1.0f - cullingRatio) * 100) << "%)" << std::endl;
                }
            }
        }

    glfwSwapBuffers(window);
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
            solver.clearParticles();
            std::cout << "All particles cleared!" << std::endl;
            cKeyPressed = true;
        }
    } else {
        cKeyPressed = false;
    }
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}