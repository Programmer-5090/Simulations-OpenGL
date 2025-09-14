/**
 * GeometryData Implementation
 * Flexible vertex attribute container for OpenGL mesh generation.
 */

#include "geometry_data.h"
#include "../mesh.h"

#include <cassert>
#include <algorithm>

GeometryData::GeometryData() {}
GeometryData::~GeometryData() {}

void GeometryData::addAttribute(const std::string& name, int components, const std::vector<float>& data) {
    m_attributes[name] = data;
    m_components[name] = components;
}

void GeometryData::setIndices(const std::vector<unsigned int>& idx) {
    m_indices = idx;
}

size_t GeometryData::countVertices() const {
    auto it = m_attributes.find("v_pos");
    if (it != m_attributes.end()) {
        auto comp = std::max(1, m_components.at("v_pos"));
        return it->second.size() / static_cast<size_t>(comp);
    }
    return 0;
}

size_t GeometryData::countIndices() const {
    return m_indices.size();
}

std::vector<Triangle> GeometryData::getTriangles() const {
    std::vector<Triangle> triangles;
    
    auto it = m_attributes.find("v_pos");
    if (it == m_attributes.end()) {
        return triangles;
    }
    
    const auto& positionData = it->second;
    
    int posComponents = 3;
    auto cIt = m_components.find("v_pos");
    if (cIt != m_components.end()) {
        posComponents = cIt->second;
    }
    if (posComponents != 3) {
        return triangles;
    }

    auto extractPoint = [&](size_t vertexIndex, float point[3]) {
        size_t dataOffset = vertexIndex * 3;
        point[0] = positionData[dataOffset + 0];
        point[1] = positionData[dataOffset + 1];
        point[2] = positionData[dataOffset + 2];
    };

    if (!m_indices.empty()) {
        for (size_t i = 0; i + 2 < m_indices.size(); i += 3) {
            Triangle triangle;
            extractPoint(m_indices[i + 0], triangle.vertex1);
            extractPoint(m_indices[i + 1], triangle.vertex2);
            extractPoint(m_indices[i + 2], triangle.vertex3);
            triangles.push_back(triangle);
        }
    } else {
        size_t vertexCount = countVertices();
        for (size_t i = 0; i + 2 < vertexCount; i += 3) {
            Triangle triangle;
            extractPoint(i + 0, triangle.vertex1);
            extractPoint(i + 1, triangle.vertex2);
            extractPoint(i + 2, triangle.vertex3);
            triangles.push_back(triangle);
        }
    }
    
    return triangles;
}

Mesh GeometryData::toMesh() const {
    std::vector<Vertex> vertices;
    size_t vertexCount = countVertices();
    vertices.resize(vertexCount);

    auto fillVec3Attribute = [&](const std::string& attributeName, 
                                  glm::vec3 Vertex::* memberPointer, 
                                  const glm::vec3& defaultValue) {
        auto it = m_attributes.find(attributeName);
        int componentCount = 0;
        if (it != m_attributes.end()) {
            auto cIt = m_components.find(attributeName);
            if (cIt != m_components.end()) componentCount = cIt->second;
        }
        
        if (it == m_attributes.end() || componentCount < 3) {
            for (auto& vertex : vertices) {
                vertex.*memberPointer = defaultValue;
            }
            return;
        }
        
        const auto& attributeData = it->second;
        for (size_t i = 0; i < vertexCount; ++i) {
            size_t dataOffset = i * componentCount;
            (vertices[i].*memberPointer) = glm::vec3(
                attributeData[dataOffset + 0],
                attributeData[dataOffset + 1],
                attributeData[dataOffset + 2]
            );
        }
    };

    auto fillVec2Attribute = [&](const std::string& attributeName, 
                                  glm::vec2 Vertex::* memberPointer, 
                                  const glm::vec2& defaultValue) {
        auto it = m_attributes.find(attributeName);
        int componentCount = 0;
        if (it != m_attributes.end()) {
            auto cIt = m_components.find(attributeName);
            if (cIt != m_components.end()) componentCount = cIt->second;
        }
        
        if (it == m_attributes.end() || componentCount < 2) {
            for (auto& vertex : vertices) {
                vertex.*memberPointer = defaultValue;
            }
            return;
        }
        
        const auto& attributeData = it->second;
        for (size_t i = 0; i < vertexCount; ++i) {
            size_t dataOffset = i * componentCount;
            (vertices[i].*memberPointer) = glm::vec2(
                attributeData[dataOffset + 0],
                attributeData[dataOffset + 1]
            );
        }
    };

    fillVec3Attribute("v_pos",  &Vertex::Position,  glm::vec3(0.0f, 0.0f, 0.0f));
    fillVec3Attribute("v_norm", &Vertex::Normal,    glm::vec3(0.0f, 1.0f, 0.0f));
    fillVec2Attribute("v_uv",   &Vertex::TexCoords, glm::vec2(0.0f, 0.0f));

    std::vector<unsigned int> indices = m_indices;
    if (indices.empty()) {
        indices.resize(vertexCount);
        for (unsigned int i = 0; i < vertexCount; ++i) {
            indices[i] = i;
        }
    }

    std::vector<Texture> textures;
    return Mesh(std::move(vertices), std::move(indices), std::move(textures));
}

