#pragma once
#include "PartCommandType.h"
#include "SketchCommandToolbox.h"

class CreateSketchCommand : public IPartCommand, public SketchCommandToolbox {
public:
    explicit CreateSketchCommand(CommandId id)
        : IPartCommand(PartCommandType::CreateSketch, id) {};

    virtual bool IsValid() override
    {
        return false;
    }

    //  for now assume that the plane is always xy.
    // std::optional<Plane3> plane;
};
