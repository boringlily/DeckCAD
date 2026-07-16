#pragma once
#include "DTL.h"
#include <string_view>

// The tool catalogue.
//
// This table is the SINGLE source of truth for what a tool is called, which group it
// renders under, and what input it needs before it can finish. The toolbar builds
// itself from the current context's available_tools() crossed with this table, so
// adding a tool is one row here plus its Finish() case — not an edit to a switch in the
// toolbox AND another switch in the UI, which is what the old design required.

enum class ToolId : u8 {
    None,

    // Part context.
    CreateSketch,

    // Sketch context — drawing.
    Line,
    Arc,
    Circle,

    // Sketch context — dimensions and constraints.
    Dimension,
    Coincident,
    Parallel,
    Perpendicular,
    Equal,
    Tangent,
    Ground,
    SymmetryGroup,

    // Context exits.
    FinishSketch,
    StopSymmetry,
};

// What a tool needs before ToolOutcome can be produced. This is the declared input
// sequence: the canvas feeds points and picks generically and never needs to know which
// tool is running.
struct ToolInfo {
    ToolId id { ToolId::None };
    std::string_view name;
    std::string_view group; // the tool-group box it renders in
    u32 points { 0 }; // canvas clicks required
    u32 picks { 0 }; // existing entities that must be selected
    bool value { false }; // needs a typed expression (the toolbox value field)
    bool plane { false }; // needs an origin plane picked in the canvas
    bool immediate { false }; // fires the moment it is chosen (needs no input at all)
};

inline constexpr ToolInfo kToolTable[] = {
    //                                                        pts picks value plane immediate
    { ToolId::CreateSketch, "Create Sketch", "Part", 0, 0, false, true, false },

    { ToolId::Line, "Line", "Draw", 2, 0, false, false, false },
    { ToolId::Circle, "Circle", "Draw", 2, 0, false, false, false },
    { ToolId::Arc, "Arc", "Draw", 3, 0, false, false, false },

    { ToolId::Dimension, "Dimension", "Dimensions", 0, 1, true, false, false },

    { ToolId::Coincident, "Coincident", "Constraints", 0, 2, false, false, false },
    { ToolId::Parallel, "Parallel", "Constraints", 0, 2, false, false, false },
    { ToolId::Perpendicular, "Perpendicular", "Constraints", 0, 2, false, false, false },
    { ToolId::Equal, "Equal", "Constraints", 0, 2, false, false, false },
    { ToolId::Tangent, "Tangent", "Constraints", 0, 2, false, false, false },
    { ToolId::Ground, "Fix", "Constraints", 0, 1, false, false, false },
    { ToolId::SymmetryGroup, "Mirror", "Constraints", 0, 1, false, false, false },

    { ToolId::FinishSketch, "Finish Sketch", "Sketch", 0, 0, false, false, true },
    { ToolId::StopSymmetry, "Stop Mirror", "Sketch", 0, 0, false, false, true },
};

inline const ToolInfo* FindTool(ToolId id)
{
    for (const ToolInfo& t : kToolTable) {
        if (t.id == id) {
            return &t;
        }
    }
    return nullptr;
}

inline std::string_view ToolName(ToolId id)
{
    const ToolInfo* t = FindTool(id);
    return t ? t->name : "";
}
