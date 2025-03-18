#include "sketch.h"
#include "raylib.h"

bool Sketch::addLineFromPoints(point2d start, point2d end)
{
    points.emplace_back(start);
    points.emplace_back(end);
    
    uint32_t size = points.size();
    lines.emplace_back(SketchLine{.start = size-2, .end = size-1 });
    return true;
}

bool Sketch::addLine(point2d start, distance length, degree angle)
{   
    position x = length * cos(angle) + start.x;
    position y = length * sin(angle) + start.y;

    return addLineFromPoints(start, {x,y});
}

bool Sketch::addArcFrom3Points(point2d start, point2d center, point2d end)
{
    points.emplace_back(start);
    points.emplace_back(center);
    points.emplace_back(end);
    
    uint32_t size = points.size();
    arcs.emplace_back(
        SketchArc{
            .start= PointIndex(size-3), 
            .center = PointIndex(size-2) , 
            .end = PointIndex(size-1) }
            );
    
    return true;
}
   
void Sketch::render()
{
    // Draw Points
    // for(auto &p : points)
    // {
        
    //     DrawSphere(p, 0.02, RED);
    // }   

    // Draw Lines
    for(auto &l : lines)
    {
        SketchPoint p = points[l.start];
        Vector3 p1 = {p.x,p.y,0};
        p = points[l.end];
        Vector3 p2 = {p.x,p.y,0};

        DrawLine3D(p1, p2, BLACK);
    }

};