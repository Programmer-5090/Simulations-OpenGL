cmake_minimum_required(VERSION 3.10)
project(OpenGLProject)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add the path where we installed GLFW4 and Assimp
set(CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/lib/glfw3" "${CMAKE_SOURCE_DIR}/lib/assimp" ${CMAKE_PREFIX_PATH})

# Find required packages
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)

# Don't use find_package for Assimp - use manual linking instead
# find_package(assimp QUIET)

# Include directories
# Project include paths
include_directories(${CMAKE_SOURCE_DIR}/includes)
include_directories(${CMAKE_SOURCE_DIR}/includes/glad)
include_directories(${CMAKE_SOURCE_DIR}/includes/glm)
include_directories(${CMAKE_SOURCE_DIR}/includes/assimp/include)
include_directories(${CMAKE_SOURCE_DIR}/SPHFluid)
include_directories(${CMAKE_SOURCE_DIR}/Collision System)
# Add project root to include path so headers located at repo root are visible to all targets
include_directories(${CMAKE_SOURCE_DIR})

# Platform-specific include directories and compiler flags
if(WIN32 AND MSVC)
    # Clear environment variables that might interfere with MSVC
    set(ENV{C_INCLUDE_PATH} "")
    set(ENV{CPLUS_INCLUDE_PATH} "") 
    set(ENV{INCLUDE} "")
    set(ENV{LIB} "")
    set(ENV{LIBPATH} "")
    set(ENV{LIBRARY_PATH} "")
    set(ENV{PATH} "C:\\Windows\\System32;C:\\Windows;C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Tools\\MSVC\\14.44.35207\\bin\\Hostx64\\x64;C:\\Program Files\\CMake\\bin")
    
    # Explicitly avoid MinGW/MSYS2 paths
    set(CMAKE_IGNORE_PATH "C:/msys64" "C:/MinGW" "/msys64" "/MinGW" "C:/msys64/ucrt64" "C:/msys64/mingw64")
    
    # Force use of Visual Studio toolchain
    set(CMAKE_SYSTEM_IGNORE_PATH "C:/msys64" "C:/MinGW" "/msys64" "/MinGW" "C:/msys64/ucrt64" "C:/msys64/mingw64" "C:/msys64/ucrt64/include" "C:/msys64/mingw64/include")
elseif(APPLE)
    include_directories(/opt/homebrew/include)
endif()

# Add all source files
file(GLOB SOURCES "*.cpp")
file(GLOB GEOMETRY_SOURCES "geometry/*.cpp")  # Add geometry folder sources
file(GLOB WORLD_SOURCES "SPHFluid/2D/*.cpp")
file(GLOB COLLISION_SOURCES "Collision System/*.cpp")  # Add Collision System sources
# Include GPUSort.cpp in main project sources so both OpenGLProject and GPU targets can link
list(APPEND SOURCES "${CMAKE_SOURCE_DIR}/SPHFluid/GPUSort.cpp")
set(GLAD_SOURCES "${CMAKE_SOURCE_DIR}/includes/glad/src/glad.c")

# GPU-specific sources
set(GPU_SOURCES 
    SPHFluid/3D/GPUFluidSimulation.cpp
    SPHFluid/3D/GPUParticleDisplay3D.cpp
    SPHFluid/3D/BoundingBox.cpp
    SPHFluid/3D/window3D.cpp
)

# Create main 3D executable
list(REMOVE_ITEM SOURCES "${CMAKE_SOURCE_DIR}/Collision System/window2d.cpp")  # Exclude collision system window from main build
list(REMOVE_ITEM SOURCES "${CMAKE_SOURCE_DIR}/gpuFluidWindow.cpp")  # Exclude accidental top-level gpuFluidWindow to avoid duplicate mains

# Also exclude any SPHFluid gpuFluidWindow files from the main world's sources to avoid duplicate mains
list(REMOVE_ITEM WORLD_SOURCES "SPHFluid/2D/gpuFluidWindow.cpp")
list(REMOVE_ITEM WORLD_SOURCES "${CMAKE_SOURCE_DIR}/SPHFluid/2D/gpuFluidWindow.cpp")

