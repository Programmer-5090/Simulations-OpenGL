#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "shader.h"
#include "camera.h"
#include "model.h"
#include "RubiksCube.h"
#include "input.h"
#include "mouse_selector.h"
#include "ai rubiks.h"
#include "thread_pool.h"

#include <array>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <cstdlib>
#include <ctime>
#include <future>
#include <chrono>

namespace {

constexpr unsigned int SCR_WIDTH = 800;
constexpr unsigned int SCR_HEIGHT = 600;

class InfiniteGridRenderer {
public:
    InfiniteGridRenderer();
    ~InfiniteGridRenderer();

    void render(const glm::mat4& projection, const glm::mat4& view, const glm::vec3& cameraPos);

private:
    Shader shader;
    unsigned int vao{0};

    float gridSize = 100.0f;
    float minPixelsBetweenCells = 2.0f;
    float cellSize = 0.025f;
    glm::vec4 colorThin{0.5f, 0.5f, 0.5f, 1.0f};
    glm::vec4 colorThick{0.0f, 0.0f, 0.0f, 1.0f};
    float alpha = 0.5f;
};

class RubiksApplication {
public:
    RubiksApplication();
    ~RubiksApplication();

    int run();
    void scramble(int depth);
    void solveCube();

private:
    void initWindow();
    void initGLAD();
    void initInputSystem();
    void initCallbacks();
    void initRenderState();
    void mainLoop();
    void updateDeltaTime();
    void processInput();
    void renderScene();
    void onFramebufferResize(int width, int height);
    void recreateFramebuffers(int width, int height);
    void cleanupFramebuffers();
    void onMouseMove(double xpos, double ypos);
    void onScroll(double yoffset);
    void updateOrbitCamera();
    void playNextSolveMove();
    bool animateMove(Move move, float duration = 0.15f);

    static void framebuffer_size_callback(GLFWwindow* window, int width, int height);
    static void mouse_callback(GLFWwindow* window, double xpos, double ypos);
    static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);

    inline static const std::array<glm::vec3, 6> kAxisDirections = {
        glm::vec3( 1.0f,  0.0f,  0.0f),
        glm::vec3(-1.0f,  0.0f,  0.0f),
        glm::vec3( 0.0f,  1.0f,  0.0f),
        glm::vec3( 0.0f, -1.0f,  0.0f),
        glm::vec3( 0.0f,  0.0f,  1.0f),
        glm::vec3( 0.0f,  0.0f, -1.0f)
    };
    inline static const glm::vec3 kAmbientStrength{0.08f};
    inline static const glm::vec3 kDiffuseStrength{0.55f};
    inline static const glm::vec3 kSpecularStrength{0.25f};

    GLFWwindow* window{nullptr};
    bool glfwInitialized{false};

    Camera camera;
    float lastX;
    float lastY;
    bool firstMouse{true};
    float deltaTime{0.0f};
    float lastFrame{0.0f};
    bool middleMouseDownLastFrame{false};
    int lastSelectedID{-2};
    int debugDisplayMode{0};
    bool faceTexturesEnabled{true};

    glm::vec3 orbitTarget{0.0f, 3.0f, 0.0f};
    float orbitYaw{-135.0f};
    float orbitPitch{-25.0f};
    float orbitDistance{6.0f};
    float orbitMinDistance{2.0f};
    float orbitMaxDistance{20.0f};
    float orbitSensitivity{0.15f};
    float orbitZoomSpeed{0.5f};

    int framebufferWidth{static_cast<int>(SCR_WIDTH)};
    int framebufferHeight{static_cast<int>(SCR_HEIGHT)};
    int msaaSamples{8};
    bool framebufferReady{false};
    unsigned int msaaFBO{0};
    unsigned int msaaColorBuffer{0};
    unsigned int msaaDepthBuffer{0};
    unsigned int resolveFBO{0};
    unsigned int resolveColorBuffer{0};

    std::unique_ptr<Shader> cubeShader;
    std::unique_ptr<InfiniteGridRenderer> gridRenderer;
    std::unique_ptr<RubiksCube> rubiksCube;
    std::unique_ptr<Input> input;
    std::unique_ptr<MouseSelector> mouseSelector;
    std::unique_ptr<Solver> solver;
    std::unique_ptr<TPThreadPool> threadPool;
    std::vector<ID> cubieIDs;
    
    // Solve state
    std::vector<Move> pendingSolve;
    size_t solveIndex{0};
    bool solving{false};
    bool solverBusy{false};
    std::future<std::vector<Move>> solveFuture;
    
    // Debug: track CompactCube state during solve
    std::unique_ptr<CompactCube> debugCompactCube;
    CubeState debugInitialState{};
};

InfiniteGridRenderer::InfiniteGridRenderer()
    : shader("shaders/infinite_grid.vs", "shaders/infinite_grid.fs") {
    glGenVertexArrays(1, &vao);
}

