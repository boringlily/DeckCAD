#pragma once
#include <optional>
#include <string>
#include "DTL.h"

#include "CanvasCamera.h"
#include "ExplorerState.h"
#include "Toolbox.h"
#include "Unit.h"
#include "Workbench.h"

// forward declaration
class AppState;

// Canvas direct-manipulation state, active only in idle mode (a sketch open, no drawing
// tool running). A hovered line shows its endpoint handles; grabbing one drags the point
// or the whole line — but only where the constraints leave freedom.
enum class DragMode : u8 {
    None,
    Point, // dragging one endpoint
    Body, // translating the whole line
};

struct CanvasDrag {
    FeatureId entity { kNullFeature };
    DragMode mode { DragMode::None };
    PointRef point { PointRef::Start };
    Geometry::Point2 lastCursor {};

    bool Active() const { return mode != DragMode::None; }
};

class Scene {
public:
    Scene() = delete;

    std::string filename { "Untitled" };

    // Gui data
    CanvasCamera camera {};
    Toolbox toolbox {};
    ExplorerState explorer {};

    // The modelling session: history, contexts, tools, parameters. The GUI reads this
    // and asks it to do things; it never reaches past it into the Document.
    Workbench workbench {};

    // The scene's default unit — what a bare number in an expression means, and what
    // dimensions display in. mm is the base, so this starts as a pure display choice.
    Param::Unit display_unit { Param::Unit::Millimeter };

    // Per-frame canvas interaction state. Lives on the Scene (not as Canvas.cpp
    // file statics) so App.dll hot-reloads don't silently reset it mid-session.
    bool was_sketch_active { false };
    bool was_sketch_valid { false };
    std::optional<Geometry::SketchPlane> hovered_plane {};

    // Direct-manipulation: the line currently under the cursor (idle mode) and the drag
    // in progress, if any.
    std::optional<FeatureId> hover_entity {};
    CanvasDrag drag {};

    // Auto-save bookkeeping. The (history revision, parameter generation) pair captures
    // every persistable change; a change (re)starts a debounce so a burst of edits
    // collapses to one cache write once editing pauses. Initialized to a fresh
    // workbench's values (revision 0, generation 1) so an untouched scene isn't dirty.
    u32 saved_doc_rev { 0 };
    u32 saved_param_gen { 1 };
    bool save_pending { false };
    f64 pending_since { 0 };

private:
    Scene(std::string name)
        : filename { name } {};

    friend class AppState;
};
