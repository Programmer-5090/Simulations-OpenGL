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
    glm::vec3(1.0f, 0.0f, 0.0f), // index 0 -> red (restore)
    glm::vec3(1.0f, 0.6f, 0.0f), // index 1 -> orange (left)
    glm::vec3(1.0f, 1.0f, 1.0f), // index 2 -> white (up)
    glm::vec3(1.0f, 1.0f, 0.0f), // index 3 -> yellow (bottom)
    glm::vec3(0.0f, 1.0f, 0.0f), // index 4 -> green (front/side)
    glm::vec3(0.0f, 0.2f, 1.0f)  // index 5 -> blue  (back)
};

struct FacePivot {
    glm::vec3 position;  // Center of the rotating face
    glm::vec3 axis;      // Rotation axis (normal to face)
};

const std::array<FacePivot, 9> kFacePivots = {
    // 6 outer faces
    FacePivot{{+1, 0, 0}, {1, 0, 0}},  // Right
    FacePivot{{-1, 0, 0}, {-1, 0, 0}}, // Left
    FacePivot{{0, +1, 0}, {0, 1, 0}},  // Up
    FacePivot{{0, -1, 0}, {0, -1, 0}}, // Down
    FacePivot{{0, 0, +1}, {0, 0, 1}},  // Front
    FacePivot{{0, 0, -1}, {0, 0, -1}}, // Back
    // 3 middle slices (M, E, S)
    FacePivot{{0, 0, 0}, {1, 0, 0}},   // M (between L/R)
    FacePivot{{0, 0, 0}, {0, 1, 0}},   // E (between U/D)
    FacePivot{{0, 0, 0}, {0, 0, 1}},   // S (between F/B)
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
    buildCubieOffsets();
    initCubieFaceColors();
    if (!faceTexturePath.empty()) {
        loadFaceTextures(faceTexturePath);
    }
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
    
    for (size_t i = 0; i < cubieOffsets.size(); ++i) {
        const auto& offset = cubieOffsets[i];
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

        const auto& faceColors = cubieFaceColors[i];
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

void RubiksCube::buildCubieOffsets() {
    cubieOffsets.reserve(26);
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            for (int z = -1; z <= 1; ++z) {
                if (x == 0 && y == 0 && z == 0) {
                    continue;
                }
                cubieOffsets.emplace_back(static_cast<float>(x),
                                          static_cast<float>(y),
                                          static_cast<float>(z));
            }
        }
    }
}

void RubiksCube::initCubieFaceColors() {
    // Face indices: 0=+X, 1=-X, 2=+Y, 3=-Y, 4=+Z, 5=-Z
    cubieFaceColors.resize(cubieOffsets.size());
    
    for (size_t i = 0; i < cubieOffsets.size(); ++i) {
        const auto& pos = cubieOffsets[i];
        CubieFaces& faces = cubieFaceColors[i];
        
        faces.colorIndex[0] = (pos.x > 0.5f) ? 0 : -1;
        faces.colorIndex[1] = (pos.x < -0.5f) ? 1 : -1;
        faces.colorIndex[2] = (pos.y > 0.5f) ? 2 : -1;
        faces.colorIndex[3] = (pos.y < -0.5f) ? 3 : -1;
        faces.colorIndex[4] = (pos.z > 0.5f) ? 4 : -1;
        faces.colorIndex[5] = (pos.z < -0.5f) ? 5 : -1;
    }
}

void RubiksCube::rotateCubieFaceColors(size_t cubieIndex, int axis, bool clockwise) {
    // axis: 0=X, 1=Y, 2=Z
    CubieFaces& faces = cubieFaceColors[cubieIndex];
    std::array<int, 6> oldColors = faces.colorIndex;
    
    if (axis == 0) {
        if (clockwise) {
            faces.colorIndex[4] = oldColors[2];
            faces.colorIndex[3] = oldColors[4];
            faces.colorIndex[5] = oldColors[3];
            faces.colorIndex[2] = oldColors[5];
        } else {
            faces.colorIndex[5] = oldColors[2];
            faces.colorIndex[3] = oldColors[5];
            faces.colorIndex[4] = oldColors[3];
            faces.colorIndex[2] = oldColors[4];
        }
    } else if (axis == 1) {
        if (clockwise) {
            faces.colorIndex[5] = oldColors[0];
            faces.colorIndex[1] = oldColors[5];
            faces.colorIndex[4] = oldColors[1];
            faces.colorIndex[0] = oldColors[4];
        } else {
            faces.colorIndex[4] = oldColors[0];
            faces.colorIndex[1] = oldColors[4];
            faces.colorIndex[5] = oldColors[1];
            faces.colorIndex[0] = oldColors[5];
        }
    } else {
        if (clockwise) {
            faces.colorIndex[2] = oldColors[0];
            faces.colorIndex[1] = oldColors[2];
            faces.colorIndex[3] = oldColors[1];
            faces.colorIndex[0] = oldColors[3];
        } else {
            faces.colorIndex[3] = oldColors[0];
            faces.colorIndex[1] = oldColors[3];
            faces.colorIndex[2] = oldColors[1];
            faces.colorIndex[0] = oldColors[2];
        }
    }
}

