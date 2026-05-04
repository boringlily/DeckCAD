#include "AppState.h"
#include <gtest/gtest.h>

using Core::AppState;

TEST(AppState, StartsOnTheHomePageWithNoScenes)
{
    AppState app;
    EXPECT_EQ(app.active_tab, 0u);
    EXPECT_TRUE(app.scenes.empty());
    EXPECT_FALSE(app.hasOpenScene());
}

TEST(AppState, NewSceneFocusesTheSceneItCreated)
{
    AppState app;

    EXPECT_EQ(app.newScene(), 1u);
    EXPECT_TRUE(app.hasOpenScene());
    EXPECT_EQ(app.getCurrentScene().name, "Untitled 1");

    EXPECT_EQ(app.newScene(), 2u);
    EXPECT_EQ(app.getCurrentScene().name, "Untitled 2");
}

TEST(AppState, ClosingTheOnlySceneReturnsToTheHomePage)
{
    AppState app;
    app.newScene();

    app.closeScene(0);

    EXPECT_TRUE(app.scenes.empty());
    EXPECT_EQ(app.active_tab, 0u);
    EXPECT_FALSE(app.hasOpenScene());
}

TEST(AppState, ClosingATabBeforeTheActiveOneShiftsTheSelection)
{
    AppState app;
    app.newScene(); // Untitled 1, tab 1
    app.newScene(); // Untitled 2, tab 2
    app.newScene(); // Untitled 3, tab 3
    ASSERT_EQ(app.active_tab, 3u);

    app.closeScene(0); // removes Untitled 1

    // Untitled 3 slid down a slot, so the selection has to follow it.
    EXPECT_EQ(app.active_tab, 2u);
    EXPECT_EQ(app.getCurrentScene().name, "Untitled 3");
}

TEST(AppState, ClosingATabAfterTheActiveOneLeavesTheSelectionAlone)
{
    AppState app;
    app.newScene();
    app.newScene();
    app.newScene();
    app.active_tab = 1;

    app.closeScene(2); // removes the last scene

    EXPECT_EQ(app.active_tab, 1u);
    EXPECT_EQ(app.getCurrentScene().name, "Untitled 1");
}

TEST(AppState, ClosingTheActiveTabKeepsTheSelectionInRange)
{
    AppState app;
    app.newScene();
    app.newScene();
    ASSERT_EQ(app.active_tab, 2u);

    app.closeScene(1); // close the active, last tab

    EXPECT_TRUE(app.hasOpenScene());
    EXPECT_LE(app.active_tab, app.scenes.size());
    EXPECT_EQ(app.getCurrentScene().name, "Untitled 1");
}

TEST(AppState, ClosingAnOutOfRangeIndexIsIgnored)
{
    AppState app;
    app.newScene();

    app.closeScene(7);

    EXPECT_EQ(app.scenes.size(), 1u);
    EXPECT_EQ(app.active_tab, 1u);
}

TEST(AppState, EachSceneKeepsItsOwnCamera)
{
    AppState app;
    app.newScene();
    app.newScene();

    app.active_tab = 1;
    app.getCurrentScene().camera.zoomTowardTarget(2.0f);
    const f32 zoomed = app.getCurrentScene().camera.getDistanceToTarget();

    app.active_tab = 2;
    // The second scene must be untouched by the first scene's navigation.
    EXPECT_GT(app.getCurrentScene().camera.getDistanceToTarget(), zoomed);
}