InfiniteGridRenderer::~InfiniteGridRenderer() {
    if (vao != 0) {
        glDeleteVertexArrays(1, &vao);
        vao = 0;
    }
}

void InfiniteGridRenderer::render(const glm::mat4& projection, const glm::mat4& view, const glm::vec3& cameraPos) {
    shader.use();
    shader.setMat4("gVP", projection * view);
    shader.setVec3("gCameraWorldPos", cameraPos);
    shader.setFloat("gGridSize", gridSize);
    shader.setFloat("gGridMinPixelsBetweenCells", minPixelsBetweenCells);
    shader.setFloat("gGridCellSize", cellSize);
    shader.setVec4("gGridColorThin", colorThin);
    shader.setVec4("gGridColorThick", colorThick);
    shader.setFloat("gGridAlpha", alpha);

    glDepthMask(GL_FALSE);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
}

RubiksApplication::RubiksApplication()
        : camera(glm::vec3(2.0f, 3.0f, 3.0f), glm::vec3(0.0f, 1.0f, 0.0f), -135.0f, -25.0f),
            lastX(static_cast<float>(SCR_WIDTH) / 2.0f),
            lastY(static_cast<float>(SCR_HEIGHT) / 2.0f) {
        threadPool = std::make_unique<TPThreadPool>();
    orbitTarget = glm::vec3(0.0f, 6.0f, 0.0f);
    orbitYaw = camera.Yaw;
    orbitPitch = camera.Pitch;
    orbitDistance = glm::length(camera.Position - orbitTarget);
    updateOrbitCamera();
}

RubiksApplication::~RubiksApplication() {
    rubiksCube.reset();
    gridRenderer.reset();
    cubeShader.reset();
    solver.reset();
    cleanupFramebuffers();
    if (threadPool) {
        threadPool->shutdown();
        threadPool.reset();
    }

    if (window) {
        glfwDestroyWindow(window);
        window = nullptr;
    }
    if (glfwInitialized) {
        glfwTerminate();
    }
}

int RubiksApplication::run() {
    initWindow();
    initGLAD();
    initInputSystem();
    initCallbacks();
    initRenderState();
    mainLoop();
    return 0;
}

void RubiksApplication::initWindow() {
    if (!glfwInit()) {
        throw std::runtime_error("Failed to initialize GLFW");
    }
    glfwInitialized = true;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Rubiks Cube", nullptr, nullptr);
    if (window == nullptr) {
        throw std::runtime_error("Failed to create GLFW window");
    }

    glfwMakeContextCurrent(window);
    glfwSetWindowUserPointer(window, this);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}

void RubiksApplication::initInputSystem() {
    input = std::make_unique<Input>(window);
}

void RubiksApplication::initGLAD() {
    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        throw std::runtime_error("Failed to initialize GLAD");
    }
}

void RubiksApplication::initCallbacks() {
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
}

void RubiksApplication::initRenderState() {
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_MULTISAMPLE);

    cubeShader = std::make_unique<Shader>("shaders/vertex.vs", "shaders/rubiks.fs");
    cubeShader->use();
    cubeShader->setInt("texture_diffuse1", 0);
    cubeShader->setBool("material.useTexture", false);
    cubeShader->setBool("useFaceTextures", false);

    gridRenderer = std::make_unique<InfiniteGridRenderer>();
    rubiksCube = std::make_unique<RubiksCube>(
        "Rubiks/assets/RCube.obj",
        "img/textures/Rubiks Col.png",
        glm::vec3(0.0f, 6.0f, 0.0f),
        0.3f,
        0.58f);
    rubiksCube->setFaceTexturesEnabled(faceTexturesEnabled);
    rubiksCube->setTurnSounds({"Rubiks/assets/Cube Turn 1.mp3", "Rubiks/assets/Cube Turn 2.mp3"}, 55.0f);

    mouseSelector = std::make_unique<MouseSelector>(camera);
    cubieIDs.clear();
    cubieIDs.reserve(rubiksCube->getCubieOffsets().size());
    for (const auto& offset : rubiksCube->getCubieOffsets()) {
        glm::mat4 transform = rubiksCube->getCubieModelMatrix(offset);
        ID id = mouseSelector->addSelectable(rubiksCube->getModel(), transform);
        cubieIDs.push_back(id);
    }
    
    // Initialize solver (will load cached tables or generate them)
    solver = std::make_unique<Solver>(*rubiksCube, "./Rubiks/assets");

    int fbWidth = 0;
    int fbHeight = 0;
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
    recreateFramebuffers(fbWidth, fbHeight);
}

