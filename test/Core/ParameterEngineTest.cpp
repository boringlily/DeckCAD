// ParameterEngine: storage, UPID stability, dependency tracking, cycle detection,
// and dependent re-evaluation.
//
// The invariant that matters most here is that consumers only ever hold a UPID.
// Geometry stores UPIDs, so if a UPID could ever be reused or silently renumbered,
// a dimension would quietly start tracking the wrong parameter.

#include "ParameterEngine.h"

#include <gtest/gtest.h>

using namespace Param;

namespace {
constexpr f64 kEps = 1e-9;
}

// ── basics ───────────────────────────────────────────────────────────────────

TEST(ParameterEngine, CreateAndEvaluate)
{
    ParameterEngine e;
    UPID w = e.Create("width", "100mm");
    ASSERT_NE(w, kNullUpid);

    EvalResult r = e.Value(w);
    ASSERT_TRUE(r.Ok()) << r.error.msg;
    EXPECT_NEAR(r.value.value, 100.0, kEps);
    EXPECT_EQ(r.value.kind, QuantityKind::Length);
}

TEST(ParameterEngine, GetExposesNameAndText)
{
    ParameterEngine e;
    UPID w = e.Create("width", "100mm");
    const ParametricExpression* p = e.Get(w);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->Name(), "width");
    EXPECT_EQ(p->Text(), "100mm");
    EXPECT_EQ(p->Id(), w);
    EXPECT_FALSE(p->IsDimension());
}

TEST(ParameterEngine, GetOnMissingIdIsNullNotUB)
{
    ParameterEngine e;
    EXPECT_EQ(e.Get(12345), nullptr);
    EXPECT_FALSE(e.Value(12345).Ok());
}

TEST(ParameterEngine, DuplicateNameIsRejected)
{
    ParameterEngine e;
    ASSERT_NE(e.Create("w", "1"), kNullUpid);
    EXPECT_EQ(e.Create("w", "2"), kNullUpid);
}

TEST(ParameterEngine, AnonymousParametersMayShareEmptyNames)
{
    // Empty names aren't referenceable, so they don't collide.
    ParameterEngine e;
    EXPECT_NE(e.Create("", "1"), kNullUpid);
    EXPECT_NE(e.Create("", "2"), kNullUpid);
    EXPECT_EQ(e.FindByName(""), kNullUpid);
}

TEST(ParameterEngine, FindByName)
{
    ParameterEngine e;
    UPID w = e.Create("width", "1");
    EXPECT_EQ(e.FindByName("width"), w);
    EXPECT_EQ(e.FindByName("nope"), kNullUpid);
}

TEST(ParameterEngine, ParseErrorIsVisibleButTheRowStillExists)
{
    // A typo must not destroy the parameter — the user has to be able to fix it.
    ParameterEngine e;
    UPID bad = e.Create("x", "1 +");
    ASSERT_NE(bad, kNullUpid);
    ASSERT_NE(e.Get(bad), nullptr);
    EXPECT_FALSE(e.Value(bad).Ok());

    ASSERT_TRUE(e.SetExpression(bad, "1 + 2"));
    EXPECT_TRUE(e.Value(bad).Ok());
}

// ── UPID stability ───────────────────────────────────────────────────────────

TEST(ParameterEngine, UpidsAreStableAcrossEdits)
{
    ParameterEngine e;
    UPID a = e.Create("a", "1");
    UPID b = e.Create("b", "2");

    ASSERT_TRUE(e.SetExpression(a, "10"));
    ASSERT_TRUE(e.Rename(b, "bee"));

    EXPECT_EQ(e.Get(a)->Id(), a);
    EXPECT_EQ(e.Get(b)->Id(), b);
    EXPECT_NEAR(e.Value(a).value.value, 10.0, kEps);
    EXPECT_EQ(e.Get(b)->Name(), "bee");
}

TEST(ParameterEngine, RemovingAParameterDoesNotRenumberSurvivors)
{
    // The whole point of a monotonic UPID rather than a storage index.
    ParameterEngine e;
    UPID a = e.Create("a", "1");
    UPID b = e.Create("b", "2");
    UPID c = e.Create("c", "3");

    ASSERT_TRUE(e.Remove(a));

    ASSERT_NE(e.Get(b), nullptr);
    ASSERT_NE(e.Get(c), nullptr);
    EXPECT_EQ(e.Get(b)->Name(), "b");
    EXPECT_EQ(e.Get(c)->Name(), "c");
    EXPECT_NEAR(e.Value(c).value.value, 3.0, kEps);
    EXPECT_EQ(e.Get(a), nullptr);
}

