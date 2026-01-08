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

static GPUFluidSimulation* g_fluidSim = nullptr;


const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;

static Camera camera(
    glm::vec3(-7.0f, 7.0f, 10.0f),          // position
    glm::vec3(0.0f, 1.0f, 0.0f),            // up
    -55.0f,                                 // yaw
    -30.0f                                  // pitch
);
static float lastX = SCR_WIDTH / 2.0f;
static float lastY = SCR_HEIGHT / 2.0f;
static bool firstMouse = true;

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

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    Shader particleShader("SPHFluid/shaders/particle3d.vs", "SPHFluid/shaders/particle3d.fs");
    Shader boxShader("SPHFluid/shaders/box.vs", "SPHFluid/shaders/box.fs");

    unsigned int gridVAO;
    glGenVertexArrays(1, &gridVAO);

    Shader infiniteGridShader("shaders/infinite_grid.vs", "shaders/infinite_grid.fs");

    GPUSimulationSettings settings;
    settings.gravity = -9.81f;
    settings.smoothingRadius = 0.2f;
    settings.targetDensity = 630.0f;
    settings.pressureMultiplier = 288.0f;
    settings.nearPressureMultiplier = 2.25f;
    settings.viscosityStrength = 0.001f;
    settings.boundsSize = glm::vec3(10.0f, 4.0f, 4.0f);
    settings.collisionDamping = 0.95f;
    settings.boundaryForceMultiplier = 8.0f;
    settings.boundaryForceDistance = 0.5f;
    settings.timeScale = 0.9f;
    settings.iterationsPerFrame = 3;

    const int numParticles = 50000 ;
    GPUFluidSimulation fluidSim(numParticles, settings);
    g_fluidSim = &fluidSim;
    GPUParticleDisplay particleDisplay(&fluidSim, &particleShader);
    BoundingBox boundingBox(settings.boundsSize);

    // Raise the simulation visuals along Y without changing physics coordinates
    const float worldYOffset = 2.0f;
    particleDisplay.SetWorldOffset(glm::vec3(0.0f, worldYOffset, 0.0f));

    std::cout << "3D GPU Fluid Simulation Started!" << std::endl;
    std::cout << "Particles: " << numParticles << std::endl;
    std::cout << "Bounds: " << settings.boundsSize.x << " x " << settings.boundsSize.y << " x " << settings.boundsSize.z << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << "  WASD: Move camera" << std::endl;
    std::cout << "  Mouse: Look around" << std::endl;
    std::cout << "  Space: Pause/Resume" << std::endl;
    std::cout << "  R: Reset simulation" << std::endl;
    std::cout << "  ESC: Exit" << std::endl;

    while (!glfwWindowShouldClose(window)) {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // FPS counter: print once per second
        static int frameCount = 0;
        static float fpsTimer = 0.0f;
        frameCount++;
        fpsTimer += deltaTime;
        if (fpsTimer >= 1.0f) {
            float fps = static_cast<float>(frameCount) / fpsTimer;
            std::cout << "FPS: " << fps << std::endl;
            frameCount = 0;
            fpsTimer = 0.0f;
        }

        processInput(window);

        if (!paused) {
            fluidSim.Update(deltaTime);
        }

        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();

        // Render infinite grid
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
        glm::mat4 boxModel = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, worldYOffset, 0.0f));
        boxShader.setMat4("model", boxModel);
        boxShader.setVec3("color", glm::vec3(1.0f, 0.0f, 0.0f));
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
    if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS)
        camera.ProcessKeyboard(UP, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        camera.ProcessKeyboard(DOWN, deltaTime);
    
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
