<div align="center">

# OpenGL Physics Simulation Suite

A modern OpenGL framework for real-time physics: GPU fluid simulation (2D/3D), collision detection, and procedural geometry.

</div>

## Table of Contents

- Overview
- Features
- Prerequisites
- Build (Windows)
- Run the programs
   - From VS Code tasks
   - From PowerShell (optional)
- Controls
- Project structure
- Performance notes
- Troubleshooting
- License

## Overview

This suite implements multiple simulation systems with modern OpenGL (core profile) and compute shaders for GPU-accelerated physics. It includes SPH-based fluid simulation (2D/3D), a particle collision system, and a small set of procedural geometry tools.

## Features

- GPU Fluid Simulation (2D/3D) using SPH + spatial hashing
- Particle Collision System with uniform grid partitioning
- Procedural geometry: parametric surfaces, polygons, spheres, and helpers
- Modern OpenGL 3.3+ rendering, OpenGL 4.3+ compute shaders
- GLM math, GLFW windowing/input, GLAD loader, optional Assimp model loading

## Prerequisites

- Windows 10/11, GPU with OpenGL 4.3+ recommended for compute shaders
- CMake 3.15+
- Visual Studio 2019/2022 (C++17)
- You must manually download and place third‑party dependencies; they are not shipped in this repo:
   - GLFW 3.4 — headers and compiled library (find_package + link)
   - Assimp — headers and compiled library + DLL (used by OpenGLProject)
   - GLAD, GLM, stb_image — headers and GLAD source file

Expected header locations under this repo (manual copy):
- `includes/glad/` (must contain `include/glad/glad.h` and `src/glad.c` per this CMake)
- `includes/glm/` (GLM headers)
- `includes/stb_image.h`
- `includes/assimp/include/` (Assimp headers)
- Optionally `includes/GLFW/` if you want local GLFW headers; the library itself is located via `lib/glfw3`.

## Dependency setup (required)

Choose ONE of the following setups before configuring/building.

### Option A — Local vendor layout (no package manager)

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

## Run the programs

### From VS Code tasks

- Run GPUFluidSim 2D (Debug) → launches `GPUFluidSim2D.exe`
- Run GPUFluidSim 3D (Debug) → launches `GPUFluidSim3D.exe`
- Run Collision System 2D (Debug) → launches `CollisionSystem.exe`
- Run OpenGL Project (Debug) → launches `OpenGLProject.exe`

Release variants are available for the main demo and collision system:

- Run OpenGL Project (Release)
- Run Collision System (Release)

Optional maintenance and alt build:

- Clean Build — removes build artifacts
- MinGW Build (Alternative) — experimental GCC build for quick checks

Note on shader paths: executables load shaders relative to the repo root (e.g., `SPHFluid/shaders/...`). Make sure your current directory is the project root (the VS Code tasks already do this).

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

## Controls

GPU Fluid Simulation (2D):
- Mouse move: interact with the fluid
- Left mouse: attract particles
- Right mouse: repel particles
- Space: pause/resume
- R: reset
- ESC: exit

GPU Fluid Simulation (3D):
- WASD: move camera
- Mouse: look around (cursor disabled)
- Space: pause/resume
- R: reset simulation
- ESC: exit

Collision System (2D):
- Auto‑spawning particle streams from the left
- Right‑click or C: clear particles
- ESC: exit

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

## Project structure

```
├── SPHFluid/             # Fluid simulation (2D/3D) and compute shaders
│   ├── 2D/
│   ├── 3D/
│   └── shaders/
├── Collision System/     # Particle collision detection demo
├── geometry/             # Procedural geometry and helpers
├── shaders/              # Vertex/fragment shaders
├── models/               # Assets for the main demo
├── includes/             # Third‑party headers (GLFW, GLAD, GLM, stb, Assimp)
├── lib/                  # Prebuilt libraries (GLFW, Assimp, etc.)
├── build/                # CMake build tree (generated)
└── output/               # Built executables (Debug/Release)
```

## Performance notes

- 2D particles: 1k–10k+ depending on GPU
- 3D particles: 1k–5k typical for smooth real-time
- Memory: ~100–200 MB for common presets
- Dedicated GPU strongly recommended for 3D + compute

## Troubleshooting

Black screen or no motion:
- Verify GPU and drivers support OpenGL 4.3+ for compute shaders
- Try the Release build for performance-sensitive paths

Slow performance:
- Reduce particle count and grid resolution in the simulation

Build errors on Windows:
- Ensure CMake and Visual Studio are installed and on PATH
- Use the provided VS Code tasks to avoid environment conflicts

Missing DLLs:
- Visual C++ Redistributable may be required for some systems

## License

This project is provided for educational and research purposes. Third‑party libraries retain their respective licenses.
