// Serialization round-trip: a .dcad file must reconstruct the exact command history and
// parameter table, and — the real invariant, since geometry is derived — replay to
// byte-identical geometry.
//
// glaze is header-only and raylib-free, so the whole save -> load cycle runs here in
// core_tests with no window.

#include "Convert.h"
#include "DcadFile.h"
#include "ConstraintSolver.h" // HybridSketchSolver: match the Workbench solver
#include "Document.h"
#include "ParameterEngine.h"
#include "StoragePath.h"
#include "Tool.h"
#include "Workbench.h"

#include <gtest/gtest.h>
#include <cmath>
#include <string>

using namespace Serialize;

namespace {

constexpr f64 kEps = 1e-9;

// Author a scene into `wb`: a sketch with parameter-driven dimensions and a mirror
// group, plus a couple of user parameters — enough to exercise nesting, dimensions,
// constraints, and $refs in one round-trip.
void AuthorRichScene(Workbench& wb)
{
    wb.Params().Create("w", "100mm");
    wb.Params().Create("h", "$w / 2");

    wb.StartTool(ToolId::CreateSketch);
    wb.ActiveTool().SetPlane(Geometry::SketchPlane::XZ);
    wb.FinishTool();

    // An axis line, then a dimensioned line, then a mirror group off the axis.
    wb.StartTool(ToolId::Line);
    wb.ActiveTool().AddPoint({ 0, 0 });
    wb.ActiveTool().AddPoint({ 0, 10 });
    wb.FinishTool();
    FeatureId axis = wb.Contexts().ActiveSketch()->children.back().Get().id;

    wb.StartTool(ToolId::Line);
    wb.ActiveTool().AddPoint({ 5, 1 });
    wb.ActiveTool().AddPoint({ 15, 1 });
    wb.FinishTool();
    FeatureId dimd = wb.Contexts().ActiveSketch()->children.back().Get().id;

    wb.StartTool(ToolId::Dimension);
    wb.ActiveTool().AddPick(dimd);
    wb.ActiveTool().SetValue("$w * 2");
    wb.FinishTool();

    // Mirror a line across the axis — produces a nested CompoundSketchCmd.
    wb.StartTool(ToolId::SymmetryGroup);
    wb.ActiveTool().AddPick(axis);
    wb.FinishTool();
    wb.StartTool(ToolId::Line);
    wb.ActiveTool().AddPoint({ 3, 2 });
    wb.ActiveTool().AddPoint({ 7, 4 });
    wb.FinishTool();
    wb.StartTool(ToolId::StopSymmetry);

    wb.StartTool(ToolId::FinishSketch);

    // A second sketch on another plane.
    wb.StartTool(ToolId::CreateSketch);
    wb.ActiveTool().SetPlane(Geometry::SketchPlane::YZ);
    wb.FinishTool();
    wb.StartTool(ToolId::Circle);
    wb.ActiveTool().AddPoint({ 0, 0 });
    wb.ActiveTool().AddPoint({ 4, 0 });
    wb.FinishTool();
    wb.StartTool(ToolId::FinishSketch);
}

// Deep-compare two evaluated models. This is the invariant that actually matters: a
// faithful round-trip is one that replays to the same geometry.
void ExpectSameGeometry(const PartDocument& a, const PartDocument& b)
{
    ASSERT_EQ(a.sketches.size(), b.sketches.size());
    for (u32 s = 0; s < a.sketches.size(); ++s) {
        const SketchDocument& sa = a.sketches[s];
        const SketchDocument& sb = b.sketches[s];
        EXPECT_EQ(sa.id, sb.id);
        EXPECT_EQ(sa.plane, sb.plane);
        ASSERT_EQ(sa.entities.size(), sb.entities.size()) << "sketch " << s;
        for (u32 e = 0; e < sa.entities.size(); ++e) {
            const SketchEntity& ea = sa.entities[e];
            const SketchEntity& eb = sb.entities[e];
            EXPECT_EQ(ea.id, eb.id);
            EXPECT_EQ(static_cast<int>(ea.kind), static_cast<int>(eb.kind));
            EXPECT_NEAR(ea.a.x, eb.a.x, kEps);
            EXPECT_NEAR(ea.a.y, eb.a.y, kEps);
            EXPECT_NEAR(ea.b.x, eb.b.x, kEps);
            EXPECT_NEAR(ea.b.y, eb.b.y, kEps);
            EXPECT_NEAR(ea.radius, eb.radius, kEps);
        }
        ASSERT_EQ(sa.dimensions.size(), sb.dimensions.size());
        ASSERT_EQ(sa.constraints.size(), sb.constraints.size());
    }
}

// Full pipeline: source Workbench -> DTO -> JSON -> DTO -> fresh Document/params.
struct Loaded {
    Param::ParameterEngine params;
    HybridSketchSolver solver; // same solver the source Workbench uses
    Document doc { &params, &solver };
    SceneMeta meta;
    std::string json;
    bool readOk { false };
};

void RoundTrip(const Workbench& src, const SceneMeta& srcMeta, Loaded& out)
{
    DocumentDto dto = ToDto(src.Doc(), src.Params(), srcMeta);
    out.json = WriteDcad(dto);

    DocumentDto parsed;
    SerError err;
    out.readOk = ReadDcad(out.json, parsed, err);
    ASSERT_TRUE(out.readOk) << err.message;

    ASSERT_TRUE(FromDto(parsed, out.doc, out.params, out.meta));
}

} // namespace

