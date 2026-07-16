// The parametric expression language: parsing, dimensional arithmetic, and errors.
//
// The build is -fno-exceptions, so every failure path here must come back as a
// ParserError with a usable source span rather than a throw. Several tests assert on
// pos/len specifically because the editor underlines exactly that span.

#include "Expression.h"

#include <gtest/gtest.h>
#include <string>
#include <vector>

using namespace Param;

namespace {

constexpr f64 kEps = 1e-9;

// A fixed parameter table standing in for ParameterEngine, so parser tests stay
// independent of engine behaviour.
struct StubTable {
    struct Entry {
        std::string name;
        Quantity value;
    };
    std::vector<Entry> entries;

    void Add(std::string name, f64 v, QuantityKind k) { entries.push_back({ std::move(name), { v, k } }); }

    static bool Thunk(void* user, std::string_view name, Quantity& out, ParserError& err)
    {
        auto* self = static_cast<StubTable*>(user);
        for (const Entry& e : self->entries) {
            if (e.name == name) {
                out = e.value;
                return true;
            }
        }
        err = MakeError(ParserError::Type::UnknownParameter, 0, 0, "unknown parameter");
        return false;
    }

    ParamResolver Resolver() { return ParamResolver { this, &Thunk }; }
};

// Parse+evaluate with no parameters available.
EvalResult Eval(std::string_view text)
{
    StubTable empty;
    return Expression::Parse(text).Evaluate(empty.Resolver());
}

EvalResult EvalWith(std::string_view text, StubTable& table)
{
    return Expression::Parse(text).Evaluate(table.Resolver());
}

} // namespace

// ── literals and arithmetic ──────────────────────────────────────────────────

TEST(ExpressionParser, PlainNumber)
{
    EvalResult r = Eval("42");
    ASSERT_TRUE(r.Ok()) << r.error.msg;
    EXPECT_NEAR(r.value.value, 42.0, kEps);
    EXPECT_EQ(r.value.kind, QuantityKind::Number);
}

TEST(ExpressionParser, DecimalNumber)
{
    EvalResult r = Eval("3.5");
    ASSERT_TRUE(r.Ok());
    EXPECT_NEAR(r.value.value, 3.5, kEps);
}

TEST(ExpressionParser, LeadingDotNumber)
{
    EvalResult r = Eval(".5 + .25");
    ASSERT_TRUE(r.Ok()) << r.error.msg;
    EXPECT_NEAR(r.value.value, 0.75, kEps);
}

TEST(ExpressionParser, OperatorPrecedence)
{
    EvalResult r = Eval("2 + 3 * 4");
    ASSERT_TRUE(r.Ok());
    EXPECT_NEAR(r.value.value, 14.0, kEps); // not 20
}

TEST(ExpressionParser, ParenthesesOverridePrecedence)
{
    EvalResult r = Eval("(2 + 3) * 4");
    ASSERT_TRUE(r.Ok());
    EXPECT_NEAR(r.value.value, 20.0, kEps);
}

TEST(ExpressionParser, SubtractionIsLeftAssociative)
{
    EvalResult r = Eval("10 - 3 - 2");
    ASSERT_TRUE(r.Ok());
    EXPECT_NEAR(r.value.value, 5.0, kEps); // not 9
}

TEST(ExpressionParser, DivisionIsLeftAssociative)
{
    EvalResult r = Eval("100 / 5 / 2");
    ASSERT_TRUE(r.Ok());
    EXPECT_NEAR(r.value.value, 10.0, kEps); // not 40
}

TEST(ExpressionParser, PowerIsRightAssociative)
{
    EvalResult r = Eval("2 ^ 3 ^ 2");
    ASSERT_TRUE(r.Ok());
    EXPECT_NEAR(r.value.value, 512.0, kEps); // 2^(3^2), not (2^3)^2 == 64
}

TEST(ExpressionParser, UnaryMinus)
{
    EvalResult r = Eval("-5 + 2");
    ASSERT_TRUE(r.Ok());
    EXPECT_NEAR(r.value.value, -3.0, kEps);
}

TEST(ExpressionParser, UnaryMinusOnParenthesizedGroup)
{
    EvalResult r = Eval("-(2 + 3) * 2");
    ASSERT_TRUE(r.Ok());
    EXPECT_NEAR(r.value.value, -10.0, kEps);
}

