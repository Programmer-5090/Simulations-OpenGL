<div align="center">

# OpenGL Physics Simulation Suite

A modern OpenGL framework for real-time physics: GPU fluid simulation (2D/3D), collision detection, and procedural geometry.

![OpenGL](https://img.shields.io/badge/OpenGL-4.3+-blue.svg)
![C++](https://img.shields.io/badge/C++-17-green.svg)
![Platform](https://img.shields.io/badge/Platform-Windows-lightgrey.svg)
![License](https://img.shields.io/badge/License-Educational-orange.svg)

</div>

## Table of Contents

- [🎯 Overview](#-overview)
- [✨ Features](#-features)
- [📋 Prerequisites](#-prerequisites)
- [🏗️ Quick Start](#️-quick-start)
- [🔧 Detailed Setup](#-detailed-setup)
- [🚀 Running the Applications](#-running-the-applications)
- [🎮 Controls](#-controls)
- [📁 Project Architecture](#-project-architecture)
- [⚡ Performance Notes](#-performance-notes)
- [🐛 Troubleshooting](#-troubleshooting)
- [🤝 Contributing](#-contributing)
- [📄 License](#-license)

## 🎯 Overview

This comprehensive physics simulation suite demonstrates advanced real-time graphics programming techniques using modern OpenGL. The project showcases three main simulation systems:

1. **SPH Fluid Simulation** - Smoothed Particle Hydrodynamics implementation with GPU compute shaders
2. **Collision Detection System** - Efficient particle collision handling with spatial partitioning
3. **Procedural Geometry Engine** - Dynamic generation of mathematical surfaces and shapes

The codebase serves as both a learning resource for graphics programming concepts and a foundation for physics-based applications.

## ✨ Features

### Core Simulation Systems
- **🌊 GPU Fluid Simulation (2D/3D)** - SPH-based fluid dynamics with spatial hashing optimization
- **💥 Particle Collision System** - Real-time collision detection using uniform grid partitioning
- **📐 Procedural Geometry** - Parametric surfaces, polygons, spheres, and mathematical helpers

### Technical Stack
- **🎮 Modern OpenGL** - Core profile 3.3+ rendering, compute shaders 4.3+
- **🧮 Mathematics** - GLM for linear algebra operations
- **🖼️ Windowing** - GLFW for cross-platform window management and input
- **🔧 Asset Loading** - GLAD OpenGL loader, optional Assimp model loading
- **⚡ GPU Acceleration** - Compute shaders for high-performance parallel processing

## 📋 Prerequisites

### System Requirements
- **OS**: Windows 10/11 (64-bit)
- **GPU**: OpenGL 4.3+ support (recommended for compute shaders)
- **RAM**: 4GB minimum, 8GB recommended
- **Storage**: ~2GB for dependencies and build artifacts

### Development Tools
- **CMake**: 3.15 or higher
- **Visual Studio**: 2019/2022 with C++17 support
- **Git**: For cloning and version control

### Third-Party Dependencies
> ⚠️ **Important**: Dependencies are not included in this repository and must be manually installed.

| Library | Version | Purpose |
|---------|---------|---------|
| GLFW | 3.4+ | Window management and input handling |
| Assimp | Latest | 3D model loading (OpenGLProject only) |
| GLAD | OpenGL 4.3+ | OpenGL function loading |
| GLM | Latest | Mathematics library |
| stb_image | Latest | Image loading |

## 🏗️ Quick Start

### Option 1: Using vcpkg (Recommended)

1. **Install vcpkg**:
   ```powershell
   git clone https://github.com/Microsoft/vcpkg.git
   cd vcpkg
   .\bootstrap-vcpkg.bat
   .\vcpkg integrate install
   ```

2. **Install dependencies**:
   ```powershell
   .\vcpkg install glfw3:x64-windows assimp:x64-windows
   ```

3. **Clone and build**:
   ```powershell
   git clone <your-repo-url>
   cd "OpenGL-C"
   cmake -B build -S . -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake"
   cmake --build build --config Debug
   ```

4. **Run applications**:
   ```powershell
   .\output\Debug\GPUFluidSim2D.exe
   ```

### Option 2: Manual Setup

If you prefer manual dependency management, see the [Detailed Setup](#-detailed-setup) section below.

## 🔧 Detailed Setup

Choose ONE of the following setups before configuring/building.

### Manual Dependency Setup

Place compiled libraries and configs into this repo so CMake can find them via `CMAKE_PREFIX_PATH` and explicit link dirs:

Place headers under `includes/` and compiled libraries under `lib/` as follows.

- GLFW 3.4 (required for all programs)
   - Build from source with CMake (Visual Studio 2022 x64, Debug/Release)
   - Install to: `<repo>/lib/glfw3`
      - Must contain `lib/glfw3/lib/glfw3.lib` and `lib/glfw3/lib/cmake/glfw3/GLFW3Config.cmake`
   - Headers: either install system‑wide or copy to `includes/GLFW/`

- Assimp (required for OpenGLProject executable)
   - Build from source with CMake (Visual Studio 2022 x64)
   - Install to: `<repo>/lib/assimp`
      - Expected libs: `lib/assimp/lib/assimp-vc143-mt.lib`, `lib/assimp/lib/zlibstatic.lib`
      - Expected DLL: `lib/assimp/bin/assimp-vc143-mt.dll` (copied to runtime dir post‑build for OpenGLProject)
   - Headers: copy to `includes/assimp/include/`

- GLAD, GLM, stb_image (headers only + GLAD source)
   - GLAD: copy to `includes/glad/` and ensure `includes/glad/src/glad.c` exists (the CMake compiles it)
   - GLM: copy to `includes/glm/`
   - stb_image: place `includes/stb_image.h`

Notes:
- `CMakeLists.txt` sets `CMAKE_PREFIX_PATH` to `lib/glfw3` and `lib/assimp` so `find_package(glfw3)` works and Assimp libs are located.
- If your filenames differ (toolset/version), adjust `CMakeLists.txt` or rename the binaries to match.

### Option B — vcpkg (recommended if you prefer a package manager)

1) Install vcpkg and integrate with VS/VS Code
2) Install packages:
    - `vcpkg install glfw3:x64-windows`
    - `vcpkg install assimp:x64-windows`
3) Configure with vcpkg toolchain (manual configure example):

```powershell
cmake -B build -S . -G "Visual Studio 17 2022" -A x64 `
   -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake" `
   -DVCPKG_TARGET_TRIPLET=x64-windows
```

You can then build with the existing VS Code tasks or via Visual Studio.

## Program details and configs

All paths are relative to the repo root (tasks set the working directory accordingly).

1) GPUFluidSim 2D
- OpenGL context: 4.3 Core (compute shaders)
- Window: 1200 x 800
- Default particles: 10,000
- Shaders: `SPHFluid/shaders/FluidSim-2D.compute`, `SPHFluid/shaders/particle2d.vs`, `SPHFluid/shaders/particle2d.fs`
- World bounds: x=[-10, 10], y=[-6.67, 6.67]

2) GPUFluidSim 3D
- OpenGL context: 4.3 Core (compute shaders)
- Window: 1280 x 720
- Default particles: 10,000
- Shaders: `SPHFluid/shaders/FluidSim-3D.compute`, `SPHFluid/shaders/particle3d.vs`, `SPHFluid/shaders/particle3d.fs`, `SPHFluid/shaders/box.*`, `shaders/infinite_grid.*`
- Simulation bounds: axis‑aligned box of size 4 x 4 x 4

3) CollisionSystem (2D)
- OpenGL context: 3.3 Core
- Window title: "Simple Circle Renderer"
- Auto‑spawning streams from the left edge; performance‑aware throttling
- Shaders: `shaders/vertex.vs`, `shaders/simple_fragment.fs`

4) OpenGLProject (main demo)
- OpenGL context: 3.3 Core
- Demonstrates geometry and basic rendering with an infinite grid
- Shaders: `shaders/vertex.vs`, `shaders/simple_fragment.fs`, `shaders/infinite_grid.*`, optional normal debug `shaders/normal_debug.*`

## Build (Windows)

Use the provided VS Code tasks (Terminal → Run Task…):

1) CMake Configure (Windows) — generates the solution into `build/`
2) CMake Build (Windows) — builds all targets in Debug
3) CMake Build Release (Windows) — builds in Release (optional)

Executables are written to `output/Debug/` and `output/Release/` (per configuration).

Tip: A Visual Studio solution is generated at `build/OpenGLProject.sln` if you prefer opening it directly in VS.

## 🚀 Running the Applications

### Available Applications

The project builds four main executables:

| Application | Description | Features |
|-------------|-------------|----------|
| `GPUFluidSim2D.exe` | 2D fluid simulation | Mouse interaction, 10k particles, real-time physics |
| `GPUFluidSim3D.exe` | 3D fluid simulation | Camera controls, 3D visualization, compute shaders |
| `CollisionSystem.exe` | Particle collision demo | Auto-spawning particles, collision detection |
| `OpenGLProject.exe` | Main geometry demo | Procedural shapes, infinite grid, model loading |

### Quick Launch

After building, navigate to the project root and run:

```powershell
# Navigate to project directory
cd "c:\Users\dimai\Documents\Programming\Programming\Gravi Sim\OpenGL-C"

# Launch applications (Debug build)
.\output\Debug\GPUFluidSim2D.exe    # 2D fluid simulation
.\output\Debug\GPUFluidSim3D.exe    # 3D fluid simulation  
.\output\Debug\CollisionSystem.exe  # Collision detection demo
.\output\Debug\OpenGLProject.exe    # Main geometry demo

# Launch applications (Release build - better performance)
.\output\Release\GPUFluidSim2D.exe
.\output\Release\GPUFluidSim3D.exe
.\output\Release\CollisionSystem.exe
.\output\Release\OpenGLProject.exe
```

> ⚠️ **Important**: Always run applications from the project root directory. Shaders and assets are loaded using relative paths from this location.

### Using VS Code Tasks

If using VS Code, you can use the predefined tasks:

- **Run GPUFluidSim 2D (Debug)** → launches `GPUFluidSim2D.exe`
- **Run GPUFluidSim 3D (Debug)** → launches `GPUFluidSim3D.exe`
- **Run Collision System 2D (Debug)** → launches `CollisionSystem.exe`
- **Run OpenGL Project (Debug)** → launches `OpenGLProject.exe`

Release variants:
- **Run OpenGL Project (Release)**
- **Run Collision System (Release)**

### From PowerShell (optional)

Run these locally, after a successful build. One command per line.

```powershell
# Navigate to the repo root (path contains spaces, so keep the quotes)
cd "c:\Users\dimai\Documents\Programming\Programming\Gravi Sim\OpenGL-C"

# Debug configuration binaries
.\output\Debug\GPUFluidSim2D.exe
.\output\Debug\GPUFluidSim3D.exe
.\output\Debug\CollisionSystem.exe
.\output\Debug\OpenGLProject.exe

# Release configuration binaries (after building Release)
.\output\Release\GPUFluidSim2D.exe
.\output\Release\GPUFluidSim3D.exe
.\output\Release\CollisionSystem.exe
.\output\Release\OpenGLProject.exe
```

## 🎮 Controls

### GPU Fluid Simulation (2D)
| Input | Action |
|-------|--------|
| **Mouse Move** | Interact with fluid particles |
| **Left Click** | Attract particles to cursor |
| **Right Click** | Repel particles from cursor |
| **Space** | Pause/resume simulation |
| **R** | Reset simulation |
| **ESC** | Exit application |

### GPU Fluid Simulation (3D)
| Input | Action |
|-------|--------|
| **WASD** | Move camera (forward/back/left/right) |
| **Mouse** | Look around (cursor disabled) |
| **Space** | Pause/resume simulation |
| **R** | Reset simulation |
| **ESC** | Exit application |

### Collision System (2D)
| Input | Action |
|-------|--------|
| **Auto-spawn** | Particles spawn automatically from left edge |
| **Right Click / C** | Clear all particles |
| **ESC** | Exit application |

### OpenGL Project (Main Demo)
| Input | Action |
|-------|--------|
| **Mouse** | Camera controls |
| **WASD** | Navigate scene |
| **ESC** | Exit application |

## Verify your setup

Before configuring/building, confirm these paths exist (examples):

- Headers
   - `includes/glad/include/glad/glad.h`
   - `includes/glad/src/glad.c`
   - `includes/glm/glm.hpp` (GLM folder structure)
   - `includes/stb_image.h`
   - `includes/assimp/include/assimp/Importer.hpp`
   - Optional: `includes/GLFW/glfw3.h`
- Libraries
   - `lib/glfw3/lib/cmake/glfw3/GLFW3Config.cmake`
   - `lib/glfw3/lib/glfw3.lib`
   - `lib/assimp/lib/assimp-vc143-mt.lib`
   - `lib/assimp/lib/zlibstatic.lib`
   - `lib/assimp/bin/assimp-vc143-mt.dll` (required at runtime for OpenGLProject)

## 📁 Project Architecture

### Directory Structure

```
OpenGL-C/
├── 🌊 SPHFluid/                    # Fluid simulation systems
│   ├── 2D/                        # 2D fluid simulation implementation
│   │   ├── gpuFluidWindow.cpp     # Main 2D window and event handling
│   │   ├── GPUFluidSimulation.cpp # 2D SPH physics computation
│   │   └── GPUParticleDisplay.cpp # 2D particle rendering
│   ├── 3D/                        # 3D fluid simulation implementation
│   │   ├── window3D.cpp           # Main 3D window and camera
│   │   ├── GPUFluidSimulation.cpp # 3D SPH physics computation
│   │   ├── GPUParticleDisplay.cpp # 3D particle rendering
│   │   └── BoundingBox.cpp        # 3D simulation boundaries
│   ├── shaders/                   # Compute and rendering shaders
│   │   ├── FluidSim-2D.compute    # 2D SPH compute shader
│   │   ├── FluidSim-3D.compute    # 3D SPH compute shader
│   │   ├── particle2d.vs/.fs      # 2D particle rendering
│   │   └── particle3d.vs/.fs      # 3D particle rendering
│   └── GPUSort.cpp/.h             # GPU-based sorting utilities
│
├── 💥 Collision System/            # Particle collision detection
│   ├── window2d.cpp               # Main collision demo window
│   ├── solver.cpp/.h              # Collision detection algorithms
│   ├── particle.h                 # Particle data structures
│   ├── grid.h                     # Spatial partitioning grid
│   └── constants.h                # Physics constants
│
├── 📐 geometry/                    # Procedural geometry generation
│   ├── geometry_data.cpp/.h       # Core geometry utilities
│   ├── parametric.cpp/.h          # Parametric surface generation
│   ├── polygon.cpp/.h             # Polygon generation
│   ├── sphere.cpp/.h              # Sphere generation
│   └── circle.h                   # Circle utilities
│
├── 🎨 shaders/                     # Core rendering shaders
│   ├── vertex.vs                  # Standard vertex shader
│   ├── fragment.fs                # Standard fragment shader
│   ├── simple_fragment.fs         # Simplified fragment shader
│   ├── infinite_grid.vs/.fs       # Infinite grid rendering
│   └── normal_debug.vs/.gs/.fs    # Normal visualization
│
├── 🏗️ Core Files/                  # Main application framework
│   ├── window.cpp                 # Main demo application
│   ├── shader.cpp/.h              # Shader loading and management
│   ├── mesh.cpp/.h                # Mesh data structures
│   ├── model.cpp/.h               # 3D model loading
│   ├── camera.h                   # Camera controls
│   └── ComputeHelper.h            # Compute shader utilities
│
├── 📦 Dependencies/                # Third-party libraries
│   ├── includes/                  # Header files (GLAD, GLM, etc.)
│   └── lib/                       # Compiled libraries
│
├── 🔨 Build System/                # Build configuration
│   ├── CMakeLists.txt             # Main CMake configuration
│   ├── build/                     # Generated build files
│   └── output/                    # Compiled executables
│       ├── Debug/                 # Debug builds
│       └── Release/               # Release builds
│
└── 📄 Documentation/               # Project documentation
    ├── README.md                  # This file
    └── geometry/GeometryData_README.md  # Geometry system docs
```

### Key Components

#### 🌊 SPH Fluid System
- **Physics Engine**: Smoothed Particle Hydrodynamics implementation
- **GPU Acceleration**: Compute shaders for parallel particle updates
- **Spatial Hashing**: Efficient neighbor finding for particle interactions
- **Rendering**: Instanced particle rendering with dynamic coloring

#### 💥 Collision System  
- **Spatial Partitioning**: Uniform grid for efficient collision detection
- **Multi-threading**: Parallel collision resolution
- **Dynamic Spawning**: Real-time particle generation and management

#### 📐 Geometry Engine
- **Parametric Surfaces**: Mathematical surface generation
- **Procedural Shapes**: Runtime geometry creation
- **Mesh Management**: Efficient vertex buffer handling

## ⚡ Performance Notes

### Recommended Specifications

| Component | Minimum | Recommended |
|-----------|---------|-------------|
| **GPU** | OpenGL 3.3+ | OpenGL 4.3+ with compute shader support |
| **VRAM** | 1GB | 4GB+ |
| **RAM** | 4GB | 8GB+ |
| **CPU** | Dual-core | Quad-core+ |

### Performance Characteristics

#### 2D Fluid Simulation
- **Particle Count**: 1,000 - 10,000+ (GPU dependent)
- **Frame Rate**: 60+ FPS on modern GPUs
- **Memory Usage**: ~50-100 MB

#### 3D Fluid Simulation  
- **Particle Count**: 1,000 - 5,000 (typical for smooth real-time)
- **Frame Rate**: 30-60 FPS on dedicated GPUs
- **Memory Usage**: ~100-200 MB

#### Collision System
- **Particle Count**: 5,000+ concurrent particles
- **Frame Rate**: 60+ FPS with spatial partitioning
- **Memory Usage**: ~20-50 MB

### Optimization Tips

1. **Reduce Particle Count**: Lower particle counts for better performance
2. **Use Release Builds**: Significant performance improvement over Debug
3. **Dedicated GPU**: Integrated graphics may struggle with compute shaders
4. **Update Drivers**: Ensure latest GPU drivers for OpenGL support

## 🐛 Troubleshooting

### Common Issues

#### Black Screen or No Motion
**Symptoms**: Application launches but shows black screen or frozen particles

**Solutions**:
- ✅ Verify GPU supports OpenGL 4.3+ for compute shaders
- ✅ Update graphics drivers to latest version
- ✅ Try Release build instead of Debug for performance-critical paths
- ✅ Check Windows Event Viewer for OpenGL errors

#### Slow Performance
**Symptoms**: Low frame rates, stuttering, or lag

**Solutions**:
- ✅ Reduce particle count in simulation parameters
- ✅ Lower grid resolution settings
- ✅ Close other GPU-intensive applications
- ✅ Use dedicated GPU instead of integrated graphics

#### Build Errors
**Symptoms**: CMake configuration or compilation failures

**Solutions**:
- ✅ Ensure CMake 3.15+ and Visual Studio 2019/2022 are installed
- ✅ Verify all dependencies are properly installed (see setup section)
- ✅ Use provided VS Code tasks to avoid environment conflicts
- ✅ Check that `CMAKE_PREFIX_PATH` points to correct library locations

#### Missing DLLs at Runtime
**Symptoms**: "DLL not found" errors when launching applications

**Solutions**:
- ✅ Install Visual C++ Redistributable for Visual Studio 2019/2022
- ✅ Ensure `assimp-vc143-mt.dll` is in the same directory as executables
- ✅ Check that all required system libraries are available

#### Shader Compilation Errors
**Symptoms**: OpenGL shader compilation failures

**Solutions**:
- ✅ Verify shader files exist in correct relative paths
- ✅ Run applications from project root directory
- ✅ Check OpenGL version compatibility
- ✅ Ensure compute shader support for fluid simulations

### Getting Help

If you encounter issues not covered here:

1. **Check GPU Compatibility**: Verify OpenGL version support
2. **Review Build Logs**: Look for specific error messages
3. **Test with Minimal Setup**: Try building with vcpkg first
4. **Update Dependencies**: Ensure latest versions of libraries

## 🤝 Contributing

We welcome contributions to improve the OpenGL Physics Simulation Suite! Here's how you can help:

### Development Setup

1. **Fork the Repository**: Create your own fork of the project
2. **Clone Locally**: `git clone <your-fork-url>`
3. **Set Up Dependencies**: Follow the setup instructions above
4. **Create Feature Branch**: `git checkout -b feature/your-feature-name`

### Contribution Guidelines

#### Code Style
- Follow existing C++ coding conventions
- Use meaningful variable and function names
- Add comments for complex algorithms
- Keep functions focused and modular

#### Areas for Contribution
- 🐛 **Bug Fixes**: Fix issues in simulation accuracy or performance
- ✨ **New Features**: Add new simulation types or rendering techniques
- 📚 **Documentation**: Improve code comments and documentation
- 🎨 **Shaders**: Enhance visual effects and rendering quality
- ⚡ **Performance**: Optimize algorithms and GPU utilization

#### Submitting Changes
1. **Test Thoroughly**: Ensure all applications build and run correctly
2. **Update Documentation**: Update README if adding new features
3. **Commit with Clear Messages**: Use descriptive commit messages
4. **Submit Pull Request**: Include description of changes and testing done

### Reporting Issues

When reporting bugs or requesting features:

1. **Check Existing Issues**: Search for similar reports first
2. **Provide System Info**: Include OS, GPU, and driver versions
3. **Include Steps to Reproduce**: Clear instructions to replicate issues
4. **Attach Logs**: Include relevant error messages or console output

## 📄 License

This project is provided for **educational and research purposes**. 

### Project License
- The original codebase is available for learning and academic use
- Commercial use requires permission from the project maintainers
- Modifications and derivatives should maintain attribution

### Third-Party Libraries
Third-party libraries retain their respective licenses:

| Library | License | Usage |
|---------|---------|-------|
| **GLFW** | zlib/libpng | Window management |
| **GLAD** | MIT | OpenGL loading |
| **GLM** | MIT | Mathematics |
| **Assimp** | BSD-3-Clause | Model loading |
| **stb_image** | MIT/Public Domain | Image loading |

### Attribution

If you use this project in academic work, please cite:
```
OpenGL Physics Simulation Suite
A modern framework for real-time physics simulation using OpenGL compute shaders
```

---

<div align="center">

**Happy Coding! 🚀**
</div>