// ── round-trip ─────────────────────────────────────────────────────────────────

TEST(Serialization, EmptySceneRoundTrips)
{
    Workbench src;
    SceneMeta meta { Param::Unit::Millimeter, "Empty" };
    Loaded ld;
    RoundTrip(src, meta, ld);

    EXPECT_EQ(ld.doc.Size(), 0u);
    EXPECT_TRUE(ld.doc.Evaluated().sketches.empty());
    EXPECT_EQ(ld.meta.name, "Empty");
}

TEST(Serialization, RichSceneReplaysToIdenticalGeometry)
{
    Workbench src;
    AuthorRichScene(src);
    SceneMeta meta { Param::Unit::Inch, "Deck" };

    Loaded ld;
    RoundTrip(src, meta, ld);

    // The invariant that matters: same evaluated geometry after save/load.
    ExpectSameGeometry(src.Evaluated(), ld.doc.Evaluated());
}

TEST(Serialization, HistorySizeAndCursorRoundTrip)
{
    Workbench src;
    AuthorRichScene(src);
    ASSERT_TRUE(src.Undo()); // leave a redo tail so the cursor differs from size

    Loaded ld;
    RoundTrip(src, { Param::Unit::Millimeter, "x" }, ld);

    EXPECT_EQ(ld.doc.Size(), src.Doc().Size());
    EXPECT_EQ(ld.doc.Cursor(), src.Doc().Cursor());
    EXPECT_TRUE(ld.doc.CanRedo());
}

TEST(Serialization, ParametersRoundTripWithNamesExpressionsAndUnits)
{
    Workbench src;
    Param::UPID w = src.Params().Create("width", "12in");
    src.Params().SetDisplayUnit(w, Param::Unit::Inch);
    src.Params().Create("half", "$width / 2");

    Loaded ld;
    RoundTrip(src, { Param::Unit::Millimeter, "p" }, ld);

    Param::UPID lw = ld.params.FindByName("width");
    ASSERT_NE(lw, Param::kNullUpid);
    EXPECT_EQ(ld.params.Get(lw)->Text(), "12in");
    EXPECT_EQ(ld.params.Get(lw)->DisplayUnit(), Param::Unit::Inch);

    Param::UPID lh = ld.params.FindByName("half");
    ASSERT_NE(lh, Param::kNullUpid);
    // The dependent still resolves against the restored table.
    EXPECT_NEAR(ld.params.Value(lh).value.value, 12.0 * 25.4 / 2.0, 1e-6);
}

TEST(Serialization, UpidsArePreservedExactly)
{
    // Geometry references dimension values by UPID, so the exact ids must survive — a
    // renumbering would silently re-point a dimension at the wrong parameter.
    Workbench src;
    src.Params().Create("a", "1mm");
    Param::UPID b = src.Params().Create("b", "2mm");
    src.Params().Remove(src.Params().FindByName("a")); // leave a gap in the id space

    Loaded ld;
    RoundTrip(src, {}, ld);

    ASSERT_NE(ld.params.Get(b), nullptr);
    EXPECT_EQ(ld.params.Get(b)->Id(), b);
    EXPECT_EQ(ld.params.FindByName("a"), Param::kNullUpid);
}

