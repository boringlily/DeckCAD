#pragma once
#include "clay.h"

// typedef (CustomRenderHook *)(Clay_RenderCommand renderCommand)

enum CustomClayElementType: uint32_t
{
    WORKBENCH_CANVAS
};

typedef struct
{
    CustomClayElementType type;
    intptr_t dataAddress;
} CustomClayElement;