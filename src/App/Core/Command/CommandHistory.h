#pragma once
#include <DTL.h>

struct SketchCommandSlice {
    u32 command_offset { 0 };
    u32 command_count { 0 };
};

class CommandHistory {
public:
    explicit CommandHistory()
    {
        // Reserve some memory to avoid early allocations.
        part_history.reserve(1000);
        sketch_history.reserve(1000);
        sketch_command_slices.reserve(100);
    };

    bool SaveSketchContextCommands(u32 id, const std::vector<SketchCommand>& commands)
    {
        SketchCommandSlice slice { .command_offset = static_cast<u32>(sketch_history.size()), .command_count = static_cast<u32>(commands.size()) };

        if (id >= sketch_command_slices.size()) {
            sketch_command_slices.resize(id + 1);
        }

        sketch_command_slices[id] = slice;
        sketch_history.insert(sketch_history.end(), commands.begin(), commands.end());

        return true;
    }

    std::optional<GeneralCommand> active_general_command;

    // Top level context history.
    std::vector<GeneralCommand> part_history;

    // Sketch history holds all commands used by all sketches of the given history,
    // and the sketch contexts store the information about which commands belong to which sketch.
    std::vector<SketchCommandSlice> sketch_command_slices;
    std::vector<SketchCommand> sketch_history;
};