TEST(ExpressionParser, DoubleUnaryMinus)
{
    EvalResult r = Eval("--5");
    ASSERT_TRUE(r.Ok()) << r.error.msg;
    EXPECT_NEAR(r.value.value, 5.0, kEps);
}

TEST(ExpressionParser, UnaryMinusPreservesKind)
{
    EvalResult r = Eval("-10mm");
    ASSERT_TRUE(r.Ok());
    EXPECT_NEAR(r.value.value, -10.0, kEps);
    EXPECT_EQ(r.value.kind, QuantityKind::Length);
}

// ── units ────────────────────────────────────────────────────────────────────

TEST(ExpressionParser, UnitSuffixConvertsToBase)
{
    EvalResult r = Eval("2in");
    ASSERT_TRUE(r.Ok());
    EXPECT_NEAR(r.value.value, 50.8, kEps);
    EXPECT_EQ(r.value.kind, QuantityKind::Length);
}

TEST(ExpressionParser, MixedUnitsResolveThroughTheBase)
{
    // The headline case from the requirements: 10mm + 2in == 60.8mm.
    EvalResult r = Eval("10mm + 2in");
    ASSERT_TRUE(r.Ok()) << r.error.msg;
    EXPECT_NEAR(r.value.value, 60.8, kEps);
    EXPECT_EQ(r.value.kind, QuantityKind::Length);
}

TEST(ExpressionParser, MetreAndMillimetreAreNotConfused)
{
    // "mm" must not lex as "m" — that would be a 1000x error.
    EvalResult mm = Eval("5mm");
    EvalResult m = Eval("5m");
    ASSERT_TRUE(mm.Ok());
    ASSERT_TRUE(m.Ok());
    EXPECT_NEAR(mm.value.value, 5.0, kEps);
    EXPECT_NEAR(m.value.value, 5000.0, kEps);
}

// ── unit spellings ───────────────────────────────────────────────────────────

TEST(ExpressionParser, InchSymbol)
{
    EvalResult r = Eval("5\"");
    ASSERT_TRUE(r.Ok()) << r.error.msg;
    EXPECT_NEAR(r.value.value, 127.0, kEps); // 5 * 25.4
    EXPECT_EQ(r.value.kind, QuantityKind::Length);
}

TEST(ExpressionParser, FootSymbol)
{
    EvalResult r = Eval("6'");
    ASSERT_TRUE(r.Ok()) << r.error.msg;
    EXPECT_NEAR(r.value.value, 1828.8, kEps); // 6 * 304.8
    EXPECT_EQ(r.value.kind, QuantityKind::Length);
}

TEST(ExpressionParser, SymbolAndWordAgree)
{
    EXPECT_NEAR(Eval("5\"").value.value, Eval("5in").value.value, kEps);
    EXPECT_NEAR(Eval("5\"").value.value, Eval("5inch").value.value, kEps);
    EXPECT_NEAR(Eval("5\"").value.value, Eval("5 inches").value.value, kEps);
    EXPECT_NEAR(Eval("6'").value.value, Eval("6ft").value.value, kEps);
    EXPECT_NEAR(Eval("6'").value.value, Eval("6foot").value.value, kEps);
    EXPECT_NEAR(Eval("6'").value.value, Eval("6 feet").value.value, kEps);
}

TEST(ExpressionParser, LongUnitWords)
{
    EXPECT_NEAR(Eval("5 millimeters").value.value, 5.0, kEps);
    EXPECT_NEAR(Eval("5 centimetres").value.value, 50.0, kEps);
    EXPECT_NEAR(Eval("5 meters").value.value, 5000.0, kEps);
    EXPECT_NEAR(Eval("180 degrees").value.value, kPi, kEps);
    EXPECT_NEAR(Eval("1 radian").value.value, 1.0, kEps);
}

TEST(ExpressionParser, SpaceBetweenNumberAndUnitIsOptional)
{
    EXPECT_NEAR(Eval("5mm").value.value, Eval("5 mm").value.value, kEps);
    EXPECT_NEAR(Eval("5in").value.value, Eval("5 in").value.value, kEps);
    EXPECT_NEAR(Eval("5\"").value.value, Eval("5 \"").value.value, kEps);
}

