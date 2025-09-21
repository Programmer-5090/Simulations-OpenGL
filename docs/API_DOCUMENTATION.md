# API Documentation

## Overview

This document provides detailed API documentation for the key classes and components in the OpenGL Physics Simulation Suite.

## Table of Contents

- [Core Classes](#core-classes)
  - [Shader](#shader-class)
  - [Camera](#camera-class)
  - [Mesh](#mesh-class)
  - [Model](#model-class)
- [Fluid Simulation](#fluid-simulation)
  - [GPUFluidSimulation](#gpufluidsimulation-class)
  - [GPUParticleDisplay](#gpuparticledisplay-class)
- [Collision System](#collision-system)
  - [Solver](#solver-class)
  - [Particle](#particle-struct)
- [Geometry System](#geometry-system)
  - [GeometryData](#geometrydata-class)
  - [Parametric](#parametric-class)

---

## Core Classes

### Shader Class

**File**: `shader.h`, `shader.cpp`

The Shader class handles OpenGL shader program creation, compilation, and management.

#### Constructor
```cpp
Shader(const char* vertexPath, const char* fragmentPath);
Shader(const char* vertexPath, const char* fragmentPath, const char* geometryPath);
```

#### Public Methods

##### `void use()`
Activates the shader program for rendering.

##### `void setBool(const std::string &name, bool value)`
Sets a boolean uniform variable.

##### `void setInt(const std::string &name, int value)`
Sets an integer uniform variable.

##### `void setFloat(const std::string &name, float value)`
Sets a float uniform variable.

##### `void setVec2(const std::string &name, const glm::vec2 &value)`
Sets a 2D vector uniform variable.

##### `void setVec3(const std::string &name, const glm::vec3 &value)`
Sets a 3D vector uniform variable.

##### `void setVec4(const std::string &name, const glm::vec4 &value)`
Sets a 4D vector uniform variable.

##### `void setMat2(const std::string &name, const glm::mat2 &mat)`
Sets a 2x2 matrix uniform variable.

##### `void setMat3(const std::string &name, const glm::mat3 &mat)`
Sets a 3x3 matrix uniform variable.

##### `void setMat4(const std::string &name, const glm::mat4 &mat)`
Sets a 4x4 matrix uniform variable.

#### Usage Example
```cpp
Shader shader("shaders/vertex.vs", "shaders/fragment.fs");
shader.use();
shader.setMat4("model", modelMatrix);
shader.setMat4("view", viewMatrix);
shader.setMat4("projection", projectionMatrix);
```

---

### Camera Class

**File**: `camera.h`

The Camera class provides first-person camera controls with mouse and keyboard input.

#### Constructor
```cpp
Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f), 
       glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f), 
       float yaw = YAW, float pitch = PITCH);
```

#### Public Methods

##### `glm::mat4 GetViewMatrix()`
Returns the view matrix for the current camera position and orientation.

##### `void ProcessKeyboard(Camera_Movement direction, float deltaTime)`
Processes keyboard input for camera movement.
- `direction`: FORWARD, BACKWARD, LEFT, RIGHT
- `deltaTime`: Time since last frame

##### `void ProcessMouseMovement(float xoffset, float yoffset, GLboolean constrainPitch = true)`
Processes mouse movement for camera rotation.

##### `void ProcessMouseScroll(float yoffset)`
Processes mouse scroll for zoom (field of view adjustment).

#### Usage Example
```cpp
Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));

// In render loop
if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    camera.ProcessKeyboard(FORWARD, deltaTime);

glm::mat4 view = camera.GetViewMatrix();
shader.setMat4("view", view);
```

---

### Mesh Class

**File**: `mesh.h`, `mesh.cpp`

The Mesh class represents a 3D mesh with vertices, indices, and textures.

#### Constructor
```cpp
Mesh(std::vector<Vertex> vertices, std::vector<unsigned int> indices, std::vector<Texture> textures);
```

#### Public Methods

##### `void Draw(Shader &shader)`
Renders the mesh using the specified shader.

#### Data Structures
```cpp
struct Vertex {
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoords;
    glm::vec3 Tangent;
    glm::vec3 Bitangent;
};

struct Texture {
    unsigned int id;
    std::string type;
    std::string path;
};
```

---

### Model Class

**File**: `model.h`, `model.cpp`

The Model class loads and renders 3D models using Assimp.

#### Constructor
```cpp
Model(std::string const &path, bool gamma = false);
```

#### Public Methods

##### `void Draw(Shader &shader)`
Renders all meshes in the model.

#### Usage Example
```cpp
Model ourModel("models/backpack/backpack.obj");

// In render loop
ourModel.Draw(shader);
```

---

## Fluid Simulation

### GPUFluidSimulation Class

**File**: `SPHFluid/2D/GPUFluidSimulation.cpp`, `SPHFluid/3D/GPUFluidSimulation.cpp`

Handles SPH (Smoothed Particle Hydrodynamics) fluid simulation using compute shaders.

#### Key Methods

##### `void initializeParticles()`
Initializes particle positions and properties.

##### `void update(float deltaTime)`
Updates fluid simulation for one time step.

##### `void setMouseInteraction(float mouseX, float mouseY, bool leftClick, bool rightClick)`
Sets mouse interaction parameters for 2D simulation.

#### Simulation Parameters
```cpp
const int NUM_PARTICLES = 10000;
const float PARTICLE_MASS = 1.0f;
const float REST_DENSITY = 1000.0f;
const float GAS_CONSTANT = 2000.0f;
const float VISCOSITY = 250.0f;
```

---

### GPUParticleDisplay Class

**File**: `SPHFluid/2D/GPUParticleDisplay.cpp`, `SPHFluid/3D/GPUParticleDisplay.cpp`

Handles rendering of fluid particles using instanced rendering.

#### Key Methods

##### `void render(const glm::mat4& view, const glm::mat4& projection)`
Renders all particles with the given view and projection matrices.

##### `void updateParticleData(const std::vector<glm::vec4>& positions)`
Updates particle position data for rendering.

---

## Collision System

### Solver Class

**File**: `Collision System/solver.h`, `Collision System/solver.cpp`

Implements particle collision detection and resolution using spatial partitioning.

#### Constructor
```cpp
Solver(float width, float height);
```

#### Public Methods

##### `void update(float dt)`
Updates all particles and resolves collisions.

##### `void addParticle(const Particle& particle)`
Adds a new particle to the simulation.

##### `void clearParticles()`
Removes all particles from the simulation.

##### `void render(Shader& shader)`
Renders all particles.

---

### Particle Struct

**File**: `Collision System/particle.h`

Represents a single particle in the collision system.

```cpp
struct Particle {
    glm::vec2 position;
    glm::vec2 velocity;
    glm::vec2 acceleration;
    float radius;
    glm::vec3 color;
    float mass;
    
    // Constructor
    Particle(glm::vec2 pos, float r, glm::vec3 col = glm::vec3(1.0f));
    
    // Update particle physics
    void update(float dt);
    
    // Check collision with another particle
    bool checkCollision(const Particle& other) const;
};
```

---

## Geometry System

### GeometryData Class

**File**: `geometry/geometry_data.h`, `geometry/geometry_data.cpp`

Provides utilities for generating procedural geometry.

#### Static Methods

##### `std::vector<float> generateSphere(float radius, int sectors, int stacks)`
Generates vertex data for a sphere.

##### `std::vector<float> generatePlane(float width, float height, int subdivisions)`
Generates vertex data for a subdivided plane.

##### `std::vector<float> generateCube(float size)`
Generates vertex data for a cube.

#### Usage Example
```cpp
auto sphereData = GeometryData::generateSphere(1.0f, 36, 18);

// Create VAO/VBO
unsigned int VAO, VBO;
glGenVertexArrays(1, &VAO);
glGenBuffers(1, &VBO);

glBindVertexArray(VAO);
glBindBuffer(GL_ARRAY_BUFFER, VBO);
glBufferData(GL_ARRAY_BUFFER, sphereData.size() * sizeof(float), sphereData.data(), GL_STATIC_DRAW);

// Set vertex attributes
glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
glEnableVertexAttribArray(0);
glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
glEnableVertexAttribArray(1);
```

---

### Parametric Class

**File**: `geometry/parametric.h`, `geometry/parametric.cpp`

Generates parametric surfaces using mathematical functions.

#### Static Methods

##### `std::vector<float> generateParametricSurface(ParametricFunction func, float uMin, float uMax, float vMin, float vMax, int uSteps, int vSteps)`
Generates a parametric surface using the provided function.

#### Function Type
```cpp
typedef glm::vec3 (*ParametricFunction)(float u, float v);
```

#### Example Functions
```cpp
// Torus
glm::vec3 torus(float u, float v) {
    float R = 2.0f; // Major radius
    float r = 0.5f; // Minor radius
    float x = (R + r * cos(v)) * cos(u);
    float y = (R + r * cos(v)) * sin(u);
    float z = r * sin(v);
    return glm::vec3(x, y, z);
}

// Klein bottle
glm::vec3 kleinBottle(float u, float v) {
    float x = (2.0f + cos(u/2.0f) * sin(v) - sin(u/2.0f) * sin(2.0f*v)) * cos(u);
    float y = (2.0f + cos(u/2.0f) * sin(v) - sin(u/2.0f) * sin(2.0f*v)) * sin(u);
    float z = sin(u/2.0f) * sin(v) + cos(u/2.0f) * sin(2.0f*v);
    return glm::vec3(x, y, z);
}
```

---

## Compute Shader Integration

### ComputeHelper Class

**File**: `ComputeHelper.h`

Provides utilities for working with compute shaders.

#### Static Methods

##### `unsigned int createComputeShader(const char* computePath)`
Creates and compiles a compute shader program.

##### `void dispatchCompute(unsigned int program, int numGroupsX, int numGroupsY, int numGroupsZ)`
Dispatches compute shader execution.

#### Usage Example
```cpp
// Create compute shader
unsigned int computeProgram = ComputeHelper::createComputeShader("shaders/fluid_simulation.compute");

// Set uniforms
glUseProgram(computeProgram);
glUniform1f(glGetUniformLocation(computeProgram, "deltaTime"), deltaTime);
glUniform1i(glGetUniformLocation(computeProgram, "numParticles"), NUM_PARTICLES);

// Dispatch compute shader
ComputeHelper::dispatchCompute(computeProgram, (NUM_PARTICLES + 255) / 256, 1, 1);

// Wait for completion
glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
```

---

## Error Handling

### Common Patterns

#### OpenGL Error Checking
```cpp
void checkGLError(const std::string& operation) {
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cerr << "OpenGL error in " << operation << ": " << error << std::endl;
    }
}
```

#### Shader Compilation Error Checking
```cpp
void checkShaderCompilation(unsigned int shader, const std::string& type) {
    int success;
    char infoLog[1024];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shader, 1024, NULL, infoLog);
        std::cerr << "Shader compilation error (" << type << "): " << infoLog << std::endl;
    }
}
```

---

## Performance Considerations

### Memory Management
- Use RAII principles for OpenGL resources
- Properly delete buffers, textures, and shader programs
- Avoid frequent memory allocations in render loops

### GPU Optimization
- Use instanced rendering for particles
- Minimize state changes between draw calls
- Use appropriate buffer usage patterns (GL_STATIC_DRAW, GL_DYNAMIC_DRAW, etc.)

### Compute Shader Best Practices
- Choose appropriate work group sizes (typically multiples of 32)
- Use shared memory for data accessed by multiple threads
- Minimize memory barriers and synchronization points

---

This API documentation provides a comprehensive overview of the key classes and their usage patterns. For more detailed implementation examples, refer to the source code and the main README.md file.
