cmake_minimum_required(VERSION 3.10)
project(OpenGLProject)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/lib/glfw3" "${CMAKE_SOURCE_DIR}/lib/assimp" ${CMAKE_PREFIX_PATH})

find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/includes)
include_directories(${CMAKE_SOURCE_DIR}/includes/glad)
include_directories(${CMAKE_SOURCE_DIR}/includes/glm)
include_directories(${CMAKE_SOURCE_DIR}/includes/assimp/include)
include_directories(${CMAKE_SOURCE_DIR}/SPHFluid)
include_directories(${CMAKE_SOURCE_DIR}/Collision System)
include_directories(${CMAKE_SOURCE_DIR})

function(configure_program_output target_name)
    set_target_properties(${target_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/output/${target_name}")
endfunction()

set(RES_SHADERS shaders)
set(RES_MODELS models)
set(RES_IMG img)
set(RES_RUBIKS_ASSETS Rubiks/assets)
set(RES_RUBIKS_AUDIO "audio/Cube Turn 1.mp3" "audio/Cube Turn 2.mp3")
set(RES_AUDIO_BEEP audio/beep.wav)
set(RUBIKS_SHADER_FILES
    shaders/infinite_grid.vs
    shaders/infinite_grid.fs
    shaders/vertex.vs
    shaders/rubiks.fs
)

function(bundle_program_resources target_name)
    if(NOT ARGN)
        return()
    endif()
    set(_resource_list ${ARGN})
    list(REMOVE_DUPLICATES _resource_list)
    foreach(rel_path IN LISTS _resource_list)
        set(src "${CMAKE_SOURCE_DIR}/${rel_path}")
        set(dst "$<TARGET_FILE_DIR:${target_name}>/${rel_path}")
        if(IS_DIRECTORY "${src}")
            add_custom_command(TARGET ${target_name} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory "${src}" "${dst}")
        elseif(EXISTS "${src}")
            get_filename_component(dst_dir "${dst}" DIRECTORY)
            add_custom_command(TARGET ${target_name} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory "${dst_dir}"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${src}" "${dst}")
        endif()
    endforeach()
endfunction()

# Platform-specific settings for Windows MSVC
if(WIN32 AND MSVC)
    set(ENV{C_INCLUDE_PATH} "")
    set(ENV{CPLUS_INCLUDE_PATH} "") 
    set(ENV{INCLUDE} "")
    set(ENV{LIB} "")
    set(ENV{LIBPATH} "")
    set(ENV{LIBRARY_PATH} "")
    set(ENV{PATH} "C:\\Windows\\System32;C:\\Windows;C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Tools\\MSVC\\14.44.35207\\bin\\Hostx64\\x64;C:\\Program Files\\CMake\\bin")
    
    set(CMAKE_IGNORE_PATH "C:/msys64" "C:/MinGW" "/msys64" "/MinGW" "C:/msys64/ucrt64" "C:/msys64/mingw64")
    set(CMAKE_SYSTEM_IGNORE_PATH "C:/msys64" "C:/MinGW" "/msys64" "/MinGW" "C:/msys64/ucrt64" "C:/msys64/mingw64" "C:/msys64/ucrt64/include" "C:/msys64/mingw64/include")
elseif(APPLE)
    include_directories(/opt/homebrew/include)
endif()

# Add source files
file(GLOB SOURCES "*.cpp")
file(GLOB GEOMETRY_SOURCES "geometry/*.cpp")
file(GLOB WORLD_SOURCES "SPHFluid/2D/*.cpp")
file(GLOB WORLD3D_SOURCES "SPHFluid/3D/*.cpp")
file(GLOB COLLISION_SOURCES "Collision System/*.cpp")
list(APPEND SOURCES "${CMAKE_SOURCE_DIR}/SPHFluid/GPUSort.cpp")
set(GLAD_SOURCES "${CMAKE_SOURCE_DIR}/includes/glad/src/glad.c")

# Main executable
list(REMOVE_ITEM SOURCES "${CMAKE_SOURCE_DIR}/Collision System/window2d.cpp")
list(REMOVE_ITEM SOURCES "${CMAKE_SOURCE_DIR}/gpuFluidWindow.cpp")

add_executable(${PROJECT_NAME} ${SOURCES} ${GEOMETRY_SOURCES} ${GLAD_SOURCES})
configure_program_output(${PROJECT_NAME})
bundle_program_resources(${PROJECT_NAME} ${RES_SHADERS} ${RES_MODELS} ${RES_IMG})

# Collision System executable
add_executable(CollisionSystem 
    "Collision System/window2d.cpp"
    "Collision System/Nsolver.cpp"
    "Collision System/stb_image_impl.cpp"
    "Collision System/utils.cpp"
    shader.cpp 
    mesh.cpp 
    ${GEOMETRY_SOURCES} 
    ${GLAD_SOURCES}
)
configure_program_output(CollisionSystem)
bundle_program_resources(CollisionSystem ${RES_SHADERS} ${RES_IMG})

target_include_directories(CollisionSystem PRIVATE
    ${CMAKE_SOURCE_DIR}/includes
    ${CMAKE_SOURCE_DIR}/includes/glad
    ${CMAKE_SOURCE_DIR}/includes/glm
    ${CMAKE_SOURCE_DIR}/Collision System
    ${CMAKE_SOURCE_DIR}
)

find_package(Threads REQUIRED)
target_link_libraries(CollisionSystem Threads::Threads)


# GPU Fluid Simulation executables
add_executable(GPUFluidSim3D 
    SPHFluid/3D/window3D.cpp
    SPHFluid/3D/GPUFluidSimulation.cpp
    SPHFluid/3D/GPUParticleDisplay.cpp
    SPHFluid/3D/BoundingBox.cpp
    shader.cpp 
    mesh.cpp 
    ${GEOMETRY_SOURCES} 
    ${CMAKE_SOURCE_DIR}/SPHFluid/GPUSort.cpp 
    ${GLAD_SOURCES}
)
configure_program_output(GPUFluidSim3D)
bundle_program_resources(GPUFluidSim3D ${RES_SHADERS})

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
configure_program_output(GPUFluidSim2D)
bundle_program_resources(GPUFluidSim2D ${RES_SHADERS})

# SFML setup
set(SFML_DIR "${CMAKE_SOURCE_DIR}/includes/SFML/SFML-2.6.1")
set(SFML_INCLUDE_DIR "${SFML_DIR}/include")
set(SFML_LIB_DIR "${SFML_DIR}/lib")
set(SFML_BIN_DIR "${CMAKE_SOURCE_DIR}/lib/SFML")

# Marching Cubes test executable
add_executable(MarchingTest
    "Marching Cubes/test.cpp"
    "Marching Cubes/CubeMarching.cpp"
    shader.cpp
    mesh.cpp
    model.cpp
    "audio/audio.cpp"
    ${GEOMETRY_SOURCES}
    ${GLAD_SOURCES}
)
configure_program_output(MarchingTest)
bundle_program_resources(MarchingTest ${RES_SHADERS} ${RES_MODELS} ${RES_IMG})

target_include_directories(MarchingTest PRIVATE
    ${CMAKE_SOURCE_DIR}/includes
    ${CMAKE_SOURCE_DIR}/includes/glad
    ${CMAKE_SOURCE_DIR}/includes/glm
    ${CMAKE_SOURCE_DIR}
    ${SFML_INCLUDE_DIR}
)

target_link_directories(MarchingTest PRIVATE ${SFML_LIB_DIR})
target_link_libraries(MarchingTest 
    OpenGL::GL 
    glfw
    optimized sfml-audio
    optimized sfml-system
    debug sfml-audio-d  
    debug sfml-system-d
)

if(WIN32 AND MSVC)
    target_compile_definitions(MarchingTest PRIVATE 
        WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
    target_compile_options(MarchingTest PRIVATE /external:W0 /external:anglebrackets /external:templates-)
endif()


# GPU Marching Cubes test executable
add_executable(testGPU
    "Marching Cubes/testGPU.cpp"
    "Marching Cubes/GPUMarchCubes.cpp"
    shader.cpp
    mesh.cpp
    model.cpp
    ${GEOMETRY_SOURCES}
    ${GLAD_SOURCES}
)
configure_program_output(testGPU)
bundle_program_resources(testGPU ${RES_SHADERS} ${RES_MODELS} ${RES_IMG})

target_include_directories(testGPU PRIVATE
    ${CMAKE_SOURCE_DIR}/includes
    ${CMAKE_SOURCE_DIR}/includes/glad
    ${CMAKE_SOURCE_DIR}/includes/glm
    ${CMAKE_SOURCE_DIR}
)

target_link_directories(testGPU PRIVATE ${CMAKE_SOURCE_DIR}/lib/assimp/lib)
target_link_libraries(testGPU 
    OpenGL::GL 
    glfw
    assimp-vc143-mt
    zlibstatic
)

if(WIN32 AND MSVC)
    target_compile_definitions(testGPU PRIVATE 
        WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
    target_compile_options(testGPU PRIVATE /external:W0 /external:anglebrackets /external:templates-)
    
    add_custom_command(TARGET testGPU POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/lib/assimp/bin/assimp-vc143-mt.dll"
        "$<TARGET_FILE_DIR:testGPU>/assimp-vc143-mt.dll"
    )
endif()


# CPU Marching Cubes bunny test executable
add_executable(testCPUBunny
    "Marching Cubes/testCPUBunny.cpp"
    "Marching Cubes/CubeMarching.cpp"
    shader.cpp
    mesh.cpp
    model.cpp
    ${GEOMETRY_SOURCES}
    ${GLAD_SOURCES}
)
configure_program_output(testCPUBunny)
bundle_program_resources(testCPUBunny ${RES_SHADERS} ${RES_MODELS} ${RES_IMG})

target_include_directories(testCPUBunny PRIVATE
    ${CMAKE_SOURCE_DIR}/includes
    ${CMAKE_SOURCE_DIR}/includes/glad
    ${CMAKE_SOURCE_DIR}/includes/glm
    ${CMAKE_SOURCE_DIR}
)

target_link_directories(testCPUBunny PRIVATE ${CMAKE_SOURCE_DIR}/lib/assimp/lib)
target_link_libraries(testCPUBunny 
    OpenGL::GL 
    glfw
    assimp-vc143-mt
    zlibstatic
)

if(WIN32 AND MSVC)
    target_compile_definitions(testCPUBunny PRIVATE 
        WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
    target_compile_options(testCPUBunny PRIVATE /external:W0 /external:anglebrackets /external:templates-)
    
    add_custom_command(TARGET testCPUBunny POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/lib/assimp/bin/assimp-vc143-mt.dll"
        "$<TARGET_FILE_DIR:testCPUBunny>/assimp-vc143-mt.dll"
    )
endif()


# Debug BVH executable
add_executable(debugBVH
    "Marching Cubes/debugBVH.cpp"
    shader.cpp
    mesh.cpp
    model.cpp
    ${GEOMETRY_SOURCES}
    ${GLAD_SOURCES}
)
configure_program_output(debugBVH)
bundle_program_resources(debugBVH ${RES_SHADERS} ${RES_MODELS})

target_include_directories(debugBVH PRIVATE
    ${CMAKE_SOURCE_DIR}/includes
    ${CMAKE_SOURCE_DIR}/includes/glad
    ${CMAKE_SOURCE_DIR}/includes/glm
    ${CMAKE_SOURCE_DIR}
)

target_link_directories(debugBVH PRIVATE ${CMAKE_SOURCE_DIR}/lib/assimp/lib)
target_link_libraries(debugBVH 
    OpenGL::GL 
    glfw
    assimp-vc143-mt
    zlibstatic
)

if(WIN32 AND MSVC)
    target_compile_definitions(debugBVH PRIVATE 
        WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
    target_compile_options(debugBVH PRIVATE /external:W0 /external:anglebrackets /external:templates-)
endif()


# Rubik's Cube executable
add_executable(RubiksCube
    "Rubiks/src/window.cpp"
    "Rubiks/src/RubiksCube.cpp"
    "Rubiks/src/CubeStateMachine.cpp"
    "Rubiks/src/CubeConversion.cpp"
    "Rubiks/src/input.cpp"
    "Rubiks/src/mouse_selector.cpp"
    "Rubiks/src/ai rubiks.cpp"
    "Rubiks/src/audio.cpp"
    shader.cpp
    mesh.cpp
    model.cpp
    ${GEOMETRY_SOURCES}
    ${GLAD_SOURCES}
)
configure_program_output(RubiksCube)
bundle_program_resources(RubiksCube ${RUBIKS_SHADER_FILES} "img/textures/Rubiks Col.png" ${RES_RUBIKS_ASSETS} ${RES_RUBIKS_AUDIO})

target_include_directories(RubiksCube PRIVATE
    ${CMAKE_SOURCE_DIR}/includes
    ${CMAKE_SOURCE_DIR}/includes/glad
    ${CMAKE_SOURCE_DIR}/includes/glm
    ${CMAKE_SOURCE_DIR}/Rubiks/include
    ${CMAKE_SOURCE_DIR}
    ${SFML_INCLUDE_DIR}
)

target_link_directories(RubiksCube PRIVATE 
    ${CMAKE_SOURCE_DIR}/lib/assimp/lib
    ${SFML_LIB_DIR}
)
target_link_libraries(RubiksCube 
    OpenGL::GL 
    glfw
    assimp-vc143-mt
    zlibstatic
    optimized sfml-audio
    optimized sfml-system
    debug sfml-audio-d
    debug sfml-system-d
)

if(WIN32 AND MSVC)
    target_compile_definitions(RubiksCube PRIVATE 
        WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
    target_compile_options(RubiksCube PRIVATE /external:W0 /external:anglebrackets /external:templates-)
endif()


# Platform-specific compiler settings
if(WIN32 AND MSVC)
    set_property(TARGET ${PROJECT_NAME} PROPERTY VS_USER_PROPS "")
    set_property(TARGET CollisionSystem PROPERTY VS_USER_PROPS "")
    
    target_compile_definitions(${PROJECT_NAME} PRIVATE 
        WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
    target_compile_definitions(CollisionSystem PRIVATE 
        WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
    target_compile_definitions(GPUFluidSim3D PRIVATE 
        WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
    target_compile_definitions(GPUFluidSim2D PRIVATE 
        WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
    
    target_compile_options(${PROJECT_NAME} PRIVATE /external:W0 /external:anglebrackets /external:templates-)
    target_compile_options(CollisionSystem PRIVATE /external:W0 /external:anglebrackets /external:templates-)
    target_compile_options(GPUFluidSim3D PRIVATE /external:W0 /external:anglebrackets /external:templates-)
    target_compile_options(GPUFluidSim2D PRIVATE /external:W0 /external:anglebrackets /external:templates-)
endif()

# Link libraries
target_link_libraries(${PROJECT_NAME} OpenGL::GL glfw)
target_link_libraries(CollisionSystem OpenGL::GL glfw)
target_link_libraries(GPUFluidSim3D OpenGL::GL glfw)
target_link_libraries(GPUFluidSim2D OpenGL::GL glfw)

# Platform-specific library linking
if(WIN32)
    # Add Assimp lib directory and link libraries to targets that use Assimp
    target_link_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/lib/assimp/lib)
    target_link_directories(MarchingTest PRIVATE ${CMAKE_SOURCE_DIR}/lib/assimp/lib)
    target_link_libraries(${PROJECT_NAME} assimp-vc143-mt zlibstatic)
    target_link_libraries(MarchingTest assimp-vc143-mt zlibstatic)
elseif(APPLE)
    target_link_directories(${PROJECT_NAME} PRIVATE /opt/homebrew/lib ${CMAKE_SOURCE_DIR}/includes/assimp/lib)
    target_link_libraries(${PROJECT_NAME} assimp)
    target_link_libraries(${PROJECT_NAME} "-framework OpenGL" "-framework Cocoa" "-framework IOKit")
    target_link_libraries(CollisionSystem "-framework OpenGL" "-framework Cocoa" "-framework IOKit")
endif()

# SFML Audio Test executable
add_executable(AudioTest 
    audio/sfml_audio_test.cpp
    audio/audio.cpp
)
configure_program_output(AudioTest)
bundle_program_resources(AudioTest ${RES_AUDIO_BEEP})

target_include_directories(AudioTest PRIVATE ${SFML_INCLUDE_DIR})
target_link_directories(AudioTest PRIVATE ${SFML_LIB_DIR})
target_link_libraries(AudioTest 
    optimized sfml-audio
    optimized sfml-system
    debug sfml-audio-d  
    debug sfml-system-d
)

if(WIN32 AND MSVC)
    target_compile_definitions(AudioTest PRIVATE 
        WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
    target_compile_options(AudioTest PRIVATE /external:W0 /external:anglebrackets /external:templates-)
endif()


# Optional SDL2 support
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(SDL2 QUIET sdl2)
    if(SDL2_FOUND)
        target_link_libraries(${PROJECT_NAME} ${SDL2_LIBRARIES})
        target_include_directories(${PROJECT_NAME} PRIVATE ${SDL2_INCLUDE_DIRS})
    endif()
endif()

# Headless Rubik's ID comparison test
add_executable(RubiksStateTest
    "Rubiks/tests/unit/state_validation.cpp"
    "Rubiks/src/ai rubiks.cpp"
    "Rubiks/src/CubeConversion.cpp"
    "Rubiks/tests/support/RubiksCube_stubs.cpp"
    "Rubiks/src/CubeStateMachine.cpp"
)
configure_program_output(RubiksStateTest)
bundle_program_resources(RubiksStateTest ${RES_RUBIKS_ASSETS})

target_include_directories(RubiksStateTest PRIVATE
    ${CMAKE_SOURCE_DIR}/includes
    ${CMAKE_SOURCE_DIR}/includes/glad
    ${CMAKE_SOURCE_DIR}/includes/glm
    ${CMAKE_SOURCE_DIR}/Rubiks/include
    ${CMAKE_SOURCE_DIR}
    ${SFML_INCLUDE_DIR}
)

if(WIN32 AND MSVC)
    target_compile_definitions(RubiksStateTest PRIVATE 
        WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
    target_compile_options(RubiksStateTest PRIVATE /external:W0 /external:anglebrackets /external:templates-)
endif()

target_link_directories(RubiksStateTest PRIVATE ${SFML_LIB_DIR})
target_link_libraries(RubiksStateTest 
    OpenGL::GL 
    glfw
    optimized sfml-audio
    optimized sfml-system
    debug sfml-audio-d
    debug sfml-system-d
)

# Solver unit tests
add_executable(RubiksSolverTest
    "Rubiks/tests/unit/solver_test.cpp"
    "Rubiks/src/ai rubiks.cpp"
    "Rubiks/src/CubeConversion.cpp"
    "Rubiks/tests/support/RubiksCube_stubs.cpp"
)
configure_program_output(RubiksSolverTest)
bundle_program_resources(RubiksSolverTest ${RES_RUBIKS_ASSETS})

target_include_directories(RubiksSolverTest PRIVATE
    ${CMAKE_SOURCE_DIR}/includes
    ${CMAKE_SOURCE_DIR}/includes/glad
    ${CMAKE_SOURCE_DIR}/includes/glm
    ${CMAKE_SOURCE_DIR}/Rubiks/include
    ${CMAKE_SOURCE_DIR}
    ${SFML_INCLUDE_DIR}
)

if(WIN32 AND MSVC)
    target_compile_definitions(RubiksSolverTest PRIVATE 
        WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
    target_compile_options(RubiksSolverTest PRIVATE /external:W0 /external:anglebrackets /external:templates-)
endif()

target_link_directories(RubiksSolverTest PRIVATE ${SFML_LIB_DIR})
target_link_libraries(RubiksSolverTest 
    OpenGL::GL 
    glfw
    optimized sfml-audio
    optimized sfml-system
    debug sfml-audio-d
    debug sfml-system-d
)

# Copy required DLLs on Windows
if(WIN32)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/lib/assimp/bin/assimp-vc143-mt.dll"
        $<TARGET_FILE_DIR:${PROJECT_NAME}>)

    # Copy SFML DLLs for AudioTest
    add_custom_command(TARGET AudioTest POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SFML_BIN_DIR}/sfml-audio-2.dll"
        "${SFML_BIN_DIR}/sfml-audio-d-2.dll"
        "${SFML_BIN_DIR}/sfml-system-2.dll"
        "${SFML_BIN_DIR}/sfml-system-d-2.dll"
        "${SFML_BIN_DIR}/openal32.dll"
        $<TARGET_FILE_DIR:AudioTest>)

    # Copy SFML DLLs for MarchingTest
    add_custom_command(TARGET MarchingTest POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SFML_BIN_DIR}/sfml-audio-2.dll"
        "${SFML_BIN_DIR}/sfml-audio-d-2.dll"
        "${SFML_BIN_DIR}/sfml-system-2.dll"
        "${SFML_BIN_DIR}/sfml-system-d-2.dll"
        "${SFML_BIN_DIR}/openal32.dll"
        $<TARGET_FILE_DIR:MarchingTest>)

    # Copy SFML DLLs for RubiksCube
    add_custom_command(TARGET RubiksCube POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SFML_BIN_DIR}/sfml-audio-2.dll"
        "${SFML_BIN_DIR}/sfml-audio-d-2.dll"
        "${SFML_BIN_DIR}/sfml-system-2.dll"
        "${SFML_BIN_DIR}/sfml-system-d-2.dll"
        "${SFML_BIN_DIR}/openal32.dll"
        $<TARGET_FILE_DIR:RubiksCube>)

    # Copy Assimp DLL for debugBVH
    add_custom_command(TARGET debugBVH POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/lib/assimp/bin/assimp-vc143-mt.dll"
        $<TARGET_FILE_DIR:debugBVH>)

    # Copy Assimp DLL for RubiksCube
    add_custom_command(TARGET RubiksCube POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/lib/assimp/bin/assimp-vc143-mt.dll"
        $<TARGET_FILE_DIR:RubiksCube>)
endif()
