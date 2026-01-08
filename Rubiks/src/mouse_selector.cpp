/**
 * @file mouse_selector.cpp
 * @brief Implements camera-based raycasting for 3D object selection.
 *
 * This module provides functionality to shoot rays from the camera through
 * screen coordinates and test for intersections with 3D objects. The process:
 *
 * 1. Convert 2D screen coordinates to normalized device coordinates (NDC)
 * 2. Unproject the NDC point through inverse projection/view matrices
 * 3. Compute the ray direction in world space
 * 4. Test ray against all registered selectable objects using triangle intersection
 * 5. Return the closest hit object
 */

#include "mouse_selector.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/transform.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>

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

/**
 * @brief Processes mouse input to perform object selection via raycasting.
 *
 * On a left mouse button click (rising edge), this method:
 * 1. Retrieves the current mouse position in screen coordinates
 * 2. Computes a ray from the camera position through the mouse location
 * 3. Tests the ray against all registered selectable objects
 * 4. Selects the closest intersected object (if any)
 *
 * @param input The input handler containing mouse state
 * @param screenSize The current screen dimensions (width, height)
 */
void MouseSelector::handleSelection(const Input& input, const std::pair<int, int>& screenSize) {
    auto buttons = input.getMouseButtons();
    bool leftPressed = false;
    bool middlePressed = false;
    if (auto it = buttons.find("left"); it != buttons.end()) leftPressed = it->second;
    if (auto it2 = buttons.find("middle"); it2 != buttons.end()) middlePressed = it2->second;

    // Only process selection on left-button rising edge, ignore during camera drag
    if (leftPressed && !mousePressedLastFrame && !middlePressed) {
        auto mousePos = input.getMousePos();
        
        // Step 1: Compute ray from camera through screen point
        glm::vec3 rayDir = calculateRayDirection(screenSize, mousePos);
        glm::vec3 rayOrigin = camera->Position;

#ifdef DEBUG_SELECTION
        std::cout << "[MouseSelector] Click at (" << mousePos.first << "," << mousePos.second << ")"
                  << " rayOrigin=(" << rayOrigin.x << "," << rayOrigin.y << "," << rayOrigin.z << ")"
                  << " rayDir=(" << rayDir.x << "," << rayDir.y << "," << rayDir.z << ")" << std::endl;
#endif

        // Step 2: Test ray against all selectable objects
        std::optional<std::pair<ID, float>> closest;

        for (const auto& selectable : selectables) {
            std::optional<std::pair<ID, float>> result;
            if (selectable.model) {
                result = testModel(rayOrigin, rayDir, selectable);
            } else if (selectable.mesh) {
                result = testMesh(rayOrigin, rayDir, selectable);
            }

            if (!result) continue;
            
            // Step 3: Track closest intersection
            if (!closest || result->second < closest->second) {
                closest = result;
            }
        }

        // Step 4: Update selection state
        if (closest) {
            selectedId = closest->first;
        } else {
            selectedId.reset();
        }
    }

    mousePressedLastFrame = leftPressed;
}

/**
 * @brief Computes a ray direction from screen coordinates through the camera.
 *
 * This is the core of screen-to-world raycasting:
 *
 * 1. Convert screen coordinates to Normalized Device Coordinates (NDC):
 *    - X: [-1, 1] from left to right
 *    - Y: [-1, 1] from bottom to top (Y is inverted from screen coords)
 *
 * 2. Create a clip-space point at the near plane (z = -1)
 *
 * 3. Unproject to eye space using inverse projection matrix
 *
 * 4. Transform to world space using inverse view matrix
 *
 * 5. Normalize to get a unit direction vector
 *
 * @param screenSize Current viewport dimensions (width, height)
 * @param mousePos Mouse position in screen coordinates (origin at top-left)
 * @return Normalized ray direction in world space
 */
