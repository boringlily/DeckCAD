// Storage wrapper round-trip on REAL disk: the atomic write (temp-then-rename) and file
// read, end to end through SaveScene/LoadScene.
//
// The DTO<->JSON core is covered exhaustively in core_tests; this exercises only the
// filesystem layer, which needs the raylib-linked wrapper, so it lives in ui_tests. It
// uses explicit temp paths (not the exe-dir cache), so no raylib window or init is needed.

#include "AppState.h"
#include "Gui/Storage.h"

#include <gtest/gtest.h>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace {

// A unique temp path per test, cleaned up in the fixture.
std::string TempDcad(std::string_view stem)
{
    fs::path p = fs::temp_directory_path() / ("deckcad_test_" + std::string { stem } + ".dcad");
    return p.string();
}

FeatureId AuthorLineSketch(Scene& scene, f64 len)
{
    Workbench& wb = scene.workbench;
    wb.StartTool(ToolId::CreateSketch);
    wb.ActiveTool().SetPlane(Geometry::SketchPlane::XZ);
    wb.FinishTool();
    wb.StartTool(ToolId::Line);
    wb.ActiveTool().AddPoint({ 0, 0 });
    wb.ActiveTool().AddPoint({ len, 0 });
    wb.FinishTool();
    FeatureId id = wb.Contexts().ActiveSketch()->children.back().Get().id;
    wb.StartTool(ToolId::FinishSketch);
    return id;
}

} // namespace

class StorageTest : public ::testing::Test {
protected:
    AppState app {};
    std::string path;

    Scene& scene()
    {
        Scene* s = app.GetActiveScene();
        EXPECT_NE(s, nullptr);
        return *s;
    }

    void TearDown() override
    {
        if (!path.empty()) {
            std::error_code ec;
            fs::remove(path, ec);
            fs::remove(path + ".tmp", ec);
        }
    }
};

TEST_F(StorageTest, SaveThenLoadReproducesGeometry)
{
    path = TempDcad("roundtrip");
    scene().workbench.Params().Create("w", "80mm");
    FeatureId line = AuthorLineSketch(scene(), 10.0);

    ASSERT_TRUE(AppStorage::SaveScene(scene(), path));
    ASSERT_TRUE(fs::exists(path));
    // The temp sidecar must be gone once the rename completes.
    EXPECT_FALSE(fs::exists(path + ".tmp"));

    // Load into a second, fresh scene.
    app.CreateNewScene();
    Serialize::SerError err;
    ASSERT_TRUE(AppStorage::LoadScene(scene(), path, err)) << err.message;

    ASSERT_EQ(scene().workbench.Evaluated().sketches.size(), 1u);
    const SketchEntity* e = scene().workbench.Evaluated().sketches[0].Find(line);
    ASSERT_NE(e, nullptr);
    EXPECT_NEAR(e->b.x, 10.0, 1e-9);

    // The parameter came back too.
    EXPECT_NE(scene().workbench.Params().FindByName("w"), Param::kNullUpid);
}

TEST_F(StorageTest, SaveIsAtomicLeavingNoTempBehind)
{
    path = TempDcad("atomic");
    AuthorLineSketch(scene(), 5.0);
    ASSERT_TRUE(AppStorage::SaveScene(scene(), path));
    EXPECT_TRUE(fs::exists(path));
    EXPECT_FALSE(fs::exists(path + ".tmp"));
}

TEST_F(StorageTest, SaveOverwritesAnExistingFile)
{
    path = TempDcad("overwrite");
    AuthorLineSketch(scene(), 3.0);
    ASSERT_TRUE(AppStorage::SaveScene(scene(), path));
    auto firstSize = fs::file_size(path);

    // Add more and re-save over the same path.
    Workbench& wb = scene().workbench;
    wb.StartTool(ToolId::CreateSketch);
    wb.ActiveTool().SetPlane(Geometry::SketchPlane::XY);
    wb.FinishTool();
    wb.StartTool(ToolId::Circle);
    wb.ActiveTool().AddPoint({ 0, 0 });
    wb.ActiveTool().AddPoint({ 2, 0 });
    wb.FinishTool();
    wb.StartTool(ToolId::FinishSketch);

    ASSERT_TRUE(AppStorage::SaveScene(scene(), path));
    EXPECT_GT(fs::file_size(path), firstSize); // grew, and there's still exactly one file
    EXPECT_FALSE(fs::exists(path + ".tmp"));
}

TEST_F(StorageTest, LoadingAMissingFileFailsCleanly)
{
    Serialize::SerError err;
    EXPECT_FALSE(AppStorage::LoadScene(scene(), TempDcad("does_not_exist"), err));
    EXPECT_FALSE(err.ok);
    EXPECT_FALSE(err.message.empty());
}

TEST_F(StorageTest, LoadingGarbageFailsCleanly)
{
    path = TempDcad("garbage");
    {
        std::ofstream out(path, std::ios::binary);
        out << "this is definitely not a dcad file";
    }
    Serialize::SerError err;
    EXPECT_FALSE(AppStorage::LoadScene(scene(), path, err));
    EXPECT_FALSE(err.ok);
}
