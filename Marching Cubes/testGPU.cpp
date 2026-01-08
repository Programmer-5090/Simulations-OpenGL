#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "GPUMarchCubes.h"
#include "../model.h"
#include "../shader.h"
#include "../camera.h"
#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <chrono>

const unsigned int SCR_WIDTH = 1200;
const unsigned int SCR_HEIGHT = 900;

Camera camera(glm::vec3(0.0f, 0.5f, 2.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

int MAX_DEPTH = 10;
bool showBVH = true;
bool showModel = true;

bool bKeyPressed = false;
bool mKeyPressed = false;

float deltaTime = 0.0f;
float lastFrame = 0.0f;

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow *window);

struct Triangle {
    glm::vec3 v0, v1, v2;
    glm::vec3 normal;
};

struct BoundingBox {
    glm::vec3 min = glm::vec3(1e10f);
    glm::vec3 max = glm::vec3(-1e10f);
    glm::vec3 center = (min + max) * 0.5f;
    unsigned int VAO = 0;
    unsigned int VBO = 0;
    unsigned int EBO = 0;
    glm::mat4 modelMatrix = glm::mat4(1.0f);

    void update(const glm::vec3& point) {
        min = glm::min(min, point);
        max = glm::max(max, point);
        center = (min + max) * 0.5f;
    }

    void update(const Triangle& tri) {
        update(tri.v0);
        update(tri.v1);
        update(tri.v2);
    }

    ~BoundingBox(){
        if(VAO) glDeleteVertexArrays(1, &VAO);
        if(VBO) glDeleteBuffers(1, &VBO);
        if(EBO) glDeleteBuffers(1, &EBO);
    }

    void setupRender() {
        if (VAO != 0) return;
        
        glm::vec3 size = max - min;
        float halfWidth = size.x / 2.0f;
        float halfHeight = size.y / 2.0f;
        float halfDepth = size.z / 2.0f;

        GLfloat vertices[] = {
            -halfWidth, -halfHeight, -halfDepth,
            halfWidth, -halfHeight, -halfDepth,
            halfWidth,  halfHeight, -halfDepth,
            -halfWidth,  halfHeight, -halfDepth,
            -halfWidth, -halfHeight,  halfDepth,
            halfWidth, -halfHeight,  halfDepth,
            halfWidth,  halfHeight,  halfDepth,
            -halfWidth,  halfHeight,  halfDepth,
        };

        GLuint indices[] = {
            0, 1, 1, 2, 2, 3, 3, 0, // Back face
            4, 5, 5, 6, 6, 7, 7, 4, // Front face
            0, 4, 1, 5, 2, 6, 3, 7  // Connecting lines
        };

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        glBindVertexArray(VAO);

        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (void*)0);
        glEnableVertexAttribArray(0);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    void render() {
        setupRender();
        
        glm::mat4 transform = glm::translate(glm::mat4(1.0f), center);
        
        glBindVertexArray(VAO);
        glDrawElements(GL_LINES, 24, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }
};

struct Node {
    BoundingBox bbox;
    std::vector<Triangle> triangles;
    Node* childA;
    Node* childB;
    bool isLeaf;

    ~Node() {
        delete childA;
        delete childB;
    }
};

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
                // Region 4
                s = glm::clamp(-d / a, 0.f, 1.f);
                t = 0.f;
            } else {
                // Region 3
                s = 0.f;
                t = glm::clamp(-e / c, 0.f, 1.f);
            }
        } else if (t < 0.f) {
            // Region 5
            s = glm::clamp(-d / a, 0.f, 1.f);
            t = 0.f;
        } else {
            // Region 0 (inside triangle)
            float invDet = 1.f / det;
            s *= invDet;
            t *= invDet;
        }
    } else {
        if (s < 0.f) {
            // Region 2
            s = 0.f;
            t = glm::clamp((-e + b) / (c - b - c + a), 0.f, 1.f);
        } else if (t < 0.f) {
            // Region 6
            t = 0.f;
            s = glm::clamp((-d + b) / (a - b - a + c), 0.f, 1.f);
        } else {
            // Region 1
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

class BVH {
public:
    BoundingBox rootBBox;
    Node* root;

    BVH(const std::vector<Triangle>& triangles) {
        root = buildBVH(triangles, 0);
        rootBBox = root->bbox;
    }
    
    ~BVH() {
        delete root;
    }

    void renderBVH(Shader& shader, int targetDepth) {
        if (root) {
            renderNodeRecursive(root, shader, 0, targetDepth);
        }
    }

    float queryDistance(const glm::vec3& point, glm::vec3* outClosestPoint = nullptr, glm::vec3* outNormal = nullptr) {
        if (!root) return 1e10f;
        
        float bestDist = 1e10f;
        glm::vec3 bestPoint, bestNormal;
        queryDistanceRecursive(root, point, bestDist, bestPoint, bestNormal);
        
        if (outClosestPoint) *outClosestPoint = bestPoint;
        if (outNormal) *outNormal = bestNormal;
        return bestDist;
    }

private:
    void queryDistanceRecursive(Node* node, const glm::vec3& point, 
                               float& bestDist, glm::vec3& bestPoint, glm::vec3& bestNormal) {
        if (!node) return;
        
        // Early exit if bounding box is too far
        float boxDist = distanceToBox(point, node->bbox.min, node->bbox.max);
        if (boxDist > bestDist) return;
        
        if (node->isLeaf) {
            // Test all triangles in this leaf
            for (const auto& tri : node->triangles) {
                glm::vec3 closestPoint;
                float dist = distanceToTriangle(point, tri.v0, tri.v1, tri.v2, &closestPoint);
                if (dist < bestDist) {
                    bestDist = dist;
                    bestPoint = closestPoint;
                    bestNormal = tri.normal;
                }
            }
        } else {
            // Recurse to children
            queryDistanceRecursive(node->childA, point, bestDist, bestPoint, bestNormal);
            queryDistanceRecursive(node->childB, point, bestDist, bestPoint, bestNormal);
        }
    }
    
    float distanceToBox(const glm::vec3& point, const glm::vec3& boxMin, const glm::vec3& boxMax) {
        glm::vec3 closest = glm::clamp(point, boxMin, boxMax);
        return glm::distance(point, closest);
    }

    void renderNodeRecursive(Node* node, Shader& shader, int currentDepth, int targetDepth) {
        if (!node) return;
        if (currentDepth == targetDepth) {
            glm::vec3 color;
            switch (currentDepth % 6) {
                case 0: color = glm::vec3(1.0f, 0.0f, 0.0f); break; // Red - Root
                case 1: color = glm::vec3(0.0f, 1.0f, 0.0f); break; // Green - Level 1
                case 2: color = glm::vec3(0.0f, 0.0f, 1.0f); break; // Blue - Level 2
                case 3: color = glm::vec3(1.0f, 1.0f, 0.0f); break; // Yellow - Level 3
                case 4: color = glm::vec3(1.0f, 0.0f, 1.0f); break; // Magenta - Level 4
                case 5: color = glm::vec3(0.0f, 1.0f, 1.0f); break; // Cyan - Level 5
            }
            
            glm::mat4 boxModel = glm::translate(glm::mat4(1.0f), node->bbox.center);
            shader.setMat4("model", boxModel);
            shader.setVec3("color", color);
            
            node->bbox.render();
        }
        
        // Continue traversing children
        if (currentDepth < targetDepth && !node->isLeaf) {
            if (node->childA) renderNodeRecursive(node->childA, shader, currentDepth + 1, targetDepth);
            if (node->childB) renderNodeRecursive(node->childB, shader, currentDepth + 1, targetDepth);
        }
    }
    Node* buildBVH(const std::vector<Triangle>& triangles, int depth) {
        Node* node = new Node();
        for (const auto& tri : triangles) {
            node->bbox.update(tri);
        }

        if (triangles.size() <= 2 || depth >= MAX_DEPTH) {
            node->triangles = triangles;
            node->isLeaf = true;
            node->childA = nullptr;
            node->childB = nullptr;
            return node;
        }

        // Split along longest axis
        glm::vec3 extent = node->bbox.max - node->bbox.min;
        int axis = 0;
        if (extent.y > extent.x && extent.y > extent.z) axis = 1;
        else if (extent.z > extent.x) axis = 2;

        float splitPos = node->bbox.min[axis] + extent[axis] * 0.5f;

        std::vector<Triangle> leftTris, rightTris;
        for (const auto& tri : triangles) {
            float centroid = (tri.v0[axis] + tri.v1[axis] + tri.v2[axis]) / 3.0f;
            if (centroid < splitPos) leftTris.push_back(tri);
            else rightTris.push_back(tri);
        }

        // Where all triangles go to one side
        if (leftTris.empty() || rightTris.empty()) {
            leftTris.clear();
            rightTris.clear();
            for (size_t i = 0; i < triangles.size(); ++i) {
                if (i < triangles.size() / 2) leftTris.push_back(triangles[i]);
                else rightTris.push_back(triangles[i]);
            }
        }

        node->childA = buildBVH(leftTris, depth + 1);
        node->childB = buildBVH(rightTris, depth + 1);
        node->isLeaf = false;
        return node;
    }
};

// Helper function to generate a signed distance field from a mesh
std::vector<float> generateSDFFromMesh(
    const Model& model,
    int gridSizeX, int gridSizeY, int gridSizeZ,
    glm::vec3& outBoundsMin, glm::vec3& outBoundsMax, 
    BVH* modelBVH = nullptr)
{
    // Collect all triangles with their normals
    std::vector<Triangle> triangles;

    // Create Bounding Box for the model

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
        return std::vector<float>();
    }
    
    // Expand bounds for padding
    glm::vec3 boundsSize = outBoundsMax - outBoundsMin;
    glm::vec3 padding = boundsSize * 0.15f;
    outBoundsMin -= padding;
    outBoundsMax += padding;
    
    std::cout << "Mesh bounds: [" << outBoundsMin.x << ", " << outBoundsMin.y << ", " << outBoundsMin.z << "] to ["
              << outBoundsMax.x << ", " << outBoundsMax.y << ", " << outBoundsMax.z << "]" << std::endl;
    std::cout << "Processing " << triangles.size() << " triangles..." << std::endl;
    
    // Create SDF grid
    std::vector<float> sdfGrid(gridSizeX * gridSizeY * gridSizeZ);
    
    glm::vec3 gridSize = outBoundsMax - outBoundsMin;
    float cellSizeX = gridSize.x / (gridSizeX - 1);
    float cellSizeY = gridSize.y / (gridSizeY - 1);
    float cellSizeZ = gridSize.z / (gridSizeZ - 1);
    
    // Compute signed distance to closest triangle for each grid point
    int totalPoints = gridSizeX * gridSizeY * gridSizeZ;
    int processedPoints = 0;
    
    for (int z = 0; z < gridSizeZ; ++z) {
        if (z % 8 == 0) {
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
                float minDist;
                glm::vec3 closestPoint;
                glm::vec3 closestNormal;
                
                if (modelBVH) {
                    // Use BVH for fast distance queries
                    minDist = modelBVH->queryDistance(gridPos, &closestPoint, &closestNormal);
                } else {
                    // Fallback to brute force
                    minDist = 1e10f;
                    
                    for (const auto& tri : triangles) {
                        glm::vec3 currentClosest;
                        float dist = distanceToTriangle(gridPos, tri.v0, tri.v1, tri.v2, &currentClosest);
                        if (dist < minDist) {
                            minDist = dist;
                            closestPoint = currentClosest;
                            closestNormal = tri.normal;
                        }
                    }
                }
                
                // Determine if point is inside using normal direction
                // If the vector from surface to query point opposes the normal, point is inside
                glm::vec3 toPoint = gridPos - closestPoint;
                float sign = glm::dot(toPoint, closestNormal) < 0.0f ? -1.0f : 1.0f;
                
                int idx = z * (gridSizeX * gridSizeY) + y * gridSizeX + x;
                sdfGrid[idx] = sign * minDist;
                processedPoints++;
            }
        }
    }
    
    std::cout << "SDF generation complete!" << std::endl;
    
    // Debug: Print some SDF statistics
    float minVal = 1e10f, maxVal = -1e10f;
    int insideCount = 0, outsideCount = 0;
    for (const auto& val : sdfGrid) {
        minVal = std::min(minVal, val);
        maxVal = std::max(maxVal, val);
        if (val < 0.0f) insideCount++;
        else outsideCount++;
    }
    std::cout << "SDF stats: min=" << minVal << ", max=" << maxVal 
              << ", inside=" << insideCount << ", outside=" << outsideCount << std::endl;
    
    return sdfGrid;
}

