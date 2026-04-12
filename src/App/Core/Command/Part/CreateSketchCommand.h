#pragma once
#include "PartCommandType.h"
#include "SketchFeature.h"

class CreateSketchCommand : public IPartCommand {
public:
    explicit CreateSketchCommand(CommandId id)
        : IPartCommand(PartCommandType::CreateSketch, id) {};

    virtual bool IsValid() override
    {
        return false;
    }

    //  for now assume that the plane is always xy.
    // std::optional<Plane3> plane;

    std::vector<SketchFeature> history;
};
