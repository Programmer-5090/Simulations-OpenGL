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

const unsigned int SCR_WIDTH = 1200;
const unsigned int SCR_HEIGHT = 900;

int MAX_DEPTH = 10;
bool showBVH = true;
bool showModel = true;
bool needsRebuild = false; 
int currentViewDepth = 0;

Camera camera(glm::vec3(0.0f, 0.5f, 2.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

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
        if (VAO != 0) return; // Already setup
        
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

private:
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
            
            // Set model matrix to center the box at its center
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
    Shader modelShader("shaders/vertex.vs", "shaders/simple_fragment.fs");
    Shader boxShader("shaders/box.vs", "shaders/box.fs");
    std::cout << "Shaders loaded successfully" << std::endl;

    
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
    
    // Print controls
    std::cout << "\n=== CONTROLS ===" << std::endl;
    std::cout << "WASD + Mouse: Camera movement" << std::endl;
    std::cout << "B: Toggle BVH visualization" << std::endl;
    std::cout << "M: Toggle model visibility" << std::endl;
    std::cout << "UP/DOWN arrows: Change BVH depth level to view" << std::endl;
    std::cout << "LEFT/RIGHT arrows: Change BVH construction depth (rebuilds BVH)" << std::endl;
    std::cout << "Current BVH construction depth: " << MAX_DEPTH << std::endl;
    std::cout << "Current view depth: " << currentViewDepth << std::endl;
    std::cout << "===============\n" << std::endl;
    
    std::cout << "Starting render loop..." << std::endl;
    while (!glfwWindowShouldClose(window)) {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        
        processInput(window);
        
        if (needsRebuild) {
            delete bvh;
            bvh = new BVH(allTriangles);
            std::cout << "BVH rebuilt with depth " << MAX_DEPTH << std::endl;
            needsRebuild = false;
        }
        
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();
        glm::mat4 model = glm::mat4(1.0f);
        
        if (showBVH) {
            boxShader.use();
            boxShader.setMat4("projection", projection);
            boxShader.setMat4("view", view);
            bvh->renderBVH(boxShader, currentViewDepth);
        }

        if (showModel) {
            modelShader.use();
            modelShader.setMat4("projection", projection);
            modelShader.setMat4("view", view);
            modelShader.setMat4("model", model);
            
            // Set lighting
            modelShader.setVec3("lightColor", glm::vec3(1.0f, 1.0f, 1.0f));
            modelShader.setVec3("viewPos", camera.Position);
            modelShader.setVec3("dirLight.direction", glm::vec3(-0.2f, -1.0f, -0.3f));
            modelShader.setVec3("dirLight.ambient", glm::vec3(0.6f, 0.6f, 0.6f));
            modelShader.setVec3("dirLight.diffuse", glm::vec3(0.8f, 0.8f, 0.8f));
            modelShader.setVec3("dirLight.specular", glm::vec3(1.0f, 1.0f, 1.0f));
            
            // Set material
            modelShader.setVec3("material.ambient", glm::vec3(0.2f, 0.2f, 0.2f));
            modelShader.setVec3("material.diffuse", glm::vec3(0.8f, 0.8f, 0.8f));
            modelShader.setVec3("material.specular", glm::vec3(1.0f, 1.0f, 1.0f));
            modelShader.setFloat("material.shininess", 32.0f);
            
            bunnyModel.Draw(modelShader);
        }
        
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    
    std::cout << "Cleaning up..." << std::endl;
    delete bvh;
    bunnyModel.meshes.clear();
    glfwTerminate();
    std::cout << "Test completed successfully!" << std::endl;
    return 0;
}

void processInput(GLFWwindow *window)
{
    static bool bKeyPressed = false;
    static bool mKeyPressed = false;
    static bool upKeyPressed = false;
    static bool downKeyPressed = false;
    static bool leftKeyPressed = false;
    static bool rightKeyPressed = false;
    
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
    
    // Toggle BVH visibility with 'B' key
    if (glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS) {
        if (!bKeyPressed) {
            showBVH = !showBVH;
            std::cout << "BVH visualization: " << (showBVH ? "ON" : "OFF") << std::endl;
            bKeyPressed = true;
        }
    } else {
        bKeyPressed = false;
    }
    
    // Toggle Model visibility with 'M' key
    if (glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS) {
        if (!mKeyPressed) {
            showModel = !showModel;
            std::cout << "Model visualization: " << (showModel ? "ON" : "OFF") << std::endl;
            mKeyPressed = true;
        }
    } else {
        mKeyPressed = false;
    }
    
    // Increase view depth with UP arrow
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
        if (!upKeyPressed) {
            if (currentViewDepth < MAX_DEPTH) {
                currentViewDepth++;
                std::cout << "Viewing depth level: " << currentViewDepth << std::endl;
            } else {
                std::cout << "Cannot view beyond max construction depth (" << MAX_DEPTH << ")" << std::endl;
            }
            upKeyPressed = true;
        }
    } else {
        upKeyPressed = false;
    }
    
    // Decrease view depth with DOWN arrow
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
        if (!downKeyPressed && currentViewDepth > 0) {
            currentViewDepth--;
            std::cout << "Viewing depth level: " << currentViewDepth << std::endl;
            downKeyPressed = true;
        }
    } else {
        downKeyPressed = false;
    }
    
    // Increase construction depth with RIGHT arrow (rebuilds BVH)
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
        if (!rightKeyPressed) {
            MAX_DEPTH++;
            std::cout << "BVH construction depth: " << MAX_DEPTH << " (rebuilding...)" << std::endl;
            needsRebuild = true;
            rightKeyPressed = true;
        }
    } else {
        rightKeyPressed = false;
    }
    
    // Decrease construction depth with LEFT arrow (rebuilds BVH)
    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
        if (!leftKeyPressed && MAX_DEPTH > 0) {
            MAX_DEPTH--;
            // Ensure view depth doesn't exceed construction depth
            if (currentViewDepth > MAX_DEPTH) {
                currentViewDepth = MAX_DEPTH;
            }
            std::cout << "BVH construction depth: " << MAX_DEPTH << " (rebuilding...)" << std::endl;
            needsRebuild = true;
            leftKeyPressed = true;
        }
    } else {
        leftKeyPressed = false;
    }
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