TEST(ExpressionParser, ASeparatedUnitDoesNotSwallowTheNextOperator)
{
    // The suffix lookahead must not eat the space in `1 + 2` looking for a unit.
    EvalResult r = Eval("1 + 2");
    ASSERT_TRUE(r.Ok()) << r.error.msg;
    EXPECT_NEAR(r.value.value, 3.0, kEps);

    EvalResult call = Eval("@Min(1, 2)");
    ASSERT_TRUE(call.Ok()) << call.error.msg;
    EXPECT_NEAR(call.value.value, 1.0, kEps);
}

TEST(ExpressionParser, UnitSymbolsWorkInArithmetic)
{
    EvalResult r = Eval("2\" + 1\"");
    ASSERT_TRUE(r.Ok()) << r.error.msg;
    EXPECT_NEAR(r.value.value, 76.2, kEps); // 3 inches
}

TEST(ExpressionParser, UnitAliasesAreCaseSensitive)
{
    // `5M` must not quietly mean millimetres (or metres).
    EXPECT_FALSE(Eval("5MM").Ok());
    EXPECT_FALSE(Eval("5IN").Ok());
    EXPECT_FALSE(Eval("5Inch").Ok());
}

TEST(ExpressionParser, ABareUnitSymbolIsInvalid)
{
    // The symbols are only ever suffixes; standalone they are nothing.
    EXPECT_FALSE(Eval("\"").Ok());
    EXPECT_FALSE(Eval("'").Ok());
    EXPECT_FALSE(Eval("1 + \"").Ok());
}

TEST(ExpressionParser, UnknownWordAfterANumberIsReported)
{
    EvalResult glued = Eval("10furlong");
    ASSERT_FALSE(glued.Ok());
    EXPECT_EQ(glued.error.type, ParserError::Type::Invalid);

    EvalResult spaced = Eval("10 furlongs");
    ASSERT_FALSE(spaced.Ok());
    EXPECT_EQ(spaced.error.type, ParserError::Type::Invalid);
}

// ── feet and inches ──────────────────────────────────────────────────────────

TEST(ExpressionParser, FeetAndInchesAddUp)
{
    // Five foot six: 5*304.8 + 6*25.4 == 1676.4mm.
    EvalResult r = Eval("5' 6\"");
    ASSERT_TRUE(r.Ok()) << r.error.msg;
    EXPECT_NEAR(r.value.value, 1676.4, kEps);
    EXPECT_EQ(r.value.kind, QuantityKind::Length);
}

TEST(ExpressionParser, FeetAndInchesWithoutASpace)
{
    EvalResult r = Eval("5'6\"");
    ASSERT_TRUE(r.Ok()) << r.error.msg;
    EXPECT_NEAR(r.value.value, 1676.4, kEps);
}

TEST(ExpressionParser, FeetAndInchesInArithmetic)
{
    EvalResult r = Eval("5' 6\" * 2");
    ASSERT_TRUE(r.Ok()) << r.error.msg;
    EXPECT_NEAR(r.value.value, 3352.8, kEps);
    EXPECT_EQ(r.value.kind, QuantityKind::Length);
}

TEST(ExpressionParser, FeetAndInchesAsACallArgument)
{
    EvalResult r = Eval("@Max(5' 6\", 1m)");
    ASSERT_TRUE(r.Ok()) << r.error.msg;
    EXPECT_NEAR(r.value.value, 1676.4, kEps);
}

TEST(ExpressionParser, FeetAloneAndInchesAloneStillWork)
{
    EXPECT_NEAR(Eval("5'").value.value, 1524.0, kEps);
    EXPECT_NEAR(Eval("6\"").value.value, 152.4, kEps);
}

TEST(ExpressionParser, ImplicitAdditionIsFeetInchesOnly)
{
    // The compound rule must not become a general "two numbers in a row means add".
    // A missing operator is a typo and has to stay an error.
    EXPECT_FALSE(Eval("5mm 6mm").Ok());
    EXPECT_FALSE(Eval("5 6").Ok());
    EXPECT_FALSE(Eval("5' 6").Ok()); // bare number, not inches
    EXPECT_FALSE(Eval("5\" 6\"").Ok()); // inches then inches
    EXPECT_FALSE(Eval("5\" 6'").Ok()); // wrong order
    EXPECT_FALSE(Eval("5m 6\"").Ok()); // metres then inches
}

