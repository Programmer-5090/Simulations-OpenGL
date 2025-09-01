// Window and input handling for the GPU fluid demo.

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "shader.h"
#include "GPUFluidSimulation.h"
#include "GPUParticleDisplay.h"
#include <iostream>
#include <chrono>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
void cursor_position_callback(GLFWwindow* window, double xpos, double ypos);

const unsigned int SCR_WIDTH = 1200;
const unsigned int SCR_HEIGHT = 800;

const float WORLD_LEFT = -10.0f;
const float WORLD_RIGHT = 10.0f;
const float WORLD_BOTTOM = -6.67f;
const float WORLD_TOP = 6.67f;
const float WORLD_WIDTH = WORLD_RIGHT - WORLD_LEFT;
const float WORLD_HEIGHT = WORLD_TOP - WORLD_BOTTOM;

bool leftMousePressed = false;
bool rightMousePressed = false;
glm::vec2 mouseWorldPos(0.0f);
bool paused = false;

// Simulation
GPUFluidSimulation* fluidSim = nullptr;
GPUParticleDisplay* particleDisplay = nullptr;

glm::vec2 screenToWorld(double xpos, double ypos) {
    float normalizedX = static_cast<float>(xpos) / SCR_WIDTH;
    float normalizedY = static_cast<float>(ypos) / SCR_HEIGHT;
    
    float worldX = WORLD_LEFT + normalizedX * WORLD_WIDTH;
    float worldY = WORLD_TOP - normalizedY * WORLD_HEIGHT;
    
    return glm::vec2(worldX, worldY);
}

void updateSimulationSettings() {
    if (fluidSim) {
        GPUSimulationSettings settings = fluidSim->GetSettings();
        settings.leftMousePressed = leftMousePressed;
        settings.rightMousePressed = rightMousePressed;
        settings.mousePosition = mouseWorldPos;
        fluidSim->SetSettings(settings);
    }
}

int main() {
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }
    
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "GPU Fluid Simulation", NULL, NULL);
    if (window == NULL) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);

    // Initialize GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // Enable blending for particle rendering
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Load shaders
    Shader particleShader("SPHFluid/shaders/particle2d.vs", "SPHFluid/shaders/particle2d.fs");

    // Check for compute shader support
    int maxComputeWorkGroupInvocations;
    glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &maxComputeWorkGroupInvocations);
    std::cout << "Max compute work group invocations: " << maxComputeWorkGroupInvocations << std::endl;

    // Initialize fluid simulation with tuned parameters for the GPU build
    GPUSimulationSettings settings;
    settings.gravity = -12.0f;          // Slightly reduced to avoid over-compression at bottom
    settings.smoothingRadius = 0.35f;   // Slightly larger kernel for smoother density gradient
    settings.targetDensity = 55.0f;     // Higher target density for more uniform packing
    settings.pressureMultiplier = 500.0f; // Balanced primary pressure
    settings.nearPressureMultiplier = 18.0f; // Strong near pressure -> pseudo surface tension
    settings.viscosityStrength = 0.06f; // Slightly lower to keep surface lively
    settings.boundsSize = glm::vec2(WORLD_WIDTH, WORLD_HEIGHT);
    settings.obstacleSize = glm::vec2(0.0f, 0.0f); 
    settings.obstacleCenter = glm::vec2(0.0f, 0.0f);
    settings.interactionRadius = 2.0f;  // Interaction search radius
    settings.interactionStrength = 150.0f; // Interaction force strength
    settings.collisionDamping = 0.95f;   // Collision damping factor
    settings.timeScale = 1.0f;          
    settings.iterationsPerFrame = 4;
    settings.boundaryForceMultiplier = 2.0f; // Disabled explicit boundary force (handled via collisions)
    settings.boundaryForceDistance = 0.0f;
    
    const int numParticles = 10000; // Number of particles
    fluidSim = new GPUFluidSimulation(numParticles, settings);
    particleDisplay = new GPUParticleDisplay(fluidSim, &particleShader);
    
    // Set up camera/view matrices
    glm::mat4 view = glm::mat4(1.0f);
    glm::mat4 projection = glm::ortho(WORLD_LEFT, WORLD_RIGHT, WORLD_BOTTOM, WORLD_TOP, -1.0f, 1.0f);

    // Timing
    float deltaTime = 0.0f;
    float lastFrame = 0.0f;
    
    // Performance monitoring
    int frameCount = 0;
    float fpsTimer = 0.0f;
    auto lastTime = std::chrono::high_resolution_clock::now();

    std::cout << "GPU Fluid Simulation initialized with " << numParticles << " particles!" << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << "  Left Mouse: Attract particles" << std::endl;
    std::cout << "  Right Mouse: Repel particles" << std::endl;
    std::cout << "  R: Reset simulation" << std::endl;
    std::cout << "  ESC: Exit" << std::endl;

    // Main render loop
    while (!glfwWindowShouldClose(window)) {
        // Calculate delta time
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        
        // Process input
        processInput(window);
        updateSimulationSettings();
        
        // Update simulation (skip physics while paused)
        try {
            if (!paused && fluidSim) {
                fluidSim->Update(deltaTime);
            }
            if (particleDisplay) particleDisplay->Update();
        } catch (const std::exception& e) {
            std::cerr << "Simulation error: " << e.what() << std::endl;
        }

        // Clear screen
        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Render particles
        particleDisplay->Render(view, projection);
        
        // Performance monitoring
        frameCount++;
        fpsTimer += deltaTime;
        if (fpsTimer >= 1.0f) {
            std::cout << "FPS: " << frameCount << ", Particles: " << numParticles << std::endl;
            frameCount = 0;
            fpsTimer = 0.0f;
        }

        // Swap buffers and poll events
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup
    delete particleDisplay;
    delete fluidSim;
    
    glfwTerminate();
    return 0;
}

void processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }
    
    // Reset simulation
    static bool rKeyPressed = false;
    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
        if (!rKeyPressed && fluidSim) {
            fluidSim->Reset();
            std::cout << "Simulation reset!" << std::endl;
        }
        rKeyPressed = true;
    } else {
        rKeyPressed = false;
    }

    // Pause / resume with Space (toggle on key press)
    static bool spaceKeyPressed = false;
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
        if (!spaceKeyPressed) {
            paused = !paused;
            std::cout << (paused ? "Simulation paused" : "Simulation resumed") << std::endl;
        }
        spaceKeyPressed = true;
    } else {
        spaceKeyPressed = false;
    }
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        leftMousePressed = (action == GLFW_PRESS);
    }
    if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        rightMousePressed = (action == GLFW_PRESS);
    }
}

void cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
    mouseWorldPos = screenToWorld(xpos, ypos);
}
