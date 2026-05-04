#include "MsdfAtlas.h"
#include <gtest/gtest.h>
#include <algorithm>
#include <string>

// Exercises the real FreeType -> msdfgen -> atlas path against a font that
// ships with the repo, so a break in the text pipeline fails a test instead of
// silently rendering nothing in the viewport.

namespace TextAtlasTestsInternal {

const std::string& FontPath()
{
    static const std::string path
        = std::string(DECKCAD_ASSETS_DIR) + "/fonts/Nunito/static/Nunito-SemiBold.ttf";
    return path;
}

Text::AtlasConfiguration SmallConfiguration()
{
    Text::AtlasConfiguration configuration {};
    // A narrow range keeps the test fast; the packing logic is the same.
    configuration.glyph_pixel_size = 32;
    configuration.first_codepoint = 32;  // space
    configuration.last_codepoint = 90;   // 'Z'
    return configuration;
}

class TextAtlas : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        build_error.clear();
        atlas_built = shared_atlas.buildAtlas(FontPath(), SmallConfiguration(), build_error);
    }

    static Text::MsdfAtlas shared_atlas;
    static bool atlas_built;
    static std::string build_error;
};

Text::MsdfAtlas TextAtlas::shared_atlas;
bool TextAtlas::atlas_built = false;
std::string TextAtlas::build_error;

} // namespace TextAtlasTestsInternal
using namespace TextAtlasTestsInternal;

TEST_F(TextAtlas, BuildsFromTheBundledFont)
{
    ASSERT_TRUE(atlas_built) << build_error;
    EXPECT_TRUE(shared_atlas.isValid());
}

TEST_F(TextAtlas, ProducesASquarePowerOfTwoAtlas)
{
    ASSERT_TRUE(atlas_built) << build_error;

    EXPECT_EQ(shared_atlas.getWidth(), shared_atlas.getHeight());
    EXPECT_GT(shared_atlas.getWidth(), 0u);
    EXPECT_EQ(shared_atlas.getWidth() & (shared_atlas.getWidth() - 1), 0u) << "expected a power of two";
    EXPECT_EQ(shared_atlas.getPixels().size(), static_cast<size_t>(shared_atlas.getWidth()) * shared_atlas.getHeight() * 4);
}

TEST_F(TextAtlas, ReportsPlausibleVerticalMetrics)
{
    ASSERT_TRUE(atlas_built) << build_error;

    const Text::AtlasMetrics& metrics_ref = shared_atlas.getMetrics();
    EXPECT_GT(metrics_ref.ascender, 0.0f);
    EXPECT_LT(metrics_ref.descender, 0.0f); // below the baseline
    EXPECT_GT(metrics_ref.line_height, metrics_ref.ascender);
}

TEST_F(TextAtlas, LettersCarryGeometryAndSpaceDoesNot)
{
    ASSERT_TRUE(atlas_built) << build_error;

    const Text::Glyph* letter_a_ptr = shared_atlas.findGlyph('A');
    ASSERT_NE(letter_a_ptr, nullptr);
    EXPECT_TRUE(letter_a_ptr->has_geometry);
    EXPECT_GT(letter_a_ptr->advance, 0.0f);
    EXPECT_GT(letter_a_ptr->plane_right, letter_a_ptr->plane_left);
    EXPECT_GT(letter_a_ptr->plane_top, letter_a_ptr->plane_bottom);

    const Text::Glyph* space_ptr = shared_atlas.findGlyph(' ');
    ASSERT_NE(space_ptr, nullptr);
    // Space advances the pen but rasterizes nothing.
    EXPECT_FALSE(space_ptr->has_geometry);
    EXPECT_GT(space_ptr->advance, 0.0f);
}

TEST_F(TextAtlas, GlyphUvsStayInsideTheAtlas)
{
    ASSERT_TRUE(atlas_built) << build_error;

    for (u32 codepoint = 33; codepoint <= 90; ++codepoint) {
        const Text::Glyph* glyph_ptr = shared_atlas.findGlyph(codepoint);
        if (!glyph_ptr || !glyph_ptr->has_geometry) {
            continue;
        }
        SCOPED_TRACE("codepoint " + std::to_string(codepoint));
        EXPECT_GE(glyph_ptr->uv_left, 0.0f);
        EXPECT_GE(glyph_ptr->uv_top, 0.0f);
        EXPECT_LE(glyph_ptr->uv_right, 1.0f);
        EXPECT_LE(glyph_ptr->uv_bottom, 1.0f);
        EXPECT_LT(glyph_ptr->uv_left, glyph_ptr->uv_right);
        EXPECT_LT(glyph_ptr->uv_top, glyph_ptr->uv_bottom);
    }
}

TEST_F(TextAtlas, RasterizesActualDistanceData)
{
    ASSERT_TRUE(atlas_built) << build_error;

    // An all-zero atlas would mean msdfgen ran but wrote nothing, which the
    // shape/UV assertions above would not catch.
    const std::vector<u8>& pixels_ref = shared_atlas.getPixels();
    const bool any_non_zero = std::any_of(pixels_ref.begin(), pixels_ref.end(),
        [](u8 value) { return value != 0 && value != 255; });
    EXPECT_TRUE(any_non_zero) << "atlas contains no intermediate distance values";
}

TEST_F(TextAtlas, MeasureWidthGrowsWithTextAndIgnoresUnknownGlyphs)
{
    ASSERT_TRUE(atlas_built) << build_error;

    const f32 single = shared_atlas.measureWidth("A");
    const f32 triple = shared_atlas.measureWidth("AAA");

    EXPECT_GT(single, 0.0f);
    EXPECT_GT(triple, single);
    EXPECT_NEAR(shared_atlas.measureWidth(""), 0.0f, 1e-6f);
}

TEST_F(TextAtlas, ReportsAFailureForAMissingFont)
{
    Text::MsdfAtlas missing;
    std::string error;

    EXPECT_FALSE(missing.buildAtlas("/nonexistent/font.ttf", SmallConfiguration(), error));
    EXPECT_FALSE(error.empty());
    EXPECT_FALSE(missing.isValid());
}
