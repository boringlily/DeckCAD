#pragma once
#include <algorithm>
#include "PartFeature.h"
#include "SketchFeature.h"
#include "Geometry.h"

class CommandToolbox {
public:
    bool IsSketchContext()
    {
        return sketch_command.has_value() || (IsPartCommandActive() && part_command.value().IsType(PartCommandType::CreateSketch));
    }

    bool IsSketchCommandActive()
    {
        return sketch_command.has_value();
    }

    bool IsPartCommandActive()
    {
        return part_command.has_value();
    }

    bool StartSketchCommand(SketchCommandType type)
    {
        if (sketch_command.has_value())
            return false;
        if (!IsPartCommandActive() || !part_command.value().IsType(PartCommandType::CreateSketch))
            return false;
        if (!part_command.value().IsValid())
            return false;

        CommandId id = next_id++;
        switch (type) {
        case SketchCommandType::Line:
            sketch_command.emplace(SketchLineCommand(id));
            break;
        case SketchCommandType::Arc:
            sketch_command.emplace(SketchArcCommand(id));
            break;
        case SketchCommandType::Circle:
            sketch_command.emplace(SketchCircleCommand(id));
            break;
        case SketchCommandType::Dimension:
            sketch_command.emplace(SketchDimensionCommand(id));
            break;
        default:
            return false;
        }
        return true;
    }

    bool StartCreateSketch()
    {
        if (part_command.has_value())
            return false;
        CommandId id = next_id++;
        part_command.emplace(CreateSketchCommand(id));
        return true;
    }

    std::optional<Geometry::SketchPlane> GetActiveSketchPlane() const
    {
        if (!part_command.has_value())
            return std::nullopt;
        if (auto* cmd = const_cast<PartFeature&>(part_command.value()).As<CreateSketchCommand>())
            return cmd->plane;
        return std::nullopt;
    }

    bool StartPartCommand(PartCommandType type)
    {
        if (part_command.has_value())
            return false;

        CommandId id = next_id++;
        switch (type) {
        case PartCommandType::CreateSketch:
            part_command.emplace(CreateSketchCommand(id));
            break;
        default:
            return false;
        }
        return true;
    }

    void CancelSketchCommand()
    {
        sketch_command.reset();
    }

    void CancelPartCommand()
    {
        sketch_command.reset();
        part_command.reset();
    }

    void FinishSketchCommand()
    {
        if (!sketch_command.has_value())
            return;
        if (IsPartCommandActive() && part_command.value().IsType(PartCommandType::CreateSketch)) {
            auto* cmd = part_command.value().As<CreateSketchCommand>();
            if (cmd) {
                cmd->history.push_back(std::move(sketch_command.value()));
            }
        }
        sketch_command.reset();
    }

    void FinishPartCommand()
    {
        if (!part_command.has_value())
            return;
        if (sketch_command.has_value()) {
            FinishSketchCommand();
        }
        part_history.push_back(std::move(part_command.value()));
        part_command.reset();
    }

    bool DeleteSketchCommand(CommandId id)
    {
        if (!part_command.has_value())
            return false;
        auto* cmd = part_command.value().As<CreateSketchCommand>();
        if (!cmd)
            return false;
        auto it = std::find_if(cmd->history.begin(), cmd->history.end(),
            [id](SketchFeature& f) { return f.GetId() == id; });
        if (it == cmd->history.end())
            return false;
        cmd->history.erase(it);
        return true;
    }

    std::optional<SketchFeature>& GetActiveSketchCommand()
    {
        return sketch_command;
    }

    std::optional<PartFeature>& GetActivePartCommand()
    {
        return part_command;
    }

private:
    CommandId next_id { 0 };

    std::optional<PartFeature> part_command { std::nullopt };
    std::optional<SketchFeature> sketch_command { std::nullopt };

    std::vector<PartFeature> part_history;
};
