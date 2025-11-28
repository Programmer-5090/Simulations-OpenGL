#include "mouse_selector.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/transform.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
constexpr float kEpsilon = 1e-6f;
}

MouseSelector::MouseSelector(Camera& cam, float nearPlaneDistance, float farPlaneDistance)
    : camera(&cam), nearPlane(nearPlaneDistance), farPlane(farPlaneDistance) {}

ID MouseSelector::addSelectable(const Model& model, const glm::mat4& transform) {
    Selectable selectable{ nextId++, &model, nullptr, transform };
    selectables.push_back(selectable);
    return selectable.id;
}

ID MouseSelector::addSelectable(const Mesh& mesh, const glm::mat4& transform) {
    Selectable selectable{ nextId++, nullptr, &mesh, transform };
    selectables.push_back(selectable);
    return selectable.id;
}

bool MouseSelector::removeSelectable(ID id) {
    auto it = std::remove_if(selectables.begin(), selectables.end(), [id](const Selectable& selectable) {
        return selectable.id == id;
    });
    if (it != selectables.end()) {
        selectables.erase(it, selectables.end());
        if (selectedId && *selectedId == id) {
            selectedId.reset();
        }
        return true;
    }
    return false;
}

void MouseSelector::updateSelectableTransform(ID id, const glm::mat4& transform) {
    for (auto& selectable : selectables) {
        if (selectable.id == id) {
            selectable.transform = transform;
            break;
        }
    }
}

void MouseSelector::clearSelection() {
    selectedId.reset();
}

std::optional<ID> MouseSelector::getSelection() const {
    return selectedId;
}

void MouseSelector::handleSelection(const Input& input, const std::pair<int, int>& screenSize) {
    // Detect left-button rising edge (click) and ignore if middle button is held (camera drag)
    auto buttons = input.getMouseButtons();
    bool leftPressed = false;
    bool middlePressed = false;
    if (auto it = buttons.find("left"); it != buttons.end()) leftPressed = it->second;
    if (auto it2 = buttons.find("middle"); it2 != buttons.end()) middlePressed = it2->second;

    if (leftPressed && !mousePressedLastFrame && !middlePressed) {
        auto mousePos = input.getMousePos();
        glm::vec3 rayDir = calculateRayDirection(screenSize, mousePos);
        glm::vec3 rayOrigin = camera->Position;

        // Debug: print click state and mouse info
        {
            int sx = static_cast<int>(screenSize.first);
            int sy = static_cast<int>(screenSize.second);
            try {
                std::cout << "[MouseSelector] Click detected. left=" << leftPressed
                          << " middle=" << middlePressed
                          << " mousePos=(" << mousePos.first << "," << mousePos.second << ")"
                          << " screenSize=(" << sx << "," << sy << ")"
                          << " selectables=" << selectables.size() << std::endl;
            } catch (...) {
                // ignore logging errors in release builds
            }
        }
        // Debug: print ray origin/direction
        try {
            std::cout << "[MouseSelector] rayOrigin=(" << rayOrigin.x << "," << rayOrigin.y << "," << rayOrigin.z << ")"
                      << " rayDir=(" << rayDir.x << "," << rayDir.y << "," << rayDir.z << ")" << std::endl;
        } catch (...) {}

        std::optional<std::pair<ID, float>> closest;

        for (const auto& selectable : selectables) {
            std::optional<std::pair<ID, float>> result;
            if (selectable.model) {
                result = testModel(rayOrigin, rayDir, selectable);
            } else if (selectable.mesh) {
                result = testMesh(rayOrigin, rayDir, selectable);
            }

            if (!result) continue;
            // Debug: log each hit candidate
            try {
                std::cout << "[MouseSelector] hit candidate id=" << result->first
                          << " dist=" << result->second << std::endl;
            } catch (...) {}
            if (!closest || result->second < closest->second) closest = result;
        }

        if (closest) selectedId = closest->first;
        else selectedId.reset();
    }

    mousePressedLastFrame = leftPressed;
}

