#pragma once
#include "SketchFeature.h"
#include "ICommandToolbox.h"

class SketchCommandToolbox : public ICommandToolbox<SketchCommandType, SketchFeature> {
};
