#include "RubiksCube.h"

#include <algorithm>
#include <iostream>
#include <vector>

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/common.hpp>
#include <stb_image.h>

namespace {
const std::array<glm::vec3, 6> kTargetColors = {
    glm::vec3(1.0f, 0.0f, 0.0f),
    glm::vec3(1.0f, 0.6f, 0.0f),
    glm::vec3(1.0f, 1.0f, 1.0f),
    glm::vec3(1.0f, 1.0f, 0.0f),
    glm::vec3(0.0f, 1.0f, 0.0f),
    glm::vec3(0.0f, 0.2f, 1.0f)
};
}


RubiksCube::RubiksCube(const std::string& modelPath,
                       const std::string& faceTexturePath,
                       const glm::vec3& cubeCenter,
                       float cubieScaleValue,
                       float cubieSpacingValue)
    : model(modelPath.c_str()),
      center(cubeCenter),
      cubieScale(cubieScaleValue),
      cubieSpacing(cubieSpacingValue) {
    if (!faceTexturePath.empty()) {
        loadFaceTextures(faceTexturePath);
    }
    cubeState.printState();
}

RubiksCube::~RubiksCube() {
    releaseTextures();
}

void RubiksCube::applyMaterial(Shader& shader) const {
    if (faceTexturesAvailable && faceTexturesEnabled) {
        shader.setBool("useFaceTextures", true);
        // Texture units 10-15 so mesh textures remain undisturbed
        for (int i = 0; i < static_cast<int>(faceTextures.size()); ++i) {
            glActiveTexture(GL_TEXTURE10 + i);
            glBindTexture(GL_TEXTURE_2D, faceTextures[i]);
            shader.setInt("faceTextures[" + std::to_string(i) + "]", 10 + i);
        }
        glActiveTexture(GL_TEXTURE0);
    } else {
        shader.setBool("useFaceTextures", false);
    }
}

void RubiksCube::draw(Shader& shader, const std::vector<ID>* cubieIDs) {
    // Precompute the active face rotation once per frame
    glm::mat4 animRotation4(1.0f);
    if (animating && animatingFaceIndex >= 0) {
        const auto& pivot = kFacePivots[animatingFaceIndex];
        animRotation4 = glm::rotate(glm::mat4(1.0f), glm::radians(currentAngle), pivot.axis);
    }

    const auto& offsets = cubeState.getCubieOffsets();
    const auto& faceColorsAll = cubeState.getCubieFaceColors();
    
    for (size_t i = 0; i < offsets.size(); ++i) {
        const auto& offset = offsets[i];
        glm::mat4 modelMatrix = getCubieModelMatrix(offset);
        
        bool isAnimatingCubie = false;
        if (animating) {
            isAnimatingCubie = std::find(animatingCubieIndices.begin(),
                                         animatingCubieIndices.end(), i) 
                               != animatingCubieIndices.end();
            if (isAnimatingCubie) {
                glm::mat4 toOrigin = glm::translate(glm::mat4(1.0f), -center);
                glm::mat4 fromOrigin = glm::translate(glm::mat4(1.0f), center);
                modelMatrix = fromOrigin * animRotation4 * toOrigin * modelMatrix;
            }
        }
        
        shader.setMat4("model", modelMatrix);

        const auto& faceColors = faceColorsAll[i];
        for (int f = 0; f < 6; ++f) {
            shader.setInt("faceColorIndex[" + std::to_string(f) + "]", faceColors.colorIndex[f]);
        }
        
        if (cubieIDs && i < cubieIDs->size()) {
            shader.setInt("cubieID", static_cast<int>((*cubieIDs)[i]));
        } else {
            shader.setInt("cubieID", -1);
        }
        model.Draw(shader);
    }
}