void RubiksApplication::updateOrbitCamera() {
    orbitPitch = glm::clamp(orbitPitch, -85.0f, 85.0f);
    orbitDistance = glm::clamp(orbitDistance, orbitMinDistance, orbitMaxDistance);
    float yawRad = glm::radians(orbitYaw);
    float pitchRad = glm::radians(orbitPitch);
    glm::vec3 offset;
    offset.x = orbitDistance * std::cos(pitchRad) * std::cos(yawRad);
    offset.y = orbitDistance * std::sin(pitchRad);
    offset.z = orbitDistance * std::cos(pitchRad) * std::sin(yawRad);

    camera.Position = orbitTarget + offset;
    camera.Front = glm::normalize(orbitTarget - camera.Position);
    camera.Right = glm::normalize(glm::cross(camera.Front, camera.WorldUp));
    camera.Up = glm::normalize(glm::cross(camera.Right, camera.Front));
    camera.Yaw = orbitYaw;
    camera.Pitch = orbitPitch;
}

void RubiksApplication::mainLoop() {
    while (!glfwWindowShouldClose(window)) {
        updateDeltaTime();
        if (input) {
            input->update();
            bool middlePressed = false;
            auto mb = input->getMouseButtons();
            if (auto it = mb.find("middle"); it != mb.end()) middlePressed = it->second;
            if (middlePressed && !middleMouseDownLastFrame) {
                // reset firstMouse on drag start
                firstMouse = true;
            }
            if (middlePressed) {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            } else {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }
            middleMouseDownLastFrame = middlePressed;

            // G toggles raw face colors
            if (input->isKeyDown(GLFW_KEY_G)) {
                debugDisplayMode = (debugDisplayMode == 1) ? 0 : 1;
            }
            // T toggles texture overlay
            if (input->isKeyDown(GLFW_KEY_T)) {
                faceTexturesEnabled = !faceTexturesEnabled;
                if (rubiksCube) {
                    rubiksCube->setFaceTexturesEnabled(faceTexturesEnabled);
                }
                std::cout << "[Rubiks] Face textures "
                          << (faceTexturesEnabled ? "enabled" : "disabled") << std::endl;
            }
        }
        processInput();

        // Step cube animation
        if (rubiksCube) {
            bool animationCompleted = rubiksCube->updateAnimation(deltaTime);

            // After snapping, refresh selection and debug state
            if (animationCompleted && mouseSelector) {
                glm::vec3 targetPos = rubiksCube->getLastSelectedPosition();
                int newCubieIndex = rubiksCube->findCubieAtPosition(targetPos);

                if (newCubieIndex >= 0 && newCubieIndex < static_cast<int>(cubieIDs.size())) {
                    mouseSelector->setSelection(cubieIDs[newCubieIndex]);

                    // refresh selectable transforms
                    const auto& offsets = rubiksCube->getCubieOffsets();
                    for (size_t i = 0; i < offsets.size() && i < cubieIDs.size(); ++i) {
                        glm::mat4 transform = rubiksCube->getCubieModelMatrix(offsets[i]);
                        mouseSelector->updateSelectableTransform(cubieIDs[i], transform);
                    }
                }
                
                if (solving) {
                    // Debug dump of visual state each move
                    const auto& visualState = rubiksCube->getState();
                    
                    // Convert visual state to CompactCube for comparison
                    CubeState stateArr;
                    std::copy(visualState.begin(), visualState.end(), stateArr.begin());
                    CompactCube visualCompact = solver->stateToCompact(stateArr);
                    
                    std::cout << "Visual cube after animation:\n";
                    std::cout << "  cPos: ";
                    for (int i = 0; i < 8; i++) std::cout << (int)visualCompact.cPos[i] << " ";
                    std::cout << "\n  cOri: ";
                    for (int i = 0; i < 8; i++) std::cout << (int)visualCompact.cOri[i] << " ";
                    std::cout << "\n  ePos: ";
                    for (int i = 0; i < 12; i++) std::cout << (int)visualCompact.ePos[i] << " ";
                    std::cout << "\n  eOri: ";
                    for (int i = 0; i < 12; i++) std::cout << (int)visualCompact.eOri[i] << " ";
                    std::cout << "\n";
                    
                    // Compare against solver expectation
                    if (debugCompactCube) {
                        bool match = true;
                        for (int i = 0; i < 8; i++) if (visualCompact.cPos[i] != debugCompactCube->cPos[i]) match = false;
                        for (int i = 0; i < 8; i++) if (visualCompact.cOri[i] != debugCompactCube->cOri[i]) match = false;
                        for (int i = 0; i < 12; i++) if (visualCompact.ePos[i] != debugCompactCube->ePos[i]) match = false;
                        for (int i = 0; i < 12; i++) if (visualCompact.eOri[i] != debugCompactCube->eOri[i]) match = false;
                        std::cout << "States match: " << (match ? "YES" : "NO - MISMATCH!") << "\n";
                        
                        if (!match) {
                            std::cout << "Expected cPos: ";
                            for (int i = 0; i < 8; i++) std::cout << (int)debugCompactCube->cPos[i] << " ";
                            std::cout << "\nExpected cOri: ";
                            for (int i = 0; i < 8; i++) std::cout << (int)debugCompactCube->cOri[i] << " ";
                            std::cout << "\nExpected ePos: ";
                            for (int i = 0; i < 12; i++) std::cout << (int)debugCompactCube->ePos[i] << " ";
                            std::cout << "\nExpected eOri: ";
                            for (int i = 0; i < 12; i++) std::cout << (int)debugCompactCube->eOri[i] << " ";
                            std::cout << "\n";
                        }
                    }
                    
                    playNextSolveMove();
                }
            }
        }

        if (solverBusy && solveFuture.valid()) {
            using namespace std::chrono_literals;
            if (solveFuture.wait_for(0ms) == std::future_status::ready) {
                try {
                    pendingSolve = solveFuture.get();
                } catch (const std::exception& e) {
                    std::cerr << "Solver error: " << e.what() << std::endl;
                    pendingSolve.clear();
                }
                solverBusy = false;
                solveIndex = 0;
                if (!pendingSolve.empty()) {
                    solving = true;
                    std::cout << "Solution ready (" << pendingSolve.size() << " moves)\n";
                    playNextSolveMove();
                } else {
                    solving = false;
                    std::cout << "Cube already solved.\n";
                }
            }
        }

        if (mouseSelector && input) {
            mouseSelector->handleSelection(*input, {SCR_WIDTH, SCR_HEIGHT});
        }

        GLuint targetFBO = framebufferReady ? msaaFBO : 0;
        glBindFramebuffer(GL_FRAMEBUFFER, targetFBO);
        glViewport(0, 0, framebufferWidth, framebufferHeight);
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        renderScene();

        if (framebufferReady) {
            glBindFramebuffer(GL_READ_FRAMEBUFFER, msaaFBO);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolveFBO);
            glBlitFramebuffer(0, 0, framebufferWidth, framebufferHeight,
                              0, 0, framebufferWidth, framebufferHeight,
                              GL_COLOR_BUFFER_BIT, GL_NEAREST);

            glBindFramebuffer(GL_READ_FRAMEBUFFER, resolveFBO);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
            glBlitFramebuffer(0, 0, framebufferWidth, framebufferHeight,
                              0, 0, framebufferWidth, framebufferHeight,
                              GL_COLOR_BUFFER_BIT, GL_NEAREST);
        } else {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }

        glfwSwapBuffers(window);
        if (!input) {
            glfwPollEvents();
        }
    }
}

