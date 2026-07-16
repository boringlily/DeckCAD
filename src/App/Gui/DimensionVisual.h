#pragma once
#include "DTL.h"
#include "Geometry.h"
#include "ParameterEngine.h"
#include "SketchDocument.h"
#include "Unit.h"
#include <cstdio>
#include <cmath>
#include <string>
#include <vector>

// Turning a sketch's dimensions into something drawable.
//
// Deliberately free of raylib: this is the geometry and the LABEL TEXT, which is pure
// logic over the document and the parameter table, and therefore testable headlessly.
// The canvas does the drawing; the "does the label fit on the dimension line" decision
// stays there because it needs real font metrics.
namespace AppUi {

// How far a dimension line sits off the entity it measures, in sketch units.
constexpr f64 DIM_OFFSET { 1.2 };
constexpr f32 DIM_TEXT_SIZE { 14.0f };

// One dimension, resolved for drawing. Built at BUILD time (it needs the parameter
// table), consumed at dispatch by Draw3D/Draw2D.
struct DimensionVisual {
    Geometry::Point2 a {}; // dimension line start (offset off the entity)
    Geometry::Point2 b {}; // dimension line end
    Geometry::Point2 extA {}; // the measured entity's endpoints, for extension lines
    Geometry::Point2 extB {};
    std::string label; // "expr = value", or just the value for a plain literal
    bool ok { false }; // did the expression resolve?
};

// Build the drawable form of every dimension in `doc`.
//
// The label shows the EXPRESSION and its RESULT ("$w * 2 = 200mm"), collapsing to just
// the result when the expression already says exactly that — "100mm = 100mm" is noise.
//
// The test is a literal string compare against the rendered value, NOT "does it contain
// an operator". Those differ in the cases that matter most here: `5' 6"` and `2in` are
// plain literals with no operator in them, but their rendered result (`1676.4mm`,
// `50.8mm`) is not what was typed — and showing the conversion is the whole point of
// letting someone type feet and inches into a millimetre model.
std::vector<DimensionVisual> BuildDimensionVisuals(const SketchDocument& doc,
    const Param::ParameterEngine& params, Param::Unit display)
{
    std::vector<DimensionVisual> out;

    for (const SketchDimensionRecord& d : doc.dimensions) {
        const SketchEntity* e = doc.Find(d.targetA);
        if (!e || e->kind != EntityKind::Line) {
            continue; // only linear dimensions have a dimension line to draw so far
        }

        DimensionVisual v {};
        v.extA = e->a;
        v.extB = e->b;

        // Offset the dimension line along the entity's normal so it doesn't sit on top
        // of the geometry it measures.
        f64 dx = e->b.x - e->a.x;
        f64 dy = e->b.y - e->a.y;
        f64 len = std::sqrt(dx * dx + dy * dy);
        f64 nx = len > 0 ? -dy / len : 0.0;
        f64 ny = len > 0 ? dx / len : 1.0;
        v.a = { e->a.x + nx * DIM_OFFSET, e->a.y + ny * DIM_OFFSET };
        v.b = { e->b.x + nx * DIM_OFFSET, e->b.y + ny * DIM_OFFSET };

        const Param::ParametricExpression* p = params.Get(d.value);
        if (!p) {
            continue;
        }
        Param::EvalResult r = params.Value(d.value);
        v.ok = r.Ok();

        if (r.Ok()) {
            Param::Unit unit = p->DisplayUnit() != Param::Unit::None
                ? p->DisplayUnit()
                : (r.value.kind == Param::QuantityKind::Length ? display : Param::BaseUnitOf(r.value.kind));
            std::string num = Param::FormatQuantity(r.value, unit);

            // Both halves, unless the expression is already literally the result.
            const std::string& text = p->Text();
            v.label = (text == num) ? num : text + " = " + num;
        } else {
            v.label = p->Text() + " = ?";
        }

        out.push_back(std::move(v));
    }

    return out;
}
} // namespace AppUi
