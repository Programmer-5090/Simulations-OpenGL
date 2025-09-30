#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../shader.h"
#include "../camera.h"
#include "CubeMarching.h"
#include <iostream>
#include "../geometry/sphere.h"
#include "../model.h"
// Local wireframe cube helper (avoids linking to BoundingBox implementation)
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
Camera camera(glm::vec3(0.0f, 15.0f, 45.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// Timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// Marching Cubes globals
CubeMarching mc;
Mesh marchingCubesMesh;
int res = 32; // lower for stepwise view
float isolevel = 0.0f;

// Stepping & control flags
bool stepNext = false;      // single-step (N or Right Arrow)
bool stepRight = false;     // Right arrow single-step
bool generateAll = false;   // Generate full mesh (G)
bool clearMesh = false;     // Clear mesh (C)
bool autoStep = true;       // auto-advance through cells
bool showNormals = false;   // toggle normal visualization (H)
bool showChunkBox = true;   // toggle chunk box visibility (B)

// wire cubes for visualization
WireCube chunkWire;
WireCube wireCube;

// Helper function to create a simple shader program from source
unsigned int createSimpleShaderProgram(const char* vsSource, const char* fsSource);

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

    // Shaders
    Shader shader("shaders/vertex.vs", "shaders/simple_fragment.fs");
    Shader infiniteGridShader("shaders/infinite_grid.vs", "shaders/infinite_grid.fs");

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

    // --- Normal debug shader (vertex, geometry, fragment) ---
    const char* normal_vs = R"glsl(
        #version 330 core
        layout(location = 0) in vec3 aPos;
        layout(location = 1) in vec3 aNormal;
        uniform mat4 model;
        uniform mat4 view;
        uniform mat4 projection;
        out vec3 FragPos;
        out vec3 Normal;
        void main() {
            FragPos = vec3(model * vec4(aPos, 1.0));
            Normal = mat3(transpose(inverse(model))) * aNormal;
            gl_Position = projection * view * model * vec4(aPos, 1.0);
        }
    )glsl";
    const char* normal_gs = R"glsl(
        #version 330 core
        layout(points) in;
        layout(line_strip, max_vertices = 2) out;
        uniform float normalLength;
        in vec3 FragPos[];
        in vec3 Normal[];
        uniform mat4 projection;
        uniform mat4 view;
        void main() {
            vec3 p = FragPos[0];
            vec3 n = normalize(Normal[0]);
            gl_Position = projection * view * vec4(p, 1.0);
            EmitVertex();
            gl_Position = projection * view * vec4(p + n * normalLength, 1.0);
            EmitVertex();
            EndPrimitive();
        }
    )glsl";
    const char* normal_fs = R"glsl(
        #version 330 core
        out vec4 FragColor;
        void main() {
            FragColor = vec4(1.0, 0.0, 0.0, 1.0); // red normals
        }
    )glsl";

    unsigned int normalVS = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(normalVS, 1, &normal_vs, NULL);
    glCompileShader(normalVS);
    unsigned int normalGS = glCreateShader(GL_GEOMETRY_SHADER);
    glShaderSource(normalGS, 1, &normal_gs, NULL);
    glCompileShader(normalGS);
    unsigned int normalFS = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(normalFS, 1, &normal_fs, NULL);
    glCompileShader(normalFS);
    unsigned int normalProgram = glCreateProgram();
    glAttachShader(normalProgram, normalVS);
    glAttachShader(normalProgram, normalGS);
    glAttachShader(normalProgram, normalFS);
    glLinkProgram(normalProgram);
    glDeleteShader(normalVS);
    glDeleteShader(normalGS);
    glDeleteShader(normalFS);

    // showNormals is a global flag toggled by H


    // Setup marching cubes
    mc.setDimensions(res, res, res);
    mc.SetVisualizeNoise(false);

    // Wire cube for the marching grid bounds (wire cube is unit and we will scale when rendering)
    wireCube.create(1.0f, 1.0f, 1.0f);

    unsigned int triVAO=0, triVBO=0;
    glGenVertexArrays(1, &triVAO);
    glGenBuffers(1, &triVBO);

    // Temporary buffers to draw normals via geometry shader (we will feed it points)
    unsigned int normalVAO = 0, normalVBO = 0;
    glGenVertexArrays(1, &normalVAO);
    glGenBuffers(1, &normalVBO);
    glBindVertexArray(normalVAO);
    glBindBuffer(GL_ARRAY_BUFFER, normalVBO);
    // initially empty; we'll glBufferData when we have vertices
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
    // aPos (location 0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float)*6, (void*)0);
    glEnableVertexAttribArray(0);
    // aNormal (location 1)
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(float)*6, (void*)(sizeof(float)*3));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    // --- Create a sphere and feed its vertices into the marching cubes input ---
    // (This is why you weren't seeing a sphere: mc needs input vertices and an influence radius.)
    Model bunny("models/stanford-bunny/source/bunny.obj");
    Mesh bunnyMesh = bunny.meshes[0]; // use first mesh of the model
    std::vector<glm::vec3> inputVerts;
    for (const auto &v : bunnyMesh.vertices) inputVerts.push_back(v.Position);
    mc.SetInputVertices(inputVerts);
    mc.SetInfluenceRadius((float)res * 0.25f);

    // Now that input is set, query grid bounds and create the chunk wire cube
    int minX, minY, minZ, maxX, maxY, maxZ;
    mc.GetInputGridBounds(minX, minY, minZ, maxX, maxY, maxZ);
    glm::vec3 chunkSize = glm::vec3((float)(maxX - minX + 1), (float)(maxY - minY + 1), (float)(maxZ - minZ + 1));
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
            mc.ClearMesh();
            gx = minX; gy = minY; gz = minZ; // reset stepping
            meshNeedsUpdate = true;
            clearMesh = false;
        }

        // Generate full mesh when G is pressed
        if (generateAll) {
            mc.ClearMesh();
            mc.GenerateMesh(isolevel);
            meshNeedsUpdate = true;
            generateAll = false;
        }

        // Single-step requests (N key or Right arrow)
        if (stepNext || stepRight) {
            mc.TriangulateCellAt(gx, gy, gz, isolevel);
            meshNeedsUpdate = true;
            stepNext = stepRight = false;

            // advance to next cell
            gx++;
            if (gx > maxX) { gx = minX; gy++; if (gy > maxY) { gy = minY; gz++; if (gz > maxZ) { gz = minZ; mc.ClearMesh(); } } }
        }

        // Auto-step timer
        if (autoStep && now - lastStepTime > stepInterval) {
            lastStepTime = now;
            mc.TriangulateCellAt(gx, gy, gz, isolevel);
            meshNeedsUpdate = true;
            gx++;
            if (gx > maxX) { gx = minX; gy++; if (gy > maxY) { gy = minY; gz++; if (gz > maxZ) { gz = minZ; mc.ClearMesh(); } } }
        }

        // If any of the above happened, update the GPU mesh object
        if (meshNeedsUpdate) {
            marchingCubesMesh = mc.CreateMesh();
            // Update normal VAO/VBO with latest vertices: interleave pos+normal
            const auto &vlist = mc.GetVertices();
            std::vector<float> interleaved;
            interleaved.reserve(vlist.size() * 6);
            for (const auto &v : vlist) {
                interleaved.push_back(v.Position.x);
                interleaved.push_back(v.Position.y);
                interleaved.push_back(v.Position.z);
                interleaved.push_back(v.Normal.x);
                interleaved.push_back(v.Normal.y);
                interleaved.push_back(v.Normal.z);
            }
            glBindBuffer(GL_ARRAY_BUFFER, normalVBO);
            glBufferData(GL_ARRAY_BUFFER, interleaved.size() * sizeof(float), interleaved.data(), GL_DYNAMIC_DRAW);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
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

        // Set up matrices

        shader.use();
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(-res * 0.5f, -res * 0.5f, -res * 0.5f));

        // --- Render marching cubes mesh ---
        const auto &verts = mc.GetVertices();
        if (!verts.empty()) {
            shader.use();
            shader.setMat4("projection", projection);
            shader.setMat4("view", view);
            shader.setMat4("model", model);
            shader.setVec3("viewPos", camera.Position);
            shader.setFloat("material.shininess", 32.0f);
            shader.setVec3("material.ambient", glm::vec3(0.3f, 0.5f, 0.8f));
            shader.setVec3("material.diffuse", glm::vec3(0.3f, 0.5f, 0.8f));
            shader.setVec3("material.specular", glm::vec3(0.8f));
            shader.setVec3("dirLight.direction", glm::vec3(-0.2f, -1.0f, -0.3f));
            shader.setVec3("dirLight.ambient", glm::vec3(0.5f));
            shader.setVec3("dirLight.diffuse", glm::vec3(0.8f));
            shader.setVec3("dirLight.specular", glm::vec3(1.0f));
            marchingCubesMesh.Draw(shader);
        }

        // --- FIX: Render wire cube with its own simple shader ---
        glUseProgram(wireframeShaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(wireframeShaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
    glUniformMatrix4fv(glGetUniformLocation(wireframeShaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(wireframeShaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniform3f(glGetUniformLocation(wireframeShaderProgram, "color"), 0.1f, 0.1f, 0.1f);
    // Render the unit wire cube at the current cell center
    glm::mat4 cellModel = glm::mat4(1.0f);
    glm::vec3 gridOffset = glm::vec3(-res * 0.5f, -res * 0.5f, -res * 0.5f);
    glm::vec3 gridCenter = glm::vec3((float)gx + 0.5f, (float)gy + 0.5f, (float)gz + 0.5f);
    cellModel = glm::translate(cellModel, gridOffset + gridCenter);
    glUniformMatrix4fv(glGetUniformLocation(wireframeShaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(cellModel));
    wireCube.render();

    // Draw normals if requested (render as red lines from vertex position)
    if (showNormals) {
        glUseProgram(normalProgram);
        glUniformMatrix4fv(glGetUniformLocation(normalProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(normalProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        // set model for normal shader so normals are in world space
        glUniformMatrix4fv(glGetUniformLocation(normalProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
        // normal length
        glUniform1f(glGetUniformLocation(normalProgram, "normalLength"), 0.5f);

        glBindVertexArray(normalVAO);
        // Number of points = number of vertices in vlist
        GLsizei pointCount = (GLsizei)mc.GetVertices().size();
        glDrawArrays(GL_POINTS, 0, pointCount);
        glBindVertexArray(0);
    }

    // Draw chunk bounds wireframe (toggleable with B)
    if (showChunkBox) {
    // Calculate chunk model: position chunk center in grid space and scale to chunkSize
    glm::mat4 chunkModel = glm::mat4(1.0f);
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

    bool nNow = glfwGetKey(window, GLFW_KEY_N) == GLFW_PRESS;
    if (nNow && !nPrev) stepNext = true;
    nPrev = nNow;

    bool rightNow = glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS;
    if (rightNow && !rightPrev) stepRight = true;
    rightPrev = rightNow;

    bool gNow = glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS;
    if (gNow && !gPrev) generateAll = true;
    gPrev = gNow;

    bool cNow = glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS;
    if (cNow && !cPrev) clearMesh = true;
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