# Ensure GPU-specific sources are not part of the main world's source list.
# WORLD_SOURCES was set to all files under SPHFluid/; remove GPU-only files
foreach(GPU_FILE ${GPU_SOURCES})
    list(REMOVE_ITEM WORLD_SOURCES "${CMAKE_SOURCE_DIR}/${GPU_FILE}")
endforeach()

# Also remove any GPU files that might appear in the top-level SOURCES list
foreach(GPU_FILE ${GPU_SOURCES})
    list(REMOVE_ITEM SOURCES "${CMAKE_SOURCE_DIR}/${GPU_FILE}")
endforeach()

add_executable(${PROJECT_NAME} ${SOURCES} ${GEOMETRY_SOURCES} ${WORLD_SOURCES} ${GLAD_SOURCES})

# Create separate 2D executable (this target is now empty since window2d.cpp doesn't exist in root)
# add_executable(OpenGL2D window2d.cpp shader.cpp mesh.cpp ${GEOMETRY_SOURCES} ${GLAD_SOURCES})

# Create Collision System executable with proper source files
add_executable(CollisionSystem 
    "Collision System/window2d.cpp"
    "Collision System/solver.cpp"
    "Collision System/threadPool.cpp" 
    "Collision System/utils.cpp"
    shader.cpp 
    mesh.cpp 
    ${GEOMETRY_SOURCES} 
    ${GLAD_SOURCES}
)

# Set specific include directories for CollisionSystem
target_include_directories(CollisionSystem PRIVATE
    ${CMAKE_SOURCE_DIR}/includes
    ${CMAKE_SOURCE_DIR}/includes/glad
    ${CMAKE_SOURCE_DIR}/includes/glm
    ${CMAKE_SOURCE_DIR}/Collision System
    ${CMAKE_SOURCE_DIR}
)

# Add threading support for CollisionSystem
find_package(Threads REQUIRED)
target_link_libraries(CollisionSystem Threads::Threads)


# Create GPU fluid simulation executable
add_executable(GPUFluidSim3D SPHFluid/3D/window3D.cpp shader.cpp mesh.cpp ${GEOMETRY_SOURCES} ${GPU_SOURCES} ${CMAKE_SOURCE_DIR}/SPHFluid/GPUSort.cpp ${GLAD_SOURCES})

# Create GPU fluid simulation 2D executable
add_executable(GPUFluidSim2D 
    SPHFluid/2D/gpuFluidWindow.cpp
    SPHFluid/2D/GPUFluidSimulation.cpp
    SPHFluid/2D/GPUParticleDisplay.cpp
    shader.cpp
    mesh.cpp
    ${GEOMETRY_SOURCES}
    ${CMAKE_SOURCE_DIR}/SPHFluid/GPUSort.cpp
    ${GLAD_SOURCES}
)

