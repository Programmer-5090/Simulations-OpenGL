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

class MouseSelector {
public:
    struct Selectable {
        ID id;
        const Model* model{nullptr};
        const Mesh* mesh{nullptr};
        glm::mat4 transform{1.0f};
    };

    MouseSelector(Camera& camera, float nearPlane = 0.1f, float farPlane = 100.0f);

    ID addSelectable(const Model& model, const glm::mat4& transform = glm::mat4(1.0f));
    ID addSelectable(const Mesh& mesh, const glm::mat4& transform = glm::mat4(1.0f));
    bool removeSelectable(ID id);
    void updateSelectableTransform(ID id, const glm::mat4& transform);

    void clearSelection();
    void setSelection(ID id) { selectedId = id; }
    std::optional<ID> getSelection() const;

    void handleSelection(const Input& input, const std::pair<int, int>& screenSize);

    const std::vector<Selectable>& getSelectables() const { return selectables; }

private:
    Camera* camera;
    float nearPlane;
    float farPlane;
    ID nextId{0};
    std::vector<Selectable> selectables;
    std::optional<ID> selectedId;
    glm::vec3 lastIntersectionPoint{0.0f};
    bool mousePressedLastFrame{false};

    glm::vec3 calculateRayDirection(const std::pair<int, int>& screenSize, const std::pair<int, int>& mousePos) const;
    std::pair<bool, float> rayIntersectsTriangle(const glm::vec3& rayOrigin,
                                                 const glm::vec3& rayDir,
                                                 const glm::vec3& v0,
                                                 const glm::vec3& v1,
                                                 const glm::vec3& v2) const;
    std::optional<std::pair<ID, float>> testModel(const glm::vec3& rayOrigin,
                                                 const glm::vec3& rayDir,
                                                 const Selectable& selectable);
    std::optional<std::pair<ID, float>> testMesh(const glm::vec3& rayOrigin,
                                                const glm::vec3& rayDir,
                                                const Selectable& selectable);
};

#endif // MOUSE_SELECTOR_H