void RubiksApplication::scramble(int depth) {
    mouseSelector->clearSelection();
    input->select(-1);
    srand(static_cast<unsigned>(time(nullptr)));

    // Faces 0-5 identify the six outer layers
    constexpr int numFaces = 6;
    std::array<float, 2> angles = { 90.0f, -90.0f };

    int lastFace = -1;

    for (int i = 0; i < depth; ++i) {
        int faceIndex;
        do {
            faceIndex = rand() % numFaces;
        } while (faceIndex == lastFace);
        lastFace = faceIndex;

        float angle = angles[rand() % 2];

        rubiksCube->queueFaceRotation(faceIndex, angle, 0.1f);

        const auto& offsets = rubiksCube->getCubieOffsets();
        for (size_t i = 0; i < offsets.size() && i < cubieIDs.size(); ++i) {
            glm::mat4 transform = rubiksCube->getCubieModelMatrix(offsets[i]);
            mouseSelector->updateSelectableTransform(cubieIDs[i], transform);
        }
    }
}

void RubiksApplication::solveCube() {
    mouseSelector->clearSelection();
    input->select(-1);
    if (!solver || !rubiksCube || solving) return;

    CubeState snapshot{};
    const auto& rawState = rubiksCube->getState();
    std::copy(rawState.begin(), rawState.end(), snapshot.begin());
    
    // Snapshot initial state for debugging
    debugInitialState = snapshot;

    std::cout << "Computing solution...\n";
    pendingSolve = solver->solveFromState(snapshot);
    solveIndex = 0;
    
    // Mirror the state into a CompactCube for debugging
    debugCompactCube = std::make_unique<CompactCube>(solver->stateToCompact(snapshot));
    std::cout << "\n=== DEBUG: Initial CompactCube state ===\n";
    std::cout << "  cPos: ";
    for (int i = 0; i < 8; i++) std::cout << (int)debugCompactCube->cPos[i] << " ";
    std::cout << "\n  cOri: ";
    for (int i = 0; i < 8; i++) std::cout << (int)debugCompactCube->cOri[i] << " ";
    std::cout << "\n  ePos: ";
    for (int i = 0; i < 12; i++) std::cout << (int)debugCompactCube->ePos[i] << " ";
    std::cout << "\n  eOri: ";
    for (int i = 0; i < 12; i++) std::cout << (int)debugCompactCube->eOri[i] << " ";
    std::cout << "\n";

    if (!pendingSolve.empty()) {
        solving = true;
        std::cout << "Solution ready (" << pendingSolve.size() << " moves)\n";
        playNextSolveMove();
    } else {
        solving = false;
        std::cout << "Cube already solved.\n";
    }
}