TEST(ExpressionParser, FeetMinusInchesIsSubtractionNotCompound)
{
    // An explicit operator always wins over the implicit rule.
    EvalResult r = Eval("5' - 6\"");
    ASSERT_TRUE(r.Ok()) << r.error.msg;
    EXPECT_NEAR(r.value.value, 1524.0 - 152.4, kEps);
}

TEST(ExpressionParser, FeetAndInchesSpansBothTokensForErrorReporting)
{
    Expression e = Expression::Parse("5' 6\" + 45deg");
    ASSERT_TRUE(e.Ok()) << e.Error().msg;
    StubTable t;
    EvalResult r = e.Evaluate(t.Resolver());
    // Length + Angle is still rejected — the compound is a literal, not a special case.
    ASSERT_FALSE(r.Ok());
    EXPECT_EQ(r.error.type, ParserError::Type::UnitMismatch);
}

TEST(ExpressionParser, AngleLiteral)
{
    EvalResult r = Eval("45deg");
    ASSERT_TRUE(r.Ok());
    EXPECT_NEAR(r.value.value, kPi / 4.0, kEps);
    EXPECT_EQ(r.value.kind, QuantityKind::Angle);
}

TEST(ExpressionParser, LengthTimesNumberStaysLength)
{
    EvalResult r = Eval("10mm * 3");
    ASSERT_TRUE(r.Ok());
    EXPECT_NEAR(r.value.value, 30.0, kEps);
    EXPECT_EQ(r.value.kind, QuantityKind::Length);
}

TEST(ExpressionParser, NumberTimesLengthStaysLength)
{
    EvalResult r = Eval("3 * 10mm");
    ASSERT_TRUE(r.Ok());
    EXPECT_EQ(r.value.kind, QuantityKind::Length);
}

TEST(ExpressionParser, LengthDividedByLengthIsARatio)
{
    EvalResult r = Eval("100mm / 10mm");
    ASSERT_TRUE(r.Ok()) << r.error.msg;
    EXPECT_NEAR(r.value.value, 10.0, kEps);
    EXPECT_EQ(r.value.kind, QuantityKind::Number);
}

TEST(ExpressionParser, LengthDividedByNumberStaysLength)
{
    EvalResult r = Eval("100mm / 4");
    ASSERT_TRUE(r.Ok());
    EXPECT_NEAR(r.value.value, 25.0, kEps);
    EXPECT_EQ(r.value.kind, QuantityKind::Length);
}

TEST(ExpressionParser, AddingDifferentKindsIsRejected)
{
    EvalResult r = Eval("10mm + 45deg");
    ASSERT_FALSE(r.Ok());
    EXPECT_EQ(r.error.type, ParserError::Type::UnitMismatch);
}

TEST(ExpressionParser, MultiplyingTwoLengthsIsRejected)
{
    // No area type exists, so this must be an error rather than a bogus Length.
    EvalResult r = Eval("10mm * 10mm");
    ASSERT_FALSE(r.Ok());
    EXPECT_EQ(r.error.type, ParserError::Type::UnitNotAllowed);
}

TEST(ExpressionParser, NumberDividedByLengthIsRejected)
{
    EvalResult r = Eval("10 / 2mm");
    ASSERT_FALSE(r.Ok());
    EXPECT_EQ(r.error.type, ParserError::Type::UnitNotAllowed);
}

TEST(ExpressionParser, ExponentRequiresPlainNumbers)
{
    EvalResult r = Eval("10mm ^ 2");
    ASSERT_FALSE(r.Ok());
    EXPECT_EQ(r.error.type, ParserError::Type::UnitNotAllowed);
}

// ── $ parameter references ───────────────────────────────────────────────────

TEST(ExpressionParser, ParameterReferenceResolves)
{
    StubTable t;
    t.Add("width", 100.0, QuantityKind::Length);
    EvalResult r = EvalWith("$width", t);
    ASSERT_TRUE(r.Ok()) << r.error.msg;
    EXPECT_NEAR(r.value.value, 100.0, kEps);
    EXPECT_EQ(r.value.kind, QuantityKind::Length);
}