void GeometryData::merge(const GeometryData& other) {
    /**
     * Merge another GeometryData object into this one
     * 
     * This is useful for combining multiple objects into a single mesh.
     * For example: merging a cube and a sphere into one renderable object.
     * 
     * Process:
     * 1. Calculate vertex offset (how many vertices we already have)
     * 2. Merge attribute data (extend arrays with other's data)
     * 3. Handle missing attributes (pad with zeros to maintain alignment)
     * 4. Merge indices with proper offset to point to correct vertices
     */
    
    // Remember how many vertices we have before merging
    // This offset will be added to all indices from 'other'
    size_t baseVertexOffset = countVertices();

    // Step 1: Merge all attributes from 'other' into this object
    for (const auto& otherAttribute : other.m_attributes) {
        const std::string& attributeName = otherAttribute.first;
        const std::vector<float>& otherData = otherAttribute.second;
        int otherComponentCount = other.m_components.at(attributeName);

        // Ensure we have a matching attribute in this object
        auto& ourComponentCount = m_components[attributeName];
        if (ourComponentCount == 0) {
            // This is a new attribute - adopt the same component count as 'other'
            ourComponentCount = otherComponentCount;
        }

        auto& ourData = m_attributes[attributeName];
        
        // If this attribute didn't exist before, pad it with zeros for existing vertices
        if (ourData.empty() && baseVertexOffset > 0) {
            // Fill with zeros: [existing vertices] * [components per vertex] = total zeros needed
            size_t zerosNeeded = baseVertexOffset * ourComponentCount;
            ourData.resize(zerosNeeded, 0.0f);
        }
        
        // Append the other object's data to our data
        ourData.insert(ourData.end(), otherData.begin(), otherData.end());
    }

    // Step 2: Handle attributes that exist in this object but not in 'other'
    // We need to pad these with zeros to keep vertex alignment correct
    size_t otherVertexCount = other.countVertices();
    for (auto& ourAttribute : m_attributes) {
        const std::string& attributeName = ourAttribute.first;
        
        // If 'other' doesn't have this attribute, we need to pad with zeros
        if (other.m_attributes.find(attributeName) == other.m_attributes.end()) {
            int componentCount = m_components[attributeName];
            size_t zerosNeeded = otherVertexCount * componentCount;
            
            // Append zeros for each vertex that 'other' contributed
            ourAttribute.second.insert(ourAttribute.second.end(), zerosNeeded, 0.0f);
        }
    }

    // Step 3: Merge triangle indices with proper vertex offset
    if (!other.m_indices.empty()) {
        size_t currentIndexCount = m_indices.size();
        size_t additionalIndices = other.m_indices.size();
        
        // Resize our index array to accommodate the new indices
        m_indices.resize(currentIndexCount + additionalIndices);
        
        // Copy indices from 'other' but add the vertex offset to each one
        for (size_t i = 0; i < additionalIndices; ++i) {
            // other.m_indices[i] points to a vertex in 'other's coordinate system
            // We need to translate it to point to the correct vertex in our merged system
            m_indices[currentIndexCount + i] = static_cast<unsigned int>(baseVertexOffset + other.m_indices[i]);
        }
    }
    // Note: If 'other' has no explicit indices, we don't generate any
    // The vertices were already appended and can be used as sequential triangles
}

void GeometryData::clear() {
    /**
     * Reset this GeometryData to an empty state
     * Clears all vertex attributes, component counts, and indices
     */
    m_attributes.clear();
    m_components.clear();
    m_indices.clear();
}