bool RubiksApplication::animateMove(Move move, float duration) {
    if (!rubiksCube) return false;
    int face = -1;
    float angle = 0.0f;

    // GLM uses outward normals with right-hand rule, so every CW move becomes a negative angle
    switch (move) {
        case Move::U:       face = 2; angle = -90.0f; break;
        case Move::U_PRIME: face = 2; angle = 90.0f; break;
        case Move::U2:      face = 2; angle = 180.0f; break;
        case Move::D:       face = 3; angle = -90.0f; break;
        case Move::D_PRIME: face = 3; angle = 90.0f; break;
        case Move::D2:      face = 3; angle = 180.0f; break;
        case Move::L:       face = 1; angle = -90.0f; break;
        case Move::L_PRIME: face = 1; angle = 90.0f; break;
        case Move::L2:      face = 1; angle = 180.0f; break;
        case Move::R:       face = 0; angle = -90.0f; break;
        case Move::R_PRIME: face = 0; angle = 90.0f; break;
        case Move::R2:      face = 0; angle = 180.0f; break;
        case Move::F:       face = 4; angle = -90.0f; break;
        case Move::F_PRIME: face = 4; angle = 90.0f; break;
        case Move::F2:      face = 4; angle = 180.0f; break;
        case Move::B:       face = 5; angle = -90.0f; break;
        case Move::B_PRIME: face = 5; angle = 90.0f; break;
        case Move::B2:      face = 5; angle = 180.0f; break;
        default: break;
    }

    if (face >= 0) {
        rubiksCube->startFaceRotation(face, angle, duration);
        return true;
    }
    return false;
}

void RubiksApplication::playNextSolveMove() {
    if (!rubiksCube || !solving) return;
    if (solveIndex >= pendingSolve.size()) {
        solving = false;
        std::cout << "\n=== Solve complete! ===\n";
        
        // Check if visual cube is solved
        const auto& finalState = rubiksCube->getState();
        bool visualSolved = true;
        for (int face = 0; face < 6 && visualSolved; ++face) {
            int centerColor = finalState[face * 9 + 4];
            for (int i = 0; i < 9; ++i) {
                if (finalState[face * 9 + i] != centerColor) {
                    visualSolved = false;
                    break;
                }
            }
        }
        std::cout << "Visual cube solved: " << (visualSolved ? "YES" : "NO") << "\n";
        
        // Validate CompactCube snapshot
        if (debugCompactCube) {
            bool compactSolved = true;
            for (int i = 0; i < 8; i++) if (debugCompactCube->cPos[i] != i) compactSolved = false;
            for (int i = 0; i < 12; i++) if (debugCompactCube->ePos[i] != i) compactSolved = false;
            for (int i = 0; i < 8; i++) if (debugCompactCube->cOri[i] != 0) compactSolved = false;
            for (int i = 0; i < 12; i++) if (debugCompactCube->eOri[i] != 0) compactSolved = false;
            std::cout << "CompactCube solved: " << (compactSolved ? "YES" : "NO") << "\n";
        }
        return;
    }
    
    Move move = pendingSolve[solveIndex++];
    std::cout << "\n=== Move " << solveIndex << "/" << pendingSolve.size() << ": " << solver->moveToString(move) << " ===\n";
    
    // Mirror the move into debug CompactCube
    if (debugCompactCube) {
        // Map Move enum to CompactCube notation
        char face = '?';
        int amount = 1;
        switch (move) {
            case Move::U: face = 'U'; amount = 1; break;
            case Move::U_PRIME: face = 'U'; amount = 3; break;
            case Move::U2: face = 'U'; amount = 2; break;
            case Move::D: face = 'D'; amount = 1; break;
            case Move::D_PRIME: face = 'D'; amount = 3; break;
            case Move::D2: face = 'D'; amount = 2; break;
            case Move::L: face = 'L'; amount = 1; break;
            case Move::L_PRIME: face = 'L'; amount = 3; break;
            case Move::L2: face = 'L'; amount = 2; break;
            case Move::R: face = 'R'; amount = 1; break;
            case Move::R_PRIME: face = 'R'; amount = 3; break;
            case Move::R2: face = 'R'; amount = 2; break;
            case Move::F: face = 'F'; amount = 1; break;
            case Move::F_PRIME: face = 'F'; amount = 3; break;
            case Move::F2: face = 'F'; amount = 2; break;
            case Move::B: face = 'B'; amount = 1; break;
            case Move::B_PRIME: face = 'B'; amount = 3; break;
            case Move::B2: face = 'B'; amount = 2; break;
            default: break;
        }
        debugCompactCube->applyMove(face, amount);
        
        std::cout << "CompactCube after move:\n";
        std::cout << "  cPos: ";
        for (int i = 0; i < 8; i++) std::cout << (int)debugCompactCube->cPos[i] << " ";
        std::cout << "\n  cOri: ";
        for (int i = 0; i < 8; i++) std::cout << (int)debugCompactCube->cOri[i] << " ";
        std::cout << "\n  ePos: ";
        for (int i = 0; i < 12; i++) std::cout << (int)debugCompactCube->ePos[i] << " ";
        std::cout << "\n  eOri: ";
        for (int i = 0; i < 12; i++) std::cout << (int)debugCompactCube->eOri[i] << " ";
        std::cout << "\n";
    }
    
    if (!animateMove(move)) {
        solving = false;
    }
}

