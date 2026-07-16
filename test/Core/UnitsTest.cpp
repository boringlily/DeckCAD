// Unit conversion and dimensional bookkeeping.
//
// Every Quantity stores its magnitude in a canonical base (mm for Length, radians for
// Angle), so these tests pin the conversion factors that the whole expression layer
// silently depends on. A wrong factor here would not fail loudly anywhere else — it
// would just quietly produce a deck of the wrong size.

#include "Unit.h"

#include <gtest/gtest.h>

using namespace Param;

namespace {
constexpr f64 kEps = 1e-9;
}

TEST(Units, KindOfClassifiesEveryUnit)
{
    EXPECT_EQ(KindOf(Unit::Millimeter), QuantityKind::Length);
    EXPECT_EQ(KindOf(Unit::Centimeter), QuantityKind::Length);
    EXPECT_EQ(KindOf(Unit::Meter), QuantityKind::Length);
    EXPECT_EQ(KindOf(Unit::Inch), QuantityKind::Length);
    EXPECT_EQ(KindOf(Unit::Foot), QuantityKind::Length);
    EXPECT_EQ(KindOf(Unit::Degree), QuantityKind::Angle);
    EXPECT_EQ(KindOf(Unit::Radian), QuantityKind::Angle);
    EXPECT_EQ(KindOf(Unit::None), QuantityKind::Number);
}

TEST(Units, LengthConvertsToMillimetreBase)
{
    EXPECT_NEAR(ToBase(1.0, Unit::Millimeter), 1.0, kEps);
    EXPECT_NEAR(ToBase(1.0, Unit::Centimeter), 10.0, kEps);
    EXPECT_NEAR(ToBase(1.0, Unit::Meter), 1000.0, kEps);
    EXPECT_NEAR(ToBase(1.0, Unit::Inch), 25.4, kEps);
    EXPECT_NEAR(ToBase(1.0, Unit::Foot), 304.8, kEps);
    EXPECT_NEAR(ToBase(12.0, Unit::Inch), ToBase(1.0, Unit::Foot), kEps);
}

TEST(Units, AngleConvertsToRadianBase)
{
    EXPECT_NEAR(ToBase(180.0, Unit::Degree), kPi, kEps);
    EXPECT_NEAR(ToBase(90.0, Unit::Degree), kPi / 2.0, kEps);
    EXPECT_NEAR(ToBase(1.0, Unit::Radian), 1.0, kEps);
}

TEST(Units, ConversionRoundTrips)
{
    const Unit all[] = { Unit::Millimeter, Unit::Centimeter, Unit::Meter, Unit::Inch,
        Unit::Foot, Unit::Degree, Unit::Radian, Unit::None };
    for (Unit u : all) {
        EXPECT_NEAR(FromBase(ToBase(7.25, u), u), 7.25, kEps) << "round trip failed for " << UnitSuffix(u);
    }
}

TEST(Units, SuffixLookupMatchesWholeRunNotPrefix)
{
    EXPECT_EQ(*UnitFromSuffix("mm"), Unit::Millimeter);
    EXPECT_EQ(*UnitFromSuffix("m"), Unit::Meter);
    EXPECT_EQ(*UnitFromSuffix("cm"), Unit::Centimeter);
    EXPECT_EQ(*UnitFromSuffix("in"), Unit::Inch);
    EXPECT_EQ(*UnitFromSuffix("ft"), Unit::Foot);
    EXPECT_EQ(*UnitFromSuffix("deg"), Unit::Degree);
    EXPECT_EQ(*UnitFromSuffix("rad"), Unit::Radian);

    // Junk must not silently resolve to a prefix match. ("inch" and "feet" are real
    // aliases now — see UnitAliases below — so the junk here is junk that merely STARTS
    // with a valid unit.)
    EXPECT_FALSE(UnitFromSuffix("mmm").has_value());
    EXPECT_FALSE(UnitFromSuffix("inches2").has_value());
    EXPECT_FALSE(UnitFromSuffix("inx").has_value());
    EXPECT_FALSE(UnitFromSuffix("ftx").has_value());
    EXPECT_FALSE(UnitFromSuffix("").has_value());
    EXPECT_FALSE(UnitFromSuffix("x").has_value());
}

