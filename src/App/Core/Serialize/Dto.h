#pragma once
#include "Document.h"
#include "Geometry.h"
#include "SketchDocument.h"
#include "Unit.h"

#include <glaze/json.hpp>

#include <string>
#include <variant>
#include <vector>

// The on-disk schema for a `.dcad` file — plain aggregate structs glaze reflects with
// zero annotation, kept deliberately separate from the runtime command types.
//
// Why a parallel schema instead of serializing the live types directly: glaze's pure
// reflection wants aggregates, but the runtime types are the opposite — SketchCmdBase
// has a virtual destructor, PolymorphicVariant keeps its variant private, and SketchCmd
// is recursive. A DTO layer sidesteps all of that AND gives a stable, versioned format
// decoupled from internal layout, so a refactor of the runtime types can't silently
// change the file format. The one-time cost is the explicit conversion in Convert.h —
// which is exactly the variant-tag switch the command design called for.
//
// ONLY persistent data lives here. Evaluated geometry is derived (replay + solve), so it
// is never written; loading replays the commands and regenerates everything.
namespace Serialize {

// Bump when the schema changes incompatibly. ReadDcad refuses an unknown version rather
// than guessing, so an old build can't silently misread a newer file.
inline constexpr u32 kDcadVersion = 1;

struct Vec2Dto {
    f64 x { 0 };
    f64 y { 0 };
};

// ── sketch commands ──────────────────────────────────────────────────────────
// Each mirrors one concrete SketchCmd alternative. `id` is the FeatureId, which must
// round-trip: dimensions and constraints reference entities by it.

struct LineDto {
    u32 id { kNullFeature };
    Vec2Dto a {};
    Vec2Dto b {};
    bool construction { false };
};

struct ArcDto {
    u32 id { kNullFeature };
    Vec2Dto center {};
    f64 radius { 0 };
    f64 startAngle { 0 };
    f64 endAngle { 0 };
    bool construction { false };
};

struct CircleDto {
    u32 id { kNullFeature };
    Vec2Dto center {};
    f64 radius { 0 };
    bool construction { false };
};

struct DimensionDto {
    u32 id { kNullFeature };
    DimensionKind kind { DimensionKind::Length };
    u32 targetA { kNullFeature };
    u32 targetB { kNullFeature };
    u32 value { Param::kNullUpid }; // the dimension's expression, by UPID
};

struct ConstraintDto {
    u32 id { kNullFeature };
    ConstraintKind kind { ConstraintKind::Coincident };
    u32 a { kNullFeature };
    u32 b { kNullFeature };
    u32 axis { kNullFeature };
    PointRef aPoint { PointRef::Start }; // Coincident only
    PointRef bPoint { PointRef::Start };
};

// The recursive case. Ordering mirrors SketchCmd's own trick: the variant wrapper is
// forward-declared, GroupDto holds a vector of it (vector tolerates an incomplete
// element type), and the variant is defined last, once GroupDto is a complete type.
// Unlike std::vector, std::variant needs its alternatives COMPLETE — so GroupDto must
// come before the variant, not after.
struct SketchCmdDto;
struct GroupDto {
    u32 id { kNullFeature };
    std::vector<SketchCmdDto> children;
};

using SketchCmdVariant = std::variant<LineDto, ArcDto, CircleDto, DimensionDto, ConstraintDto, GroupDto>;
struct SketchCmdDto {
    SketchCmdVariant v;
};

// ── part commands ────────────────────────────────────────────────────────────
struct SketchFeatureDto {
    u32 id { kNullFeature };
    Geometry::SketchPlane plane { Geometry::SketchPlane::XY };
    std::vector<SketchCmdDto> children;
};

// A single-alternative tagged variant today; grows as part commands (Extrude, ...) do.
using CommandVariant = std::variant<SketchFeatureDto>;
struct CommandDto {
    CommandVariant v;
};

// ── parameters ───────────────────────────────────────────────────────────────
struct ParameterDto {
    u32 uid { Param::kNullUpid };
    std::string name;
    std::string expression;
    u32 owner { Param::kNoOwner }; // the owning sketch's FeatureId, or kNoOwner
    Param::Unit display { Param::Unit::None };
};

// ── the document ─────────────────────────────────────────────────────────────
struct DocumentDto {
    u32 version { kDcadVersion };

    // Id counters — must round-trip so a command or parameter created after a load can
    // never collide with a loaded id.
    u32 featureNextId { 1 };
    u32 paramNextId { 0 };

    u32 cursor { 0 }; // undo position

    std::vector<CommandDto> history;
    std::vector<ParameterDto> parameters;

    Param::Unit displayUnit { Param::Unit::Millimeter }; // the scene's default unit
    std::string sceneName;
};

} // namespace Serialize

// ── glaze reflection ───────────────────────────────────────────────────────────
// Enums map to STRINGS, not integers: readable in the file, and stable if an enum is
// ever reordered (the name is the key, not the ordinal). Add an enumerator here whenever
// one is added to the runtime enum — e.g. the new constraint kinds from the solver work.

template <>
struct glz::meta<Geometry::SketchPlane> {
    using enum Geometry::SketchPlane;
    static constexpr auto value = enumerate(XY, XZ, YZ);
};

template <>
struct glz::meta<DimensionKind> {
    using enum DimensionKind;
    static constexpr auto value = enumerate(Length, Radius, Angle, Distance);
};

template <>
struct glz::meta<ConstraintKind> {
    using enum ConstraintKind;
    static constexpr auto value = enumerate(Coincident, Symmetry, Horizontal, Vertical,
        Parallel, Perpendicular, Tangent, Equal, Ground);
};

template <>
struct glz::meta<PointRef> {
    using enum PointRef;
    static constexpr auto value = enumerate(Start, End, Center);
};

template <>
struct glz::meta<Param::Unit> {
    using enum Param::Unit;
    static constexpr auto value = enumerate(
        None, Millimeter, Centimeter, Meter, Inch, Foot, Degree, Radian);
};

// Tagged variants: a "type" discriminator makes each command self-describing on disk, so
// read-back is unambiguous rather than shape-guessed.
//
// The tag is "type", NOT "kind", on purpose: DimensionDto and ConstraintDto already have
// a member field named `kind`, and a tag of the same name collides — the member value
// overwrites the tag on write, leaving read unable to match the variant. Keep this tag
// distinct from every DTO field name.
template <>
struct glz::meta<Serialize::SketchCmdVariant> {
    static constexpr std::string_view tag = "type";
    static constexpr auto ids = std::array<std::string_view, 6> {
        "line", "arc", "circle", "dimension", "constraint", "group"
    };
};

template <>
struct glz::meta<Serialize::CommandVariant> {
    static constexpr std::string_view tag = "type";
    static constexpr auto ids = std::array<std::string_view, 1> { "sketch" };
};
