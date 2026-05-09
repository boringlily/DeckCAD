#pragma once
#include "Geometry.h"
#include <vector>

namespace Geometry {

struct SketchLine {
    Point2 start;
    Point2 end;
};

class Sketch {
public:
    SketchPlane plane { SketchPlane::XY };
    std::vector<SketchLine> lines;
};

}
