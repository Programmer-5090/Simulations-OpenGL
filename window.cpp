#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stb_image.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "shader.h"
#include "camera.h"
#include "model.h"
#include "geometry/sphere.h"
#include "geometry/circle.h"
#include <iostream>
#include <sstream>
#include <random>
#include <vector>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow *window);

const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

Camera camera(glm::vec3(0.0f, 2.0f, 0.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

float deltaTime = 0.0f;
float lastFrame = 0.0f;

struct SphereData {
    glm::vec3 position;
    glm::vec3 color;
    float radius;
    float rotationSpeed;
    float currentRotation;
};

struct CircleData {
    glm::vec3 position;
    glm::vec3 color;
    float radius;
};

float randomFloat(float min, float max) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(min, max);
    return dis(gen);
}

int main()
{
    if (!glfwInit()) {
        std::cout << "Failed to initialize GLFW" << std::endl;
        return -1;
    }
    
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "LearnOpenGL", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // configure global opengl state
    // -----------------------------
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    unsigned int gridVAO;
    glGenVertexArrays(1, &gridVAO);

    std::cout << "Loading shaders..." << std::endl;
    Shader ourShader("shaders/vertex.vs", "shaders/simple_fragment.fs");
    std::cout << "Basic shaders loaded successfully" << std::endl;
    
    Shader infiniteGridShader("shaders/infinite_grid.vs", "shaders/infinite_grid.fs");
    std::cout << "Infinite grid shaders loaded successfully" << std::endl;
    
    Shader normalDebugShader("shaders/normal_debug.vs", "shaders/normal_debug.fs", "shaders/normal_debug.gs");
    std::cout << "Normal debug shaders loaded successfully" << std::endl;

    std::cout << "Generating sphere mesh..." << std::endl;
    Sphere baseSphere(2.0f, 32);
    Mesh sphereMesh = baseSphere.toMesh();
    Circle baseCircle(32, 1.0f);
    Mesh circleMesh = baseCircle.toMesh();
    std::cout << "Circle mesh generated successfully" << std::endl;
    std::cout << "Sphere mesh generated successfully" << std::endl;

    const int numSpheres = 8;
    std::vector<SphereData> spheres;
    
    std::cout << "Generating " << numSpheres << " spheres with random properties..." << std::endl;
    for (int i = 0; i < numSpheres; ++i) {
        SphereData sphere;
        sphere.position = glm::vec3(
            randomFloat(-15.0f, 15.0f),
            randomFloat(2.0f, 8.0f),
            randomFloat(-15.0f, 5.0f)
        );
        
        sphere.color = glm::vec3(
            randomFloat(0.2f, 1.0f),
            randomFloat(0.2f, 1.0f),
            randomFloat(0.2f, 1.0f)
        );
        
        sphere.radius = randomFloat(0.8f, 2.5f);
        sphere.rotationSpeed = randomFloat(10.0f, 50.0f);
        sphere.currentRotation = randomFloat(0.0f, 360.0f);
        
        spheres.push_back(sphere);
        
        std::cout << "Sphere " << i << ": pos(" << sphere.position.x << ", " << sphere.position.y 
                  << ", " << sphere.position.z << ") color(" << sphere.color.r << ", " 
                  << sphere.color.g << ", " << sphere.color.b << ")" << std::endl;
    }

    unsigned int texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    
    unsigned char whitePixel[] = {255, 255, 255, 255};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, whitePixel);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);


    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();

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

        ourShader.use();

        ourShader.setVec3("lightColor", glm::vec3(1.0f, 1.0f, 1.0f));
        ourShader.setVec3("viewPos", camera.Position);
        
        ourShader.setVec3("dirLight.direction", glm::vec3(-0.2f, -1.0f, -0.3f));
        ourShader.setVec3("dirLight.ambient", glm::vec3(0.6f, 0.6f, 0.6f));
        ourShader.setVec3("dirLight.diffuse", glm::vec3(0.6f, 0.6f, 0.6f));
        ourShader.setVec3("dirLight.specular", glm::vec3(1.0f, 1.0f, 1.0f));

        ourShader.setMat4("projection", projection);
        ourShader.setMat4("view", view);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        ourShader.setInt("texture1", 0);

        for (size_t i = 0; i < spheres.size(); ++i) {
            auto& sphereData = spheres[i];
            
            sphereData.currentRotation += sphereData.rotationSpeed * deltaTime;
            if (sphereData.currentRotation > 360.0f) {
                sphereData.currentRotation -= 360.0f;
            }
            
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, sphereData.position);
            model = glm::rotate(model, glm::radians(sphereData.currentRotation), glm::vec3(0.0f, 1.0f, 0.0f));
            model = glm::rotate(model, glm::radians(sphereData.currentRotation * 0.7f), glm::vec3(1.0f, 0.0f, 0.0f));
            model = glm::scale(model, glm::vec3(sphereData.radius));
            
            ourShader.setMat4("model", model);
            
            ourShader.setVec3("material.ambient", sphereData.color * 0.2f);
            ourShader.setVec3("material.diffuse", sphereData.color);
            ourShader.setVec3("material.specular", glm::vec3(1.0f, 1.0f, 1.0f));
            ourShader.setFloat("material.shininess", 64.0f);
            
            sphereMesh.Draw(ourShader);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &gridVAO);
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
    camera.ProcessMouseScroll(static_cast<float>(yoffset));
}