TEST(ExpressionParser, ParameterReferenceInArithmetic)
{
    StubTable t;
    t.Add("w", 100.0, QuantityKind::Length);
    EvalResult r = EvalWith("$w * 2 + 5mm", t);
    ASSERT_TRUE(r.Ok()) << r.error.msg;
    EXPECT_NEAR(r.value.value, 205.0, kEps);
    EXPECT_EQ(r.value.kind, QuantityKind::Length);
}

TEST(ExpressionParser, UnknownParameterIsReported)
{
    StubTable t;
    EvalResult r = EvalWith("$nope + 1", t);
    ASSERT_FALSE(r.Ok());
    EXPECT_EQ(r.error.type, ParserError::Type::UnknownParameter);
}

TEST(ExpressionParser, UnknownParameterErrorSpansTheReference)
{
    StubTable t;
    Expression e = Expression::Parse("1 + $missing");
    ASSERT_TRUE(e.Ok());
    EvalResult r = e.Evaluate(t.Resolver());
    ASSERT_FALSE(r.Ok());
    // "$missing" starts at byte 4 and is 8 bytes long — the editor underlines this.
    EXPECT_EQ(r.error.pos, 4u);
    EXPECT_EQ(r.error.len, 8u);
}

TEST(ExpressionParser, ReferencesAreCollectedInFirstSeenOrderWithoutDuplicates)
{
    Expression e = Expression::Parse("$b + $a * $b");
    ASSERT_TRUE(e.Ok()) << e.Error().msg;
    ASSERT_EQ(e.References().size(), 2u);
    EXPECT_EQ(e.References()[0], "b");
    EXPECT_EQ(e.References()[1], "a");
}

TEST(ExpressionParser, BareDollarIsInvalid)
{
    EvalResult r = Eval("$ + 1");
    ASSERT_FALSE(r.Ok());
    EXPECT_EQ(r.error.type, ParserError::Type::Invalid);
}

// ── @ builtin functions ──────────────────────────────────────────────────────

TEST(ExpressionParser, MinAndMax)
{
    EvalResult lo = Eval("@Min(3, 1, 2)");
    ASSERT_TRUE(lo.Ok()) << lo.error.msg;
    EXPECT_NEAR(lo.value.value, 1.0, kEps);

    EvalResult hi = Eval("@Max(3, 1, 2)");
    ASSERT_TRUE(hi.Ok());
    EXPECT_NEAR(hi.value.value, 3.0, kEps);
}

TEST(ExpressionParser, MinPreservesUnitsAndCompaysInBase)
{
    // 1in (25.4mm) is larger than 10mm — comparison must happen in base units.
    EvalResult r = Eval("@Min(1in, 10mm)");
    ASSERT_TRUE(r.Ok()) << r.error.msg;
    EXPECT_NEAR(r.value.value, 10.0, kEps);
    EXPECT_EQ(r.value.kind, QuantityKind::Length);
}

TEST(ExpressionParser, MinRejectsMixedKinds)
{
    EvalResult r = Eval("@Min(10mm, 45deg)");
    ASSERT_FALSE(r.Ok());
    EXPECT_EQ(r.error.type, ParserError::Type::UnitMismatch);
}

TEST(ExpressionParser, FloorCeilRoundAbs)
{
    EXPECT_NEAR(Eval("@Floor(10.7)").value.value, 10.0, kEps);
    EXPECT_NEAR(Eval("@Ceil(10.2)").value.value, 11.0, kEps);
    EXPECT_NEAR(Eval("@Round(10.5)").value.value, 11.0, kEps);
    EXPECT_NEAR(Eval("@Abs(-7)").value.value, 7.0, kEps);
}

TEST(ExpressionParser, FloorPreservesKind)
{
    EvalResult r = Eval("@Floor(10.7mm)");
    ASSERT_TRUE(r.Ok());
    EXPECT_NEAR(r.value.value, 10.0, kEps);
    EXPECT_EQ(r.value.kind, QuantityKind::Length);
}

