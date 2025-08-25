#include "polygon.h"

class Circle : public Polygon {
public:
    Circle(int segments = 32, float radius = 1.0f) : Polygon(segments, radius) {}
    ~Circle() {}
};
