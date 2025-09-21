# Quick Start Guide

## 🚀 Get Running in 5 Minutes

This guide will get you up and running with the OpenGL Physics Simulation Suite as quickly as possible.

## Prerequisites Check

Before starting, ensure you have:
- ✅ Windows 10/11 (64-bit)
- ✅ Visual Studio 2019/2022 with C++ support
- ✅ CMake 3.15+
- ✅ GPU with OpenGL 4.3+ support

## Method 1: vcpkg (Recommended)

### Step 1: Install vcpkg
```powershell
# Clone vcpkg
git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg
cd C:\vcpkg

# Bootstrap vcpkg
.\bootstrap-vcpkg.bat

# Integrate with Visual Studio
.\vcpkg integrate install
```

### Step 2: Install Dependencies
```powershell
# Install required packages
.\vcpkg install glfw3:x64-windows
.\vcpkg install assimp:x64-windows
.\vcpkg install glad:x64-windows
.\vcpkg install glm:x64-windows
```

### Step 3: Clone and Build
```powershell
# Clone the repository
git clone <your-repo-url> "C:\OpenGL-Physics-Suite"
cd "C:\OpenGL-Physics-Suite"

# Configure with vcpkg
cmake -B build -S . -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE="C:\vcpkg\scripts\buildsystems\vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows

# Build the project
cmake --build build --config Debug
```

### Step 4: Run Applications
```powershell
# Navigate to project root
cd "C:\OpenGL-Physics-Suite"

# Run the 2D fluid simulation
.\output\Debug\GPUFluidSim2D.exe

# Run the 3D fluid simulation
.\output\Debug\GPUFluidSim3D.exe

# Run the collision system
.\output\Debug\CollisionSystem.exe

# Run the main demo
.\output\Debug\OpenGLProject.exe
```

## Method 2: Manual Setup (Alternative)

If you prefer manual dependency management:

### Step 1: Download Dependencies

Create the following directory structure:
```
OpenGL-C/
├── includes/
│   ├── glad/
│   ├── glm/
│   ├── assimp/
│   └── stb_image.h
└── lib/
    ├── glfw3/
    └── assimp/
```

### Step 2: Install GLFW
1. Download GLFW 3.4 source from https://www.glfw.org/
2. Build with CMake:
   ```powershell
   cmake -B build -G "Visual Studio 17 2022" -A x64
   cmake --build build --config Debug
   cmake --build build --config Release
   ```
3. Copy built libraries to `lib/glfw3/`

### Step 3: Install Other Dependencies
- **GLAD**: Generate from https://glad.dav1d.de/ (OpenGL 4.3, Core profile)
- **GLM**: Download headers from https://github.com/g-truc/glm
- **Assimp**: Build from source or download prebuilt binaries
- **stb_image**: Download single header from https://github.com/nothings/stb

### Step 4: Build Project
```powershell
cmake -B build -S . -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
```

## First Run Experience

### 2D Fluid Simulation
1. Launch `GPUFluidSim2D.exe`
2. Move your mouse to interact with particles
3. Left-click to attract particles
4. Right-click to repel particles
5. Press `Space` to pause/resume
6. Press `R` to reset

### 3D Fluid Simulation
1. Launch `GPUFluidSim3D.exe`
2. Use `WASD` to move the camera
3. Move mouse to look around
4. Press `Space` to pause/resume
5. Press `R` to reset

### Collision System
1. Launch `CollisionSystem.exe`
2. Watch particles spawn automatically
3. Right-click or press `C` to clear particles
4. Observe collision detection in action

## Troubleshooting Quick Fixes

### Black Screen
```powershell
# Check OpenGL support
dxdiag
# Look for OpenGL version in Display tab
```

### Build Errors
```powershell
# Clean and rebuild
cmake --build build --target clean
cmake --build build --config Debug
```

### Missing DLLs
```powershell
# Install Visual C++ Redistributable
# Download from Microsoft's website
```

### Performance Issues
- Try Release build: `cmake --build build --config Release`
- Reduce particle count in source code
- Update GPU drivers

## Next Steps

Once you have the basic setup working:

1. **Explore the Code**: Check out the source files in each directory
2. **Modify Parameters**: Adjust particle counts, physics constants
3. **Add Features**: Implement new simulation types or rendering effects
4. **Read Documentation**: Check out `docs/API_DOCUMENTATION.md`

## Getting Help

If you encounter issues:

1. **Check System Requirements**: Ensure OpenGL 4.3+ support
2. **Review Error Messages**: Look for specific error details
3. **Try Different Build Configs**: Debug vs Release
4. **Update Dependencies**: Ensure latest versions

## Common Commands Reference

```powershell
# Build commands
cmake -B build -S .                    # Configure
cmake --build build --config Debug     # Build Debug
cmake --build build --config Release   # Build Release

# Run commands (from project root)
.\output\Debug\GPUFluidSim2D.exe      # 2D Fluid
.\output\Debug\GPUFluidSim3D.exe      # 3D Fluid
.\output\Debug\CollisionSystem.exe    # Collisions
.\output\Debug\OpenGLProject.exe      # Main Demo

# Cleanup
cmake --build build --target clean    # Clean build
rmdir /s build                        # Remove build directory
```

---

**🎉 Congratulations!** You should now have a working OpenGL Physics Simulation Suite. Enjoy exploring real-time physics simulation!