TEST(Serialization, FeatureIdsArePreservedAndDimensionsStillResolve)
{
    Workbench src;
    AuthorRichScene(src);

    Loaded ld;
    RoundTrip(src, {}, ld);

    // Every dimension's target still exists in the loaded, evaluated sketch.
    for (const SketchDocument& doc : ld.doc.Evaluated().sketches) {
        for (const SketchDimensionRecord& d : doc.dimensions) {
            EXPECT_NE(doc.Find(d.targetA), nullptr) << "dangling dimension target after load";
        }
    }
}

TEST(Serialization, NestedGroupsSurviveTheVariantRoundTrip)
{
    Workbench src;
    AuthorRichScene(src); // contains a mirror -> CompoundSketchCmd

    Loaded ld;
    RoundTrip(src, {}, ld);

    // Find the sketch feature and confirm a compound child came back as a compound.
    bool foundGroup = false;
    for (const Command& c : ld.doc.History()) {
        if (const auto* sf = c.As<SketchFeatureCommand>()) {
            for (const SketchCmd& child : sf->children) {
                if (child.As<CompoundSketchCmd>()) {
                    foundGroup = true;
                }
            }
        }
    }
    EXPECT_TRUE(foundGroup) << "mirror group did not survive as a compound command";
}

// ── id counters ──────────────────────────────────────────────────────────────

TEST(Serialization, NextIdCountersRoundTripSoNewIdsDoNotCollide)
{
    Workbench src;
    AuthorRichScene(src);

    Loaded ld;
    RoundTrip(src, {}, ld);

    // A FeatureId minted after load is beyond every loaded id.
    FeatureId fresh = ld.doc.NextId();
    for (const SketchDocument& doc : ld.doc.Evaluated().sketches) {
        for (const SketchEntity& e : doc.entities) {
            EXPECT_NE(e.id, fresh);
            EXPECT_LT(e.id, fresh);
        }
    }

    // A UPID minted after load likewise can't reuse a loaded one.
    Param::UPID up = ld.params.Create("brand_new", "1mm");
    ASSERT_NE(up, Param::kNullUpid);
    for (const Param::ParametricExpression& p : ld.params.Parameters()) {
        if (&p != ld.params.Get(up)) {
            EXPECT_NE(p.Id(), up);
        }
    }
}

TEST(Serialization, ContinuingToEditAfterLoadWorks)
{
    Workbench src;
    AuthorRichScene(src);

    Loaded ld;
    RoundTrip(src, {}, ld);
    u32 before = ld.doc.Size();

    // Commit a new sketch straight into the loaded document.
    SketchFeatureCommand sf;
    sf.id = ld.doc.NextId();
    sf.plane = Geometry::SketchPlane::XY;
    SketchLine l;
    l.id = ld.doc.NextId();
    l.a = { 0, 0 };
    l.b = { 1, 1 };
    sf.children.push_back(SketchCmd { l });
    ld.doc.PushCommand(Command { std::move(sf) });

    EXPECT_EQ(ld.doc.Size(), before + 1);
    EXPECT_FALSE(ld.doc.Evaluated().sketches.empty());
}

// ── robustness ─────────────────────────────────────────────────────────────────

TEST(Serialization, MalformedJsonReturnsErrorNeverThrows)
{
    DocumentDto out;
    SerError err;

    EXPECT_FALSE(ReadDcad("{ this is not json", out, err));
    EXPECT_FALSE(err.ok);
    EXPECT_FALSE(err.message.empty());

    EXPECT_FALSE(ReadDcad("", out, err));
    EXPECT_FALSE(ReadDcad("[1, 2, 3]", out, err)); // valid json, wrong shape
}

TEST(Serialization, UnknownVersionIsRejectedNotGuessed)
{
    Workbench src;
    AuthorRichScene(src);
    DocumentDto dto = ToDto(src.Doc(), src.Params(), {});
    dto.version = kDcadVersion + 99;
    std::string json = WriteDcad(dto);

    DocumentDto parsed;
    SerError err;
    ASSERT_TRUE(ReadDcad(json, parsed, err)); // parses fine...

    Param::ParameterEngine params;
    HybridSketchSolver solver; // same solver the source Workbench uses
    Document doc { &params, &solver };
    SceneMeta meta;
    EXPECT_FALSE(FromDto(parsed, doc, params, meta)); // ...but load refuses the version
}

