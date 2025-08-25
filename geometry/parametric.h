#include "geometry_data.h"
#include <functional>
#include <glm/glm/glm.hpp>

class ParametricSurface : public GeometryData {
public:
    ParametricSurface();
    ~ParametricSurface();

    // Generate the surface mesh based on the parameterization
    void generateMesh(float uStart, float uEnd, int uSegments, float vStart, float vEnd, int vSegments, std::function<glm::vec3(float, float)> surface_fn);
};
