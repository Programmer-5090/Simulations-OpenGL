#include "CubeMarching.h"
#include <glm/gtc/noise.hpp>
#include "tables.h"

// Edge to vertex mapping - which two vertices does each edge connect?
static const int edgeToVertices[12][2] = {
    {0,1}, {1,2}, {2,3}, {3,0},  // Bottom face edges
    {4,5}, {5,6}, {6,7}, {7,4},  // Top face edges
    {0,4}, {1,5}, {2,6}, {3,7}   // Vertical edges
};

CubeMarching::CubeMarching() : m_width(0), m_height(0), m_depth(0) {
}

CubeMarching::~CubeMarching() {}

void CubeMarching::ClearMesh() {
    m_vertices.clear();
}

void CubeMarching::setDimensions(int w, int h, int d) {
    m_width = w;
    m_height = h;
    m_depth = d;
    m_voxel_grid.resize(m_width * m_height * m_depth);
    
    // Generate scalar field values for all grid points
    for (int z = 0; z < m_depth; ++z) {
        for (int y = 0; y < m_height; ++y) {
            for (int x = 0; x < m_width; ++x) {
                int idx = x + y * m_width + z * m_width * m_height;
                m_voxel_grid[idx] = scalarField(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
            }
        }
    }
}

void CubeMarching::GenerateMesh(float isolevel) {
    ClearMesh();
    if (m_width <= 1 || m_height <= 1 || m_depth <= 1) return;

    // Iterate over each cube in the grid
    for (int z = 0; z < m_depth - 1; ++z) {
        for (int y = 0; y < m_height - 1; ++y) {
            for (int x = 0; x < m_width - 1; ++x) {
                GridCell cell;
                
                // Define the 8 corners of the cube
                glm::vec3 corners[8] = {
                    {(float)x,     (float)y,     (float)z},
                    {(float)x + 1, (float)y,     (float)z},
                    {(float)x + 1, (float)y,     (float)z + 1},
                    {(float)x,     (float)y,     (float)z + 1},
                    {(float)x,     (float)y + 1, (float)z},
                    {(float)x + 1, (float)y + 1, (float)z},
                    {(float)x + 1, (float)y + 1, (float)z + 1},
                    {(float)x,     (float)y + 1, (float)z + 1}
                };

                // Get scalar values and positions at each corner
                for(int i = 0; i < 8; ++i) {
                    cell.positions[i] = corners[i];
                    int idx = (int)corners[i].x + (int)corners[i].y * m_width + (int)corners[i].z * m_width * m_height;
                    cell.values[i] = m_voxel_grid[idx];
                }
                
                TriangulateCell(cell, isolevel);
            }
        }
    }
}

void CubeMarching::TriangulateCellAt(int gx, int gy, int gz, float isolevel) {
    // Ensure grid is valid
    if (gx < 0 || gy < 0 || gz < 0 || gx >= m_width-1 || gy >= m_height-1 || gz >= m_depth-1) return;

    GridCell cell;
    glm::vec3 corners[8] = {
        {(float)gx,     (float)gy,     (float)gz},
        {(float)gx + 1, (float)gy,     (float)gz},
        {(float)gx + 1, (float)gy,     (float)gz + 1},
        {(float)gx,     (float)gy,     (float)gz + 1},
        {(float)gx,     (float)gy + 1, (float)gz},
        {(float)gx + 1, (float)gy + 1, (float)gz},
        {(float)gx + 1, (float)gy + 1, (float)gz + 1},
        {(float)gx,     (float)gy + 1, (float)gz + 1}
    };

    for (int i = 0; i < 8; ++i) {
        cell.positions[i] = corners[i];
        int idx = (int)corners[i].x + (int)corners[i].y * m_width + (int)corners[i].z * m_width * m_height;
        cell.values[i] = m_voxel_grid[idx];
    }

    TriangulateCell(cell, isolevel);
}

void CubeMarching::TriangulateCell(const GridCell& cell, float isolevel) {
    int cubeIndex = calculateCubeIndex(cell, isolevel);
    if (cubeIndex == 0 || cubeIndex == 255) return;
    
    int edges = edgeTable[cubeIndex];
    if (edges == 0) return;
    
    Vertex intersections[12];
    for (int i = 0; i < 12; ++i) {
        if (edges & (1 << i)) {
            int v_idx1 = edgeToVertices[i][0];
            int v_idx2 = edgeToVertices[i][1];
            
            Vertex p1, p2;
            p1.Position = cell.positions[v_idx1];
            p1.Normal = CalculateGradient(p1.Position);
            p1.TexCoords = glm::vec2(0.0f);

            p2.Position = cell.positions[v_idx2];
            p2.Normal = CalculateGradient(p2.Position);
            p2.TexCoords = glm::vec2(0.0f);

            intersections[i] = VertexInterp(isolevel, p1, p2, cell.values[v_idx1], cell.values[v_idx2]);
        }
    }
    
    for (int i = 0; triTable[cubeIndex][i] != -1; i += 3) {
        m_vertices.push_back(intersections[triTable[cubeIndex][i]]);
        m_vertices.push_back(intersections[triTable[cubeIndex][i+1]]);
        m_vertices.push_back(intersections[triTable[cubeIndex][i+2]]);
    }
}

int CubeMarching::calculateCubeIndex(const GridCell& cell, float isolevel) const {
    int cubeIndex = 0;
    for (int i = 0; i < 8; ++i) {
        if (cell.values[i] < isolevel) {
            cubeIndex |= (1 << i);
        }
    }
    return cubeIndex;
}

Vertex CubeMarching::VertexInterp(float isolevel, const Vertex& p1, const Vertex& p2, float valp1, float valp2) {
    if (std::abs(valp2 - valp1) < 1e-6) {
        return p1;
    }
    
    float mu = (isolevel - valp1) / (valp2 - valp1);
    
    Vertex vert;
    // Compute interpolated position first
    vert.Position = glm::mix(p1.Position, p2.Position, mu);

    // Compute normal via gradient at the interpolated position
    glm::vec3 sampledNormal = CalculateGradient(vert.Position);
    if (glm::dot(sampledNormal, sampledNormal) < 1e-8f) {
        // If gradient is zero, interpolate the corner normals
        vert.Normal = glm::normalize(glm::mix(p1.Normal, p2.Normal, mu));
    } else {
        vert.Normal = glm::normalize(sampledNormal);
    }
    vert.TexCoords = glm::vec2(0.0f); 
    
    return vert;
}

glm::vec3 CubeMarching::CalculateGradient(const glm::vec3& p) const {
    // Relative finite-difference step (in voxel units). Tuneable.
    const float relH = 0.4f; // use less than half-voxel for better locality

    // Helper to sample scalar field while clamping to valid voxel index range
    auto sampleClamped = [&](float sxpos, float sypos, float szpos){
        float cx = glm::clamp(sxpos, 0.0f, (float)(m_width - 1));
        float cy = glm::clamp(sypos, 0.0f, (float)(m_height - 1));
        float cz = glm::clamp(szpos, 0.0f, (float)(m_depth - 1));
        return scalarField(cx, cy, cz);
    };

    // For each axis, pick an h that stays inside the valid voxel range; if we're
    // too close to the boundary, fall back to a one-sided difference.
    float hx = std::min(relH, std::min(p.x, (float)(m_width - 1) - p.x));
    float hy = std::min(relH, std::min(p.y, (float)(m_height - 1) - p.y));
    float hz = std::min(relH, std::min(p.z, (float)(m_depth - 1) - p.z));

    float dx;
    if (hx > 1e-6f) {
        dx = sampleClamped(p.x + hx, p.y, p.z) - sampleClamped(p.x - hx, p.y, p.z);
    } else {
        // one-sided forward/backward difference
        if (p.x <= 0.5f) dx = sampleClamped(p.x + relH, p.y, p.z) - sampleClamped(p.x, p.y, p.z);
        else dx = sampleClamped(p.x, p.y, p.z) - sampleClamped(p.x - relH, p.y, p.z);
    }

    float dy;
    if (hy > 1e-6f) {
        dy = sampleClamped(p.x, p.y + hy, p.z) - sampleClamped(p.x, p.y - hy, p.z);
    } else {
        if (p.y <= 0.5f) dy = sampleClamped(p.x, p.y + relH, p.z) - sampleClamped(p.x, p.y, p.z);
        else dy = sampleClamped(p.x, p.y, p.z) - sampleClamped(p.x, p.y - relH, p.z);
    }

    float dz;
    if (hz > 1e-6f) {
        dz = sampleClamped(p.x, p.y, p.z + hz) - sampleClamped(p.x, p.y, p.z - hz);
    } else {
        if (p.z <= 0.5f) dz = sampleClamped(p.x, p.y, p.z + relH) - sampleClamped(p.x, p.y, p.z);
        else dz = sampleClamped(p.x, p.y, p.z) - sampleClamped(p.x, p.y, p.z - relH);
    }

    glm::vec3 grad(dx, dy, dz);
    float len2 = glm::dot(grad, grad);
    if (len2 < 1e-14f) return glm::vec3(0.0f);
    return -glm::normalize(grad);
}

void CubeMarching::SetNoiseParameters(float resolution, float scale) {
    m_noiseResolution = resolution;
    m_noiseScale = scale;
}

void CubeMarching::SetVisualizeNoise(bool visualize) {
    m_visualizeNoise = visualize;
}

void CubeMarching::SetInfluenceRadius(float radius) {
    m_influenceRadius = radius;
}

void CubeMarching::GetInputGridBounds(int &minX, int &minY, int &minZ, int &maxX, int &maxY, int &maxZ) const {
    // Default to entire grid
    minX = minY = minZ = 0;
    maxX = m_width - 1;
    maxY = m_height - 1;
    maxZ = m_depth - 1;

    if (!m_useInputVertices || m_inputVertices.empty()) return;

    // Map each input vertex (world-space) into grid index space [0..dim-1]
    glm::vec3 gridSizeF = glm::vec3(m_width - 1, m_height - 1, m_depth - 1);
    glm::vec3 span = (m_gridMax - m_gridMin);
    // Avoid division by zero
    if (span.x == 0.0f) span.x = 1.0f;
    if (span.y == 0.0f) span.y = 1.0f;
    if (span.z == 0.0f) span.z = 1.0f;

    glm::vec3 minIdxF(std::numeric_limits<float>::infinity());
    glm::vec3 maxIdxF(-std::numeric_limits<float>::infinity());

    for (const auto &v : m_inputVertices) {
        glm::vec3 t = (v - m_gridMin) / span; // 0..1
        glm::vec3 idxF = t * gridSizeF;
        minIdxF = glm::min(minIdxF, idxF);
        maxIdxF = glm::max(maxIdxF, idxF);
    }

    // Expand by influence radius in grid-space units
    float maxScale = glm::max(glm::max(gridSizeF.x / span.x, gridSizeF.y / span.y), gridSizeF.z / span.z);
    float influenceGrid = (m_influenceRadius / glm::max(glm::max(span.x, span.y), span.z)) * glm::max(glm::max(gridSizeF.x, gridSizeF.y), gridSizeF.z);

    minX = (int)glm::clamp(std::floor(minIdxF.x - influenceGrid), 0.0f, (float)(m_width - 1));
    minY = (int)glm::clamp(std::floor(minIdxF.y - influenceGrid), 0.0f, (float)(m_height - 1));
    minZ = (int)glm::clamp(std::floor(minIdxF.z - influenceGrid), 0.0f, (float)(m_depth - 1));

    maxX = (int)glm::clamp(std::ceil(maxIdxF.x + influenceGrid), 0.0f, (float)(m_width - 1));
    maxY = (int)glm::clamp(std::ceil(maxIdxF.y + influenceGrid), 0.0f, (float)(m_height - 1));
    maxZ = (int)glm::clamp(std::ceil(maxIdxF.z + influenceGrid), 0.0f, (float)(m_depth - 1));
}

void CubeMarching::SetInputVertices(const std::vector<glm::vec3>& vertices) {
    if (vertices.empty()) return;

    m_inputVertices = vertices;
    m_useInputVertices = true;
    
    m_gridMin = vertices[0];
    m_gridMax = vertices[0];
    
    for (const auto& v : vertices) {
        m_gridMin = glm::min(m_gridMin, v);
        m_gridMax = glm::max(m_gridMax, v);
    }
    
    glm::vec3 padding = (m_gridMax - m_gridMin) * 0.2f;
    m_gridMin -= padding;
    m_gridMax += padding;
}

void CubeMarching::GenerateMeshFromVertices(float isolevel, float radius) {
    if (m_inputVertices.empty()) return;
    
    m_influenceRadius = radius;
    m_useInputVertices = true;
    
    // You can adjust this resolution
    int resolution = 64; 
    setDimensions(resolution, resolution, resolution);
    GenerateMesh(isolevel);
}

Mesh CubeMarching::CreateMesh() {
    std::vector<unsigned int> indices(m_vertices.size());
    std::iota(indices.begin(), indices.end(), 0); // Fills indices with 0, 1, 2, ...
    
    std::vector<Texture> textures; // No textures for this mesh

    return Mesh(m_vertices, indices, textures);
}

const std::vector<Vertex>& CubeMarching::GetVertices() const {
    return m_vertices;
}

float CubeMarching::scalarField(float x, float y, float z) const {
    if (m_useInputVertices) {
        glm::vec3 worldPos = m_gridMin + (glm::vec3(x, y, z) / glm::vec3(m_width-1, m_height-1, m_depth-1)) * (m_gridMax - m_gridMin);
        
        float minDist = std::numeric_limits<float>::max();
        for (const auto& v : m_inputVertices) {
            minDist = std::min(minDist, glm::distance(worldPos, v));
        }
        return minDist - m_influenceRadius;
        
    } else if (m_visualizeNoise) {
        glm::vec3 pos = glm::vec3(x, y, z) * m_noiseResolution;
        return glm::perlin(pos) * m_noiseScale;
    } else {
        // Simple sphere: distance from center
        glm::vec3 center = {m_width * 0.5f, m_height * 0.5f, m_depth * 0.5f};
    float radius = std::min(std::min((float)m_width, (float)m_height), (float)m_depth) * 0.3f;
        return glm::distance(glm::vec3(x, y, z), center) - radius;
    }
}