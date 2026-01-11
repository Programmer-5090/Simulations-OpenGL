#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../shader.h"
#include "../mesh.h"
#include "../geometry/circle.h"
#include "constants.h"
#include "Nsolver.h"
#include "utils.h"
#include <iostream>
#include <random>
#include <vector>
#include <iomanip>
#include <string>
#include "mapPixel.h"

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);

float autoSpawnTimer = 0.0f;
bool spawnEnabled = true;

const float fixedDeltaTime = 1.0f / 120.0f;
float deltaTime = 0.0f;
float lastFrame = 0.0f;
float accumulator = 0.0f;

Nsolver solver;

// Image mapping state machine
enum class SpawnState {
    INITIAL_GENERATION,  // Generate particles to establish positions
    MAPPING_COLORS,      // Map particles to image colors
    SPAWNING_COLORED     // Spawn particles with mapped colors
};

SpawnState currentState = SpawnState::INITIAL_GENERATION;
MapPixel mapPixel;
size_t mapPixelIndex = 0;
const std::string IMAGE_PATH1 = "img/textures/teto.png";
const std::string IMAGE_PATH2 = "img/textures/doge.png";
const std::string IMAGE_PATH3 = "img/textures/nyan cat.png";
static bool useFirstImage = true;
bool debugPaused = false;
bool awaitingPhase3Input = false;

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
        solver.clearParticles();
        mapPixel.idToColor.clear();
        mapPixelIndex = 0;
        currentState = SpawnState::INITIAL_GENERATION;
        spawnEnabled = true;
        autoSpawnTimer = 0.0f;
        debugPaused = false;
        awaitingPhase3Input = false;
        std::cout << "Particles cleared - restarting color mapping" << std::endl;
    }
}

