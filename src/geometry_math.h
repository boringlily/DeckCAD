#pragma once
#include <cinttypes>
#include <math.h>
#include "raymath.h"

using position = float;
using distance = float;
using degree = float;

namespace CustomMath
{
    constexpr float Pi{3.14};
    constexpr degree radians_to_deg{180/Pi};

    inline distance point_distance(position x_1, position y_1, position x_2,position y_2 )
    {
        // Equation for getting distance between points d=√((x2 – x1)² + (y2 – y1)²).
        float difference_x = pow((x_2 - x_1), 2);
        float difference_y = pow((y_2 - y_1), 2);
        float sum_xy{difference_x+difference_y};
        return sqrt(sum_xy);
    }

    enum GridSector2D
    {
        /*
                |
      2(-x,+y)  |   1(+x,+y)
       _ _ _ _ _|_ _ _ _ _ _
                |
      3(-x,-y)  |  4(+x,-y)
                |
        */

        POSx_POSy = 1,  
        NEGx_POSy,
        NEGx_NEGy,
        POSx_NEGy,

    };

    inline GridSector2D get_point_sector(Vector2 point)
    {
       GridSector2D sector{};
        bool positive_x{point.x > -1.0};
        bool positive_y{point.y > -1.0};
        
        if(positive_x)
        {
            if(positive_y)
            {
                sector = POSx_POSy;
            }
            else
            {
                sector = POSx_NEGy;
            }
        }
        else
        {

        }
        
        return sector;
    }
    
    // inline degree line_angle_to_x_axis(Vector2 start_point, Vector2 end_point)
    // {
    //     Vector2 delta = {end_point.x - start_point.x, (start_point.y - end_point.y)};
    //     auto sector_angle = atan(delta.y/delta.x) * CustomMath::radians_to_deg;

    //     DrawText(std::format("B({:.2f},{:.2f}) - A:({:.2f},{:.2f}) = delta({:.2f},{:.2f})", B.x, B.y, A.x, A.y, delta.x, delta.y).c_str()
    //     ,10, 10, 16, DARKBLUE);
    //     return sector_angle;
    // }

    // inline degree line_angle_to_x_axis(Vector2 start_point, Vector2 end_point)
    // {
    //     Vector2 delta = {end_point.x - start_point.x, -(start_point.y - end_point.y)};
    //     auto sector_angle = abs(atan(delta.x/delta.y)*radians_to_deg);

    //     DrawText(std::format("B({:.2f},{:.2f}) - A:({:.2f},{:.2f}) = delta({:.2f},{:.2f})", B.x, B.y, A.x, A.y, delta_x, delta_y).c_str()
    //     ,10, 10, 16, DARKBLUE);
    //     return sector_angle;
    // }
};
