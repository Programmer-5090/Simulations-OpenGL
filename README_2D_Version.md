# OpenGL 2D Circle Test

This project now includes two separate executables:

## OpenGLProject.exe (3D Version)
- **File**: `window.cpp`
- **Features**: 
  - 3D perspective projection
  - Camera movement (WASD + mouse)
  - Depth testing
  - Complex lighting (directional light, material properties)
  - Infinite grid background
  - 3D spheres floating above the grid
  - Rotating 3D objects

## OpenGL2D.exe (2D Version)
- **File**: `window2d.cpp`
- **Features**:
  - 2D orthographic projection
  - No camera movement (fixed view)
  - No depth testing (pure 2D)
  - Simplified lighting
  - Dark gray background
  - Flat circles in 2D space
  - Optional rotation animation
  - Perfect for testing 2D geometry

## Building and Running

### Build both versions:
```powershell
cmake -B build -S . -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
```

### Run 3D version:
```powershell
.\output\Debug\OpenGLProject.exe
```

### Run 2D version:
```powershell
.\output\Debug\OpenGL2D.exe
```

## Use Cases

**Use the 2D version (`OpenGL2D.exe`) when you want to:**
- Test circle/polygon geometry in pure 2D
- Prototype 2D games or simulations  
- Debug geometry generation without 3D complexity
- Start new 2D projects with a clean base
- Test shaders in 2D space

**Use the 3D version (`OpenGLProject.exe`) when you want to:**
- Work with 3D objects and scenes
- Test lighting and materials
- Use camera controls
- Build 3D applications

## Technical Differences

| Feature | 3D Version | 2D Version |
|---------|------------|------------|
| Projection | Perspective | Orthographic |
| Camera | Full 6DOF movement | Fixed |
| Depth | Z-buffer enabled | No depth testing |
| Lighting | Complex PBR-style | Minimal flat lighting |
| Background | Infinite grid | Solid color |
| Objects | 3D spheres/meshes | 2D circles |
| Dependencies | Full (Assimp, Model loading) | Minimal (geometry only) |
| Complexity | High | Low |

The 2D version is perfect as a starting point for new projects or for testing circle generation and rendering in a simplified environment.
