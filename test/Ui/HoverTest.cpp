// Unit tests for the Ui framework's hover/hit-test attribution.
//
// Regression coverage for the "delete button blink": a container that reveals a
// hit-testable child on hover (list row -> Del button, tab -> close chip) must
// stay hovered while the pointer is over that child. Exact hover (IsHovered)
// resolves to a single top-most node, so the child steals the container's hover;
// subtree hover (IsHoverWithin) credits the container while any descendant is hot.
// Clicks (IsClicked) stay exact so input still lands only on the real element.
//
// Runs headless: a no-op DrawBackend lets EndFrame's dispatch run without a window
// (only the geometry/hit-test path is exercised).

#include "Core/UiContext.h"
#include "Backend/IBackend.h"

#include <gtest/gtest.h>
#include <array>

using namespace Ui;

namespace {

void NoopFill(void*, Rect, UiColor, f32) {}
void NoopBorder(void*, Rect, Edges, UiColor, f32) {}

// A parent (200x100) containing one hit-testable child (50x50) at its top-left.
// The child, built after the parent, wins exact hit-testing where the two overlap.
void BuildParentChild(UiId parent, UiId child, bool emitChild = true)
{
    LayoutConfig p {};
    p.sizing = { Fixed(200), Fixed(100) };
    p.direction = Direction::LeftToRight;
    OpenElement(p, parent);
    if (emitChild) {
        LayoutConfig c {};
        c.sizing = { Fixed(50), Fixed(50) };
        OpenElement(c, child); // hitTestable defaults to true
        CloseElement();
    }
    CloseElement();
}

} // namespace

class HoverTest : public ::testing::Test {
protected:
    std::array<unsigned char, 1u << 20> buffer {};
    Context ctx {};
    UiId parent = NameId("parent");
    UiId child = NameId("child");

    void SetUp() override
    {
        UiInitDesc desc {};
        desc.buffer = buffer.data();
        desc.bufferBytes = buffer.size();
        desc.maxNodes = 256;
        desc.maxCommands = 256;
        desc.backend.draw.FillRect = &NoopFill;
        desc.backend.draw.Border = &NoopBorder;
        ASSERT_TRUE(ctx.Init(desc));
        SetCurrent(&ctx);
    }

    void TearDown() override { SetCurrent(nullptr); }

    // One frame: build the tree and resolve hover for `pos`.
    void Settle(Vec2 pos, bool pressed = false)
    {
        PointerState pt {};
        pt.pos = pos;
        pt.pressed = pressed;
        BeginFrame({ 200, 100 }, pt);
        BuildParentChild(parent, child);
        EndFrame();
    }

    struct HoverQ {
        bool parentExact, parentWithin, childExact, childWithin;
    };

    // Resolve hover for `pos` (frame 1), then read the queries at the next frame's
    // build time (frame 2) exactly as the app does — hover reflects last frame.
    HoverQ Probe(Vec2 pos)
    {
        Settle(pos);
        PointerState pt {};
        pt.pos = pos;
        BeginFrame({ 200, 100 }, pt);
        HoverQ q { IsHovered(parent), IsHoverWithin(parent), IsHovered(child), IsHoverWithin(child) };
        BuildParentChild(parent, child);
        EndFrame();
        return q;
    }

    // The reported bug's feedback loop: the child is emitted ONLY on frames where
    // the container reports hovered, with the pointer held over the child's spot.
    // Returns how many of `frames` the child was actually emitted.
    int RevealChildStability(bool useSubtreeHover, int frames)
    {
        UiId row = NameId("row");
        UiId del = NameId("del");
        int present = 0;
        for (int f = 0; f < frames; ++f) {
            PointerState pt {};
            pt.pos = { 25, 25 };
            BeginFrame({ 200, 100 }, pt);
            bool rowHovered = useSubtreeHover ? IsHoverWithin(row) : IsHovered(row);
            LayoutConfig p {};
            p.sizing = { Fixed(200), Fixed(100) };
            p.direction = Direction::LeftToRight;
            OpenElement(p, row);
            if (rowHovered) { // conditional emission — the App.cpp UiLineToolView pattern
                LayoutConfig c {};
                c.sizing = { Fixed(50), Fixed(50) };
                OpenElement(c, del);
                CloseElement();
                ++present;
            }
            CloseElement();
            EndFrame();
        }
        return present;
    }
};

// Exact hover is precise: the pointer over the child does NOT mark the parent.
TEST_F(HoverTest, ExactHoverDoesNotBubbleToParent)
{
    HoverQ q = Probe({ 25, 25 });
    EXPECT_TRUE(q.childExact);
    EXPECT_FALSE(q.parentExact);
}

// The fix: subtree hover credits the parent while the pointer is over its child.
TEST_F(HoverTest, SubtreeHoverCreditsAncestorOverChild)
{
    HoverQ q = Probe({ 25, 25 });
    EXPECT_TRUE(q.childWithin);
    EXPECT_TRUE(q.parentWithin);
}

// Pointer over the parent's own area (not the child): both flavors agree on parent.
TEST_F(HoverTest, HoverOverParentOnly)
{
    HoverQ q = Probe({ 120, 25 });
    EXPECT_TRUE(q.parentExact);
    EXPECT_TRUE(q.parentWithin);
    EXPECT_FALSE(q.childExact);
    EXPECT_FALSE(q.childWithin);
}

// Pointer outside everything: nothing is hovered under either flavor.
TEST_F(HoverTest, NothingHovered)
{
    HoverQ q = Probe({ 500, 500 });
    EXPECT_FALSE(q.parentExact);
    EXPECT_FALSE(q.parentWithin);
    EXPECT_FALSE(q.childExact);
    EXPECT_FALSE(q.childWithin);
}

// Core regression: with subtree hover, a hover-revealed child stays put every frame.
TEST_F(HoverTest, SubtreeHoverPreventsRevealedChildBlink)
{
    const int frames = 12;
    int present = RevealChildStability(/*useSubtreeHover=*/true, frames);
    EXPECT_GE(present, frames - 1); // present every frame after the first settle frame
}

// Characterization of the old bug: exact hover makes the revealed child oscillate,
// confirming why the fix is necessary. (Documents the failure mode under test.)
TEST_F(HoverTest, ExactHoverExhibitsTheRevealedChildBlink)
{
    const int frames = 12;
    int present = RevealChildStability(/*useSubtreeHover=*/false, frames);
    EXPECT_LE(present, frames / 2 + 1); // oscillates ~every other frame
}

// Clicks stay exact: a press over the child registers on the child, not the parent,
// even though subtree hover now considers the parent "hovered". This is the
// "mouse events only registered for the correct element" guarantee.
TEST_F(HoverTest, ClickRegistersOnChildNotParent)
{
    Settle({ 25, 25 }); // resolve: child is the hot node
    PointerState pt {};
    pt.pos = { 25, 25 };
    pt.pressed = true;
    BeginFrame({ 200, 100 }, pt);
    bool childClicked = IsClicked(child);
    bool parentClicked = IsClicked(parent);
    // subtree hover still true for the parent — proves click != hover attribution
    bool parentHovered = IsHoverWithin(parent);
    BuildParentChild(parent, child);
    EndFrame();

    EXPECT_TRUE(childClicked);
    EXPECT_FALSE(parentClicked);
    EXPECT_TRUE(parentHovered);
}