TEST(Serialization, VersionZeroIsRejected)
{
    Workbench src;
    DocumentDto dto = ToDto(src.Doc(), src.Params(), {});
    dto.version = 0;
    DocumentDto parsed;
    SerError err;
    ASSERT_TRUE(ReadDcad(WriteDcad(dto), parsed, err));

    Param::ParameterEngine params;
    HybridSketchSolver solver; // same solver the source Workbench uses
    Document doc { &params, &solver };
    SceneMeta meta;
    EXPECT_FALSE(FromDto(parsed, doc, params, meta));
}

// ── determinism ────────────────────────────────────────────────────────────────

TEST(Serialization, WriteIsByteStable)
{
    // A diff-friendly format: serializing the same document twice is byte-identical.
    Workbench src;
    AuthorRichScene(src);
    DocumentDto dto = ToDto(src.Doc(), src.Params(), { Param::Unit::Millimeter, "d" });

    EXPECT_EQ(WriteDcad(dto), WriteDcad(dto));
}

TEST(Serialization, DoubleRoundTripIsStable)
{
    // load(save(load(save(x)))) == load(save(x)) — no drift across cycles.
    Workbench src;
    AuthorRichScene(src);

    DocumentDto d1 = ToDto(src.Doc(), src.Params(), { Param::Unit::Millimeter, "d" });
    std::string j1 = WriteDcad(d1);

    DocumentDto parsed;
    SerError err;
    ASSERT_TRUE(ReadDcad(j1, parsed, err));

    Param::ParameterEngine params;
    HybridSketchSolver solver; // same solver the source Workbench uses
    Document doc { &params, &solver };
    SceneMeta meta;
    ASSERT_TRUE(FromDto(parsed, doc, params, meta));

    std::string j2 = WriteDcad(ToDto(doc, params, meta));
    EXPECT_EQ(j1, j2);
}

TEST(Serialization, JsonIsHumanReadableWithTagsAndUnitNames)
{
    Workbench src;
    AuthorRichScene(src);
    std::string json = WriteDcad(ToDto(src.Doc(), src.Params(), { Param::Unit::Inch, "Deck" }));

    // Tagged commands (tag is "type", distinct from the DimensionDto/ConstraintDto
    // member field "kind") and string-mapped enums, not bare integers.
    EXPECT_NE(json.find("\"type\": \"sketch\""), std::string::npos);
    EXPECT_NE(json.find("\"type\": \"line\""), std::string::npos);
    EXPECT_NE(json.find("\"type\": \"dimension\""), std::string::npos);
    EXPECT_NE(json.find("\"kind\": \"Length\""), std::string::npos); // the dimension's own kind
    EXPECT_NE(json.find("\"plane\": \"XZ\""), std::string::npos);
}

// ── path construction (pure, raylib-free) ────────────────────────────────────────

TEST(StoragePath, ExtensionsAndCacheLayout)
{
    EXPECT_EQ(DcadFileName("Deck"), "Deck.dcad");
    EXPECT_EQ(CacheFileName("Deck"), "Deck.cache.dcad");
    EXPECT_EQ(CacheDir("C:/app"), "C:/app/cache");
    EXPECT_EQ(CachePath("C:/app", "Deck"), "C:/app/cache/Deck.cache.dcad");
}

TEST(StoragePath, SanitizesUnsafeSceneNames)
{
    EXPECT_EQ(SanitizeStem("Untitled 0"), "Untitled 0"); // spaces are fine
    EXPECT_EQ(SanitizeStem("Deck #1"), "Deck _1"); // '#' -> '_'
    EXPECT_EQ(SanitizeStem("a/b\\c:d"), "a_b_c_d"); // path separators neutralized
    EXPECT_EQ(SanitizeStem("   "), "untitled"); // empty after trim -> fallback
    EXPECT_EQ(SanitizeStem(""), "untitled");
}

TEST(StoragePath, JoinAvoidsDoubledOrMissingSeparators)
{
    EXPECT_EQ(JoinPath("a", "b"), "a/b");
    EXPECT_EQ(JoinPath("a/", "b"), "a/b");
    EXPECT_EQ(JoinPath("a\\", "b"), "a\\b"); // existing backslash kept, not doubled
    EXPECT_EQ(JoinPath("", "b"), "b");
}

TEST(StoragePath, CachePathIsStableForAGivenName)
{
    // Same scene name -> same cache file, every time (so auto-save overwrites, not piles).
    EXPECT_EQ(CachePath("/x", "My Deck"), CachePath("/x", "My Deck"));
}