void RubiksApplication::updateDeltaTime() {
    float currentFrame = static_cast<float>(glfwGetTime());
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;
}

void RubiksApplication::processInput() {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }

    if (input->isKeyDown(GLFW_KEY_P))
        scramble(50);
    if (input->isKeyDown(GLFW_KEY_O) && !solving)
        solveCube();

    // Keys 1-6 trigger U/D/R/L/F/B test turns via animateMove to match solver mapping
    if (rubiksCube && !rubiksCube->isAnimating() && input) {
        // 1 = U (Up CW)
        if (input->isKeyDown(GLFW_KEY_1)) {
            std::cout << "Test: U move (Up CW)\n";
            animateMove(Move::U, 0.3f);
        }
        // 2 = D (Down CW)
        if (input->isKeyDown(GLFW_KEY_2)) {
            std::cout << "Test: D move (Down CW)\n";
            animateMove(Move::D, 0.3f);
        }
        // 3 = R (Right CW)
        if (input->isKeyDown(GLFW_KEY_3)) {
            std::cout << "Test: R move (Right CW)\n";
            animateMove(Move::R, 0.3f);
        }
        // 4 = L (Left CW)
        if (input->isKeyDown(GLFW_KEY_4)) {
            std::cout << "Test: L move (Left CW)\n";
            animateMove(Move::L, 0.3f);
        }
        // 5 = F (Front CW)
        if (input->isKeyDown(GLFW_KEY_5)) {
            std::cout << "Test: F move (Front CW)\n";
            animateMove(Move::F, 0.3f);
        }
        // 6 = B (Back CW)
        if (input->isKeyDown(GLFW_KEY_6)) {
            std::cout << "Test: B move (Back CW)\n";
            animateMove(Move::B, 0.3f);
        }
    }

    // Arrow keys rotate faces relative to the current selection
    if (rubiksCube && !rubiksCube->isAnimating() && input) {
        int selectedID = -1;
        if (mouseSelector) {
            if (auto s = mouseSelector->getSelection(); s) {
                selectedID = static_cast<int>(*s);
            }
        }

        // Retrieve the selected cubie's offset
        glm::vec3 selectedOffset(0.0f);
        bool hasSelection = false;
        if (selectedID >= 0 && selectedID < static_cast<int>(cubieIDs.size())) {
            for (size_t i = 0; i < cubieIDs.size(); ++i) {
                if (static_cast<int>(cubieIDs[i]) == selectedID) {
                    selectedOffset = rubiksCube->getCubieOffsets()[i];
                    hasSelection = true;
                    break;
                }
            }
        }

        if (hasSelection) {
            glm::vec3 camForward = camera.Front;
            glm::vec3 camRight = camera.Right;
            glm::vec3 camUp = camera.Up;
            glm::vec3 worldUp{0.0f, 1.0f, 0.0f};

            bool leftPressed = input->isKeyDown(GLFW_KEY_LEFT);
            bool rightPressed = input->isKeyDown(GLFW_KEY_RIGHT);
            bool upPressed = input->isKeyDown(GLFW_KEY_UP);
            bool downPressed = input->isKeyDown(GLFW_KEY_DOWN);

            bool invertedView = glm::dot(camUp, worldUp) < 0.0f;
            if (invertedView) {
                std::swap(leftPressed, rightPressed);
                std::swap(upPressed, downPressed);
            }

            glm::vec3 cameraOffset = camera.Position - orbitTarget;
            bool onRedOrangeSide = std::abs(cameraOffset.x) > std::abs(cameraOffset.z);
            bool facingYellow = glm::dot(camForward, worldUp) > 0.2f;
            bool facingWhite = glm::dot(camForward, worldUp) < -0.2f;
            if (facingYellow && onRedOrangeSide) {
                // Viewing yellow face from the ±X sides should feel like white controls, but fully inverted
                std::swap(leftPressed, rightPressed);
                std::swap(upPressed, downPressed);
            }

            bool onOrangeSide = (cameraOffset.x < 0.0f) && (std::abs(cameraOffset.x) > std::abs(cameraOffset.z));
            if (onOrangeSide && (facingYellow || facingWhite)) {
                // Looking toward yellow/white from orange face feels flipped vertically; correct it explicitly
                std::swap(upPressed, downPressed);
            }

            bool onGreenSide = (cameraOffset.z > 0.0f) && (std::abs(cameraOffset.z) > std::abs(cameraOffset.x));
            if (onGreenSide && facingYellow) {
                // From the green face while looking up at yellow, keep vertical arrows matching white orientation
                std::swap(upPressed, downPressed);
            }

            bool onBlueSide = (cameraOffset.z < 0.0f) && (std::abs(cameraOffset.z) > std::abs(cameraOffset.x));
            if (onBlueSide && facingWhite) {
                // From the blue face while looking down at white, keep vertical arrows matching baseline
                std::swap(upPressed, downPressed);
            }

            int x = static_cast<int>(std::round(selectedOffset.x));
            int y = static_cast<int>(std::round(selectedOffset.y));
            int z = static_cast<int>(std::round(selectedOffset.z));

            int faceIndex = -1;
            float angle = 90.0f;

            // Determine which world axis the camera favors
            float dotY = std::abs(camForward.y);
            float dotXZ = std::sqrt(camForward.x * camForward.x + camForward.z * camForward.z);
            bool lookingVertical = dotY > dotXZ;  // Looking from above or below

            if (upPressed || downPressed) {
                // Up/Down rotate around the camera's right axis
                float dotX = std::abs(camRight.x);
                float dotZ = std::abs(camRight.z);

                if (dotX > dotZ) {
                    // Camera right aligns with X, so rotate around X
                    if (x == 1) faceIndex = 0;
                    else if (x == -1) faceIndex = 1;
                    else faceIndex = 6;

                    angle = (camForward.z > 0) ? 90.0f : -90.0f;
                    if (camForward.y > 0.5f) angle = 90.0f;   // Looking down from above
                    if (camForward.y < -0.5f) angle = -90.0f;// Looking up from below 
                    if (faceIndex == 1) angle = -angle;
                } else {
                    // Camera right aligns with Z, so rotate around Z
                    if (z == 1) faceIndex = 4;
                    else if (z == -1) faceIndex = 5;
                    else faceIndex = 8;

                    angle = (camForward.x > 0) ? -90.0f : 90.0f;
                    if (camForward.y > 0.5f) angle = -90.0f;  // Looking down from above
                    if (camForward.y < -0.5f) angle = 90.0f;  // Looking up from below
                    if (faceIndex == 5) angle = -angle;
                }

                if (downPressed) {
                    angle = -angle;
                }
            }
            else if (leftPressed || rightPressed) {
                if (lookingVertical) {
                    // Looking from above/below: Left/Right rotates the horizontal slices
                    float dotX = std::abs(camUp.x);
                    float dotZ = std::abs(camUp.z);

                    if (dotX > dotZ) {
                        // Camera up aligns with X, so rotate around X
                        if (x == 1) faceIndex = 0;
                        else if (x == -1) faceIndex = 1;
                        else faceIndex = 6;

                        angle = (camForward.y > 0) ? 90.0f : -90.0f;
                        if (camUp.x < 0.0f) angle = -angle; // keep horizontal controls consistent when screen-up points toward -X
                        if (faceIndex == 1) angle = -angle;
                    } else {
                        // Camera up aligns with Z, so rotate around Z
                        if (z == 1) faceIndex = 4;
                        else if (z == -1) faceIndex = 5;
                        else faceIndex = 8;

                        angle = (camForward.y > 0) ? -90.0f : 90.0f;
                        if (camUp.z < 0.0f) angle = -angle; // same idea for Z aligned views
                        if (faceIndex == 5) angle = -angle;
                    }

                    if (rightPressed) {
                        angle = -angle;
                    }
                } else {
                    // From the side, Left/Right rotates Y slices
                    if (y == 1) faceIndex = 2;
                    else if (y == -1) faceIndex = 3;
                    else faceIndex = 7;

                    angle = -90.0f;
                    if (faceIndex == 3) angle = -angle;
                    if (rightPressed) angle = -angle;
                }
            }

            if (faceIndex >= 0) {
                rubiksCube->setLastSelectedPosition(selectedOffset);
                rubiksCube->startFaceRotation(faceIndex, angle, 0.25f);
            }
        }
    }
}