TEST(ParameterEngine, NewParametersNeverReuseARemovedUpid)
{
    ParameterEngine e;
    UPID a = e.Create("a", "1");
    ASSERT_TRUE(e.Remove(a));
    UPID fresh = e.Create("fresh", "2");
    EXPECT_NE(fresh, a);
    EXPECT_EQ(e.Get(a), nullptr);
}

TEST(ParameterEngine, RemoveOnMissingIdFails)
{
    ParameterEngine e;
    EXPECT_FALSE(e.Remove(999));
}

// ── renaming ─────────────────────────────────────────────────────────────────

TEST(ParameterEngine, RenameToATakenNameIsRejected)
{
    ParameterEngine e;
    e.Create("a", "1");
    UPID b = e.Create("b", "2");
    EXPECT_FALSE(e.Rename(b, "a"));
    EXPECT_EQ(e.Get(b)->Name(), "b");
}

TEST(ParameterEngine, RenameToOwnNameIsAllowed)
{
    ParameterEngine e;
    UPID a = e.Create("a", "1");
    EXPECT_TRUE(e.Rename(a, "a"));
}

TEST(ParameterEngine, RenamingBreaksDependentsThatStillUseTheOldName)
{
    // Renaming does not rewrite referencing expressions; the dependent must report an
    // unknown parameter rather than silently keeping a stale value.
    ParameterEngine e;
    UPID w = e.Create("w", "10");
    UPID d = e.Create("d", "$w * 2");
    ASSERT_TRUE(e.Value(d).Ok());

    ASSERT_TRUE(e.Rename(w, "width"));

    EvalResult r = e.Value(d);
    EXPECT_FALSE(r.Ok());
    EXPECT_EQ(r.error.type, ParserError::Type::UnknownParameter);
}

// ── dependencies ─────────────────────────────────────────────────────────────

TEST(ParameterEngine, DependentValueUpdatesWhenItsSourceChanges)
{
    ParameterEngine e;
    UPID w = e.Create("w", "100mm");
    UPID d = e.Create("d", "$w * 2");
    EXPECT_NEAR(e.Value(d).value.value, 200.0, kEps);

    ASSERT_TRUE(e.SetExpression(w, "50mm"));
    EXPECT_NEAR(e.Value(d).value.value, 100.0, kEps);
}

TEST(ParameterEngine, TransitiveDependenciesReEvaluateInOrder)
{
    ParameterEngine e;
    UPID a = e.Create("a", "2");
    UPID b = e.Create("b", "$a * 3");
    UPID c = e.Create("c", "$b + 1");
    EXPECT_NEAR(e.Value(c).value.value, 7.0, kEps);

    ASSERT_TRUE(e.SetExpression(a, "10"));
    EXPECT_NEAR(e.Value(b).value.value, 30.0, kEps);
    EXPECT_NEAR(e.Value(c).value.value, 31.0, kEps);
}

TEST(ParameterEngine, DependenciesListsDirectRefsOnly)
{
    ParameterEngine e;
    UPID a = e.Create("a", "1");
    UPID b = e.Create("b", "$a + 1");
    UPID c = e.Create("c", "$b + $a");

    std::vector<UPID> depsB = e.Dependencies(b);
    ASSERT_EQ(depsB.size(), 1u);
    EXPECT_EQ(depsB[0], a);

    std::vector<UPID> depsC = e.Dependencies(c);
    EXPECT_EQ(depsC.size(), 2u);
}

TEST(ParameterEngine, DependenciesSkipsUnresolvableNames)
{
    ParameterEngine e;
    UPID x = e.Create("x", "$ghost + 1");
    EXPECT_TRUE(e.Dependencies(x).empty());
    EXPECT_FALSE(e.Value(x).Ok());
}

TEST(ParameterEngine, DependentsFindsTransitiveConsumers)
{
    ParameterEngine e;
    UPID a = e.Create("a", "1");
    UPID b = e.Create("b", "$a + 1");
    UPID c = e.Create("c", "$b + 1");
    e.Create("unrelated", "99");

    std::vector<UPID> dep = e.Dependents(a);
    ASSERT_EQ(dep.size(), 2u);
    bool hasB = false;
    bool hasC = false;
    for (UPID id : dep) {
        hasB = hasB || id == b;
        hasC = hasC || id == c;
    }
    EXPECT_TRUE(hasB);
    EXPECT_TRUE(hasC);
}

