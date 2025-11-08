#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <stb_image.h>

#include "../shader.h"
#include "../camera.h"
#include "CubeMarching.h"
#include <iostream>
#include "../geometry/sphere.h"
#include "../model.h"
#include "../audio/audio.h"
#include <cmath>

class WireCube {
public:
    unsigned int VAO = 0, VBO = 0, EBO = 0;
    WireCube() { }
    void create(float x, float y, float z) {
        float hx = x * 0.5f, hy = y * 0.5f, hz = z * 0.5f;
        float verts[] = {
            -hx, -hy, -hz,
             hx, -hy, -hz,
             hx,  hy, -hz,
            -hx,  hy, -hz,
            -hx, -hy,  hz,
             hx, -hy,  hz,
             hx,  hy,  hz,
            -hx,  hy,  hz,
        };
        unsigned int inds[] = {
            0,1, 1,2, 2,3, 3,0,
            4,5, 5,6, 6,7, 7,4,
            0,4, 1,5, 2,6, 3,7
        };
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(inds), inds, GL_STATIC_DRAW);
        glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);
        glEnableVertexAttribArray(0);
        glBindVertexArray(0);
    }
    void render() {
        glBindVertexArray(VAO);
        glDrawElements(GL_LINES, 24, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }
};

// Callbacks and globals
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow *window);

// Settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

