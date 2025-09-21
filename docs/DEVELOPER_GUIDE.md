# Developer Guide

## 🛠️ Development Environment Setup

This guide covers advanced setup, development workflows, and contribution guidelines for the OpenGL Physics Simulation Suite.

## Table of Contents

- [Development Environment](#development-environment)
- [Code Architecture](#code-architecture)
- [Adding New Features](#adding-new-features)
- [Performance Optimization](#performance-optimization)
- [Debugging Techniques](#debugging-techniques)
- [Testing Guidelines](#testing-guidelines)
- [Contribution Workflow](#contribution-workflow)

## Development Environment

### Recommended IDE Setup

#### Visual Studio Code
```json
// .vscode/settings.json
{
    "C_Cpp.default.configurationProvider": "ms-vscode.cmake-tools",
    "cmake.buildDirectory": "${workspaceFolder}/build",
    "cmake.generator": "Visual Studio 17 2022",
    "cmake.platform": "x64"
}
```

#### Visual Studio 2022
- Install "Game development with C++" workload
- Enable CMake tools
- Configure vcpkg integration

### Build Configurations

#### Debug Configuration
```cmake
# Optimized for debugging
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")
```

#### Release Configuration
```cmake
# Optimized for performance
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
```

#### Profile Configuration
```cmake
# Optimized with debug symbols for profiling
set(CMAKE_BUILD_TYPE RelWithDebInfo)
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG")
```

## Code Architecture

### Project Structure Overview

```
OpenGL-C/
├── Core Systems/
│   ├── Rendering Pipeline
│   ├── Shader Management
│   ├── Resource Loading
│   └── Input Handling
├── Physics Systems/
│   ├── SPH Fluid Simulation
│   ├── Collision Detection
│   └── Spatial Partitioning
├── Graphics Systems/
│   ├── Geometry Generation
│   ├── Particle Rendering
│   └── Post-processing
└── Utilities/
    ├── Math Helpers
    ├── File I/O
    └── Debug Tools
```

### Design Patterns Used

#### Resource Management (RAII)
```cpp
class Shader {
private:
    unsigned int ID;
    
public:
    Shader(const char* vertexPath, const char* fragmentPath) {
        // Load and compile shaders
        ID = createShaderProgram(vertexPath, fragmentPath);
    }
    
    ~Shader() {
        glDeleteProgram(ID);
    }
    
    // Disable copy, enable move
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&& other) noexcept : ID(other.ID) {
        other.ID = 0;
    }
};
```

#### Component System
```cpp
class Entity {
    std::vector<std::unique_ptr<Component>> components;
    
public:
    template<typename T, typename... Args>
    T* AddComponent(Args&&... args) {
        auto component = std::make_unique<T>(std::forward<Args>(args)...);
        T* ptr = component.get();
        components.push_back(std::move(component));
        return ptr;
    }
};
```

#### Observer Pattern for Events
```cpp
class EventManager {
    std::unordered_map<EventType, std::vector<std::function<void(Event&)>>> listeners;
    
public:
    void Subscribe(EventType type, std::function<void(Event&)> callback) {
        listeners[type].push_back(callback);
    }
    
    void Publish(Event& event) {
        for (auto& callback : listeners[event.GetType()]) {
            callback(event);
        }
    }
};
```

## Adding New Features

### Creating a New Simulation Type

#### 1. Define the Simulation Class
```cpp
// NewSimulation.h
class NewSimulation {
private:
    std::vector<Particle> particles;
    unsigned int computeShader;
    unsigned int particleBuffer;
    
public:
    NewSimulation(int numParticles);
    ~NewSimulation();
    
    void Initialize();
    void Update(float deltaTime);
    void Render(const glm::mat4& view, const glm::mat4& projection);
    
private:
    void InitializeComputeShader();
    void InitializeBuffers();
    void UpdatePhysics(float deltaTime);
};
```

#### 2. Implement Core Methods
```cpp
// NewSimulation.cpp
void NewSimulation::Update(float deltaTime) {
    // Bind compute shader
    glUseProgram(computeShader);
    
    // Set uniforms
    glUniform1f(glGetUniformLocation(computeShader, "deltaTime"), deltaTime);
    glUniform1i(glGetUniformLocation(computeShader, "numParticles"), particles.size());
    
    // Dispatch compute shader
    glDispatchCompute((particles.size() + 255) / 256, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}
```

#### 3. Create Compute Shader
```glsl
// new_simulation.compute
#version 430

layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

layout(std430, binding = 0) restrict buffer ParticleBuffer {
    vec4 positions[];
};

layout(std430, binding = 1) restrict buffer VelocityBuffer {
    vec4 velocities[];
};

uniform float deltaTime;
uniform int numParticles;

void main() {
    uint index = gl_GlobalInvocationID.x;
    if (index >= numParticles) return;
    
    // Your simulation logic here
    vec3 position = positions[index].xyz;
    vec3 velocity = velocities[index].xyz;
    
    // Update physics
    velocity += computeForces(position) * deltaTime;
    position += velocity * deltaTime;
    
    // Write back results
    positions[index] = vec4(position, 1.0);
    velocities[index] = vec4(velocity, 0.0);
}
```

#### 4. Add to CMakeLists.txt
```cmake
# Add new executable
add_executable(NewSimulation
    NewSimulation/main.cpp
    NewSimulation/NewSimulation.cpp
    shader.cpp
    mesh.cpp
    ${GLAD_SOURCES}
)

target_link_libraries(NewSimulation OpenGL::GL glfw)
```

### Adding New Geometry Types

#### 1. Extend GeometryData Class
```cpp
// In geometry_data.h
class GeometryData {
public:
    static std::vector<float> generateTorus(float majorRadius, float minorRadius, 
                                          int majorSegments, int minorSegments);
    static std::vector<float> generateHeightmap(const std::string& heightmapPath, 
                                               float heightScale, float textureScale);
};
```

#### 2. Implement Generation Function
```cpp
// In geometry_data.cpp
std::vector<float> GeometryData::generateTorus(float majorRadius, float minorRadius,
                                             int majorSegments, int minorSegments) {
    std::vector<float> vertices;
    
    for (int i = 0; i <= majorSegments; ++i) {
        float u = 2.0f * M_PI * i / majorSegments;
        for (int j = 0; j <= minorSegments; ++j) {
            float v = 2.0f * M_PI * j / minorSegments;
            
            // Calculate position
            float x = (majorRadius + minorRadius * cos(v)) * cos(u);
            float y = (majorRadius + minorRadius * cos(v)) * sin(u);
            float z = minorRadius * sin(v);
            
            // Calculate normal
            glm::vec3 center(majorRadius * cos(u), majorRadius * sin(u), 0.0f);
            glm::vec3 position(x, y, z);
            glm::vec3 normal = normalize(position - center);
            
            // Add vertex data (position + normal)
            vertices.insert(vertices.end(), {x, y, z, normal.x, normal.y, normal.z});
        }
    }
    
    return vertices;
}
```

## Performance Optimization

### GPU Optimization Techniques

#### 1. Memory Access Patterns
```cpp
// Bad: Non-coalesced memory access
for (int i = 0; i < numParticles; i += 32) {
    // Threads access non-contiguous memory
    process(particles[i * stride]);
}

// Good: Coalesced memory access
for (int i = threadId; i < numParticles; i += blockSize) {
    // Threads access contiguous memory
    process(particles[i]);
}
```

#### 2. Shared Memory Usage
```glsl
// In compute shader
shared vec3 sharedPositions[256];

void main() {
    uint localId = gl_LocalInvocationID.x;
    uint globalId = gl_GlobalInvocationID.x;
    
    // Load data into shared memory
    if (globalId < numParticles) {
        sharedPositions[localId] = positions[globalId].xyz;
    }
    
    barrier();
    
    // Process using shared memory
    vec3 force = vec3(0.0);
    for (uint i = 0; i < gl_WorkGroupSize.x; ++i) {
        if (i != localId) {
            force += computeForce(sharedPositions[localId], sharedPositions[i]);
        }
    }
}
```

#### 3. Instanced Rendering
```cpp
// Setup instanced rendering for particles
void setupInstancedRendering() {
    // Create instance buffer
    glGenBuffers(1, &instanceVBO);
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
    glBufferData(GL_ARRAY_BUFFER, numParticles * sizeof(InstanceData), nullptr, GL_DYNAMIC_DRAW);
    
    // Setup instance attributes
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(InstanceData), (void*)0);
    glEnableVertexAttribArray(2);
    glVertexAttribDivisor(2, 1); // Instance attribute
}

void renderParticles() {
    // Update instance data
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
    InstanceData* instanceData = (InstanceData*)glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
    
    for (int i = 0; i < numParticles; ++i) {
        instanceData[i].position = particles[i].position;
        instanceData[i].color = particles[i].color;
    }
    
    glUnmapBuffer(GL_ARRAY_BUFFER);
    
    // Render all instances
    glDrawArraysInstanced(GL_TRIANGLES, 0, 36, numParticles);
}
```

### CPU Optimization Techniques

#### 1. Spatial Data Structures
```cpp
class SpatialGrid {
private:
    float cellSize;
    std::unordered_map<int, std::vector<int>> grid;
    
    int hash(int x, int y, int z) {
        return x * 73856093 ^ y * 19349663 ^ z * 83492791;
    }
    
public:
    void insert(int particleId, const glm::vec3& position) {
        int x = (int)(position.x / cellSize);
        int y = (int)(position.y / cellSize);
        int z = (int)(position.z / cellSize);
        grid[hash(x, y, z)].push_back(particleId);
    }
    
    std::vector<int> query(const glm::vec3& position, float radius) {
        std::vector<int> neighbors;
        int range = (int)(radius / cellSize) + 1;
        
        int cx = (int)(position.x / cellSize);
        int cy = (int)(position.y / cellSize);
        int cz = (int)(position.z / cellSize);
        
        for (int x = cx - range; x <= cx + range; ++x) {
            for (int y = cy - range; y <= cy + range; ++y) {
                for (int z = cz - range; z <= cz + range; ++z) {
                    auto it = grid.find(hash(x, y, z));
                    if (it != grid.end()) {
                        neighbors.insert(neighbors.end(), it->second.begin(), it->second.end());
                    }
                }
            }
        }
        
        return neighbors;
    }
};
```

#### 2. Multi-threading
```cpp
class ThreadPool {
private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queueMutex;
    std::condition_variable condition;
    bool stop;
    
public:
    ThreadPool(size_t threads) : stop(false) {
        for (size_t i = 0; i < threads; ++i) {
            workers.emplace_back([this] {
                for (;;) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(this->queueMutex);
                        this->condition.wait(lock, [this] { return this->stop || !this->tasks.empty(); });
                        if (this->stop && this->tasks.empty()) return;
                        task = std::move(this->tasks.front());
                        this->tasks.pop();
                    }
                    task();
                }
            });
        }
    }
    
    template<class F>
    void enqueue(F&& f) {
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            tasks.emplace(std::forward<F>(f));
        }
        condition.notify_one();
    }
};

// Usage in collision detection
void updateCollisions() {
    ThreadPool pool(std::thread::hardware_concurrency());
    
    const int batchSize = particles.size() / pool.size();
    
    for (int i = 0; i < pool.size(); ++i) {
        int start = i * batchSize;
        int end = (i == pool.size() - 1) ? particles.size() : (i + 1) * batchSize;
        
        pool.enqueue([this, start, end] {
            for (int j = start; j < end; ++j) {
                updateParticleCollisions(j);
            }
        });
    }
}
```

## Debugging Techniques

### OpenGL Debugging

#### 1. Debug Context Setup
```cpp
void setupDebugContext() {
    // Enable debug output
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    
    // Set debug callback
    glDebugMessageCallback(debugCallback, nullptr);
    
    // Filter messages
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);
}

void GLAPIENTRY debugCallback(GLenum source, GLenum type, GLuint id, GLenum severity,
                             GLsizei length, const GLchar* message, const void* userParam) {
    std::cout << "OpenGL Debug: " << message << std::endl;
    
    if (severity == GL_DEBUG_SEVERITY_HIGH) {
        // Break on high severity errors
        __debugbreak();
    }
}
```

#### 2. Shader Debugging
```cpp
void debugShader(unsigned int shader, const std::string& name) {
    int success;
    char infoLog[1024];
    
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shader, 1024, NULL, infoLog);
        std::cerr << "Shader compilation error (" << name << "):\n" << infoLog << std::endl;
        
        // Print shader source for debugging
        int sourceLength;
        glGetShaderiv(shader, GL_SHADER_SOURCE_LENGTH, &sourceLength);
        std::vector<char> source(sourceLength);
        glGetShaderSource(shader, sourceLength, NULL, source.data());
        std::cout << "Shader source:\n" << source.data() << std::endl;
    }
}
```

#### 3. Performance Profiling
```cpp
class GPUTimer {
private:
    unsigned int queries[2];
    int currentQuery;
    
public:
    GPUTimer() : currentQuery(0) {
        glGenQueries(2, queries);
    }
    
    void begin() {
        glBeginQuery(GL_TIME_ELAPSED, queries[currentQuery]);
    }
    
    void end() {
        glEndQuery(GL_TIME_ELAPSED);
    }
    
    float getTime() {
        unsigned int timeElapsed;
        glGetQueryObjectuiv(queries[currentQuery], GL_QUERY_RESULT, &timeElapsed);
        currentQuery = 1 - currentQuery; // Swap queries
        return timeElapsed / 1000000.0f; // Convert to milliseconds
    }
};

// Usage
GPUTimer timer;
timer.begin();
// Render operations
timer.end();
float renderTime = timer.getTime();
```

## Testing Guidelines

### Unit Testing Framework
```cpp
// Simple testing framework
class TestFramework {
private:
    int passed = 0;
    int failed = 0;
    
public:
    template<typename T>
    void assertEqual(const T& expected, const T& actual, const std::string& testName) {
        if (expected == actual) {
            std::cout << "PASS: " << testName << std::endl;
            passed++;
        } else {
            std::cout << "FAIL: " << testName << " - Expected: " << expected << ", Actual: " << actual << std::endl;
            failed++;
        }
    }
    
    void printResults() {
        std::cout << "Tests passed: " << passed << ", Tests failed: " << failed << std::endl;
    }
};

// Example tests
void testMathFunctions() {
    TestFramework test;
    
    test.assertEqual(5.0f, glm::length(glm::vec3(3, 4, 0)), "Vector length test");
    test.assertEqual(glm::vec3(0, 1, 0), glm::normalize(glm::vec3(0, 5, 0)), "Vector normalize test");
    
    test.printResults();
}
```

### Performance Benchmarking
```cpp
class Benchmark {
private:
    std::chrono::high_resolution_clock::time_point startTime;
    
public:
    void start() {
        startTime = std::chrono::high_resolution_clock::now();
    }
    
    double stop() {
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
        return duration.count() / 1000.0; // Return milliseconds
    }
};

void benchmarkFluidSimulation() {
    Benchmark bench;
    
    bench.start();
    fluidSimulation.update(0.016f); // 60 FPS
    double updateTime = bench.stop();
    
    std::cout << "Fluid simulation update time: " << updateTime << "ms" << std::endl;
}
```

## Contribution Workflow

### Git Workflow

#### 1. Feature Branch Workflow
```bash
# Create feature branch
git checkout -b feature/new-simulation-type

# Make changes and commit
git add .
git commit -m "Add new simulation type with compute shaders"

# Push to remote
git push origin feature/new-simulation-type

# Create pull request
```

#### 2. Commit Message Format
```
type(scope): brief description

Detailed explanation of changes made.

- Added new feature X
- Fixed bug in Y
- Improved performance of Z

Closes #123
```

### Code Review Checklist

#### Before Submitting PR
- [ ] Code compiles without warnings
- [ ] All tests pass
- [ ] Performance benchmarks show no regression
- [ ] Documentation updated
- [ ] Code follows project style guidelines

#### Review Criteria
- [ ] Code is readable and well-commented
- [ ] No memory leaks or resource leaks
- [ ] Proper error handling
- [ ] Thread safety where applicable
- [ ] OpenGL best practices followed

### Release Process

#### 1. Version Numbering
```
MAJOR.MINOR.PATCH
- MAJOR: Breaking changes
- MINOR: New features, backward compatible
- PATCH: Bug fixes, backward compatible
```

#### 2. Release Checklist
- [ ] Update version numbers
- [ ] Update CHANGELOG.md
- [ ] Run full test suite
- [ ] Build release binaries
- [ ] Create release notes
- [ ] Tag release in Git

---

This developer guide provides comprehensive information for contributing to and extending the OpenGL Physics Simulation Suite. For specific implementation details, refer to the source code and API documentation.
