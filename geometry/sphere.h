#include "parametric.h"

class Sphere : public ParametricSurface {
public:
    Sphere(float radius, int rSegments);
    ~Sphere();


private:
    float m_radius;
    int m_uSegments;
    int m_vSegments;
};
