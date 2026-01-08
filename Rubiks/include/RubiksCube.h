#pragma once

#include <array>
#include <string>
#include <vector>
#include <queue>
#include <random>
#include <memory>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "shader.h"
#include "model.h"
#include "globals.h"
#include "CubeStateMachine.h"
#include "audio.h"

// Queued move for animation
struct QueuedMove {
    int faceIndex;
    float angle;
    float duration;
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
    // draw optionally highlights cubies via ID overlay
    void draw(Shader& shader, const std::vector<ID>* cubieIDs = nullptr);

    const Model& getModel() const { return model; }
    const std::vector<glm::vec3>& getCubieOffsets() const { return cubeState.getCubieOffsets(); }
    glm::vec3 getCenter() const { return center; }
    glm::mat4 getCubieModelMatrix(const glm::vec3& offset) const;
    void setFaceTexturesEnabled(bool enabled);
    bool areFaceTexturesEnabled() const { return faceTexturesAvailable && faceTexturesEnabled; }

    // Instant snap rotation
    void rotateFace(int faceIndex, float angle);
    
    // Animated rotations
    void startFaceRotation(int faceIndex, float targetAngle, float duration = 0.3f);
    void queueFaceRotation(int faceIndex, float targetAngle, float duration = 0.3f);
    bool updateAnimation(float deltaTime);
    bool isAnimating() const { return animating || !moveQueue.empty(); }
    bool hasQueuedMoves() const { return !moveQueue.empty(); }
    float getAnimationProgress() const { return animating ? currentAngle / targetAngle : 0.0f; }
    int getAnimatingFace() const { return animatingFaceIndex; }
    float getCurrentAnimationAngle() const { return currentAngle; }
    
    // Position captured right before a rotation begins
    glm::vec3 getLastSelectedPosition() const { return lastSelectedPosition; }
    void setLastSelectedPosition(const glm::vec3& pos) { lastSelectedPosition = pos; }
    
    // Find cubie index at a given position
    int findCubieAtPosition(const glm::vec3& position) const;
    
    // Sticker state: 54 entries, faces ordered Right/Left/Up/Down/Front/Back using color indices 0-5
    const std::array<int, 54>& getState() const { return cubeState.getState(); }
    bool isSolved() const { return cubeState.isSolved(); }
    void printState() const { cubeState.printState(); }  // Debug: print state to console

    void setTurnSounds(const std::vector<std::string>& soundPaths, float volume = 70.0f);

private:
    void loadFaceTextures(const std::string& texturePath);
    void releaseTextures();
    bool cubieOnFace(const glm::vec3& offset, int faceIndex) const;

    Model model;
    glm::vec3 center;
    float cubieScale;
    float cubieSpacing;
    CubeStateMachine cubeState;
    std::vector<CubieFaces> animStartFaceColors;
    std::array<unsigned int, 6> faceTextures{};
    bool faceTexturesAvailable{false};
    bool faceTexturesEnabled{true};

    std::vector<std::string> turnSoundPaths;
    std::vector<std::unique_ptr<Audio>> turnSounds;
    bool turnSoundEnabled{false};
    float turnSoundVolume{100.0f};
    std::mt19937 turnSoundRng{std::random_device{}()};
    std::uniform_real_distribution<float> turnPitchRange{0.9f, 1.1f};

    void playTurnSound(float moveDuration);
    
    // Animation state
    bool animating{false};
    int animatingFaceIndex{-1};
    float targetAngle{0.0f};
    float currentAngle{0.0f};
    float animationDuration{0.3f};
    float animationTime{0.0f};
    std::vector<size_t> animatingCubieIndices;
    glm::vec3 lastSelectedPosition{0.0f};
    std::queue<QueuedMove> moveQueue;
};