TEST(Units, SymbolAliases)
{
    // How imperial lumber is actually written.
    EXPECT_EQ(*UnitFromSuffix("\""), Unit::Inch);
    EXPECT_EQ(*UnitFromSuffix("'"), Unit::Foot);
    EXPECT_TRUE(IsUnitSymbol('"'));
    EXPECT_TRUE(IsUnitSymbol('\''));
    EXPECT_FALSE(IsUnitSymbol('m'));
    EXPECT_FALSE(IsUnitSymbol(' '));
}

TEST(Units, WordAliases)
{
    EXPECT_EQ(*UnitFromSuffix("inch"), Unit::Inch);
    EXPECT_EQ(*UnitFromSuffix("inches"), Unit::Inch);
    EXPECT_EQ(*UnitFromSuffix("foot"), Unit::Foot);
    EXPECT_EQ(*UnitFromSuffix("feet"), Unit::Foot);
    EXPECT_EQ(*UnitFromSuffix("millimeters"), Unit::Millimeter);
    EXPECT_EQ(*UnitFromSuffix("metre"), Unit::Meter);
    EXPECT_EQ(*UnitFromSuffix("degrees"), Unit::Degree);
    EXPECT_EQ(*UnitFromSuffix("radians"), Unit::Radian);
}

TEST(Units, EveryAliasResolvesToItsOwnUnit)
{
    for (const UnitAlias& a : kUnitAliases) {
        DTL::Optional<Unit> got = UnitFromSuffix(a.text);
        ASSERT_TRUE(got.has_value()) << a.text;
        EXPECT_EQ(*got, a.unit) << a.text;
    }
}

TEST(Units, AliasesAreUnambiguous)
{
    // Two units must never claim the same spelling — the table is scanned in order, so a
    // duplicate would silently shadow whichever came second.
    for (const UnitAlias& a : kUnitAliases) {
        u32 hits = 0;
        for (const UnitAlias& b : kUnitAliases) {
            if (a.text == b.text) {
                ++hits;
                EXPECT_EQ(a.unit, b.unit) << "conflicting alias: " << a.text;
            }
        }
        EXPECT_EQ(hits, 1u) << "duplicate alias: " << a.text;
    }
}

TEST(Units, DisplayIsAlwaysCanonicalNeverAnAlias)
{
    // Input is generous, output is not: however a value was typed, it renders short.
    EXPECT_EQ(UnitSuffix(Unit::Inch), "in");
    EXPECT_EQ(UnitSuffix(Unit::Foot), "ft");
    EXPECT_EQ(UnitSuffix(Unit::Millimeter), "mm");
    // And every canonical form is itself a valid input.
    for (Unit u : { Unit::Millimeter, Unit::Centimeter, Unit::Meter, Unit::Inch,
             Unit::Foot, Unit::Degree, Unit::Radian }) {
        ASSERT_TRUE(UnitFromSuffix(UnitSuffix(u)).has_value()) << UnitSuffix(u);
        EXPECT_EQ(*UnitFromSuffix(UnitSuffix(u)), u);
    }
}

TEST(Units, SuffixRoundTripsThroughLookup)
{
    const Unit all[] = { Unit::Millimeter, Unit::Centimeter, Unit::Meter, Unit::Inch,
        Unit::Foot, Unit::Degree, Unit::Radian };
    for (Unit u : all) {
        ASSERT_TRUE(UnitFromSuffix(UnitSuffix(u)).has_value());
        EXPECT_EQ(*UnitFromSuffix(UnitSuffix(u)), u);
    }
}

TEST(Units, DisplayConvertsOutOfBase)
{
    Quantity metre { 1000.0, QuantityKind::Length }; // 1000mm
    EXPECT_NEAR(Display(metre, Unit::Millimeter), 1000.0, kEps);
    EXPECT_NEAR(Display(metre, Unit::Centimeter), 100.0, kEps);
    EXPECT_NEAR(Display(metre, Unit::Meter), 1.0, kEps);
    EXPECT_NEAR(Display(metre, Unit::Inch), 1000.0 / 25.4, kEps);

    Quantity halfTurn { kPi, QuantityKind::Angle };
    EXPECT_NEAR(Display(halfTurn, Unit::Degree), 180.0, kEps);
}

