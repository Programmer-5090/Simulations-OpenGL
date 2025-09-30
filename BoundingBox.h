#ifndef BOUNDINGBOX_H
#define BOUNDINGBOX_H

#include <glad/glad.h>
#include <glm/glm.hpp>

class BoundingBox {
public:
    BoundingBox(const glm::vec3& size);
    ~BoundingBox();

    void Render(const glm::mat4& view, const glm::mat4& projection);
    void SetModelMatrix(const glm::mat4& model);

private:
    GLuint VAO, VBO, EBO;
    glm::mat4 modelMatrix;
};

#endif // BOUNDINGBOX_H