void RubiksApplication::renderScene() {
    float aspect = static_cast<float>(framebufferWidth) / static_cast<float>(framebufferHeight);
    glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom),
                                            aspect,
                                            0.1f,
                                            100.0f);
    glm::mat4 view = camera.GetViewMatrix();

    if (gridRenderer) {
        gridRenderer->render(projection, view, camera.Position);
    }

    if (!cubeShader || !rubiksCube) {
        return;
    }

    cubeShader->use();
    cubeShader->setVec3("material.ambient", glm::vec3(0.3f));
    cubeShader->setVec3("material.diffuse", glm::vec3(1.0f));
    cubeShader->setVec3("material.specular", glm::vec3(0.3f));
    cubeShader->setFloat("material.shininess", 32.0f);
    cubeShader->setVec3("lightColor", glm::vec3(1.0f));
    cubeShader->setVec3("viewPos", camera.Position);

    cubeShader->setInt("dirLightCount", static_cast<int>(kAxisDirections.size()));
    for (int i = 0; i < static_cast<int>(kAxisDirections.size()); ++i) {
        std::string baseName = "dirLights[" + std::to_string(i) + "]";
        cubeShader->setVec3(baseName + ".direction", kAxisDirections[i]);
        cubeShader->setVec3(baseName + ".ambient", kAmbientStrength);
        cubeShader->setVec3(baseName + ".diffuse", kDiffuseStrength);
        cubeShader->setVec3(baseName + ".specular", kSpecularStrength);
    }

    cubeShader->setMat4("projection", projection);
    cubeShader->setMat4("view", view);
    cubeShader->setInt("debugDisplayMode", debugDisplayMode);

    int sel = -1;
    if (mouseSelector) {
        if (auto s = mouseSelector->getSelection(); s) sel = static_cast<int>(*s);
    }
    cubeShader->setInt("selectedID", sel);
    if (sel != lastSelectedID) {
        if (sel >= 0) std::cout << "Selected ID: " << sel << std::endl;
        else std::cout << "Selection cleared" << std::endl;
        lastSelectedID = sel;
    }
    rubiksCube->applyMaterial(*cubeShader);
    rubiksCube->draw(*cubeShader, &cubieIDs);
}

