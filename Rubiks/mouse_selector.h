/**
 * @file mouse_selector.h
 * @brief Camera-based raycasting system for 3D object selection.
 *
 * This module implements raycasting from the camera through screen coordinates
 * to enable mouse-based selection of 3D objects. The raycasting pipeline:
 *
 * 1. Screen coordinates → Normalized Device Coordinates (NDC)
 * 2. NDC → Eye space via inverse projection matrix
 * 3. Eye space → World space via inverse view matrix
 * 4. Ray-triangle intersection testing using Möller–Trumbore algorithm
 *
 * Usage:
 *   MouseSelector selector(camera);
 *   ID id = selector.addSelectable(model, transform);
 *   selector.handleSelection(input, screenSize);
 *   if (auto selected = selector.getSelection()) { ... }
 */

#ifndef MOUSE_SELECTOR_H
#define MOUSE_SELECTOR_H

#include <glm/glm.hpp>

#include <optional>
#include <utility>
#include <vector>

#include "../camera.h"
#include "../mesh.h"
#include "../model.h"
#include "globals.h"
#include "input.h"

/**
 * @brief Raycasting-based mouse selection system for 3D objects.
 *
 * Provides functionality to:
 * - Register 3D objects (Models or Meshes) as selectable
 * - Compute rays from screen coordinates through the camera
 * - Test ray intersections with triangles
 * - Track and report selected objects
 */
class MouseSelector {
public:
    /**
     * @brief Represents a selectable 3D object.
     */
    struct Selectable {
        ID id;                          ///< Unique identifier for this selectable
        const Model* model{nullptr};    ///< Pointer to Model (if model-based)
        const Mesh* mesh{nullptr};      ///< Pointer to Mesh (if mesh-based)
        glm::mat4 transform{1.0f};      ///< World transform matrix
    };

    /**
     * @brief Constructs a MouseSelector with camera reference.
     * @param camera Reference to the camera for ray computation
     * @param nearPlane Near clipping plane distance
     * @param farPlane Far clipping plane distance
     */
    MouseSelector(Camera& camera, float nearPlane = 0.1f, float farPlane = 100.0f);

    /// @name Selectable Management
    /// @{
    
    /** Registers a Model as selectable, returns unique ID */
    ID addSelectable(const Model& model, const glm::mat4& transform = glm::mat4(1.0f));
    
    /** Registers a Mesh as selectable, returns unique ID */
    ID addSelectable(const Mesh& mesh, const glm::mat4& transform = glm::mat4(1.0f));
    
    /** Removes a selectable by ID, returns true if found */
    bool removeSelectable(ID id);
    
    /** Updates the world transform of a selectable */
    void updateSelectableTransform(ID id, const glm::mat4& transform);
    /// @}

    /// @name Selection State
    /// @{
    
    /** Clears current selection */
    void clearSelection();
    
    /** Programmatically sets selection to given ID */
    void setSelection(ID id) { selectedId = id; }
    
    /** Returns currently selected ID, or nullopt if none */
    std::optional<ID> getSelection() const;
    /// @}

    /**
     * @brief Processes mouse input and performs raycasting selection.
     * 
     * Call each frame to handle mouse-based object selection.
     * On left-click, shoots a ray through the cursor and selects
     * the closest intersected object.
     */
    void handleSelection(const Input& input, const std::pair<int, int>& screenSize);

    /** Returns all registered selectables (for debugging/iteration) */
    const std::vector<Selectable>& getSelectables() const { return selectables; }

private:
    Camera* camera;                      ///< Camera for ray computation
    float nearPlane;                     ///< Near clipping plane
    float farPlane;                      ///< Far clipping plane
    ID nextId{0};                        ///< Counter for generating unique IDs
    std::vector<Selectable> selectables; ///< All registered selectables
    std::optional<ID> selectedId;        ///< Currently selected object ID
    glm::vec3 lastIntersectionPoint{0.0f}; ///< Last ray-triangle hit point
    bool mousePressedLastFrame{false};   ///< For detecting click rising edge

    /// @name Raycasting Implementation
    /// @{
    
    /**
     * @brief Computes world-space ray direction from screen coordinates.
     *
     * Unprojects screen position through inverse projection and view matrices.
     */
    glm::vec3 calculateRayDirection(const std::pair<int, int>& screenSize, 
                                    const std::pair<int, int>& mousePos) const;
    
    /**
     * @brief Tests ray-triangle intersection using Möller–Trumbore algorithm.
     * @return Pair of (hit, distance) where distance is along ray if hit
     */
    std::pair<bool, float> rayIntersectsTriangle(const glm::vec3& rayOrigin,
                                                 const glm::vec3& rayDir,
                                                 const glm::vec3& v0,
                                                 const glm::vec3& v1,
                                                 const glm::vec3& v2) const;
    
    /** Tests ray against all meshes in a Model */
    std::optional<std::pair<ID, float>> testModel(const glm::vec3& rayOrigin,
                                                 const glm::vec3& rayDir,
                                                 const Selectable& selectable);
    
    /** Tests ray against all triangles in a Mesh */
    std::optional<std::pair<ID, float>> testMesh(const glm::vec3& rayOrigin,
                                                const glm::vec3& rayDir,
                                                const Selectable& selectable);
    /// @}
};

#endif // MOUSE_SELECTOR_H