// Camera
Camera camera(glm::vec3(-4.0f, 2.0f, 45.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// Timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// Marching Cubes globals
CubeMarching mc;
Mesh marchingCubesMesh;
int res = 16; // lower for stepwise view
float isolevel = 0.0f;

// Fixed world-space parameters (independent of resolution)
const float worldSize = 20.0f;        // Fixed world size in units
const float sphereRadius = 6.0f;      // Fixed sphere radius in world units

// Stepping & control flags
bool stepNext = false;      // single-step (N or Right Arrow)
bool stepRight = false;     // Right arrow single-step
bool generateAll = false;   // Generate full mesh (G)
bool clearMesh = false;     // Clear mesh (C)
bool autoStep = true;       // auto-advance through cells
bool showNormals = false;   // toggle normal visualization (H)
bool showChunkBox = true;   // toggle chunk box visibility (B)
bool wireFrame = false;    // toggle wireframe mode (W)

// State tracking for generation blocking
bool canGenerate = true;    // Allow generation (blocked after G until C is pressed)

WireCube chunkWire;
WireCube wireCube;

Audio audio;

unsigned int createSimpleShaderProgram(const char* vsSource, const char* fsSource);
unsigned int loadTexture(const char* path);

int main() {
    if (!glfwInit()) return -1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Marching Cubes Stepwise", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    glEnable(GL_DEPTH_TEST);
    glLineWidth(1.5f); // Make wireframe lines a bit thicker

     if (!audio.load("audio/beep.wav")) {
        std::cerr << "Failed to load audio file!" << std::endl;
    }

    // Shaders
    Shader shader("shaders/vertex.vs", "shaders/simple_fragment.fs");
    Shader texturedShader("shaders/vertex.vs", "shaders/textured_fragment.fs");
    Shader infiniteGridShader("shaders/infinite_grid.vs", "shaders/infinite_grid.fs");

    // Load texture
    unsigned int diffuseTexture = loadTexture("img/textures/emoji.png");
    
    unsigned int gridVAO;
    glGenVertexArrays(1, &gridVAO);
    
    // --- FIX: Create a dedicated simple shader for the wireframe cube ---
    const char* wire_vs = R"glsl(
        #version 330 core
        layout (location = 0) in vec3 aPos;
        uniform mat4 model;
        uniform mat4 view;
        uniform mat4 projection;
        void main() {
            gl_Position = projection * view * model * vec4(aPos, 1.0);
        }
    )glsl";
    const char* wire_fs = R"glsl(
        #version 330 core
        out vec4 FragColor;
        uniform vec3 color;
        void main() {
            FragColor = vec4(color, 1.0);
        }
    )glsl";
    unsigned int wireframeShaderProgram = createSimpleShaderProgram(wire_vs, wire_fs);

    // Normal debug shader using proper shader files
    Shader normalDebugShader("shaders/normal_debug.vs", "shaders/normal_debug.fs", "shaders/normal_debug.gs");

    // showNormals is a global flag toggled by H


    // Setup marching cubes - create a test scalar field
    // Generate a simple sphere-like field for demonstration
    std::vector<std::vector<std::vector<float>>> scalarField(res, std::vector<std::vector<float>>(res, std::vector<float>(res, 0.0f)));
    
    // Calculate grid scale and sphere parameters
    const float gridScale = worldSize / res; // Scale factor from grid to world    
    float center = res * 0.5f;              // Center in grid coordinates
    float radiusInGrid = sphereRadius / gridScale; // Convert world radius to grid coordinates
    
    for (int z = 0; z < res; z++) {
        for (int y = 0; y < res; y++) {
            for (int x = 0; x < res; x++) {
                float dx = x - center;
                float dy = y - center; 
                float dz = z - center;
                float distance = std::sqrt(dx*dx + dy*dy + dz*dz);
                scalarField[z][y][x] = radiusInGrid - distance; // positive inside sphere
            }
        }
    }

    // Wire cube for the marching grid bounds
    wireCube.create(1.0f, 1.0f, 1.0f);

    // Normal visualization will use the mesh directly via geometry shader

    // Set grid bounds for visualization
    int minX = 0, minY = 0, minZ = 0;
    int maxX = res-1, maxY = res-1, maxZ = res-1;
    glm::vec3 chunkSize = glm::vec3((float)res, (float)res, (float)res);
    chunkWire.create(1.0f, 1.0f, 1.0f);

    // stepping state (start at min coords)
    int gx = minX, gy = minY, gz = minZ;
    double lastStepTime = glfwGetTime();
    double stepInterval = 0.001; // seconds

    // Render loop
    while (!glfwWindowShouldClose(window)) {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        // --- Improved mesh generation & stepping logic ---
        bool meshNeedsUpdate = false;
        double now = glfwGetTime();

        // Clear mesh when C is pressed
        if (clearMesh) {
            mc.clearMesh(); // clear the marching cubes mesh
            marchingCubesMesh = Mesh(); // clear the rendering mesh
            gx = minX; gy = minY; gz = minZ; // reset stepping
            meshNeedsUpdate = true;
            clearMesh = false;
        }

        // Generate full mesh when G is pressed
        if (generateAll) {
            mc.clearMesh();
            mc.generateMesh(scalarField, isolevel);
            meshNeedsUpdate = true;
            generateAll = false;
        }

        // Single-step requests (N key or Right arrow) - process single cube
        if (stepNext || stepRight) {
            // Process the current cube and add it to the existing mesh
            mc.processSingleCube(scalarField, gx, gy, gz, isolevel);
            meshNeedsUpdate = true;
            stepNext = stepRight = false;

            // advance to next cell
            gx++;
            if (gx > maxX) { 
                gx = minX; 
                gy++;
                if (gy > maxY) {
                    gy = minY;
                    gz++;
                    if (gz > maxZ) {
                        gz = minZ;
                        // Only clear when we've completed ALL cells and are starting over
                        mc.clearMesh();
                        canGenerate = true; // Allow new generation after stepping through all
                    }
                }
            }
        }
        float distanceToEnd = std::sqrt((gx - maxX)*(gx - maxX) + (gy - maxY)*(gy - maxY) + (gz - maxZ)*(gz - maxZ));
        
        float normalizedDistance = glm::clamp(distanceToEnd / (res * 1.732f), 0.0f, 1.0f);
        float progress = 1.0f - normalizedDistance;
        
        // Exponential curve: starts slow, accelerates dramatically toward the end
        float exponentialCurve = std::pow(progress, 3.0f); // Cubic curve for dramatic effect
        
        // Alternative curves (comment/uncomment to try different effects):
       // float sineWave = 0.5f * (1.0f + std::sin(progress * 3.14159f - 3.14159f/2.0f)); // Smooth S-curve
       // float logarithmic = std::log(1.0f + progress * (std::exp(1.0f) - 1.0f)); // Starts fast, slows down

        audio.setPitch(1.0f + exponentialCurve * 4.0f);
        // Auto-step timer - process one cube at a time
        if (autoStep && now - lastStepTime > stepInterval) {
            lastStepTime = now;
            mc.processSingleCube(scalarField, gx, gy, gz, isolevel);
            meshNeedsUpdate = true;
            gx++;
            if (gx > maxX) { 
                gx = minX; 
                gy++; 
                if (gy > maxY) { 
                    gy = minY; 
                    gz++; 
                    if (gz > maxZ) { 
                        gz = minZ;
                        // Only clear when we've completed ALL cells and are starting over
                        mc.clearMesh();
                        canGenerate = true;
                    }
                }
            }
            audio.play();
        }

        if (meshNeedsUpdate) {
            const auto& vertices = mc.getVertices();
            const auto& indices = mc.getIndices();
            
            if (!vertices.empty() && !indices.empty()) {
                // Vertices are already in the correct Mesh format!
                std::vector<unsigned int> meshIndices(indices.begin(), indices.end());
                std::vector<Texture> textures; // empty textures
                
                marchingCubesMesh = Mesh(vertices, meshIndices, textures);
            }
        }

        glClearColor(0.9f, 0.92f, 0.95f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Render infinite grid FIRST (so spheres render on top)
        infiniteGridShader.use();
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 1000.0f);
        glm::mat4 view = camera.GetViewMatrix();
        // Set up view-projection matrix for the grid
        glm::mat4 viewProjection = projection * view;
        infiniteGridShader.setMat4("gVP", viewProjection);
        infiniteGridShader.setVec3("gCameraWorldPos", camera.Position);
        
        // Set grid parameters
        infiniteGridShader.setFloat("gGridSize", 100.0f);
        infiniteGridShader.setFloat("gGridMinPixelsBetweenCells", 2.0f);
        infiniteGridShader.setFloat("gGridCellSize", 0.025f);
        infiniteGridShader.setVec4("gGridColorThin", glm::vec4(0.5f, 0.5f, 0.5f, 1.0f));
        infiniteGridShader.setVec4("gGridColorThick", glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
        infiniteGridShader.setFloat("gGridAlpha", 0.5f);

        glDepthMask(GL_FALSE);

        // Bind VAO and render the infinite grid as triangles
        glBindVertexArray(gridVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glDepthMask(GL_TRUE);

        // Set up matrices - use the global world size constants
        const float gridScale = worldSize / res; // Calculate grid scale

        shader.use();
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::scale(model, glm::vec3(gridScale));
        model = glm::translate(model, glm::vec3(-res * 0.5f, -res * 0.5f, -res * 0.5f));

        // --- Render marching cubes mesh ---
        const auto &verts = mc.getVertices();
        if (!verts.empty()) {
            texturedShader.use();
            texturedShader.setMat4("projection", projection);
            texturedShader.setMat4("view", view);
            texturedShader.setMat4("model", model);
            texturedShader.setVec3("viewPos", camera.Position);
            texturedShader.setFloat("material.shininess", 32.0f);
            texturedShader.setVec3("material.ambient", glm::vec3(0.3f, 0.5f, 0.8f));
            texturedShader.setVec3("material.specular", glm::vec3(0.8f));
            texturedShader.setVec3("dirLight.direction", glm::vec3(-0.2f, -1.0f, -0.3f));
            texturedShader.setVec3("dirLight.ambient", glm::vec3(0.5f));
            texturedShader.setVec3("dirLight.diffuse", glm::vec3(0.8f));
            texturedShader.setVec3("dirLight.specular", glm::vec3(1.0f));
            
            // Bind texture
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, diffuseTexture);
            texturedShader.setInt("material_diffuse", 0);
            
            if (wireFrame) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            else glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            marchingCubesMesh.Draw(texturedShader);
        }

    // Render wire cube
    glUseProgram(wireframeShaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(wireframeShaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
    glUniformMatrix4fv(glGetUniformLocation(wireframeShaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(wireframeShaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniform3f(glGetUniformLocation(wireframeShaderProgram, "color"), 0.1f, 0.1f, 0.1f);

    // Render the unit wire cube at the current cell center (with same scaling as mesh)
    glm::mat4 cellModel = glm::mat4(1.0f);
    cellModel = glm::scale(cellModel, glm::vec3(gridScale));
    glm::vec3 gridOffset = glm::vec3(-res * 0.5f, -res * 0.5f, -res * 0.5f);
    glm::vec3 gridCenter = glm::vec3((float)gx + 0.5f, (float)gy + 0.5f, (float)gz + 0.5f);
    cellModel = glm::translate(cellModel, gridOffset + gridCenter);
    glUniformMatrix4fv(glGetUniformLocation(wireframeShaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(cellModel));
    wireCube.render();

    // Draw normals if requested (render as yellow lines from vertex positions)
    if (showNormals && !verts.empty()) {
        normalDebugShader.use();
        normalDebugShader.setMat4("projection", projection);
        normalDebugShader.setMat4("view", view);
        normalDebugShader.setMat4("model", model);
        normalDebugShader.setFloat("normalLength", 0.05f);
        
        // Render the mesh as triangles for normal visualization
        marchingCubesMesh.Draw(normalDebugShader);
    }

    // Draw chunk bounds wireframe (toggleable with B)
    if (showChunkBox) {
    // Calculate chunk model: position chunk center in grid space and scale to chunkSize (with world scaling)
    glm::mat4 chunkModel = glm::mat4(1.0f);
    chunkModel = glm::scale(chunkModel, glm::vec3(gridScale));
    glm::vec3 chunkCenter = glm::vec3((float)minX + chunkSize.x * 0.5f, (float)minY + chunkSize.y * 0.5f, (float)minZ + chunkSize.z * 0.5f);
    chunkModel = glm::translate(chunkModel, gridOffset + chunkCenter);
    chunkModel = glm::scale(chunkModel, chunkSize);
    glUniformMatrix4fv(glGetUniformLocation(wireframeShaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(chunkModel));
    glUniform3f(glGetUniformLocation(wireframeShaderProgram, "color"), 0.0f, 0.0f, 0.0f);
    // render chunk as wireframe
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    chunkWire.render();
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteProgram(wireframeShaderProgram);
    glfwTerminate();
    return 0;
}

void processInput(GLFWwindow *window) {
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
    // Edge-detect a few control keys so a single press triggers once.
    // We keep previous state statically so repeated frames don't re-trigger actions.
    static bool nPrev=false, gPrev=false, cPrev=false, spacePrev=false, rightPrev=false;
    static bool hPrev = false;
    static bool bPrev = false;
    static bool fPrev = false;

    bool nNow = glfwGetKey(window, GLFW_KEY_N) == GLFW_PRESS;
    if (nNow && !nPrev) stepNext = true;
    nPrev = nNow;

    bool rightNow = glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS;
    if (rightNow && !rightPrev) stepRight = true;
    rightPrev = rightNow;

    bool gNow = glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS;
    if (gNow && !gPrev && canGenerate) {
        generateAll = true;
        canGenerate = false; // Block further generation until C is pressed
    }
    gPrev = gNow;

    bool cNow = glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS;
    if (cNow && !cPrev) {
        clearMesh = true;
        canGenerate = true; // Allow generation again after clearing
    }
    cPrev = cNow;

    bool spaceNow = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
    if (spaceNow && !spacePrev) autoStep = !autoStep;
    spacePrev = spaceNow;

    bool hNow = glfwGetKey(window, GLFW_KEY_H) == GLFW_PRESS;
    if (hNow && !hPrev) showNormals = !showNormals;
    hPrev = hNow;

    bool bNow = glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS;
    if (bNow && !bPrev) showChunkBox = !showChunkBox;
    bPrev = bNow;

    bool fNow = glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS;
    if (fNow && !fPrev) wireFrame = !wireFrame;
    fPrev = fNow;
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn) {
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);
    if (firstMouse) {
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

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    camera.ProcessMouseScroll(static_cast<float>(yoffset));
}

unsigned int createSimpleShaderProgram(const char* vsSource, const char* fsSource) {
    int success;
    char infoLog[512];

    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vsSource, NULL);
    glCompileShader(vertexShader);
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
    }

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fsSource, NULL);
    glCompileShader(fragmentShader);
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
    }

    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}

unsigned int loadTexture(const char* path) {
    unsigned int textureID;
    glGenTextures(1, &textureID);
    
    int width, height, nrComponents;
    stbi_set_flip_vertically_on_load(true );
    unsigned char *data = stbi_load(path, &width, &height, &nrComponents, 0);
    if (data) {
        GLenum format;
        if (nrComponents == 1)
            format = GL_RED;
        else if (nrComponents == 3)
            format = GL_RGB;
        else if (nrComponents == 4)
            format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    } else {
        std::cout << "Failed to load texture: " << path << std::endl;
        stbi_image_free(data);
    }

    return textureID;
}
