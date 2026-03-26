#pragma once
#include "PartCommandType.h"
#include "SketchCommandToolbox.h"

class CreateSketchCommand : public IPartCommand, public SketchCommandToolbox {
public:
    virtual bool IsValid() override
    {
        return false;
    }
};