glm::mat4 RubiksCube::getCubieModelMatrix(const glm::vec3& offset) const {
    glm::mat4 modelMatrix = glm::mat4(1.0f);
    glm::vec3 worldPos = center + offset * cubieSpacing;
    modelMatrix = glm::translate(modelMatrix, worldPos);
    modelMatrix = glm::scale(modelMatrix, glm::vec3(cubieScale));
    return modelMatrix;
}

void RubiksCube::setFaceTexturesEnabled(bool enabled) {
    faceTexturesEnabled = enabled;
}

void RubiksCube::loadFaceTextures(const std::string& texturePath) {
    int width = 0;
    int height = 0;
    int channels = 0;

    stbi_set_flip_vertically_on_load(false);
    unsigned char* img = stbi_load(texturePath.c_str(), &width, &height, &channels, 0);
    if (!img) {
        std::cerr << "Failed to load Rubiks base tile texture: " << texturePath << std::endl;
        return;
    }

    const int pixelCount = width * height;
    GLenum format = (channels == 4) ? GL_RGBA : GL_RGB;
    const int stride = (format == GL_RGBA) ? 4 : 3;

    std::vector<unsigned char> recolored(pixelCount * stride, 255);
    glGenTextures(static_cast<GLsizei>(faceTextures.size()), faceTextures.data());

    for (int face = 0; face < static_cast<int>(faceTextures.size()); ++face) {
        for (int i = 0; i < pixelCount; ++i) {
            int srcIdx = i * channels;
            int dstIdx = i * stride;

            unsigned char r = img[srcIdx];
            unsigned char g = (channels >= 2) ? img[srcIdx + 1] : img[srcIdx];
            unsigned char b = (channels >= 3) ? img[srcIdx + 2] : img[srcIdx];
            unsigned char a = (channels == 4) ? img[srcIdx + 3] : 255;

            float brightness = std::max({r, g, b}) / 255.0f;
            brightness = std::min(1.0f, brightness * 1.2f);
            glm::vec3 tinted = kTargetColors[face] * brightness;

            recolored[dstIdx] = static_cast<unsigned char>(glm::clamp(tinted.r, 0.0f, 1.0f) * 255.0f);
            recolored[dstIdx + 1] = static_cast<unsigned char>(glm::clamp(tinted.g, 0.0f, 1.0f) * 255.0f);
            recolored[dstIdx + 2] = static_cast<unsigned char>(glm::clamp(tinted.b, 0.0f, 1.0f) * 255.0f);
            if (format == GL_RGBA) {
                recolored[dstIdx + 3] = a;
            }
        }

        glBindTexture(GL_TEXTURE_2D, faceTextures[face]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        // Ensure pixel storage alignment matches our tightly-packed recolored buffer
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, recolored.data());
        glGenerateMipmap(GL_TEXTURE_2D);

        // Debug: sample center pixel to verify recolor math
        try {
            if (!recolored.empty() && width > 0 && height > 0) {
                int sx = width / 2;
                int sy = height / 2;
                int sampleIdx = sy * width + sx;
                int dstIdx = sampleIdx * stride;
                if (dstIdx + 2 < static_cast<int>(recolored.size())) {
                    int r = static_cast<int>(recolored[dstIdx]);
                    int g = static_cast<int>(recolored[dstIdx + 1]);
                    int b = static_cast<int>(recolored[dstIdx + 2]);
                    std::cout << "[RubiksCube] face=" << face << " centerPixel=(" << r << "," << g << "," << b << ")" << std::endl;
                }
            }
        } catch (...) {}
    }

    stbi_image_free(img);
    faceTexturesAvailable = true;
}

void RubiksCube::releaseTextures() {
    if (faceTexturesAvailable) {
        glDeleteTextures(static_cast<GLsizei>(faceTextures.size()), faceTextures.data());
        faceTexturesAvailable = false;
    }
}

void RubiksCube::rotateFace(int faceIndex, float angle){
    cubeState.rotateFace(faceIndex, angle);
    cubeState.printState();
}

