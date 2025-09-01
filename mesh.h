#pragma once

#include <iostream>
#include <vector>
#include "glad/glad.h"
#include "glm/glm/glm.hpp"
#include "shader.h"



struct Vertex {
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoords;
};
struct Texture {
    unsigned int id;
    std::string type;
    std::string path;  // we store the path of the texture to compare with other textures
};

class Mesh {
    public:
        // mesh data
        std::vector<Vertex>       vertices;
        std::vector<unsigned int> indices;
        std::vector<Texture>      textures;

        Mesh(std::vector<Vertex> vertices, std::vector<unsigned int> indices, std::vector<Texture> textures);
        void Draw(Shader &shader);
        
        // Getters for instanced rendering
        unsigned int getVAO() const { return VAO; }
        unsigned int getIndexCount() const { return static_cast<unsigned int>(indices.size()); }

    private:
        // render data
        unsigned int VAO, VBO, EBO;

        void setupMesh();
};