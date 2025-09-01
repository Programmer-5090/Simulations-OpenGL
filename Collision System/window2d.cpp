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

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);

// Mouse interaction
bool mousePressed = false;
glm::vec2 mouseWorldPos(0.0f);
float ballSpawnTimer = 0.0f;
const float BALL_SPAWN_RATE = 0.01f;

// Global physics solver
PhysicsSolver solver;

// Mouse callbacks
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            mousePressed = true;
            ballSpawnTimer = 0.0f;
            
            double xpos, ypos;
            glfwGetCursorPos(window, &xpos, &ypos);
            mouseWorldPos = screenToWorld(xpos, ypos);
            
            Particle newParticle = solver.createBall(mouseWorldPos);
            solver.addParticle(newParticle);
            
            std::cout << "Ball stream started" << std::endl;
        } else if (action == GLFW_RELEASE) {
            mousePressed = false;
            std::cout << "Ball stream stopped" << std::endl;
        }
    } else if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
        std::cout << "Right click - clear all particles" << std::endl;
        solver.clearParticles();
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

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Simple Circle Renderer", NULL, NULL);
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
        Particle testParticle = solver.createBall(testPos);
        solver.addParticle(testParticle);
    }
    std::cout << "Initial particles added: " << solver.getParticleCount() << std::endl;

    // Timing
    const float fixedDeltaTime = 1.0f / 120.0f;
    float deltaTime = 0.0f;
    float lastFrame = 0.0f;
    float accumulator = 0.0f;
    
    // Performance monitoring
    int frameCount = 0;
    float fpsTimer = 0.0f;

    std::cout << "Physics engine ready! Hold left mouse to spawn balls, right click to clear." << std::endl;

    // Main loop
    std::cout << "Entering main loop..." << std::endl;
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
                Particle newParticle = solver.createBall(mouseWorldPos);
                solver.addParticle(newParticle);
                ballSpawnTimer -= BALL_SPAWN_RATE;
            }
        }

        // Fixed timestep physics - SAFE VERSION
        try {
            while (accumulator >= fixedDeltaTime) {
                static bool firstUpdate = true;
                if (firstUpdate) {
                    std::cout << "About to call solver.update() for first time..." << std::endl;
                    firstUpdate = false;
                }
                solver.update(fixedDeltaTime);
                accumulator -= fixedDeltaTime;
            }
            accumulator = 0.0f; // Reset to prevent spiral
        } catch (const std::exception& e) {
            std::cerr << "Solver update error: " << e.what() << std::endl;
            break;
        }

        // Performance monitoring
        frameCount++;
        fpsTimer += deltaTime;
        
        // Debug first few frames
        static int debugFrameCount = 0;
        debugFrameCount++;
        if (debugFrameCount < 5) {
            std::cout << "Frame " << debugFrameCount << " - Particles: " << solver.getParticleCount() << std::endl;
        }
        
        if (fpsTimer >= 2.0f) {
            std::cout << "FPS: " << static_cast<int>(frameCount / 2.0f) 
                      << " | Particles: " << solver.getParticleCount() << std::endl;
            frameCount = 0;
            fpsTimer = 0.0f;
        }

        // Rendering
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

            // Simple lighting
            shader2D.setVec3("lightColor", glm::vec3(1.0f));
            shader2D.setVec3("viewPos", glm::vec3(0.0f, 0.0f, 1.0f));
            shader2D.setVec3("dirLight.direction", glm::vec3(0.0f, 0.0f, -1.0f));
            shader2D.setVec3("dirLight.ambient", glm::vec3(0.7f));
            shader2D.setVec3("dirLight.diffuse", glm::vec3(0.3f));
            shader2D.setVec3("dirLight.specular", glm::vec3(0.1f));

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texture);
            shader2D.setInt("texture1", 0);

            // Render particles
            for (const Particle& particle : particles) {
                glm::mat4 model = glm::mat4(1.0f);
                model = glm::translate(model, glm::vec3(particle.position.x, particle.position.y, 0.0f));
                model = glm::scale(model, glm::vec3(particle.radius));

                shader2D.setMat4("model", model);

                shader2D.setVec3("material.ambient", particle.color * 0.5f);
                shader2D.setVec3("material.diffuse", particle.color);
                shader2D.setVec3("material.specular", glm::vec3(0.1f));
                shader2D.setFloat("material.shininess", 16.0f);

                circleMesh.Draw(shader2D);
            }
        }

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