void RubiksCube::startFaceRotation(int faceIndex, float angle, float duration) {
    if (animating) return;
    
    animating = true;
    animatingFaceIndex = faceIndex;
    targetAngle = angle;
    currentAngle = 0.0f;
    animationDuration = duration;
    animationTime = 0.0f;
    
    // Cache cubies belonging to this face
    animatingCubieIndices.clear();
    const auto& offsets = cubeState.getCubieOffsets();
    for (size_t i = 0; i < offsets.size(); ++i) {
        if (cubieOnFace(offsets[i], faceIndex)) {
            animatingCubieIndices.push_back(i);
        }
    }

    playTurnSound(duration);
}

void RubiksCube::queueFaceRotation(int faceIndex, float angle, float duration) {
    moveQueue.push({faceIndex, angle, duration});
    
    // Start immediately if idle
    if (!animating) {
        QueuedMove move = moveQueue.front();
        moveQueue.pop();
        startFaceRotation(move.faceIndex, move.angle, move.duration);
    }
}

bool RubiksCube::updateAnimation(float deltaTime) {
    if (!animating) {
        // Start any queued move before exiting
        if (!moveQueue.empty()) {
            QueuedMove move = moveQueue.front();
            moveQueue.pop();
            startFaceRotation(move.faceIndex, move.angle, move.duration);
        }
        return false;
    }
    
    animationTime += deltaTime;
    
    // Ease-out cubic for smoother finish
    float t = std::min(animationTime / animationDuration, 1.0f);
    float easedT = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
    
    currentAngle = targetAngle * easedT;
    
    if (t >= 1.0f) {
        // Snap to exact angle before handing off next move
        rotateFace(animatingFaceIndex, targetAngle);
        
        animating = false;
        animatingFaceIndex = -1;
        targetAngle = 0.0f;
        currentAngle = 0.0f;
        animationTime = 0.0f;
        animatingCubieIndices.clear();
        
        // Chain the next queued move if present
        if (!moveQueue.empty()) {
            QueuedMove move = moveQueue.front();
            moveQueue.pop();
            startFaceRotation(move.faceIndex, move.angle, move.duration);
        }
        
        return true;
    }
    
    return false;
}

int RubiksCube::findCubieAtPosition(const glm::vec3& position) const {
    return cubeState.findCubieAtPosition(position);
}

bool RubiksCube::cubieOnFace(const glm::vec3& offset, int faceIndex) const {
    return cubeState.cubieOnFace(offset, faceIndex);
}

void RubiksCube::setTurnSounds(const std::vector<std::string>& soundPaths, float volume) {
    turnSoundPaths = soundPaths;
    turnSoundVolume = volume;
    turnSounds.clear();
    turnSoundEnabled = false;

    for (const auto& path : turnSoundPaths) {
        if (path.empty()) {
            continue;
        }
        auto sound = std::make_unique<Audio>();
        if (!sound->load(path)) {
            std::cerr << "[RubiksCube] Failed to load turn sound: " << path << std::endl;
            continue;
        }
        sound->setVolume(turnSoundVolume);
        turnSounds.push_back(std::move(sound));
    }

    turnSoundEnabled = !turnSounds.empty();
}

void RubiksCube::playTurnSound(float moveDuration) {
    if (!turnSoundEnabled || turnSounds.empty()) {
        return;
    }

    std::uniform_int_distribution<size_t> soundIndexDist(0, turnSounds.size() - 1);
    size_t idx = soundIndexDist(turnSoundRng);
    auto& sound = *turnSounds[idx];

    float durationSeconds = std::max(moveDuration, 0.01f);
    float sourceDuration = std::max(sound.getDuration(), 0.01f);
    float durationPitch = sourceDuration / durationSeconds;
    float jitter = turnPitchRange(turnSoundRng);
    float pitch = std::clamp(durationPitch * jitter, 0.25f, 4.0f);
    sound.stop();
    sound.setPitch(pitch);
    sound.play();
}