void RubiksCube::loadFaceTextures(const std::string& texturePath) {
    int width = 0;
    int height = 0;
    int channels = 0;

    stbi_set_flip_vertically_on_load(false);
    unsigned char* img = stbi_load(texturePath.c_str(), &width, &height, &channels, 0);
    if (!img) {
        std::cerr << "Failed to load Rubik's base tile texture: " << texturePath << std::endl;
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

        // Debug: print a central pixel of the recolored texture to verify channel values
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
    const auto& pivot = kFacePivots[faceIndex];
    glm::mat4 rotation4 = glm::rotate(glm::mat4(1.0f), glm::radians(angle), pivot.axis);

    int axis;
    bool clockwise;
    
    if (std::abs(pivot.axis.x) > 0.5f) axis = 0;
    else if (std::abs(pivot.axis.y) > 0.5f) axis = 1;
    else axis = 2;
    
    float axisSign = (axis == 0) ? pivot.axis.x : ((axis == 1) ? pivot.axis.y : pivot.axis.z);
    clockwise = (angle * axisSign) > 0.0f;

    for (size_t i = 0; i < cubieOffsets.size(); ++i) {
        if (cubieOnFace(cubieOffsets[i], faceIndex)) {
            glm::vec4 pos = rotation4 * glm::vec4(cubieOffsets[i], 1.0f);
            cubieOffsets[i] = glm::vec3(pos);

            cubieOffsets[i].x = std::round(cubieOffsets[i].x);
            cubieOffsets[i].y = std::round(cubieOffsets[i].y);
            cubieOffsets[i].z = std::round(cubieOffsets[i].z);

            rotateCubieFaceColors(i, axis, clockwise);
        }
    }
}

void RubiksCube::startFaceRotation(int faceIndex, float angle, float duration) {
    if (animating) return;  // Don't start new animation while one is in progress
    
    animating = true;
    animatingFaceIndex = faceIndex;
    targetAngle = angle;
    currentAngle = 0.0f;
    animationDuration = duration;
    animationTime = 0.0f;
    
    // Store indices of cubies that will be animated
    animatingCubieIndices.clear();
    for (size_t i = 0; i < cubieOffsets.size(); ++i) {
        if (cubieOnFace(cubieOffsets[i], faceIndex)) {
            animatingCubieIndices.push_back(i);
        }
    }
}

bool RubiksCube::updateAnimation(float deltaTime) {
    if (!animating) return false;
    
    animationTime += deltaTime;
    
    // Use smooth easing (ease-out cubic)
    float t = std::min(animationTime / animationDuration, 1.0f);
    float easedT = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
    
    currentAngle = targetAngle * easedT;
    
    // Animation complete
    if (t >= 1.0f) {
        // Apply the final rotation and snap
        rotateFace(animatingFaceIndex, targetAngle);
        
        // Reset animation state
        animating = false;
        animatingFaceIndex = -1;
        targetAngle = 0.0f;
        currentAngle = 0.0f;
        animationTime = 0.0f;
        animatingCubieIndices.clear();
        
        return true;  // Animation just completed
    }
    
    return false;
}

int RubiksCube::findCubieAtPosition(const glm::vec3& position) const {
    const float epsilon = 0.5f;
    for (size_t i = 0; i < cubieOffsets.size(); ++i) {
        if (glm::length(cubieOffsets[i] - position) < epsilon) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool RubiksCube::cubieOnFace(const glm::vec3& offset, int faceIndex) {
    const float epsilon = 0.1f;  // Tolerance for floating-point comparison
    
    switch (faceIndex) {
        case 0: // Right face (x == +1)
            return std::abs(offset.x - 1.0f) < epsilon;
        case 1: // Left face (x == -1)
            return std::abs(offset.x + 1.0f) < epsilon;
        case 2: // Up face (y == +1)
            return std::abs(offset.y - 1.0f) < epsilon;
        case 3: // Down face (y == -1)
            return std::abs(offset.y + 1.0f) < epsilon;
        case 4: // Front face (z == +1)
            return std::abs(offset.z - 1.0f) < epsilon;
        case 5: // Back face (z == -1)
            return std::abs(offset.z + 1.0f) < epsilon;
        case 6: // M slice (x == 0, middle between L/R)
            return std::abs(offset.x) < epsilon;
        case 7: // E slice (y == 0, middle between U/D)
            return std::abs(offset.y) < epsilon;
        case 8: // S slice (z == 0, middle between F/B)
            return std::abs(offset.z) < epsilon;
        default:
            return false;
    }
}