int main()
{
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }
    
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "GPU Marching Cubes - Stanford Bunny", NULL, NULL);
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
    Shader boxShader("shaders/box.vs", "shaders/box.fs");
    std::cout << "Shaders loaded successfully" << std::endl;
    
    // Load the Stanford bunny model
    std::cout << "Loading Stanford bunny model..." << std::endl;
    Model bunnyModel("models/stanford-bunny/source/bunny.obj");
    std::cout << "Model loaded with " << bunnyModel.meshes.size() << " mesh(es)" << std::endl;

    BoundingBox boundingBox;
    // Collect all triangles and compute bounding box
    std::vector<Triangle> allTriangles;
    for (const auto& mesh : bunnyModel.meshes) {
        for (size_t i = 0; i < mesh.indices.size(); i += 3) {
            Triangle tri;
            tri.v0 = glm::vec3(mesh.vertices[mesh.indices[i]].Position);
            tri.v1 = glm::vec3(mesh.vertices[mesh.indices[i + 1]].Position);
            tri.v2 = glm::vec3(mesh.vertices[mesh.indices[i + 2]].Position);
            glm::vec3 edge1 = tri.v1 - tri.v0;
            glm::vec3 edge2 = tri.v2 - tri.v0;
            tri.normal = glm::normalize(glm::cross(edge1, edge2));
            allTriangles.push_back(tri);
            boundingBox.update(tri);
        }
    }
    
    std::cout << "Building BVH with " << allTriangles.size() << " triangles..." << std::endl;
    BVH* bvh = new BVH(allTriangles);
    std::cout << "BVH construction completed" << std::endl;
    
    // Set grid resolution (higher = more detail)
    int gridSizeX = 80;
    int gridSizeY = 80;
    int gridSizeZ = 80;
    
    std::cout << "Generating signed distance field from bunny model using BVH acceleration..." << std::endl;
    glm::vec3 boundsMin, boundsMax;
    
    auto startTime = std::chrono::high_resolution_clock::now();
    std::vector<float> sdfGrid = generateSDFFromMesh(bunnyModel, gridSizeX, gridSizeY, gridSizeZ, boundsMin, boundsMax, bvh);
    auto endTime = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    std::cout << "SDF generation completed in " << duration.count() << " ms using BVH acceleration" << std::endl;
    
    if (sdfGrid.empty()) {
        std::cerr << "Failed to generate SDF grid" << std::endl;
        glfwTerminate();
        return -1;
    }
    
    std::cout << "Initializing GPU Marching Cubes..." << std::endl;
    GPUMarchCubes marchCubes;
    
    CMarchSettings settings;
    settings.gridSizeX = gridSizeX;
    settings.gridSizeY = gridSizeY;
    settings.gridSizeZ = gridSizeZ;
    settings.isoLevel = 0.0f;
    settings.boundsMin = boundsMin;
    settings.boundsMax = boundsMax;
    
    marchCubes.setSettings(settings);
    marchCubes.initialize();
    std::cout << "GPU Marching Cubes initialized with grid size: " << gridSizeX << "x" << gridSizeY << "x" << gridSizeZ << std::endl;
    
    // Upload SDF to GPU
    std::cout << "Uploading SDF to GPU..." << std::endl;
    marchCubes.uploadScalarField(sdfGrid);
    
    std::cout << "Executing GPU marching cubes..." << std::endl;
    marchCubes.execute();
    
    std::cout << "Retrieving generated mesh..." << std::endl;
    std::vector<float> vertices = marchCubes.getVertices();
    std::vector<unsigned int> indices = marchCubes.getIndices();
    
    std::cout << "Generated mesh: " << marchCubes.getVertexCount() << " vertices, "
              << marchCubes.getTriangleCount() << " triangles" << std::endl;
    
    // Debug: Print first few vertices
    std::cout << "First 3 vertices (position[3], normal[3], texcoord[2]):" << std::endl;
    for (int i = 0; i < std::min(3, marchCubes.getVertexCount()); i++) {
        int idx = i * 8;
        std::cout << "  v" << i << ": pos(" << vertices[idx] << "," << vertices[idx+1] << "," << vertices[idx+2] 
                  << ") norm(" << vertices[idx+3] << "," << vertices[idx+4] << "," << vertices[idx+5] << ")" << std::endl;
    }
    
    GLuint marchVAO, marchVBO, marchEBO;
    glGenVertexArrays(1, &marchVAO);
    glGenBuffers(1, &marchVBO);
    glGenBuffers(1, &marchEBO);
    
    glBindVertexArray(marchVAO);
    
    // Upload vertex data
    glBindBuffer(GL_ARRAY_BUFFER, marchVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    
    // Upload index data
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, marchEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
    
    // Set vertex attributes (matching Vertex structure: position[3], normal[3], texcoord[2])
    // Position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    // TexCoord
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    
    std::cout << "Mesh VAO created successfully" << std::endl;
    
    std::cout << "Starting render loop..." << std::endl;
    while (!glfwWindowShouldClose(window)) {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        
        processInput(window);
        
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();
        glm::mat4 model = glm::mat4(1.0f);
        // model = glm::rotate(model, currentFrame * glm::radians(30.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        // Model is already in world space, no transformation needed
        
        if (showModel) {
            // Render the marched mesh
            marchShader.use();
            marchShader.setMat4("projection", projection);
            marchShader.setMat4("view", view);
            marchShader.setMat4("model", model);
            
            // Set lighting
            marchShader.setVec3("lightColor", glm::vec3(1.0f, 1.0f, 1.0f));
            marchShader.setVec3("viewPos", camera.Position);
            marchShader.setVec3("dirLight.direction", glm::vec3(-0.2f, -1.0f, -0.3f));
            marchShader.setVec3("dirLight.ambient", glm::vec3(0.6f, 0.6f, 0.6f));
            marchShader.setVec3("dirLight.diffuse", glm::vec3(0.8f, 0.8f, 0.8f));
            marchShader.setVec3("dirLight.specular", glm::vec3(1.0f, 1.0f, 1.0f));
            
            // Set material
            marchShader.setVec3("material.ambient", glm::vec3(0.2f, 0.2f, 0.2f));
            marchShader.setVec3("material.diffuse", glm::vec3(0.8f, 0.8f, 0.8f));
            marchShader.setVec3("material.specular", glm::vec3(1.0f, 1.0f, 1.0f));
            marchShader.setFloat("material.shininess", 32.0f);
            
            // Render
            glBindVertexArray(marchVAO);
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, 0);
        }

        // Render BVH Bounding Boxes
        if (showBVH) {
            boxShader.use();
            boxShader.setMat4("projection", projection);
            boxShader.setMat4("view", view);
            bvh->renderBVH(boxShader, MAX_DEPTH);
        }
                
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    
    std::cout << "Cleaning up..." << std::endl;
    glDeleteBuffers(1, &marchVBO);
    glDeleteBuffers(1, &marchEBO);
    glDeleteVertexArrays(1, &marchVAO);
    
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
    
    // Handle B key with debouncing
    bool bCurrentlyPressed = glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS;
    if (bCurrentlyPressed && !bKeyPressed) {
        showBVH = !showBVH;
        std::cout << "BVH display: " << (showBVH ? "ON" : "OFF") << std::endl;
    }
    bKeyPressed = bCurrentlyPressed;
    
    // Handle M key with debouncing
    bool mCurrentlyPressed = glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS;
    if (mCurrentlyPressed && !mKeyPressed) {
        showModel = !showModel;
        std::cout << "Model display: " << (showModel ? "ON" : "OFF") << std::endl;
    }
    mKeyPressed = mCurrentlyPressed;
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
