#pragma once
#include "DTL.h"
#include "FeatureCommand.h"

class PartFeature : public FeatureCommand {
public:
    DTL::Optional<PartFeature> Create(CommandType type)
    {
        if (type.IsPartCommand()) {
            return PartFeature(type);
        }

        return std::nullopt;
    }

private:
    PartFeature() = delete;
    PartFeature(CommandType type)
        : FeatureCommand(type) {};
};

class SketchFeature : public FeatureCommand {
public:
    DTL::Optional<SketchFeature> Create(CommandType type)
    {
        if (type.IsSketchCommand()) {
            return SketchFeature(type);
        }

        return std::nullopt;
    }

private:
    SketchFeature() = delete;
    SketchFeature(CommandType type)
        : FeatureCommand(type) {};
};

enum class CommandContext {
    Part,
    Sketch
};

class CommandManager {
public:
    bool StartCommand(CommandType type)
    {
    }

    void CancelCommand()
    {
    }

    bool FinishCommand()
    {
        if (context == CommandContext::Sketch) {
            if (active_sketch_command.has_value() && active_sketch_command->Valid()) {

            } else if (active_part_command().has_value() && active_part_command->Valid()) {
            }
            return false;
        } else if (context == CommandContext::Part && active_part_command.has_value()) {
        }

        return false;
    }

    CommandContext GetContext()
    {
        return context;
    }

private:
    bool StartSketchContext()
    {
        if (context != CommandContext::Sketch
            && active_part_command.has_value()
            && active_part_command->type == CommandType::CreateSketch) {
            context = CommandContext::Sketch;
            return true;
        } else if (context == CommandContext::Sketch) {
            // Context already active, do nothing.
            return true;
        }
        return false;
    }

    bool EndSketchContext()
    {
    }

    CommandContext context;

    DTL::Optional<PartFeature> active_part_command;
    DTL::Optional<SketchFeature> active_sketch_command;

    DTL::List<PartFeature> part_history;
    DTL::List<SketchFeature> sketch_history;
};