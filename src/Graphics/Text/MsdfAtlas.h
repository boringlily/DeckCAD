#pragma once
#include "Types.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace Text
{

/// Placement and metrics for one glyph in the atlas.
struct Glyph
{
    /// Horizontal advance, in em units (1.0 == one em).
    f32 advance { 0.0f };

    /// Quad corners relative to the glyph origin on the baseline, in em units.
    /// +Y is up, matching font convention.
    f32 plane_left { 0.0f };
    f32 plane_bottom { 0.0f };
    f32 plane_right { 0.0f };
    f32 plane_top { 0.0f };

    /// Normalized atlas texture coordinates. uv_top corresponds to plane_top.
    f32 uv_left { 0.0f };
    f32 uv_top { 0.0f };
    f32 uv_right { 0.0f };
    f32 uv_bottom { 0.0f };

    /// False for whitespace, which advances the pen but rasterizes nothing.
    bool has_geometry { false };
};

struct AtlasMetrics
{
    f32 line_height { 0.0f }; // em units
    f32 ascender { 0.0f }; // em units
    f32 descender { 0.0f }; // em units, negative
    f32 pixel_range { 4.0f }; // distance field range, in atlas pixels
};

struct AtlasConfiguration
{
    /// Em size in pixels used when rasterizing. Larger captures finer detail at
    /// the cost of atlas area; MSDF stays sharp well beyond this when magnified.
    u32 glyph_pixel_size { 48 };

    /// Width of the distance field ramp, in pixels. Governs how far the shader
    /// can push effects like outlines before the field runs out.
    f32 pixel_range { 4.0f };

    /// Inclusive codepoint range to bake. Defaults to printable ASCII.
    u32 first_codepoint { 32 };
    u32 last_codepoint { 126 };

    /// Hard ceiling on atlas dimensions.
    u32 max_atlas_size { 4096 };
};

/**
 * @brief Bakes a font into a multi-channel signed distance field atlas.
 * @note FreeType supplies the outlines, msdfgen turns each into an MSDF
 * bitmap, and a shelf packer arranges them into a single RGBA8 texture ready
 * for upload.
 */
class MsdfAtlas
{
public:
    bool buildAtlas(const std::string& font_path_ref, const AtlasConfiguration& configuration_ref, std::string& out_error_ref);

    /// Returns nullptr for codepoints outside the baked set.
    const Glyph* findGlyph(u32 codepoint) const;

    /// Kerning adjustment between two glyphs, in em units. Zero when unknown.
    f32 getKerning(u32 left, u32 right) const;

    /// Advance width of a string in em units, kerning included.
    f32 measureWidth(const std::string& text_ref) const;

    const std::vector<u8>& getPixels() const { return pixels_; } // RGBA8, row-major, top-down
    u32 getWidth() const { return width_; }
    u32 getHeight() const { return height_; }
    const AtlasMetrics& getMetrics() const { return metrics_; }
    bool isValid() const { return width_ > 0 && height_ > 0; }

private:
    std::unordered_map<u32, Glyph> glyphs_;
    std::unordered_map<u64, f32> kerning_;
    std::vector<u8> pixels_;
    u32 width_ { 0 };
    u32 height_ { 0 };
    AtlasMetrics metrics_ {};
};

} // namespace Text
