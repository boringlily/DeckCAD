#pragma once
#include "PartCommandType.h"
#include "Sketch/SketchCommandToolbox.h"

class PartFeature : public CommandVariant<PartCommandType, CreateSketchCommand> {
};