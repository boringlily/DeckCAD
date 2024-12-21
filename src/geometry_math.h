#pragma once
#include <cinttypes>
#include <math.h>

using position = float;
using distance = float;
using degree = float;

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
};
