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

#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

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
    void onMouseMove(double xpos, double ypos);
    void onScroll(double yoffset);

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

    std::unique_ptr<Shader> cubeShader;
    std::unique_ptr<InfiniteGridRenderer> gridRenderer;
    std::unique_ptr<RubiksCube> rubiksCube;
    std::unique_ptr<Input> input;
    std::unique_ptr<MouseSelector> mouseSelector;
    std::vector<ID> cubieIDs;
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
    : camera(glm::vec3(3.0f, 3.0f, 5.0f), glm::vec3(0.0f, 1.0f, 0.0f), -135.0f, -25.0f),
      lastX(static_cast<float>(SCR_WIDTH) / 2.0f),
      lastY(static_cast<float>(SCR_HEIGHT) / 2.0f) {}

RubiksApplication::~RubiksApplication() {
    rubiksCube.reset();
    gridRenderer.reset();
    cubeShader.reset();

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

    window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Rubik's Cube", nullptr, nullptr);
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

    cubeShader = std::make_unique<Shader>("shaders/vertex.vs", "shaders/rubiks.fs");
    cubeShader->use();
    cubeShader->setInt("texture_diffuse1", 0);
    cubeShader->setBool("material.useTexture", false);
    cubeShader->setBool("useFaceTextures", false);

    gridRenderer = std::make_unique<InfiniteGridRenderer>();
    rubiksCube = std::make_unique<RubiksCube>(
        "Rubiks/RCube.obj",
        "img/textures/Rubiks Col.png",
        glm::vec3(0.0f, 3.0f, 0.0f),
        0.3f,
        0.58f);
    rubiksCube->setFaceTexturesEnabled(faceTexturesEnabled);

    mouseSelector = std::make_unique<MouseSelector>(camera);
    cubieIDs.clear();
    cubieIDs.reserve(rubiksCube->getCubieOffsets().size());
    for (const auto& offset : rubiksCube->getCubieOffsets()) {
        glm::mat4 transform = rubiksCube->getCubieModelMatrix(offset);
        ID id = mouseSelector->addSelectable(rubiksCube->getModel(), transform);
        cubieIDs.push_back(id);
    }
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
                // entering drag mode, reset firstMouse to avoid jump on first movement
                firstMouse = true;
            }
            if (middlePressed) {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            } else {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                }
                middleMouseDownLastFrame = middlePressed;

                // Toggle raw base-color display (no lighting) with G
                if (input->isKeyDown(GLFW_KEY_G)) {
                    debugDisplayMode = (debugDisplayMode == 1) ? 0 : 1;
                }
                // Toggle sticker face textures with T
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
        
        // Update Rubik's cube animation
        if (rubiksCube) {
            bool animationCompleted = rubiksCube->updateAnimation(deltaTime);
            
            // After rotation completes, update selection to the cubie now at the previously selected position
            if (animationCompleted && mouseSelector) {
                glm::vec3 targetPos = rubiksCube->getLastSelectedPosition();
                int newCubieIndex = rubiksCube->findCubieAtPosition(targetPos);
                
                if (newCubieIndex >= 0 && newCubieIndex < static_cast<int>(cubieIDs.size())) {
                    // Update selection to the new cubie at the same position
                    mouseSelector->setSelection(cubieIDs[newCubieIndex]);
                    
                    // Update all selectable transforms to match new cubie positions
                    const auto& offsets = rubiksCube->getCubieOffsets();
                    for (size_t i = 0; i < offsets.size() && i < cubieIDs.size(); ++i) {
                        glm::mat4 transform = rubiksCube->getCubieModelMatrix(offsets[i]);
                        mouseSelector->updateSelectableTransform(cubieIDs[i], transform);
                    }
                }
            }
        }
        
        if (mouseSelector && input) {
            mouseSelector->handleSelection(*input, {SCR_WIDTH, SCR_HEIGHT});
        }

        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        renderScene();

        glfwSwapBuffers(window);
        if (!input) {
            glfwPollEvents();
        }
    }
}