TEST(ExpressionParser, SqrtRequiresPlainNumber)
{
    EvalResult ok = Eval("@Sqrt(16)");
    ASSERT_TRUE(ok.Ok());
    EXPECT_NEAR(ok.value.value, 4.0, kEps);

    EvalResult bad = Eval("@Sqrt(16mm)");
    ASSERT_FALSE(bad.Ok());
    EXPECT_EQ(bad.error.type, ParserError::Type::UnitNotAllowed);
}

TEST(ExpressionParser, TrigTakesAnAngleAndReturnsANumber)
{
    EvalResult r = Eval("@Sin(90deg)");
    ASSERT_TRUE(r.Ok()) << r.error.msg;
    EXPECT_NEAR(r.value.value, 1.0, kEps);
    EXPECT_EQ(r.value.kind, QuantityKind::Number);
}

TEST(ExpressionParser, TrigRejectsALength)
{
    EvalResult r = Eval("@Cos(10mm)");
    ASSERT_FALSE(r.Ok());
    EXPECT_EQ(r.error.type, ParserError::Type::UnitNotAllowed);
}

TEST(ExpressionParser, NestedCalls)
{
    EvalResult r = Eval("@Max(@Min(5, 3), 4)");
    ASSERT_TRUE(r.Ok()) << r.error.msg;
    EXPECT_NEAR(r.value.value, 4.0, kEps);
}

TEST(ExpressionParser, CallWithParameterAndArithmetic)
{
    StubTable t;
    t.Add("w", 90.0, QuantityKind::Length);
    EvalResult r = EvalWith("@Floor($w / 4)", t);
    ASSERT_TRUE(r.Ok()) << r.error.msg;
    EXPECT_NEAR(r.value.value, 22.0, kEps);
    EXPECT_EQ(r.value.kind, QuantityKind::Length);
}

TEST(ExpressionParser, UnknownFunctionIsReported)
{
    EvalResult r = Eval("@Bogus(1)");
    ASSERT_FALSE(r.Ok());
    EXPECT_EQ(r.error.type, ParserError::Type::UnknownFunction);
}

TEST(ExpressionParser, WrongArityIsReported)
{
    EvalResult tooMany = Eval("@Floor(1, 2)");
    ASSERT_FALSE(tooMany.Ok());
    EXPECT_EQ(tooMany.error.type, ParserError::Type::BadArity);

    EvalResult tooFew = Eval("@Floor()");
    ASSERT_FALSE(tooFew.Ok());
    EXPECT_EQ(tooFew.error.type, ParserError::Type::BadArity);
}

TEST(ExpressionParser, FunctionNameWithoutCallIsRejected)
{
    EvalResult r = Eval("@Min + 1");
    ASSERT_FALSE(r.Ok());
    EXPECT_EQ(r.error.type, ParserError::Type::Invalid);
}

// ── error handling ───────────────────────────────────────────────────────────

TEST(ExpressionParser, EmptyExpression)
{
    EvalResult r = Eval("");
    ASSERT_FALSE(r.Ok());
    EXPECT_EQ(r.error.type, ParserError::Type::Empty);
}

TEST(ExpressionParser, WhitespaceOnlyExpression)
{
    EvalResult r = Eval("   ");
    ASSERT_FALSE(r.Ok());
    EXPECT_EQ(r.error.type, ParserError::Type::Empty);
}

TEST(ExpressionParser, UnbalancedOpenParen)
{
    EvalResult r = Eval("(1 + 2");
    ASSERT_FALSE(r.Ok());
    EXPECT_EQ(r.error.type, ParserError::Type::ParenthesisMismatch);
}

TEST(ExpressionParser, UnbalancedCloseParen)
{
    EvalResult r = Eval("1 + 2)");
    ASSERT_FALSE(r.Ok());
    // Either a mismatch or trailing input is a fair diagnosis; both are located.
    EXPECT_FALSE(r.error.Ok());
}

TEST(ExpressionParser, DanglingOperator)
{
    EvalResult r = Eval("1 +");
    ASSERT_FALSE(r.Ok());
    EXPECT_EQ(r.error.type, ParserError::Type::Invalid);
}

TEST(ExpressionParser, InvalidCharacter)
{
    EvalResult r = Eval("1 # 2");
    ASSERT_FALSE(r.Ok());
    EXPECT_EQ(r.error.type, ParserError::Type::Invalid);
    EXPECT_EQ(r.error.pos, 2u);
}

