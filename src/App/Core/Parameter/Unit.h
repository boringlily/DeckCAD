#pragma once
#include "DTL.h"
#include <cstdio>
#include <string>
#include <string_view>

// Units and dimensional arithmetic for parametric expressions.
//
// Every Quantity carries its value in a CANONICAL BASE unit (millimetres for Length,
// radians for Angle, plain for Number) plus the kind it belongs to. Suffixed literals
// convert to base at parse time, so `10mm + 2in` is just 10 + 50.8 by the time the
// evaluator sees it; only display converts back out. That keeps the whole evaluator
// unit-agnostic and makes mixed-unit expressions fall out for free.
namespace Param {

// The dimension a value belongs to. Number is the unitless scalar.
enum class QuantityKind : u8 {
    Number,
    Length,
    Angle,
};

// A concrete unit a literal can be written in. `None` is a bare number.
enum class Unit : u8 {
    None,
    Millimeter,
    Centimeter,
    Meter,
    Inch,
    Foot,
    Degree,
    Radian,
};

constexpr f64 kPi = 3.14159265358979323846;

constexpr QuantityKind KindOf(Unit u)
{
    switch (u) {
    case Unit::Millimeter:
    case Unit::Centimeter:
    case Unit::Meter:
    case Unit::Inch:
    case Unit::Foot:
        return QuantityKind::Length;
    case Unit::Degree:
    case Unit::Radian:
        return QuantityKind::Angle;
    case Unit::None:
        break;
    }
    return QuantityKind::Number;
}

// How many base units one of `u` is worth (base: mm for Length, radians for Angle).
constexpr f64 BaseFactor(Unit u)
{
    switch (u) {
    case Unit::Millimeter:
        return 1.0;
    case Unit::Centimeter:
        return 10.0;
    case Unit::Meter:
        return 1000.0;
    case Unit::Inch:
        return 25.4;
    case Unit::Foot:
        return 304.8; // 12 in
    case Unit::Degree:
        return kPi / 180.0;
    case Unit::Radian:
        return 1.0;
    case Unit::None:
        break;
    }
    return 1.0;
}

constexpr f64 ToBase(f64 value, Unit u) { return value * BaseFactor(u); }
constexpr f64 FromBase(f64 base, Unit u) { return base / BaseFactor(u); }

// The canonical unit for a kind — what a base-unit value is implicitly written in.
constexpr Unit BaseUnitOf(QuantityKind k)
{
    switch (k) {
    case QuantityKind::Length:
        return Unit::Millimeter;
    case QuantityKind::Angle:
        return Unit::Radian;
    case QuantityKind::Number:
        break;
    }
    return Unit::None;
}

constexpr std::string_view UnitSuffix(Unit u)
{
    switch (u) {
    case Unit::Millimeter:
        return "mm";
    case Unit::Centimeter:
        return "cm";
    case Unit::Meter:
        return "m";
    case Unit::Inch:
        return "in";
    case Unit::Foot:
        return "ft";
    case Unit::Degree:
        return "deg";
    case Unit::Radian:
        return "rad";
    case Unit::None:
        break;
    }
    return "";
}

constexpr std::string_view KindName(QuantityKind k)
{
    switch (k) {
    case QuantityKind::Length:
        return "Length";
    case QuantityKind::Angle:
        return "Angle";
    case QuantityKind::Number:
        break;
    }
    return "Number";
}

// Every spelling a unit may be WRITTEN as.
//
// Input is generous, output is canonical: several aliases map to one unit, but
// UnitSuffix() always renders the short form. So `5"`, `5in` and `5 inches` all parse
// to the same thing and all display as `5in` — the table never round-trips an alias
// back out, which keeps rendered text stable no matter how it was typed.
//
// The symbol forms matter for a deck: `6'` and `5"` is how imperial lumber is actually
// written, and typing `5' 6"` is the normal way to say five foot six (see the
// feet-and-inches rule in Expression.h, which is what makes that pair add up).
//
// Case-sensitive by design. `M` is not `m` in any unit system worth imitating, and
// silently accepting either invites `5M` to quietly mean millimetres.
struct UnitAlias {
    std::string_view text;
    Unit unit;
};

inline constexpr UnitAlias kUnitAliases[] = {
    { "mm", Unit::Millimeter },
    { "millimeter", Unit::Millimeter },
    { "millimeters", Unit::Millimeter },
    { "millimetre", Unit::Millimeter },
    { "millimetres", Unit::Millimeter },

    { "cm", Unit::Centimeter },
    { "centimeter", Unit::Centimeter },
    { "centimeters", Unit::Centimeter },
    { "centimetre", Unit::Centimeter },
    { "centimetres", Unit::Centimeter },

    { "m", Unit::Meter },
    { "meter", Unit::Meter },
    { "meters", Unit::Meter },
    { "metre", Unit::Meter },
    { "metres", Unit::Meter },

    { "in", Unit::Inch },
    { "\"", Unit::Inch }, // 5"
    { "inch", Unit::Inch },
    { "inches", Unit::Inch },

    { "ft", Unit::Foot },
    { "'", Unit::Foot }, // 6'
    { "foot", Unit::Foot },
    { "feet", Unit::Foot },

    { "deg", Unit::Degree },
    { "degree", Unit::Degree },
    { "degrees", Unit::Degree },

    { "rad", Unit::Radian },
    { "radian", Unit::Radian },
    { "radians", Unit::Radian },
};

// Exact-match suffix lookup. The tokenizer hands over a WHOLE suffix — either a maximal
// run of letters or a single symbol — never a prefix, so "mm" can't be mistaken for "m"
// followed by junk and "inches" can't be read as "in" plus a stray "ches".
constexpr DTL::Optional<Unit> UnitFromSuffix(std::string_view s)
{
    for (const UnitAlias& a : kUnitAliases) {
        if (a.text == s) {
            return a.unit;
        }
    }
    return std::nullopt;
}

// Is `c` a symbolic unit suffix? These are not letters, so the tokenizer has to look
// for them explicitly rather than sweeping them up with an alpha run.
constexpr bool IsUnitSymbol(char c) { return c == '"' || c == '\''; }

// A resolved value: magnitude in base units + the dimension it belongs to.
struct Quantity {
    f64 value { 0 }; // in base units (mm / radians / plain)
    QuantityKind kind { QuantityKind::Number };
};

// Present a base-unit quantity in `display` (falls back to the kind's base unit when
// `display` belongs to a different kind — e.g. an Angle asked to render as mm).
constexpr f64 Display(Quantity q, Unit display)
{
    if (KindOf(display) != q.kind) {
        return FromBase(q.value, BaseUnitOf(q.kind));
    }
    return FromBase(q.value, display);
}

// How many decimals a displayed value keeps. 3 is a micron in mm and a thou in inches —
// finer than anything a deck is built to, and coarse enough to hide binary noise.
inline constexpr int kDisplayDecimals = 3;

// Render a number for display: fixed-point, trailing zeros trimmed.
//
// NOT "%g". %g counts SIGNIFICANT digits, so %.4g renders 1676.4 as "1676" — dropping
// real precision the moment a value passes 1000 — and flips to scientific ("1e+04") for
// anything larger. Both are wrong for a dimension the user typed by hand. Fixed-point
// with trimming gives "1676.4", "100", "0.5" and "10000".
inline std::string FormatValue(f64 v)
{
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.*f", kDisplayDecimals, v);
    std::string s { buf };

    if (s.find('.') != std::string::npos) {
        while (!s.empty() && s.back() == '0') {
            s.pop_back();
        }
        if (!s.empty() && s.back() == '.') {
            s.pop_back();
        }
    }
    if (s == "-0") {
        s = "0"; // a rounded-away negative is just zero
    }
    return s;
}

// A quantity as the user should read it: value in `display`, then the canonical suffix.
inline std::string FormatQuantity(Quantity q, Unit display)
{
    Unit u = KindOf(display) == q.kind ? display : BaseUnitOf(q.kind);
    return FormatValue(Display(q, u)) + std::string { UnitSuffix(u) };
}

} // namespace Param