glm::vec3 MouseSelector::calculateRayDirection(const std::pair<int, int>& screenSize,
                                               const std::pair<int, int>& mousePos) const {
    // Step 1: Convert screen coordinates to NDC [-1, 1]
    float ndcX = (2.0f * mousePos.first) / screenSize.first - 1.0f;
    float ndcY = 1.0f - (2.0f * mousePos.second) / screenSize.second;  // Y inverted
    
    // Step 2: Create clip-space ray (at near plane, pointing into screen)
    glm::vec4 rayClip(ndcX, ndcY, -1.0f, 1.0f);

    // Step 3: Build projection and view matrices
    glm::mat4 projection = glm::perspective(
        glm::radians(camera->Zoom),
        static_cast<float>(screenSize.first) / static_cast<float>(screenSize.second),
        nearPlane,
        farPlane);
    glm::mat4 view = camera->GetViewMatrix();

    // Step 4: Unproject from clip space to eye space
    glm::vec4 rayEye = glm::inverse(projection) * rayClip;
    rayEye = glm::vec4(rayEye.x, rayEye.y, -1.0f, 0.0f);  // Direction, not point

    // Step 5: Transform from eye space to world space
    glm::vec4 worldRay4 = glm::inverse(view) * rayEye;
    glm::vec3 worldRay = glm::normalize(glm::vec3(worldRay4));
    
    return worldRay;
}

/**
 * @brief Tests ray intersection with a triangle using the Möller–Trumbore algorithm.
 *
 * This algorithm efficiently computes ray-triangle intersection without
 * explicitly computing the plane equation. It uses barycentric coordinates
 * to determine if the intersection point lies within the triangle.
 *
 * Mathematical basis:
 * - A point P on a ray: P = O + t*D (origin O, direction D, parameter t)
 * - A point in triangle: P = (1-u-v)*V0 + u*V1 + v*V2
 * - Solve for t, u, v using Cramer's rule
 *
 * @param rayOrigin Ray origin point (camera position)
 * @param rayDir Normalized ray direction
 * @param v0, v1, v2 Triangle vertices in world space
 * @return Pair of (hit success, distance along ray if hit)
 */
std::pair<bool, float> MouseSelector::rayIntersectsTriangle(const glm::vec3& rayOrigin,
                                                            const glm::vec3& rayDir,
                                                            const glm::vec3& v0,
                                                            const glm::vec3& v1,
                                                            const glm::vec3& v2) const {
    // Compute edge vectors from v0
    glm::vec3 edge1 = v1 - v0;
    glm::vec3 edge2 = v2 - v0;
    
    // Begin calculating determinant (also used to calculate u parameter)
    glm::vec3 h = glm::cross(rayDir, edge2);
    float det = glm::dot(edge1, h);
    
    // If determinant is near zero, ray lies in plane of triangle
    if (std::abs(det) < kEpsilon) {
        return {false, 0.0f};
    }

    float invDet = 1.0f / det;
    
    // Calculate distance from v0 to ray origin
    glm::vec3 s = rayOrigin - v0;
    
    // Calculate u parameter and test bounds
    float u = invDet * glm::dot(s, h);
    if (u < 0.0f || u > 1.0f) {
        return {false, 0.0f};
    }

    // Prepare to test v parameter
    glm::vec3 q = glm::cross(s, edge1);
    
    // Calculate v parameter and test bounds
    float v = invDet * glm::dot(rayDir, q);
    if (v < 0.0f || u + v > 1.0f) {
        return {false, 0.0f};
    }

    // Calculate t (distance along ray to intersection)
    float t = invDet * glm::dot(edge2, q);
    
    // Ray intersection (t must be positive - in front of camera)
    if (t > kEpsilon) {
        return {true, t};
    }
    
    return {false, 0.0f};
}

/**
 * @brief Tests ray intersection with all meshes in a Model.
 *
 * Iterates through each mesh in the model and finds the closest intersection.
 *
 * @param rayOrigin Ray origin (camera position)
 * @param rayDir Normalized ray direction
 * @param selectable The selectable containing the model to test
 * @return Optional pair of (ID, distance) if hit, nullopt otherwise
 */
