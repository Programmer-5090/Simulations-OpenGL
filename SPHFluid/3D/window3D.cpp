// Window and input handling for the GPU fluid demo.

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "shader.h"
#include "GPUFluidSimulation3D.h"
#include "GPUParticleDisplay3D.h"
#include "BoundingBox.h"
#include "camera.h"
#include <iostream>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);


const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;

// Camera
Camera camera(glm::vec3(0.0f, 5.0f, 25.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// Timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;
bool paused = false;


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

    // Simulation
    GPUSimulationSettings settings;
    settings.gravity = -9.8f;
    settings.smoothingRadius = 0.5f;
    settings.targetDensity = 20.0f;
    settings.pressureMultiplier = 50.0f;
    settings.nearPressureMultiplier = 10.0f;
    settings.viscosityStrength = 0.1f;
    settings.boundsSize = glm::vec3(12.0f, 15.0f, 12.0f);
    settings.collisionDamping = 0.5f;
    settings.timeScale = 1.0f;
    settings.iterationsPerFrame = 2;

    const int numParticles = 8000;
    GPUFluidSimulation fluidSim(numParticles, settings);
    GPUParticleDisplay3D particleDisplay(&fluidSim, &particleShader);
    BoundingBox boundingBox(settings.boundsSize);

    // Main render loop
    while (!glfwWindowShouldClose(window)) {
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        if (!paused) {
            fluidSim.Update(deltaTime);
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

void processInput(GLFWwindow *window)
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
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; 

    lastX = xpos;
    lastY = ypos;

    camera.ProcessMouseMovement(xoffset, yoffset);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    camera.ProcessMouseScroll(yoffset);
}