void RubiksApplication::onFramebufferResize(int width, int height) {
    recreateFramebuffers(width, height);
}

void RubiksApplication::onMouseMove(double xposIn, double yposIn) {
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS) {
        float xoffset = xpos - lastX;
        float yoffset = lastY - ypos;
        orbitYaw += xoffset * orbitSensitivity;
        orbitPitch += yoffset * orbitSensitivity;
        updateOrbitCamera();
    }

    lastX = xpos;
    lastY = ypos;
}

void RubiksApplication::onScroll(double yoffset) {
    orbitDistance -= static_cast<float>(yoffset) * orbitZoomSpeed;
    updateOrbitCamera();
}

void RubiksApplication::framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    if (auto* app = static_cast<RubiksApplication*>(glfwGetWindowUserPointer(window))) {
        app->onFramebufferResize(width, height);
    }
}

void RubiksApplication::cleanupFramebuffers() {
    if (msaaDepthBuffer != 0) {
        glDeleteRenderbuffers(1, &msaaDepthBuffer);
        msaaDepthBuffer = 0;
    }
    if (msaaColorBuffer != 0) {
        glDeleteTextures(1, &msaaColorBuffer);
        msaaColorBuffer = 0;
    }
    if (msaaFBO != 0) {
        glDeleteFramebuffers(1, &msaaFBO);
        msaaFBO = 0;
    }
    if (resolveColorBuffer != 0) {
        glDeleteTextures(1, &resolveColorBuffer);
        resolveColorBuffer = 0;
    }
    if (resolveFBO != 0) {
        glDeleteFramebuffers(1, &resolveFBO);
        resolveFBO = 0;
    }
    framebufferReady = false;
}

void RubiksApplication::recreateFramebuffers(int width, int height) {
    framebufferWidth = std::max(1, width);
    framebufferHeight = std::max(1, height);

    cleanupFramebuffers();

    glGenFramebuffers(1, &msaaFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, msaaFBO);

    glGenTextures(1, &msaaColorBuffer);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msaaColorBuffer);
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, msaaSamples, GL_RGBA8,
                            framebufferWidth, framebufferHeight, GL_TRUE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D_MULTISAMPLE, msaaColorBuffer, 0);

    glGenRenderbuffers(1, &msaaDepthBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, msaaDepthBuffer);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, msaaSamples, GL_DEPTH24_STENCIL8,
                                     framebufferWidth, framebufferHeight);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                              GL_RENDERBUFFER, msaaDepthBuffer);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "MSAA framebuffer incomplete" << std::endl;
        cleanupFramebuffers();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return;
    }

    glGenFramebuffers(1, &resolveFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, resolveFBO);

    glGenTextures(1, &resolveColorBuffer);
    glBindTexture(GL_TEXTURE_2D, resolveColorBuffer);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, framebufferWidth, framebufferHeight,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, resolveColorBuffer, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Resolve framebuffer incomplete" << std::endl;
        cleanupFramebuffers();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    framebufferReady = true;
}

void RubiksApplication::mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    if (auto* app = static_cast<RubiksApplication*>(glfwGetWindowUserPointer(window))) {
        app->onMouseMove(xpos, ypos);
    }
}

void RubiksApplication::scroll_callback(GLFWwindow* window, double /*xoffset*/, double yoffset) {
    if (auto* app = static_cast<RubiksApplication*>(glfwGetWindowUserPointer(window))) {
        app->onScroll(yoffset);
    }
}

} // namespace

int main() {
    try {
        RubiksApplication app;
        return app.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return -1;
    }
}