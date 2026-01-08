#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../shader.h"
#include "../camera.h"
#include "../model.h"
#include "CubeMarching.h"
#include <iostream>
#include <vector>

const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

Camera camera(glm::vec3(0.0f, 0.1f, 0.5f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

float deltaTime = 0.0f;
float lastFrame = 0.0f;

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow *window);

// Helper to compute distance from point to triangle and return closest point
float distanceToTriangle(const glm::vec3& p, const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2, glm::vec3* outClosestPoint = nullptr) {
    glm::vec3 edge0 = v1 - v0;
    glm::vec3 edge1 = v2 - v0;
    glm::vec3 v0p = p - v0;
    
    float a = glm::dot(edge0, edge0);
    float b = glm::dot(edge0, edge1);
    float c = glm::dot(edge1, edge1);
    float d = glm::dot(edge0, v0p);
    float e = glm::dot(edge1, v0p);
    
    float det = a * c - b * b;
    float s = b * e - c * d;
    float t = b * d - a * e;
    
    if (s + t <= det) {
        if (s < 0.f) {
            if (t < 0.f) {
                s = glm::clamp(-d / a, 0.f, 1.f);
                t = 0.f;
            } else {
                s = 0.f;
                t = glm::clamp(-e / c, 0.f, 1.f);
            }
        } else if (t < 0.f) {
            s = glm::clamp(-d / a, 0.f, 1.f);
            t = 0.f;
        } else {
            float invDet = 1.f / det;
            s *= invDet;
            t *= invDet;
        }
    } else {
        if (s < 0.f) {
            s = 0.f;
            t = glm::clamp((-e + b) / (c - b - c + a), 0.f, 1.f);
        } else if (t < 0.f) {
            t = 0.f;
            s = glm::clamp((-d + b) / (a - b - a + c), 0.f, 1.f);
        } else {
            float numer = c + e - b - d;
            float denom = a - 2.f * b + c;
            s = glm::clamp(numer / denom, 0.f, 1.f);
            t = 1.f - s;
        }
    }
    
    glm::vec3 closest = v0 + s * edge0 + t * edge1;
    if (outClosestPoint) {
        *outClosestPoint = closest;
    }
    return glm::distance(p, closest);
}

// Helper function to generate a signed distance field from a mesh
std::vector<std::vector<std::vector<float>>> generateSDFFromMesh(
    const Model& model,
    int gridSizeX, int gridSizeY, int gridSizeZ,
    glm::vec3& outBoundsMin, glm::vec3& outBoundsMax)
{
    // Collect all triangles with their normals
    struct Triangle {
        glm::vec3 v0, v1, v2;
        glm::vec3 normal;
    };
    std::vector<Triangle> triangles;
    
    outBoundsMin = glm::vec3(1e10f);
    outBoundsMax = glm::vec3(-1e10f);
    
    for (const auto& mesh : model.meshes) {
        for (size_t i = 0; i < mesh.indices.size(); i += 3) {
            Triangle tri;
            tri.v0 = mesh.vertices[mesh.indices[i]].Position;
            tri.v1 = mesh.vertices[mesh.indices[i + 1]].Position;
            tri.v2 = mesh.vertices[mesh.indices[i + 2]].Position;
            
            // Compute face normal
            glm::vec3 edge1 = tri.v1 - tri.v0;
            glm::vec3 edge2 = tri.v2 - tri.v0;
            tri.normal = glm::normalize(glm::cross(edge1, edge2));
            
            triangles.push_back(tri);
            
            outBoundsMin = glm::min(outBoundsMin, glm::min(tri.v0, glm::min(tri.v1, tri.v2)));
            outBoundsMax = glm::max(outBoundsMax, glm::max(tri.v0, glm::max(tri.v1, tri.v2)));
        }
    }
    
    if (triangles.empty()) {
        std::cerr << "Error: Model has no triangles!" << std::endl;
        return std::vector<std::vector<std::vector<float>>>();
    }
    
    // Expand bounds for padding
    glm::vec3 boundsSize = outBoundsMax - outBoundsMin;
    glm::vec3 padding = boundsSize * 0.15f;
    outBoundsMin -= padding;
    outBoundsMax += padding;
    
    std::cout << "Mesh bounds: [" << outBoundsMin.x << ", " << outBoundsMin.y << ", " << outBoundsMin.z << "] to ["
              << outBoundsMax.x << ", " << outBoundsMax.y << ", " << outBoundsMax.z << "]" << std::endl;
    std::cout << "Processing " << triangles.size() << " triangles..." << std::endl;
    
    // Create SDF grid (3D vector for CPU marching cubes)
    std::vector<std::vector<std::vector<float>>> sdfGrid(
        gridSizeZ, 
        std::vector<std::vector<float>>(
            gridSizeY, 
            std::vector<float>(gridSizeX, 0.0f)
        )
    );
    
    glm::vec3 gridSize = outBoundsMax - outBoundsMin;
    float cellSizeX = gridSize.x / (gridSizeX - 1);
    float cellSizeY = gridSize.y / (gridSizeY - 1);
    float cellSizeZ = gridSize.z / (gridSizeZ - 1);
    
    // Compute signed distance to closest triangle for each grid point
    int totalPoints = gridSizeX * gridSizeY * gridSizeZ;
    int processedPoints = 0;
    
    for (int z = 0; z < gridSizeZ; ++z) {
        if (z % 4 == 0) {
            std::cout << "Progress: " << (100 * processedPoints / totalPoints) << "%" << std::endl;
        }
        for (int y = 0; y < gridSizeY; ++y) {
            for (int x = 0; x < gridSizeX; ++x) {
                glm::vec3 gridPos = outBoundsMin + glm::vec3(
                    x * cellSizeX,
                    y * cellSizeY,
                    z * cellSizeZ
                );
                
                // Find minimum distance to any triangle and track closest
                float minDist = 1e10f;
                glm::vec3 closestPoint;
                glm::vec3 closestNormal;
                
                for (const auto& tri : triangles) {
                    glm::vec3 currentClosest;
                    float dist = distanceToTriangle(gridPos, tri.v0, tri.v1, tri.v2, &currentClosest);
                    if (dist < minDist) {
                        minDist = dist;
                        closestPoint = currentClosest;
                        closestNormal = tri.normal;
                    }
                }
                
                // Determine if point is inside using normal direction
                glm::vec3 toPoint = gridPos - closestPoint;
                float sign = glm::dot(toPoint, closestNormal) < 0.0f ? -1.0f : 1.0f;
                
                sdfGrid[z][y][x] = sign * minDist;
                processedPoints++;
            }
        }
    }
    
    std::cout << "SDF generation complete!" << std::endl;
    return sdfGrid;
}

int main()
{
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }
    
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "CPU Marching Cubes - Stanford Bunny", NULL, NULL);
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
    
    
    std::cout << "Loading shaders..." << std::endl;
    Shader marchShader("shaders/vertex.vs", "shaders/simple_fragment.fs");
    std::cout << "Shaders loaded successfully" << std::endl;
    
    std::cout << "Loading Stanford bunny model..." << std::endl;
    Model bunnyModel("models/stanford-bunny/source/bunny.obj");
    std::cout << "Model loaded with " << bunnyModel.meshes.size() << " mesh(es)" << std::endl;
    
    // Set grid resolution
    int gridSizeX = 64;
    int gridSizeY = 64;
    int gridSizeZ = 64;
    
    // Generate SDF from bunny model
    std::cout << "Generating signed distance field from bunny model..." << std::endl;
    glm::vec3 boundsMin, boundsMax;
    auto sdfGrid = generateSDFFromMesh(bunnyModel, gridSizeX, gridSizeY, gridSizeZ, boundsMin, boundsMax);
    
    if (sdfGrid.empty()) {
        std::cerr << "Failed to generate SDF grid" << std::endl;
        glfwTerminate();
        return -1;
    }
    
    std::cout << "Running CPU marching cubes..." << std::endl;
    CubeMarching mc;
    mc.generateMesh(sdfGrid, 0.0f);  // Use isolevel 0.0 for signed distance
    
    std::cout << "Generated mesh: " << mc.getVertices().size() << " vertices, " 
              << (mc.getIndices().size() / 3) << " triangles" << std::endl;
    
    Mesh marchingCubesMesh;
    const auto& vertices = mc.getVertices();
    const auto& indices = mc.getIndices();
    
    if (!vertices.empty() && !indices.empty()) {
        std::vector<unsigned int> meshIndices(indices.begin(), indices.end());
        std::vector<Texture> textures;
        marchingCubesMesh = Mesh(vertices, meshIndices, textures);
    }
    
    std::cout << "Mesh ready for rendering. Starting render loop..." << std::endl;
    
    while (!glfwWindowShouldClose(window)) {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        
        processInput(window);
        
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();
        
        // Transform from grid space to world space
        glm::vec3 gridSize = boundsMax - boundsMin;
        glm::vec3 gridScale = gridSize / glm::vec3(gridSizeX - 1, gridSizeY - 1, gridSizeZ - 1);
        
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, boundsMin);
        model = glm::scale(model, gridScale);
        
        // Render the marched mesh
        marchShader.use();
        marchShader.setMat4("projection", projection);
        marchShader.setMat4("view", view);
        marchShader.setMat4("model", model);
        
        // Set lighting
        marchShader.setVec3("lightColor", glm::vec3(1.0f, 1.0f, 1.0f));
        marchShader.setVec3("viewPos", camera.Position);
        marchShader.setVec3("dirLight.direction", glm::vec3(-0.2f, -1.0f, -0.3f));
        marchShader.setVec3("dirLight.ambient", glm::vec3(0.2f, 0.2f, 0.2f));
        marchShader.setVec3("dirLight.diffuse", glm::vec3(0.5f, 0.5f, 0.5f));
        marchShader.setVec3("dirLight.specular", glm::vec3(1.0f, 1.0f, 1.0f));
        
        // Set material
        marchShader.setVec3("material.ambient", glm::vec3(0.2f, 0.2f, 0.2f));
        marchShader.setVec3("material.diffuse", glm::vec3(0.8f, 0.8f, 0.8f));
        marchShader.setVec3("material.specular", glm::vec3(1.0f, 1.0f, 1.0f));
        marchShader.setFloat("material.shininess", 32.0f);
        
        // Render
        marchingCubesMesh.Draw(marchShader);
        
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    
    std::cout << "Cleaning up..." << std::endl;
    glfwTerminate();
    std::cout << "Test completed successfully!" << std::endl;
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
    if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS)
        camera.ProcessKeyboard(UP, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        camera.ProcessKeyboard(DOWN, deltaTime);
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn)
{
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

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    camera.ProcessMouseScroll(static_cast<float>(yoffset));
}
