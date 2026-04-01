#pragma once
#include "PartCommandType.h"
#include "CreateSketchCommand.h"
#include "Sketch/SketchCommandType.h"
#include "Sketch/SketchCommandToolbox.h"

class PartFeature : public CommandVariant<PartCommandType, CreateSketchCommand> {
};