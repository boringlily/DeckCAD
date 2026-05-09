#pragma once
#include "PartCommandType.h"
#include "SketchFeature.h"
#include "Geometry.h"

class CreateSketchCommand : public IPartCommand {
public:
    explicit CreateSketchCommand(CommandId id)
        : IPartCommand(PartCommandType::CreateSketch, id)
    {
    }

    virtual bool IsValid() override
    {
        return plane.has_value();
    }

    std::optional<Geometry::SketchPlane> plane;

    std::vector<SketchFeature> history;
};