TEST(ExpressionParser, UnknownUnitSuffix)
{
    EvalResult r = Eval("10furlong");
    ASSERT_FALSE(r.Ok());
    EXPECT_EQ(r.error.type, ParserError::Type::Invalid);
}

TEST(ExpressionParser, DivisionByZero)
{
    EvalResult r = Eval("1 / 0");
    ASSERT_FALSE(r.Ok());
    EXPECT_EQ(r.error.type, ParserError::Type::DivideByZero);
}

TEST(ExpressionParser, ErrorsCarryASourceSpan)
{
    Expression e = Expression::Parse("1 # 2");
    ASSERT_FALSE(e.Ok());
    EXPECT_EQ(e.Error().pos, 2u);
    EXPECT_EQ(e.Error().len, 1u);
    EXPECT_FALSE(e.Error().msg.empty());
}

TEST(ExpressionParser, ParseErrorSurvivesEvaluation)
{
    // Evaluating an expression that failed to parse must report the parse error, not
    // crash or silently produce a value.
    StubTable t;
    Expression e = Expression::Parse("((");
    ASSERT_FALSE(e.Ok());
    EvalResult r = e.Evaluate(t.Resolver());
    EXPECT_FALSE(r.Ok());
}

TEST(ExpressionParser, WhitespaceIsInsignificant)
{
    EvalResult tight = Eval("1+2*3");
    EvalResult loose = Eval("  1  +  2  *  3  ");
    ASSERT_TRUE(tight.Ok());
    ASSERT_TRUE(loose.Ok());
    EXPECT_NEAR(tight.value.value, loose.value.value, kEps);
}

// ── tokenizer (shared with the editor's highlighter) ─────────────────────────

TEST(Tokenizer, ClassifiesEachTokenKind)
{
    std::vector<Token> t = Tokenize("$a + @Min(1mm, 2) ^ 3");
    // $a + @Min ( 1mm , 2 ) ^ 3 End
    ASSERT_EQ(t.size(), 11u);
    EXPECT_EQ(t[0].type, Token::Type::ParamRef);
    EXPECT_EQ(t[0].text, "a");
    EXPECT_EQ(t[1].type, Token::Type::Plus);
    EXPECT_EQ(t[2].type, Token::Type::Func);
    EXPECT_EQ(t[2].text, "Min");
    EXPECT_EQ(t[3].type, Token::Type::LParen);
    EXPECT_EQ(t[4].type, Token::Type::Number);
    EXPECT_EQ(t[4].unit, Unit::Millimeter);
    EXPECT_EQ(t[5].type, Token::Type::Comma);
    EXPECT_EQ(t[6].type, Token::Type::Number);
    EXPECT_EQ(t[7].type, Token::Type::RParen);
    EXPECT_EQ(t[8].type, Token::Type::Caret);
    EXPECT_EQ(t[9].type, Token::Type::Number);
    EXPECT_EQ(t.back().type, Token::Type::End);
}

TEST(Tokenizer, AlwaysTerminatesWithEnd)
{
    EXPECT_EQ(Tokenize("").back().type, Token::Type::End);
    EXPECT_EQ(Tokenize("###").back().type, Token::Type::End);
}

TEST(Tokenizer, ReportsInvalidTokenInPlaceSoTheRestStillColours)
{
    // The highlighter needs every token, not a bail-out at the first bad byte.
    std::vector<Token> t = Tokenize("1 # 2");
    ASSERT_EQ(t.size(), 4u); // 1, #, 2, End
    EXPECT_EQ(t[0].type, Token::Type::Number);
    EXPECT_EQ(t[1].type, Token::Type::Invalid);
    EXPECT_EQ(t[2].type, Token::Type::Number);
}

TEST(Tokenizer, NumberTokenCarriesBaseConvertedValue)
{
    std::vector<Token> t = Tokenize("2in");
    ASSERT_GE(t.size(), 1u);
    EXPECT_EQ(t[0].type, Token::Type::Number);
    EXPECT_NEAR(t[0].value, 50.8, kEps);
    EXPECT_EQ(t[0].unit, Unit::Inch);
}