// ── cycles ───────────────────────────────────────────────────────────────────

TEST(ParameterEngine, DirectCycleIsDetected)
{
    ParameterEngine e;
    UPID a = e.Create("a", "$b + 1");
    UPID b = e.Create("b", "$a");

    EvalResult ra = e.Value(a);
    EXPECT_FALSE(ra.Ok());
    EXPECT_EQ(ra.error.type, ParserError::Type::CyclicReference);

    EvalResult rb = e.Value(b);
    EXPECT_FALSE(rb.Ok());
    EXPECT_EQ(rb.error.type, ParserError::Type::CyclicReference);
}

TEST(ParameterEngine, SelfReferenceIsDetected)
{
    ParameterEngine e;
    UPID a = e.Create("a", "$a + 1");
    EvalResult r = e.Value(a);
    EXPECT_FALSE(r.Ok());
    EXPECT_EQ(r.error.type, ParserError::Type::CyclicReference);
}

TEST(ParameterEngine, LongerCycleIsDetected)
{
    ParameterEngine e;
    UPID a = e.Create("a", "$b");
    e.Create("b", "$c");
    e.Create("c", "$a");
    EXPECT_EQ(e.Value(a).error.type, ParserError::Type::CyclicReference);
}

TEST(ParameterEngine, BreakingACycleRestoresEvaluation)
{
    // A cycle verdict must not be cached — fixing the expression has to recover.
    ParameterEngine e;
    UPID a = e.Create("a", "$b + 1");
    UPID b = e.Create("b", "$a");
    ASSERT_FALSE(e.Value(a).Ok());

    ASSERT_TRUE(e.SetExpression(b, "10"));

    EvalResult r = e.Value(a);
    ASSERT_TRUE(r.Ok()) << r.error.msg;
    EXPECT_NEAR(r.value.value, 11.0, kEps);
}

TEST(ParameterEngine, CycleDoesNotPoisonUnrelatedParameters)
{
    ParameterEngine e;
    e.Create("a", "$b");
    e.Create("b", "$a");
    UPID ok = e.Create("ok", "5 * 2");
    EXPECT_NEAR(e.Value(ok).value.value, 10.0, kEps);
}

TEST(ParameterEngine, DependentsTerminatesOnACyclicGraph)
{
    // Guards the seen-set in DependsOn: without it this hangs forever.
    ParameterEngine e;
    UPID a = e.Create("a", "$b");
    e.Create("b", "$a");
    (void)e.Dependents(a);
    SUCCEED();
}

// ── dimensions ───────────────────────────────────────────────────────────────

TEST(ParameterEngine, DimensionCarriesItsOwningSketch)
{
    ParameterEngine e;
    constexpr u32 kSketch = 7;
    UPID d = e.CreateDimension("100mm", kSketch);
    ASSERT_NE(d, kNullUpid);

    const ParametricExpression* p = e.Get(d);
    ASSERT_NE(p, nullptr);
    EXPECT_TRUE(p->IsDimension());
    EXPECT_EQ(p->Owner(), kSketch);
    EXPECT_NEAR(e.Value(d).value.value, 100.0, kEps);
}

TEST(ParameterEngine, DimensionsGetAutoNamesSoTheyCanBeReferenced)
{
    ParameterEngine e;
    UPID d = e.CreateDimension("10mm", 1);
    const ParametricExpression* p = e.Get(d);
    ASSERT_NE(p, nullptr);
    EXPECT_FALSE(p->Name().empty());
    EXPECT_EQ(e.FindByName(p->Name()), d);
}

TEST(ParameterEngine, AutoNamedDimensionsAreUnique)
{
    ParameterEngine e;
    UPID a = e.CreateDimension("1mm", 1);
    UPID b = e.CreateDimension("2mm", 1);
    UPID c = e.CreateDimension("3mm", 2);
    ASSERT_NE(a, kNullUpid);
    ASSERT_NE(b, kNullUpid);
    ASSERT_NE(c, kNullUpid);
    EXPECT_NE(e.Get(a)->Name(), e.Get(b)->Name());
    EXPECT_NE(e.Get(b)->Name(), e.Get(c)->Name());
}

