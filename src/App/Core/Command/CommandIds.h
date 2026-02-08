#pragma once
#include "DumbTypes.h"

class GeneralCommandId {
public:
    enum Type : u32 {
        StartSketch,
        Extrude
    };

    Type type;
    u32 index;
};

class SketchCommandId {
public:
    enum Type : u32 {
        StartSketch,
        Extrude
    };

    Type type;
    u32 index;
};

class WireframeCommandId {
public:
    enum Type : u32 {
        WireCommand,
    };

    Type type;
    u32 index;
};