glm::vec3 MouseSelector::calculateRayDirection(const std::pair<int, int>& screenSize,
                                               const std::pair<int, int>& mousePos) const {
    float x = (2.0f * mousePos.first) / screenSize.first - 1.0f;
    float y = 1.0f - (2.0f * mousePos.second) / screenSize.second;
    glm::vec4 rayClip(x, y, -1.0f, 1.0f);

    // The standard unproject: clip -> eye (inverse projection) -> world (inverse view)
    glm::mat4 projection = glm::perspective(glm::radians(camera->Zoom),
                                            static_cast<float>(screenSize.first) / static_cast<float>(screenSize.second),
                                            nearPlane,
                                            farPlane);
    glm::mat4 view = camera->GetViewMatrix();

    // From clip space to eye space
    glm::vec4 rayEye = glm::inverse(projection) * rayClip;
    // forward in eye space points down -Z, so set z = -1 and w = 0 to represent a direction
    rayEye = glm::vec4(rayEye.x, rayEye.y, -1.0f, 0.0f);

    // From eye space to world space
    glm::vec4 worldRay4 = glm::inverse(view) * rayEye;
    glm::vec3 worldRay = glm::normalize(glm::vec3(worldRay4));
    return worldRay;
}

std::pair<bool, float> MouseSelector::rayIntersectsTriangle(const glm::vec3& rayOrigin,
                                                            const glm::vec3& rayDir,
                                                            const glm::vec3& v0,
                                                            const glm::vec3& v1,
                                                            const glm::vec3& v2) const {
    glm::vec3 edge1 = v1 - v0;
    glm::vec3 edge2 = v2 - v0;
    glm::vec3 h = glm::cross(rayDir, edge2);
    float a = glm::dot(edge1, h);
    if (std::abs(a) < kEpsilon) {
        return {false, 0.0f};
    }

    float f = 1.0f / a;
    glm::vec3 s = rayOrigin - v0;
    float u = f * glm::dot(s, h);
    if (u < 0.0f || u > 1.0f) {
        return {false, 0.0f};
    }

    glm::vec3 q = glm::cross(s, edge1);
    float v = f * glm::dot(rayDir, q);
    if (v < 0.0f || u + v > 1.0f) {
        return {false, 0.0f};
    }

    float t = f * glm::dot(edge2, q);
    if (t > kEpsilon) {
        return {true, t};
    }
    return {false, 0.0f};
}

std::optional<std::pair<ID, float>> MouseSelector::testModel(const glm::vec3& rayOrigin,
                                                             const glm::vec3& rayDir,
                                                             const Selectable& selectable) {
    if (!selectable.model) {
        return std::nullopt;
    }

    std::optional<std::pair<ID, float>> closest;
    for (const auto& mesh : selectable.model->meshes) {
        Selectable temp{selectable.id, nullptr, &mesh, selectable.transform};
        auto result = testMesh(rayOrigin, rayDir, temp);
        if (result && (!closest || result->second < closest->second)) {
            closest = result;
        }
    }
    return closest;
}

std::optional<std::pair<ID, float>> MouseSelector::testMesh(const glm::vec3& rayOrigin,
                                                            const glm::vec3& rayDir,
                                                            const Selectable& selectable) {
    if (!selectable.mesh) {
        return std::nullopt;
    }

    const auto& mesh = *selectable.mesh;
    float closestDistance = std::numeric_limits<float>::max();
    bool hit = false;

    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        Vertex v0 = mesh.vertices[mesh.indices[i]];
        Vertex v1 = mesh.vertices[mesh.indices[i + 1]];
        Vertex v2 = mesh.vertices[mesh.indices[i + 2]];

        glm::vec3 p0 = glm::vec3(selectable.transform * glm::vec4(v0.Position, 1.0f));
        glm::vec3 p1 = glm::vec3(selectable.transform * glm::vec4(v1.Position, 1.0f));
        glm::vec3 p2 = glm::vec3(selectable.transform * glm::vec4(v2.Position, 1.0f));

        auto [intersects, distance] = rayIntersectsTriangle(rayOrigin, rayDir, p0, p1, p2);
        if (intersects && distance < closestDistance) {
            closestDistance = distance;
            hit = true;
            lastIntersectionPoint = rayOrigin + rayDir * distance;
        }
    }

    if (hit) {
        return std::make_pair(selectable.id, closestDistance);
    }
    return std::nullopt;
}