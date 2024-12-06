#pragma once
#include <cinttypes>
#include <math.h>

using position = float;
using distance = float;
using degree = float;

struct Plane
{
    degree x_theta{0};
    degree y_theta{0};
    distance offset{0};
};

namespace DefaultPrimitives
{
   constexpr Plane plane_xy{0,0,0};
   constexpr Plane plane_xz{90,0,0};
   constexpr Plane plane_yz{90,0,0};
}

struct point2d
{
    position x{0};
    position y{0};
};

struct point3d : point2d
{
    position z{0};
};

namespace CustomMath
{
    inline distance point_distance(position x_1, position y_1, position x_2,position y_2 )
    {
        // Equation for getting distance between points d=√((x2 – x1)² + (y2 – y1)²).
        float difference_x = pow((x_2 - x_1), 2);
        float difference_y = pow((y_2 - y_1), 2);
        float sum_xy{difference_x+difference_y};
        return sqrt(sum_xy);
    }

    /* Equation for getting distance between points d=√((x2 – x1)² + (y2 – y1)²).
     x2 - x1 = d*cos(theta)
     y2 - y1 = d*sin(theta)

    */
}