void RubiksApplication::scramble(int depth){
    Camera scrambleCam;
    std::array<glm::vec3, 6> camPos = {
        glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 0.0f, -1.0f)
    };

    

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

    // Handle Rubik's cube face rotations with arrow keys
    if (rubiksCube && !rubiksCube->isAnimating() && input) {
        int selectedID = -1;
        if (mouseSelector) {
            if (auto s = mouseSelector->getSelection(); s) {
                selectedID = static_cast<int>(*s);
            }
        }

        // Find the cubie offset for the selected ID
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
            // Camera vectors for determining visual directions
            glm::vec3 camForward = camera.Front;  // Where camera looks
            glm::vec3 camRight = camera.Right;    // Camera's right direction
            glm::vec3 camUp = camera.Up;          // Camera's up direction

            // Round offset to nearest integer for comparison
            int x = static_cast<int>(std::round(selectedOffset.x));
            int y = static_cast<int>(std::round(selectedOffset.y));
            int z = static_cast<int>(std::round(selectedOffset.z));

            int faceIndex = -1;
            float angle = 90.0f;

            if (input->isKeyDown(GLFW_KEY_UP) || input->isKeyDown(GLFW_KEY_DOWN)) {
                // UP/DOWN: Rotate around an axis parallel to camera's RIGHT vector
                // This makes pieces move up/down visually
                
                // Find which world axis camera.Right is most aligned with
                float dotX = std::abs(camRight.x);
                float dotZ = std::abs(camRight.z);
                
                if (dotX > dotZ) {
                    // Camera right is along X, so rotate around X axis
                    // Use X-slices: Right(0), Left(1), M(6)
                    if (x == 1) faceIndex = 0;
                    else if (x == -1) faceIndex = 1;
                    else faceIndex = 6;
                    
                    // Rotation direction: positive X rotation moves +Y toward +Z
                    // For "UP" to move pieces upward visually, we need to consider
                    // which way the camera is looking (camForward)
                    // If looking toward -Z (front face), UP should rotate negatively around X
                    // If looking toward +Z (back face), UP should rotate positively around X
                    angle = (camForward.z > 0) ? 90.0f : -90.0f;
                    
                    // Also need to flip based on which side of the X axis we're rotating
                    // Right face (x=1) rotates with positive X axis
                    // Left face (x=-1) rotates with negative X axis (per kFacePivots)
                    // But M slice (x=0) uses positive X axis
                    if (faceIndex == 1) angle = -angle;  // Left face has inverted axis
                    
                } else {
                    // Camera right is along Z, so rotate around Z axis
                    // Use Z-slices: Front(4), Back(5), S(8)
                    if (z == 1) faceIndex = 4;
                    else if (z == -1) faceIndex = 5;
                    else faceIndex = 8;
                    
                    // Rotation direction: positive Z rotation moves +X toward +Y
                    // If looking toward -X, UP should rotate positively around Z
                    // If looking toward +X (red face), UP should rotate negatively around Z
                    angle = (camForward.x > 0) ? -90.0f : 90.0f;
                    
                    // Back face has inverted axis
                    if (faceIndex == 5) angle = -angle;
                }
                
                // Flip for DOWN key
                if (input->isKeyDown(GLFW_KEY_DOWN)) {
                    angle = -angle;
                }
            }
            else if (input->isKeyDown(GLFW_KEY_LEFT) || input->isKeyDown(GLFW_KEY_RIGHT)) {
                // LEFT/RIGHT: Rotate around Y axis (world up)
                // Use Y-slices: Up(2), Down(3), E(7)
                if (y == 1) faceIndex = 2;
                else if (y == -1) faceIndex = 3;
                else faceIndex = 7;
                
                angle = -90.0f;
                
                if (faceIndex == 3) angle = -angle;
                
                // Flip for RIGHT key
                if (input->isKeyDown(GLFW_KEY_RIGHT)) {
                    angle = -angle;
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
    glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom),
                                            static_cast<float>(SCR_WIDTH) / static_cast<float>(SCR_HEIGHT),
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

    // Provide the cubie ID mapping so the shader can highlight selected cubie
    int sel = -1;
    if (mouseSelector) {
        if (auto s = mouseSelector->getSelection(); s) sel = static_cast<int>(*s);
    }
    cubeShader->setInt("selectedID", sel);
    // Debug output: print when selection changes
    if (sel != lastSelectedID) {
        if (sel >= 0) std::cout << "Selected ID: " << sel << std::endl;
        else std::cout << "Selection cleared" << std::endl;
        lastSelectedID = sel;
    }
    rubiksCube->applyMaterial(*cubeShader);
    rubiksCube->draw(*cubeShader, &cubieIDs);
}

void RubiksApplication::onFramebufferResize(int width, int height) {
    glViewport(0, 0, width, height);
}

void RubiksApplication::onMouseMove(double xposIn, double yposIn) {
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    // Only process camera movement while middle mouse is held (query GLFW directly to avoid races)
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS) {
        float xoffset = xpos - lastX;
        float yoffset = lastY - ypos;
        lastX = xpos;
        lastY = ypos;
        camera.ProcessMouseMovement(xoffset, yoffset);
    } else {
        // update last positions so we don't get a jump when entering drag
        lastX = xpos;
        lastY = ypos;
    }
}

void RubiksApplication::onScroll(double yoffset) {
    camera.ProcessMouseScroll(static_cast<float>(yoffset));
}

void RubiksApplication::framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    if (auto* app = static_cast<RubiksApplication*>(glfwGetWindowUserPointer(window))) {
        app->onFramebufferResize(width, height);
    }
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