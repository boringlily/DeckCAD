#pragma once
#include <vector>
#include <cinttypes>
#include <math.h>

using position = float;
using distance = float;
using degree = float;
using PointIndex = uint32_t;

struct point2d
{
    position x{0};
    position y{0};
};

struct point3d : point2d
{
    position z{0};
};

struct Curve{};

struct Line2d : public Curve
{
    Line2d(point2d start, distance length, degree angle):start{start},
    length{length},
    angle{angle}
    {};

    Line2d(point2d start, point2d end):start{start},end{end}{};

point2d start{0};
point2d end{0};
distance length{0};
degree angle{0};
};

namespace CustomMath
{
    distance point_distance(position x_1,position y_1, position x_2,position y_2 )
    {
        // Equation for getting distance between points d=√((x2 – x1)² + (y2 – y1)²).
        float difference_x = pow((x_2 - x_1), 2);
        float difference_y = pow((y_2 - y_1), 2);
        float sum_xy{difference_x+difference_y};
        return sqrt(sum_xy);
    }
}

struct SketchPoint
{
    position x;
    position y;
};

struct SketchLine
{
    PointIndex start;
    PointIndex end;
};

struct SketchCurve
{
    PointIndex start;
    PointIndex end;
    PointIndex center;
};


// A sketch object represents a 2D drawing made up of simple lines and arcs. All sketches exist
// on a cartesian XY-plane. A sketch is created on a 3d plane, which is used for translating from 2d to 3d space.
class Sketch
{
public:

distance lineLength();


private:
std::vector<SketchPoint> points; 
std::vector<SketchLine> lines; 
std::vector<SketchCurve> curves; 


};