std::optional<std::pair<ID, float>> MouseSelector::testModel(const glm::vec3& rayOrigin,
                                                             const glm::vec3& rayDir,
                                                             const Selectable& selectable) const {
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

/**
 * @brief Tests ray intersection with all triangles in a Mesh.
 *
 * For each triangle in the mesh:
 * 1. Transform vertices to world space using the selectable's transform matrix
 * 2. Test ray-triangle intersection
 * 3. Track the closest intersection point
 *
 * @param rayOrigin Ray origin (camera position)
 * @param rayDir Normalized ray direction
 * @param selectable The selectable containing the mesh to test
 * @return Optional pair of (ID, distance) if hit, nullopt otherwise
 */
std::optional<std::pair<ID, float>> MouseSelector::testMesh(const glm::vec3& rayOrigin,
                                                            const glm::vec3& rayDir,
                                                            const Selectable& selectable) const {
    if (!selectable.mesh) {
        return std::nullopt;
    }

    const auto& mesh = *selectable.mesh;
    float closestDistance = std::numeric_limits<float>::max();
    bool hit = false;

    // Iterate through triangles (3 indices per triangle)
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        // Get vertex data
        Vertex v0 = mesh.vertices[mesh.indices[i]];
        Vertex v1 = mesh.vertices[mesh.indices[i + 1]];
        Vertex v2 = mesh.vertices[mesh.indices[i + 2]];

        // Transform vertices to world space
        glm::vec3 p0 = glm::vec3(selectable.transform * glm::vec4(v0.Position, 1.0f));
        glm::vec3 p1 = glm::vec3(selectable.transform * glm::vec4(v1.Position, 1.0f));
        glm::vec3 p2 = glm::vec3(selectable.transform * glm::vec4(v2.Position, 1.0f));

        // Test intersection
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

std::vector<std::pair<ID, float>> MouseSelector::raycast(const glm::vec3& direction) const {
    return raycast(camera->Position, direction);
}

std::vector<std::pair<ID, float>> MouseSelector::raycast(const glm::vec3& origin, const glm::vec3& direction) const {
    glm::vec3 rayOrigin = origin;
    glm::vec3 rayDir = glm::normalize(direction);

    std::vector<std::pair<ID, float>> hits;

    for (const auto& selectable : selectables) {
        std::optional<std::pair<ID, float>> result;
        if (selectable.model) {
            result = testModel(rayOrigin, rayDir, selectable);
        } else if (selectable.mesh) {
            result = testMesh(rayOrigin, rayDir, selectable);
        }

        if (result) {
            hits.push_back(*result);
        }
    }

    std::sort(hits.begin(), hits.end(), [](const auto& a, const auto& b) {
        return a.second < b.second;
    });

    return hits;
}

std::optional<ID> MouseSelector::selectFromCameraCenter() {
    // Shoot a ray from camera position in the direction the camera is looking
    auto hits = raycast(camera->Front);
    
    if (!hits.empty()) {
        selectedId = hits.front().first;  // Select the closest
        return selectedId;
    }
    
    selectedId.reset();
    return std::nullopt;
}

std::vector<ID> MouseSelector::raycastGrid(int gridSize, float spreadAngle, const Camera& cam) const {
    std::vector<ID> uniqueHits;
    std::unordered_set<ID> hitSet;
    
    glm::vec3 forward = glm::normalize(cam.Front);
    glm::vec3 right = glm::normalize(cam.Right);
    glm::vec3 up = glm::normalize(cam.Up);
    glm::vec3 origin = cam.Position;
    
    float halfSpread = glm::radians(spreadAngle * 0.5f);
    float step = (gridSize > 1) ? (2.0f * halfSpread) / (gridSize - 1) : 0.0f;
    
    for (int row = 0; row < gridSize; ++row) {
        for (int col = 0; col < gridSize; ++col) {
            float yawOffset = -halfSpread + col * step;
            float pitchOffset = -halfSpread + row * step;
            
            glm::vec3 rayDir = forward;
            rayDir = glm::vec3(glm::rotate(glm::mat4(1.0f), yawOffset, up) * glm::vec4(rayDir, 0.0f));
            rayDir = glm::vec3(glm::rotate(glm::mat4(1.0f), pitchOffset, right) * glm::vec4(rayDir, 0.0f));
            rayDir = glm::normalize(rayDir);
            
            auto hits = raycast(origin, rayDir);
            if (!hits.empty()) {
                ID hitId = hits.front().first;
                if (hitSet.find(hitId) == hitSet.end()) {
                    hitSet.insert(hitId);
                    uniqueHits.push_back(hitId);
                }
            }
        }
    }
    
    return uniqueHits;
}