#pragma once
#include "DumbTypes.h"

namespace Geometry {

struct Point2 {
    f64 x { 0 };
    f64 y { 0 };
};

struct Point3 : public Point2 {
    f64 z { 0 };
};

struct Point4 : public Point3 {
    f64 w { 0 };
};

/// @brief A 2D plane in 3D space with a direction independent of the normal.
struct Plane3d {
    Point3 center;
    Point3 direction;
};

};