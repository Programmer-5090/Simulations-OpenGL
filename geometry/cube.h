#ifndef CUBOID_H
#define CUBOID_H
#include "geometry_data.h"

class Cuboid : public GeometryData {
    public:
    Cuboid(float width, float height, float depth){
        float halfWidth = width / 2.0f;
        float halfHeight = height / 2.0f;
        float halfDepth = depth / 2.0f;

        // 24 vertices (4 per face, 6 faces) with proper normals for each face
        float vertices[] = {
            // Back face (z = -halfDepth)
            -halfWidth, -halfHeight, -halfDepth,
             halfWidth, -halfHeight, -halfDepth,
             halfWidth,  halfHeight, -halfDepth,
            -halfWidth,  halfHeight, -halfDepth,
            
            // Front face (z = halfDepth)
            -halfWidth, -halfHeight,  halfDepth,
             halfWidth, -halfHeight,  halfDepth,
             halfWidth,  halfHeight,  halfDepth,
            -halfWidth,  halfHeight,  halfDepth,
            
            // Left face (x = -halfWidth)
            -halfWidth, -halfHeight, -halfDepth,
            -halfWidth, -halfHeight,  halfDepth,
            -halfWidth,  halfHeight,  halfDepth,
            -halfWidth,  halfHeight, -halfDepth,
            
            // Right face (x = halfWidth)
             halfWidth, -halfHeight, -halfDepth,
             halfWidth, -halfHeight,  halfDepth,
             halfWidth,  halfHeight,  halfDepth,
             halfWidth,  halfHeight, -halfDepth,
            
            // Bottom face (y = -halfHeight)
            -halfWidth, -halfHeight, -halfDepth,
             halfWidth, -halfHeight, -halfDepth,
             halfWidth, -halfHeight,  halfDepth,
            -halfWidth, -halfHeight,  halfDepth,
            
            // Top face (y = halfHeight)
            -halfWidth,  halfHeight, -halfDepth,
             halfWidth,  halfHeight, -halfDepth,
             halfWidth,  halfHeight,  halfDepth,
            -halfWidth,  halfHeight,  halfDepth,
        };

        // Indices for triangles (6 faces * 2 triangles * 3 vertices)
        unsigned int indices[] = {
            // Back face
            0, 1, 2,  2, 3, 0,
            // Front face
            4, 5, 6,  6, 7, 4,
            // Left face
            8, 9, 10,  10, 11, 8,
            // Right face
            12, 13, 14,  14, 15, 12,
            // Bottom face
            16, 17, 18,  18, 19, 16,
            // Top face
            20, 21, 22,  22, 23, 20,
        };

        // Normals for each vertex
        float normals[] = {
            // Back face
            0.0f, 0.0f, -1.0f,
            0.0f, 0.0f, -1.0f,
            0.0f, 0.0f, -1.0f,
            0.0f, 0.0f, -1.0f,
            
            // Front face
            0.0f, 0.0f, 1.0f,
            0.0f, 0.0f, 1.0f,
            0.0f, 0.0f, 1.0f,
            0.0f, 0.0f, 1.0f,
            
            // Left face
            -1.0f, 0.0f, 0.0f,
            -1.0f, 0.0f, 0.0f,
            -1.0f, 0.0f, 0.0f,
            -1.0f, 0.0f, 0.0f,
            
            // Right face
            1.0f, 0.0f, 0.0f,
            1.0f, 0.0f, 0.0f,
            1.0f, 0.0f, 0.0f,
            1.0f, 0.0f, 0.0f,
            
            // Bottom face
            0.0f, -1.0f, 0.0f,
            0.0f, -1.0f, 0.0f,
            0.0f, -1.0f, 0.0f,
            0.0f, -1.0f, 0.0f,
            
            // Top face
            0.0f, 1.0f, 0.0f,
            0.0f, 1.0f, 0.0f,
            0.0f, 1.0f, 0.0f,
            0.0f, 1.0f, 0.0f,
        };

        float textureCoords[] = {
            // Back face
            0.0f, 0.0f,  1.0f, 0.0f,  1.0f, 1.0f,  0.0f, 1.0f,
            // Front face
            0.0f, 0.0f,  1.0f, 0.0f,  1.0f, 1.0f,  0.0f, 1.0f,
            // Left face
            0.0f, 0.0f,  1.0f, 0.0f,  1.0f, 1.0f,  0.0f, 1.0f,
            // Right face
            0.0f, 0.0f,  1.0f, 0.0f,  1.0f, 1.0f,  0.0f, 1.0f,
            // Bottom face
            0.0f, 0.0f,  1.0f, 0.0f,  1.0f, 1.0f,  0.0f, 1.0f,
            // Top face
            0.0f, 0.0f,  1.0f, 0.0f,  1.0f, 1.0f,  0.0f, 1.0f,
        };

        addAttribute("v_pos", 3, std::vector<float>(vertices, vertices + sizeof(vertices) / sizeof(float)));
        addAttribute("v_norm", 3, std::vector<float>(normals, normals + sizeof(normals) / sizeof(float)));
        addAttribute("v_uv", 2, std::vector<float>(textureCoords, textureCoords + sizeof(textureCoords) / sizeof(float)));

        setIndices(std::vector<unsigned int>(indices, indices + sizeof(indices) / sizeof(unsigned int)));
    }
    
    Cuboid(float length) : Cuboid(length, length, length) {
        // Delegate to the main constructor
    }
    ~Cuboid(){}
};

#endif