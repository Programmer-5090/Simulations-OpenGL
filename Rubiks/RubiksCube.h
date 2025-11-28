#pragma once

#include <array>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "shader.h"
#include "model.h"
#include "globals.h"

// Face indices: 0=+X(Right), 1=-X(Left), 2=+Y(Up), 3=-Y(Down), 4=+Z(Front), 5=-Z(Back)
// Color indices: 0=Red, 1=Orange, 2=White, 3=Yellow, 4=Green, 5=Blue, -1=Black(internal)
struct CubieFaces {
    std::array<int, 6> colorIndex;
    
    CubieFaces() : colorIndex{-1, -1, -1, -1, -1, -1} {}
};

class RubiksCube {
public:
    RubiksCube(const std::string& modelPath,
               const std::string& faceTexturePath,
               const glm::vec3& cubeCenter,
               float cubieScale,
               float cubieSpacing);
    ~RubiksCube();

    RubiksCube(const RubiksCube&) = delete;
    RubiksCube& operator=(const RubiksCube&) = delete;
    RubiksCube(RubiksCube&&) = delete;
    RubiksCube& operator=(RubiksCube&&) = delete;

    void applyMaterial(Shader& shader) const;
    // draw optionally accepts a vector of cubie IDs parallel to getCubieOffsets()
    void draw(Shader& shader, const std::vector<ID>* cubieIDs = nullptr);

    const Model& getModel() const { return model; }
    const std::vector<glm::vec3>& getCubieOffsets() const { return cubieOffsets; }
    glm::mat4 getCubieModelMatrix(const glm::vec3& offset) const;
    void setFaceTexturesEnabled(bool enabled);
    bool areFaceTexturesEnabled() const { return faceTexturesAvailable && faceTexturesEnabled; }

    // Instant rotation (snaps immediately)
    void rotateFace(int faceIndex, float angle);
    
    // Animated rotation methods
    void startFaceRotation(int faceIndex, float targetAngle, float duration = 0.3f);
    bool updateAnimation(float deltaTime);  // Returns true if animation just completed
    bool isAnimating() const { return animating; }
    float getAnimationProgress() const { return animating ? currentAngle / targetAngle : 0.0f; }
    int getAnimatingFace() const { return animatingFaceIndex; }
    float getCurrentAnimationAngle() const { return currentAngle; }
    
    // Get the position that was selected before rotation started
    glm::vec3 getLastSelectedPosition() const { return lastSelectedPosition; }
    void setLastSelectedPosition(const glm::vec3& pos) { lastSelectedPosition = pos; }
    
    // Find cubie index at a given position
    int findCubieAtPosition(const glm::vec3& position) const;

private:
    void buildCubieOffsets();
    void initCubieFaceColors();
    void rotateCubieFaceColors(size_t cubieIndex, int axis, bool clockwise);
    void loadFaceTextures(const std::string& texturePath);
    void releaseTextures();
    bool cubieOnFace(const glm::vec3& offset, int faceIndex);

    Model model;
    glm::vec3 center;
    float cubieScale;
    float cubieSpacing;
    std::vector<glm::vec3> cubieOffsets;
    std::vector<CubieFaces> cubieFaceColors;
    std::vector<CubieFaces> animStartFaceColors;
    std::array<unsigned int, 6> faceTextures{};
    bool faceTexturesAvailable{false};
    bool faceTexturesEnabled{true};
    
    // Animation state
    bool animating{false};
    int animatingFaceIndex{-1};
    float targetAngle{0.0f};
    float currentAngle{0.0f};
    float animationDuration{0.3f};
    float animationTime{0.0f};
    std::vector<size_t> animatingCubieIndices;
    glm::vec3 lastSelectedPosition{0.0f};
};
