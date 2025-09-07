// Window and input handling for the GPU fluid demo.

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "shader.h"
#include "GPUFluidSimulation.h"
#include "GPUParticleDisplay.h"
#include "BoundingBox.h"
#include "camera.h"
#include <iostream>

static void framebuffer_size_callback(GLFWwindow* window, int width, int height);
static void processInput(GLFWwindow* window);
static void mouse_callback(GLFWwindow* window, double xpos, double ypos);
static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);

// Global pointer to fluid simulation for input handling
static GPUFluidSimulation* g_fluidSim = nullptr;


const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;

// Camera (static to avoid conflicts with other files)
static Camera camera(glm::vec3(0.0f, 5.0f, 25.0f));
static float lastX = SCR_WIDTH / 2.0f;
static float lastY = SCR_HEIGHT / 2.0f;
static bool firstMouse = true;

// Timing (static to avoid conflicts with other files)
static float deltaTime = 0.0f;
static float lastFrame = 0.0f;
static bool paused = false;


int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "3D GPU Fluid Simulation", NULL, NULL);
    if (window == NULL) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);

    // Tell GLFW to capture our mouse
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    glEnable(GL_DEPTH_TEST);

    // Shaders
    Shader particleShader("SPHFluid/shaders/particle3d.vs", "SPHFluid/shaders/particle3d.fs");
    Shader boxShader("SPHFluid/shaders/box.vs", "SPHFluid/shaders/box.fs"); // You will need to create this simple shader

    // Create VAO for the infinite grid (required for OpenGL Core Profile)
    unsigned int gridVAO;
    glGenVertexArrays(1, &gridVAO);

    // Infinite grid shader (same as used in the 2D window)
    Shader infiniteGridShader("shaders/infinite_grid.vs", "shaders/infinite_grid.fs");

    // Simulation settings - tuned to match Unity reference implementation
    GPUSimulationSettings settings;
    settings.gravity = -10.0f;  // Match Unity reference
    settings.smoothingRadius = 0.2f;  // Match Unity reference
    settings.targetDensity = 630.0f;  // Much higher like Unity reference settings (typical 500-1000)
    settings.pressureMultiplier = 288.0f;  // Match Unity reference
    settings.nearPressureMultiplier = 2.25f;  // Match Unity reference
    settings.viscosityStrength = 0.001f;  // Match Unity reference
    settings.boundsSize = glm::vec3(5.0f, 5.0f, 5.0f);  // Reasonable bounds
    settings.collisionDamping = 0.05f;  // Match Unity reference (much lower!)
    settings.boundaryForceMultiplier = 8.0f;  
    settings.boundaryForceDistance = 0.5f;  
    settings.timeScale = 1.0f;  // Match Unity reference
    settings.iterationsPerFrame = 3;  // Match Unity reference

        // Initialize GPU fluid simulation with 3D parameters
    const int numParticles = 10000; // Reduced to match Unity reference density
    GPUFluidSimulation fluidSim(numParticles, settings);
    g_fluidSim = &fluidSim;  // Set global pointer for input handling
    GPUParticleDisplay particleDisplay(&fluidSim, &particleShader);
    BoundingBox boundingBox(settings.boundsSize);

    // Print simulation info
    std::cout << "3D GPU Fluid Simulation Started!" << std::endl;
    std::cout << "Particles: " << numParticles << std::endl;
    std::cout << "Bounds: " << settings.boundsSize.x << " x " << settings.boundsSize.y << " x " << settings.boundsSize.z << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << "  WASD: Move camera" << std::endl;
    std::cout << "  Mouse: Look around" << std::endl;
    std::cout << "  Space: Pause/Resume" << std::endl;
    std::cout << "  R: Reset simulation" << std::endl;
    std::cout << "  ESC: Exit" << std::endl;

    // Main render loop
    while (!glfwWindowShouldClose(window)) {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        if (!paused) {
            fluidSim.Update(deltaTime);
            
            // Debug: Check if particles are staying in bounds
            static int frameCounter = 0;
            frameCounter++;
            if (frameCounter % 60 == 0) { // Check every 60 frames
                auto particles = fluidSim.GetParticles();
                int escapedCount = 0;
                glm::vec3 halfBounds = settings.boundsSize * 0.5f;
                
                for (const auto& particle : particles) {
                    if (abs(particle.position.x) > halfBounds.x || 
                        abs(particle.position.y) > halfBounds.y || 
                        abs(particle.position.z) > halfBounds.z) {
                        escapedCount++;
                    }
                }
                
                if (escapedCount > 0) {
                    std::cout << "WARNING: " << escapedCount << " particles outside bounds!" << std::endl;
                }
            }
        }
        particleDisplay.Update();

    // Use same white background as the 2D window
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();

    // Render infinite grid FIRST (so particles render on top)
    infiniteGridShader.use();
    glm::mat4 viewProjection = projection * view;
    infiniteGridShader.setMat4("gVP", viewProjection);
    infiniteGridShader.setVec3("gCameraWorldPos", camera.Position);
    infiniteGridShader.setFloat("gGridSize", 100.0f);
    infiniteGridShader.setFloat("gGridMinPixelsBetweenCells", 2.0f);
    infiniteGridShader.setFloat("gGridCellSize", 0.025f);
    infiniteGridShader.setVec4("gGridColorThin", glm::vec4(0.5f, 0.5f, 0.5f, 1.0f));
    infiniteGridShader.setVec4("gGridColorThick", glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
    infiniteGridShader.setFloat("gGridAlpha", 0.5f);

    glDepthMask(GL_FALSE);
    glBindVertexArray(gridVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDepthMask(GL_TRUE);

    // Render Bounding Box
    boxShader.use();
    boxShader.setMat4("projection", projection);
    boxShader.setMat4("view", view);
    boxShader.setMat4("model", glm::mat4(1.0f));
    boxShader.setVec3("color", glm::vec3(1.0f, 1.0f, 0.0f)); // Yellow color for the box
    boundingBox.Render(view, projection);

        // Render Particles
        particleShader.use();
        particleShader.setMat4("projection", projection);
        particleShader.setMat4("view", view);
        particleShader.setVec3("lightPos", glm::vec3(10.0, 20.0, 10.0));
        particleShader.setVec3("viewPos", camera.Position);
        particleDisplay.Render(view, projection);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}

static void processInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, deltaTime);
    
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
    
    // Reset simulation with R key
    static bool rKeyPressed = false;
    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
        if (!rKeyPressed && g_fluidSim) {
            g_fluidSim->Reset();
            std::cout << "Simulation reset" << std::endl;
        }
        rKeyPressed = true;
    } else {
        rKeyPressed = false;
    }
}

static void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

static void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    if (firstMouse)
    {
        lastX = static_cast<float>(xpos);
        lastY = static_cast<float>(ypos);
        firstMouse = false;
    }

    float xoffset = static_cast<float>(xpos) - lastX;
    float yoffset = lastY - static_cast<float>(ypos); 

    lastX = static_cast<float>(xpos);
    lastY = static_cast<float>(ypos);

    camera.ProcessMouseMovement(xoffset, yoffset);
}

static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    camera.ProcessMouseScroll(static_cast<float>(yoffset));
}