TEST(ParameterEngine, AutoNamedDimensionAvoidsAUserTakenName)
{
    ParameterEngine e;
    e.Create("D1", "999");
    UPID d = e.CreateDimension("1mm", 1);
    ASSERT_NE(d, kNullUpid);
    EXPECT_NE(e.Get(d)->Name(), "D1");
}

TEST(ParameterEngine, DimensionCanReferenceAUserParameter)
{
    // The headline workflow: dimension a line to $w * 2, then edit w.
    ParameterEngine e;
    UPID w = e.Create("w", "100mm");
    UPID d = e.CreateDimension("$w * 2", 1);
    EXPECT_NEAR(e.Value(d).value.value, 200.0, kEps);

    ASSERT_TRUE(e.SetExpression(w, "150mm"));
    EXPECT_NEAR(e.Value(d).value.value, 300.0, kEps);
}

TEST(ParameterEngine, RemoveOwnedByDropsOnlyThatSketchesDimensions)
{
    ParameterEngine e;
    UPID keep = e.Create("w", "1mm");
    UPID d1 = e.CreateDimension("1mm", 1);
    UPID d2 = e.CreateDimension("2mm", 1);
    UPID other = e.CreateDimension("3mm", 2);

    EXPECT_EQ(e.RemoveOwnedBy(1), 2u);

    EXPECT_EQ(e.Get(d1), nullptr);
    EXPECT_EQ(e.Get(d2), nullptr);
    EXPECT_NE(e.Get(other), nullptr);
    EXPECT_NE(e.Get(keep), nullptr);
}

TEST(ParameterEngine, RemoveOwnedByLeavesUserParametersAlone)
{
    ParameterEngine e;
    e.Create("w", "1mm");
    EXPECT_EQ(e.RemoveOwnedBy(kNoOwner), 0u);
    EXPECT_NE(e.FindByName("w"), kNullUpid);
}

// ── ad-hoc evaluation + display units ────────────────────────────────────────

TEST(ParameterEngine, EvaluateTextUsesTheLiveTableWithoutStoring)
{
    ParameterEngine e;
    e.Create("w", "100mm");

    EvalResult r = e.EvaluateText("$w / 2");
    ASSERT_TRUE(r.Ok()) << r.error.msg;
    EXPECT_NEAR(r.value.value, 50.0, kEps);

    // Nothing was added to the table.
    EXPECT_EQ(e.Parameters().size(), 1u);
}

TEST(ParameterEngine, EvaluateTextReportsErrorsForLiveValidation)
{
    ParameterEngine e;
    EXPECT_FALSE(e.EvaluateText("$nope").Ok());
    EXPECT_FALSE(e.EvaluateText("1 +").Ok());
    EXPECT_EQ(e.Parameters().size(), 0u);
}

TEST(ParameterEngine, DisplayUnitIsPresentationalOnly)
{
    ParameterEngine e;
    UPID w = e.Create("w", "1000mm");
    ASSERT_TRUE(e.SetDisplayUnit(w, Unit::Meter));

    EXPECT_EQ(e.Get(w)->DisplayUnit(), Unit::Meter);
    // The stored value stays in base units.
    EXPECT_NEAR(e.Value(w).value.value, 1000.0, kEps);
    EXPECT_NEAR(Display(e.Value(w).value, e.Get(w)->DisplayUnit()), 1.0, kEps);
}

TEST(ParameterEngine, GenerationAdvancesOnMutation)
{
    ParameterEngine e;
    u32 g0 = e.Generation();
    UPID a = e.Create("a", "1");
    EXPECT_NE(e.Generation(), g0);

    u32 g1 = e.Generation();
    e.SetExpression(a, "2");
    EXPECT_NE(e.Generation(), g1);
}

TEST(ParameterEngine, RepeatedReadsAreConsistent)
{
    ParameterEngine e;
    UPID a = e.Create("a", "2");
    UPID b = e.Create("b", "$a * 3");
    EXPECT_NEAR(e.Value(b).value.value, 6.0, kEps);
    EXPECT_NEAR(e.Value(b).value.value, 6.0, kEps); // cached path
    EXPECT_NEAR(e.Value(b).value.value, 6.0, kEps);
    (void)a;
}
