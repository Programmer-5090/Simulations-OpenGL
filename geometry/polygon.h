#include "geometry_data.h"

#ifndef GEOMETRY_POLYGON_H

class Polygon : public GeometryData{
    public:
        Polygon(int sides, float radius);
        ~Polygon();

    private:
        int sides;
        float radius;
};

#endif // GEOMETRY_POLYGON_H