TEST(Units, DisplayFallsBackWhenAskedForAForeignKind)
{
    // An Angle asked to render as millimetres falls back to its own base rather than
    // producing a nonsense number.
    Quantity halfTurn { kPi, QuantityKind::Angle };
    EXPECT_NEAR(Display(halfTurn, Unit::Millimeter), kPi, kEps);
}

// ── formatting ───────────────────────────────────────────────────────────────

TEST(Units, FormatValueKeepsPrecisionPastAThousand)
{
    // The bug %g had: %.4g counts SIGNIFICANT digits, so 1676.4 rendered as "1676" and
    // silently dropped 0.4mm — and 10000 became "1e+04".
    EXPECT_EQ(FormatValue(1676.4), "1676.4");
    EXPECT_EQ(FormatValue(2438.4), "2438.4");
    EXPECT_EQ(FormatValue(10000.0), "10000");
    EXPECT_EQ(FormatValue(123456.5), "123456.5");
}

TEST(Units, FormatValueTrimsTrailingZeros)
{
    EXPECT_EQ(FormatValue(100.0), "100");
    EXPECT_EQ(FormatValue(0.5), "0.5");
    EXPECT_EQ(FormatValue(0.25), "0.25");
    EXPECT_EQ(FormatValue(1.100), "1.1");
    EXPECT_EQ(FormatValue(0.0), "0");
}

TEST(Units, FormatValueNeverUsesScientificNotation)
{
    EXPECT_EQ(FormatValue(1e6).find('e'), std::string::npos);
    EXPECT_EQ(FormatValue(1e-4).find('e'), std::string::npos);
}

TEST(Units, FormatValueHasNoNegativeZero)
{
    // A value rounded away to nothing is zero, not "-0".
    EXPECT_EQ(FormatValue(-0.0), "0");
    EXPECT_EQ(FormatValue(-0.0001), "0");
}

TEST(Units, FormatValueHandlesNegatives)
{
    EXPECT_EQ(FormatValue(-5.5), "-5.5");
    EXPECT_EQ(FormatValue(-1676.4), "-1676.4");
}

TEST(Units, FormatQuantityAppendsTheCanonicalSuffix)
{
    EXPECT_EQ(FormatQuantity({ 1676.4, QuantityKind::Length }, Unit::Millimeter), "1676.4mm");
    EXPECT_EQ(FormatQuantity({ 1524.0, QuantityKind::Length }, Unit::Foot), "5ft");
    EXPECT_EQ(FormatQuantity({ 50.8, QuantityKind::Length }, Unit::Inch), "2in");
    EXPECT_EQ(FormatQuantity({ kPi, QuantityKind::Angle }, Unit::Degree), "180deg");
    EXPECT_EQ(FormatQuantity({ 7.5, QuantityKind::Number }, Unit::None), "7.5");
}

TEST(Units, FormatQuantityFallsBackForAForeignDisplayUnit)
{
    // An Angle asked to render as millimetres renders in its own base instead.
    EXPECT_EQ(FormatQuantity({ kPi, QuantityKind::Angle }, Unit::Millimeter), "3.142rad");
}

TEST(Units, BaseUnitOfMatchesTheChosenCanonicalUnits)
{
    EXPECT_EQ(BaseUnitOf(QuantityKind::Length), Unit::Millimeter);
    EXPECT_EQ(BaseUnitOf(QuantityKind::Angle), Unit::Radian);
    EXPECT_EQ(BaseUnitOf(QuantityKind::Number), Unit::None);
    // A base unit must convert 1:1 by definition.
    EXPECT_NEAR(BaseFactor(BaseUnitOf(QuantityKind::Length)), 1.0, kEps);
    EXPECT_NEAR(BaseFactor(BaseUnitOf(QuantityKind::Angle)), 1.0, kEps);
}
