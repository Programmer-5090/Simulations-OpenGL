#include "BoundingBox.h"
#include <vector>

BoundingBox::BoundingBox(const glm::vec3& size) : VAO(0), VBO(0), EBO(0), modelMatrix(1.0f) {
    float halfWidth = size.x / 2.0f;
    float halfHeight = size.y / 2.0f;
    float halfDepth = size.z / 2.0f;

    GLfloat vertices[] = {
        -halfWidth, -halfHeight, -halfDepth,
         halfWidth, -halfHeight, -halfDepth,
         halfWidth,  halfHeight, -halfDepth,
        -halfWidth,  halfHeight, -halfDepth,
        -halfWidth, -halfHeight,  halfDepth,
         halfWidth, -halfHeight,  halfDepth,
         halfWidth,  halfHeight,  halfDepth,
        -halfWidth,  halfHeight,  halfDepth,
    };

    GLuint indices[] = {
        0, 1, 1, 2, 2, 3, 3, 0, // Back face
        4, 5, 5, 6, 6, 7, 7, 4, // Front face
        0, 4, 1, 5, 2, 6, 3, 7  // Connecting lines
    };

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (void*)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

BoundingBox::~BoundingBox() {
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
}

void BoundingBox::SetModelMatrix(const glm::mat4& model) {
    this->modelMatrix = model;
}

void BoundingBox::Render(const glm::mat4& view, const glm::mat4& projection) {
    glBindVertexArray(VAO);
    glDrawElements(GL_LINES, 24, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}