int main() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }
    
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Collision System", NULL, NULL);
    if (window == NULL) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        glfwTerminate();
        return -1;
    }

    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "GLSL Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;

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

    // Performance monitoring
    int frameCount = 0;
    float fpsTimer = 0.0f;

    std::cout << "Physics engine ready! Generating initial particle layout for color mapping..." << std::endl;
    std::cout << "Right-click or press C to clear and restart." << std::endl;
    std::cout << "Press P to toggle auto-spawning once the simulation is running." << std::endl;
    std::cout << "Press K to switch images" << std::endl;

    std::cout << "Entering main loop..." << std::endl;
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        processInput(window);

        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        if (deltaTime > 0.0167f) deltaTime = 0.0167f;

        if (!debugPaused) {
            accumulator += deltaTime;
        } else {
            accumulator = 0.0f;
        }

        // State machine for particle generation and color mapping
        switch (currentState) {
            case SpawnState::INITIAL_GENERATION:
                // Phase 1: Generate particles to establish positions (no colors yet)
                // NOTE: Actual spawning happens inside the physics fixed-step loop to ensure determinism.
                {
                    // Transition when we've reached max particles or density limit
                    float worldArea = (WORLD_RIGHT - WORLD_LEFT) * (WORLD_TOP - WORLD_BOTTOM);
                    float currentDensity = solver.getParticleCount() / worldArea;
                    if (solver.getParticleCount() >= MAX_PARTICLES || currentDensity >= MAX_PARTICLE_DENSITY) {
                        std::cout << "Generated " << solver.getParticleCount()
                                  << " particles (density: " << currentDensity
                                  << "). Mapping to image colors..." << std::endl;
                        // Stop any further spawning before mapping
                        spawnEnabled = false;
                        awaitingPhase3Input = false;
                        currentState = SpawnState::MAPPING_COLORS;
                    }
                }
                break;

            case SpawnState::MAPPING_COLORS:
                // Phase 2: Map current particle positions to image colors
                {
                    if (awaitingPhase3Input) {
                        break;
                    }

                    std::vector<Particle>& particles = solver.getParticles();
                    float worldWidth = WORLD_RIGHT - WORLD_LEFT;
                    float worldHeight = WORLD_TOP - WORLD_BOTTOM;
                    
                    // CRITICAL CHECK: Are particles stored in ID order in the vector?
                    // This matters because Phase 3 spawns assume sequential ID assignment
                    bool idOrderMismatch = false;
                    for (size_t i = 0; i < particles.size(); ++i) {
                        if (particles[i].id != (int)i) {
                            idOrderMismatch = true;
                            break;
                        }
                    }
                    
                    if (idOrderMismatch) {
                        std::cout << "\n!!! WARNING: Particle IDs do NOT match vector indices !!!" << std::endl;
                        std::cout << "This means the ID-to-color mapping will be incorrect!" << std::endl;
                        std::cout << "First 10 particles showing [index] vs ID:" << std::endl;
                        for (int i = 0; i < std::min(10, (int)particles.size()); ++i) {
                            std::cout << "  particles[" << i << "].id = " << particles[i].id;
                            if (particles[i].id != i) std::cout << " *** MISMATCH ***";
                            std::cout << std::endl;
                        }
                    }
                    
                    try {
                        if (useFirstImage) {
                            std::cout << "Mapping particles to colors from image: " << IMAGE_PATH1 << std::endl;
                            mapPixel.addParticles(particles, IMAGE_PATH3, worldWidth, worldHeight);
                        } else {
                            std::cout << "Mapping particles to colors from image: " << IMAGE_PATH2 << std::endl;
                            mapPixel.addParticles(particles, IMAGE_PATH2, worldWidth, worldHeight);
                        }
                        std::cout << "Color mapping complete. " << mapPixel.size() 
                                  << " colors stored." << std::endl;
                        
                        // DEBUG: Apply mapped colors to current particles for visualization
                        std::cout << "\n=== DEBUG MODE ===" << std::endl;
                        std::cout << "Applying mapped colors to settled particles for verification..." << std::endl;
                        for (auto& p : particles) {
                            auto col = mapPixel.getColorById(p.id);
                            p.color = glm::vec3(col[0], col[1], col[2]);
                        }
                        std::cout << "Colors applied. Press SPACE to continue to phase 3, or C to restart." << std::endl;
                        std::cout << "==================\n" << std::endl;
                        
                        debugPaused = true;
                        awaitingPhase3Input = true;
                        spawnEnabled = false;
                        accumulator = 0.0f;
                        autoSpawnTimer = 0.0f;
                        // Don't clear particles yet - stay in this state until user presses space
                    } catch (const std::exception& e) {
                        std::cerr << "Image mapping failed: " << e.what() << std::endl;
                        // Fall back to initial generation with default colors
                        currentState = SpawnState::INITIAL_GENERATION;
                        mapPixel.idToColor.clear();
                        awaitingPhase3Input = false;
                        debugPaused = false;
                        spawnEnabled = true;
                    }
                }
                break;

            case SpawnState::SPAWNING_COLORED:
                // Phase 3: Spawn particles with pre-mapped colors (looked up by ID)
                // NOTE: Actual spawning happens inside the physics fixed-step loop to ensure determinism.
                // Finished spawning check (executed every frame)
                if (mapPixelIndex >= mapPixel.size() && spawnEnabled) {
                    std::cout << "All " << mapPixel.size()
                              << " colored particles spawned! Waiting 5 seconds for settling..." << std::endl;
                    spawnEnabled = false;
                }
                
                break;
        }

        // Physics update (and deterministic spawning, synchronized with physics steps)
        // Skip physics updates when in debug pause
        if (!debugPaused) {
            try {
                while (accumulator >= fixedDeltaTime) {
                    // 1) Deterministic spawning happens FIRST in each fixed step
                    if (spawnEnabled) {
                        autoSpawnTimer += fixedDeltaTime;
                        while (autoSpawnTimer >= AUTO_SPAWN_INTERVAL) {
                            float baseY = WORLD_TOP - TOP_MARGIN;
                            float x = WORLD_LEFT + SPAWN_MARGIN_X;
                            for (int i = 0; i < STREAM_COUNT; ++i) {
                                // Stop conditions per mode
                                if (currentState == SpawnState::INITIAL_GENERATION) {
                                    if (solver.getParticleCount() >= MAX_PARTICLES) break;
                                } else if (currentState == SpawnState::SPAWNING_COLORED) {
                                    if (mapPixelIndex >= mapPixel.size()) break;
                                }

                                float y = baseY - i * STREAM_SPACING;
                                if (y - 0.2f < WORLD_BOTTOM) break;

                                glm::vec2 spawnPos(x, y);

                                if (currentState == SpawnState::INITIAL_GENERATION) {
                                    Particle p = solver.createParticle(spawnPos, CONSTANT_VELOCITY, 0.07f,
                                                                       fixedDeltaTime, true);
                                    solver.addParticle(p);
                                } else if (currentState == SpawnState::SPAWNING_COLORED) {
                                    Particle p = solver.createParticle(spawnPos, CONSTANT_VELOCITY, 0.07f,
                                                                       fixedDeltaTime, false);
                                    auto col = mapPixel.getColorById(p.id);
                                    p.color = glm::vec3(col[0], col[1], col[2]);

                                    solver.addParticle(p);
                                    ++mapPixelIndex;
                                }
                            }
                            autoSpawnTimer -= AUTO_SPAWN_INTERVAL;
                        }
                    }

                    // 2) Then advance physics one fixed step
                    solver.update(fixedDeltaTime);
                    accumulator -= fixedDeltaTime;
                    
                    static int updatesThisSecond = 0;
                    static float upsTimer = 0.0f;
                    
                    frameCount++;
                    fpsTimer += deltaTime;
                    upsTimer += deltaTime;
                    
                    if (accumulator < fixedDeltaTime) {
                        updatesThisSecond++;
                    }
                    
                    if (fpsTimer >= 1.0f) {
                        float fps = frameCount / fpsTimer;
                        float ups = updatesThisSecond / fpsTimer;
                        int particleCount = solver.getParticleCount();

                        std::cout << std::fixed << std::setprecision(1)
                                << "FPS: " << fps
                                << " | UPS: " << ups
                                << " | Particles: " << particleCount 
                                << " | State: ";
                        
                        switch(currentState) {
                            case SpawnState::INITIAL_GENERATION: std::cout << "Generating"; break;
                            case SpawnState::MAPPING_COLORS: std::cout << "Mapping"; break;
                            case SpawnState::SPAWNING_COLORED: std::cout << "Colored (" << mapPixelIndex << "/" << mapPixel.size() << ")"; break;
                        }
                        std::cout << std::endl;

                        frameCount = 0;
                        updatesThisSecond = 0;
                        fpsTimer = 0.0f;
                        upsTimer = 0.0f;
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "Solver update error: " << e.what() << std::endl;
                break;
            }
        }

        glClearColor(0.05f, 0.05f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        const std::vector<Particle>& particles = solver.getParticles();
        
        if (!particles.empty()) {
            glm::mat4 projection = glm::ortho(WORLD_LEFT, WORLD_RIGHT, WORLD_BOTTOM, WORLD_TOP, -1.0f, 1.0f);
            glm::mat4 view = glm::mat4(1.0f);

            shader2D.use();
            shader2D.setMat4("projection", projection);
            shader2D.setMat4("view", view);

            shader2D.setVec3("lightColor", glm::vec3(1.0f));
            shader2D.setVec3("viewPos", glm::vec3(0.0f, 0.0f, 1.0f));
            shader2D.setVec3("dirLight.direction", glm::vec3(0.0f, 0.0f, -1.0f));
            shader2D.setVec3("dirLight.ambient", glm::vec3(0.7f));
            shader2D.setVec3("dirLight.diffuse", glm::vec3(0.3f));
            shader2D.setVec3("dirLight.specular", glm::vec3(0.1f));

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texture);
            shader2D.setInt("texture1", 0);

            for (const Particle& particle : particles) {
                glm::mat4 model = glm::mat4(1.0f);
                model = glm::translate(model, glm::vec3(particle.position.x, particle.position.y, 0.0f));
                model = glm::scale(model, glm::vec3(particle.radius));

                shader2D.setMat4("model", model);
                shader2D.setVec3("material.ambient", particle.color * 0.6f);
                shader2D.setVec3("material.diffuse", particle.color);
                shader2D.setVec3("material.specular", glm::vec3(0.05f));
                shader2D.setFloat("material.shininess", 8.0f);

                circleMesh.Draw(shader2D);
            }
        }

        glfwSwapBuffers(window);
    }

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
            mapPixel.idToColor.clear();
            mapPixelIndex = 0;
            currentState = SpawnState::INITIAL_GENERATION;
            debugPaused = false;
            spawnEnabled = true;
            autoSpawnTimer = 0.0f;
            awaitingPhase3Input = false;
            std::cout << "All particles cleared - restarting color mapping!" << std::endl;
            cKeyPressed = true;
        }
    } else {
        cKeyPressed = false;
    }

    static bool pKeyPressed = false;
    if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS) {
        if (!pKeyPressed && !debugPaused) {
            spawnEnabled = !spawnEnabled;
            std::cout << (spawnEnabled ? "Auto-spawning enabled" : "Auto-spawning paused") << std::endl;
        }
        pKeyPressed = true;
    } else {
        pKeyPressed = false;
    }

    static bool kKeyPressed = false;
    if (glfwGetKey(window, GLFW_KEY_K) == GLFW_PRESS) {
        if (!kKeyPressed) {
            useFirstImage = !useFirstImage;
            std::cout << "Switched to " << (useFirstImage ? "first" : "second") << " image for color mapping." << std::endl;
            kKeyPressed = true;
        }
    } else {
        kKeyPressed = false;
    }

    static bool spacePressed = false;
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
        if (!spacePressed) {
            if (debugPaused) {
                if (awaitingPhase3Input) {
                    debugPaused = false;
                    awaitingPhase3Input = false;
                    std::cout << "\n=== PHASE 3: SPAWNING WITH MAPPED COLORS ===" << std::endl;
                    solver.clearParticles();
                    mapPixelIndex = 0;
                    spawnEnabled = true;
                    autoSpawnTimer = 0.0f;
                    currentState = SpawnState::SPAWNING_COLORED;
                }
            }
            spacePressed = true;
        }
    } else {
        spacePressed = false;
    }
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}