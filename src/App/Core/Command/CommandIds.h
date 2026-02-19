#pragma once
#include "DumbTypes.h"

class WireframeCommandId {
public:
    enum Type : u32 {
        WireCommand,
    };

    Type type;
    u32 index;
};
