#include "sketch.h"

   
bool Sketch::addLineFromPoints(point2d start, point2d end)
{
    points.emplace_back(start);
    points.emplace_back(end);
    
    uint32_t size = points.size();
    lines.emplace_back(SketchLine{.start{size-2}, .end{size-1}});
}

bool Sketch::addArcFrom3Points(point2d start, point2d center, point2d end)
{
    points.emplace_back(start);
    points.emplace_back(center);
    points.emplace_back(end);
    
    uint32_t size = points.size();
    arcs.emplace_back(
        SketchArc{
            .start{size-3}, 
            .center{size-2}, 
            .end{size-1}}
            );
}
   