# Platform-specific target properties (must be after target creation)
if(WIN32 AND MSVC)
    # Force MSVC to use its own headers by setting system include directories
    set_property(TARGET ${PROJECT_NAME} PROPERTY VS_USER_PROPS "")
    # set_property(TARGET OpenGL2D PROPERTY VS_USER_PROPS "")
    set_property(TARGET CollisionSystem PROPERTY VS_USER_PROPS "")
    
    # Add compiler flags to avoid external includes and define WIN32_LEAN_AND_MEAN
    target_compile_definitions(${PROJECT_NAME} PRIVATE 
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
    
    # target_compile_definitions(OpenGL2D PRIVATE 
    #     WIN32_LEAN_AND_MEAN
    #     NOMINMAX
    #     _CRT_SECURE_NO_WARNINGS
    # )
    
    target_compile_definitions(CollisionSystem PRIVATE 
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
    
    target_compile_definitions(GPUFluidSim3D PRIVATE 
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
    target_compile_definitions(GPUFluidSim2D PRIVATE 
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
    
    target_compile_options(${PROJECT_NAME} PRIVATE
        /external:W0
        /external:anglebrackets
        /external:templates-
    )
    
    # target_compile_options(OpenGL2D PRIVATE
    #     /external:W0
    #     /external:anglebrackets
    #     /external:templates-
    # )
    
    target_compile_options(CollisionSystem PRIVATE
        /external:W0
        /external:anglebrackets
        /external:templates-
    )
    
    target_compile_options(GPUFluidSim3D PRIVATE
        /external:W0
        /external:anglebrackets
        /external:templates-
    )
    target_compile_options(GPUFluidSim2D PRIVATE
        /external:W0
        /external:anglebrackets
        /external:templates-
    )
endif()

# Link libraries
target_link_libraries(${PROJECT_NAME} 
    OpenGL::GL
    glfw
)

# target_link_libraries(OpenGL2D 
#     OpenGL::GL
#     glfw
# )

target_link_libraries(CollisionSystem 
    OpenGL::GL
    glfw
)

target_link_libraries(GPUFluidSim3D 
    OpenGL::GL
    glfw
)

target_link_libraries(GPUFluidSim2D 
    OpenGL::GL
    glfw
)

# Manual Assimp linking for Windows
if(WIN32)
    target_link_directories(${PROJECT_NAME} PRIVATE 
        ${CMAKE_SOURCE_DIR}/lib/assimp/lib
    )
    target_link_libraries(${PROJECT_NAME} assimp-vc143-mt zlibstatic)
    
    # 2D version doesn't need Assimp since it doesn't load 3D models
    # target_link_directories(OpenGL2D PRIVATE 
    #     ${CMAKE_SOURCE_DIR}/lib/assimp/lib
    # )
    # target_link_libraries(OpenGL2D assimp-vc143-mt zlibstatic)
elseif(APPLE)
    # macOS-specific library paths
    target_link_directories(${PROJECT_NAME} PRIVATE 
        /opt/homebrew/lib
        ${CMAKE_SOURCE_DIR}/includes/assimp/lib
    )
    target_link_libraries(${PROJECT_NAME} assimp)
    
    # 2D version doesn't need Assimp
    # target_link_directories(OpenGL2D PRIVATE 
    #     /opt/homebrew/lib
    #     ${CMAKE_SOURCE_DIR}/includes/assimp/lib
    # )
    # target_link_libraries(OpenGL2D assimp)
endif()

# Link system frameworks (macOS)
if(APPLE)
    target_link_libraries(${PROJECT_NAME} 
        "-framework OpenGL"
        "-framework Cocoa" 
        "-framework IOKit"
    )
    # target_link_libraries(OpenGL2D 
    #     "-framework OpenGL"
    #     "-framework Cocoa" 
    #     "-framework IOKit"
    # )
    target_link_libraries(CollisionSystem 
        "-framework OpenGL"
        "-framework Cocoa" 
        "-framework IOKit"
    )
endif()

# Optional: Link SDL2 if you're using it
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(SDL2 QUIET sdl2)
    if(SDL2_FOUND)
        target_link_libraries(${PROJECT_NAME} ${SDL2_LIBRARIES})
        target_include_directories(${PROJECT_NAME} PRIVATE ${SDL2_INCLUDE_DIRS})
    endif()
endif()

# Set output directory
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output
)

# set_target_properties(OpenGL2D PROPERTIES
#     RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output
# )

set_target_properties(CollisionSystem PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output
)

set_target_properties(GPUFluidSim3D PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output
)

set_target_properties(GPUFluidSim2D PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output
)

set_target_properties(GPUFluidSim3D PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output
)

# Copy required DLLs to output directory on Windows
if(WIN32)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/lib/assimp/bin/assimp-vc143-mt.dll"
        $<TARGET_FILE_DIR:${PROJECT_NAME}>)
endif()
