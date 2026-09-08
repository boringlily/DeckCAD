#include "MsdfAtlas.h"

#include <msdfgen.h>
#include <msdfgen-ext.h>

#include <algorithm>
#include <cmath>

namespace Text
{
namespace MsdfAtlasInternal
{

    /// One glyph staged before packing.
    struct Baked
    {
        u32 codepoint { 0 };
        msdfgen::Bitmap<float, 3> bitmap;
        f32 advance { 0.0f };
        double left { 0 }, bottom { 0 }, right { 0 }, top { 0 }; // padded bounds, em units
        u32 width { 0 }, height { 0 };
        bool has_geometry { false };
    };

    u8 FloatToByte(float value)
    {
        // msdfgen centers distances on 0.5; clamp before quantizing to avoid wraparound
        float scaled = value * 255.0f + 0.5f;
        return static_cast<u8>(std::clamp(scaled, 0.0f, 255.0f));
    }

    u64 KerningKey(u32 left, u32 right)
    {
        return (static_cast<u64>(left) << 32) | static_cast<u64>(right);
    }

    /// Smallest power of two >= value.
    u32 NextPowerOfTwo(u32 value)
    {
        u32 result = 1;
        while(result < value)
        {
            result <<= 1;
        }
        return result;
    }

    /**
     * @brief Shelf packer: places glyphs in rows, tallest first.
     * @note A perfect bin pack would save only a few percent of atlas area
     * for a static ASCII atlas; the simpler shelf approach is good enough here.
     */
    bool ShelfPack(std::vector<Baked>& glyphs_ref, u32 atlas_size, u32 padding, std::vector<std::pair<u32, u32>>& out_positions_ref)
    {
        out_positions_ref.assign(glyphs_ref.size(), { 0, 0 });

        std::vector<size_t> order;
        order.reserve(glyphs_ref.size());
        for(size_t i = 0; i < glyphs_ref.size(); ++i)
        {
            if(glyphs_ref[i].has_geometry)
            {
                order.push_back(i);
            }
        }
        std::sort(order.begin(), order.end(), [&](size_t a, size_t b)
            { return glyphs_ref[a].height > glyphs_ref[b].height; });

        u32 pen_x = padding;
        u32 pen_y = padding;
        u32 shelf_height = 0;

        for(size_t index : order)
        {
            const Baked& glyph_ref = glyphs_ref[index];
            if(pen_x + glyph_ref.width + padding > atlas_size)
            {
                // Start a new shelf.
                pen_x = padding;
                pen_y += shelf_height + padding;
                shelf_height = 0;
            }
            if(pen_y + glyph_ref.height + padding > atlas_size)
            {
                return false; // caller retries with a larger atlas
            }

            out_positions_ref[index] = { pen_x, pen_y };
            pen_x += glyph_ref.width + padding;
            shelf_height = std::max(shelf_height, glyph_ref.height);
        }
        return true;
    }

} // namespace MsdfAtlasInternal
using namespace MsdfAtlasInternal;

bool MsdfAtlas::buildAtlas(const std::string& font_path_ref, const AtlasConfiguration& configuration_ref, std::string& out_error_ref)
{
    glyphs_.clear();
    kerning_.clear();
    pixels_.clear();
    width_ = 0;
    height_ = 0;

    msdfgen::FreetypeHandle* freetype_ptr = msdfgen::initializeFreetype();
    if(!freetype_ptr)
    {
        out_error_ref = "Failed to initialize FreeType";
        return false;
    }

    msdfgen::FontHandle* font_ptr = msdfgen::loadFont(freetype_ptr, font_path_ref.c_str());
    if(!font_ptr)
    {
        msdfgen::deinitializeFreetype(freetype_ptr);
        out_error_ref = "Failed to load font '" + font_path_ref + "'";
        return false;
    }

    msdfgen::FontMetrics font_metrics {};
    if(msdfgen::getFontMetrics(font_metrics, font_ptr, msdfgen::FONT_SCALING_EM_NORMALIZED))
    {
        metrics_.line_height = static_cast<f32>(font_metrics.lineHeight);
        metrics_.ascender = static_cast<f32>(font_metrics.ascenderY);
        metrics_.descender = static_cast<f32>(font_metrics.descenderY);
    }
    // Some fonts report no line height; fall back to a sane typographic default.
    if(metrics_.line_height <= 0.0f)
    {
        metrics_.line_height = 1.2f;
    }
    metrics_.pixel_range = configuration_ref.pixel_range;

    const double pixels_per_em = static_cast<double>(configuration_ref.glyph_pixel_size);
    // range in shape (em) units, required by the generator
    const double range_em = static_cast<double>(configuration_ref.pixel_range) / pixels_per_em;
    // quad padded by the full range to avoid clipping the field at the edges
    const double pad_em = range_em;

    std::vector<Baked> baked;
    baked.reserve(configuration_ref.last_codepoint - configuration_ref.first_codepoint + 1);

    for(u32 codepoint = configuration_ref.first_codepoint; codepoint <= configuration_ref.last_codepoint; ++codepoint)
    {
        msdfgen::Shape shape;
        double advance = 0.0;
        if(!msdfgen::loadGlyph(shape, font_ptr, codepoint, msdfgen::FONT_SCALING_EM_NORMALIZED, &advance))
        {
            continue; // codepoint absent from the font
        }

        Baked entry_ref;
        entry_ref.codepoint = codepoint;
        entry_ref.advance = static_cast<f32>(advance);

        shape.normalize();
        if(shape.edgeCount() == 0 || !shape.validate())
        {
            // Whitespace and other empty glyphs still need their advance recorded.
            baked.push_back(std::move(entry_ref));
            continue;
        }

        // each edge gets one of three colours; the three texture channels then
        // encode corners exactly, the "multi-channel" part of MSDF
        msdfgen::edgeColoringSimple(shape, 3.0);

        msdfgen::Shape::Bounds bounds = shape.getBounds();
        entry_ref.left = bounds.l - pad_em;
        entry_ref.bottom = bounds.b - pad_em;
        entry_ref.right = bounds.r + pad_em;
        entry_ref.top = bounds.t + pad_em;

        entry_ref.width = static_cast<u32>(std::ceil((entry_ref.right - entry_ref.left) * pixels_per_em));
        entry_ref.height = static_cast<u32>(std::ceil((entry_ref.top - entry_ref.bottom) * pixels_per_em));
        if(entry_ref.width == 0 || entry_ref.height == 0)
        {
            baked.push_back(std::move(entry_ref));
            continue;
        }

        entry_ref.bitmap = msdfgen::Bitmap<float, 3>(static_cast<int>(entry_ref.width), static_cast<int>(entry_ref.height));

        // Map em space onto the glyph's own bitmap: uniform scale, then shift
        // the padded lower-left corner to the bitmap origin.
        msdfgen::Projection projection(
            msdfgen::Vector2(pixels_per_em, pixels_per_em),
            msdfgen::Vector2(-entry_ref.left, -entry_ref.bottom));

        msdfgen::generateMSDF(entry_ref.bitmap, shape, projection, msdfgen::Range(range_em));

        entry_ref.has_geometry = true;
        baked.push_back(std::move(entry_ref));
    }

    if(baked.empty())
    {
        msdfgen::destroyFont(font_ptr);
        msdfgen::deinitializeFreetype(freetype_ptr);
        out_error_ref = "Font '" + font_path_ref + "' produced no usable glyphs";
        return false;
    }

    // --- Pack ---------------------------------------------------------------
    constexpr u32 PADDING = 2; // keeps bilinear taps from bleeding between glyphs

    u64 area = 0;
    u32 max_glyph_edge = 1;
    for(const Baked& entry_ref : baked)
    {
        if(!entry_ref.has_geometry)
        {
            continue;
        }
        area += static_cast<u64>(entry_ref.width + PADDING) * (entry_ref.height + PADDING);
        max_glyph_edge = std::max({ max_glyph_edge, entry_ref.width + 2 * PADDING, entry_ref.height + 2 * PADDING });
    }

    // Start from the area-implied square, with slack for shelf waste, and grow
    // until everything fits.
    u32 atlas_size = NextPowerOfTwo(std::max(max_glyph_edge,
        static_cast<u32>(std::ceil(std::sqrt(static_cast<double>(area) * 1.25)))));

    std::vector<std::pair<u32, u32>> positions;
    while(!ShelfPack(baked, atlas_size, PADDING, positions))
    {
        atlas_size <<= 1;
        if(atlas_size > configuration_ref.max_atlas_size)
        {
            msdfgen::destroyFont(font_ptr);
            msdfgen::deinitializeFreetype(freetype_ptr);
            out_error_ref = "Glyphs do not fit within the maximum atlas size";
            return false;
        }
    }

    width_ = atlas_size;
    height_ = atlas_size;
    pixels_.assign(static_cast<size_t>(atlas_size) * atlas_size * 4, 0);

    // --- Blit and record metrics -------------------------------------------
    for(size_t i = 0; i < baked.size(); ++i)
    {
        const Baked& entry_ref = baked[i];

        Glyph glyph;
        glyph.advance = entry_ref.advance;
        glyph.has_geometry = entry_ref.has_geometry;

        if(entry_ref.has_geometry)
        {
            const u32 origin_x = positions[i].first;
            const u32 origin_y = positions[i].second;

            for(u32 y = 0; y < entry_ref.height; ++y)
            {
                // msdfgen bitmaps are bottom-up, atlas is top-down: rows flipped
                // here, UVs below need no further flipping
                const u32 source_y = entry_ref.height - 1 - y;
                for(u32 x = 0; x < entry_ref.width; ++x)
                {
                    const float* texel_ptr = entry_ref.bitmap(static_cast<int>(x), static_cast<int>(source_y));
                    const size_t dest = (static_cast<size_t>(origin_y + y) * atlas_size + (origin_x + x)) * 4;
                    pixels_[dest + 0] = FloatToByte(texel_ptr[0]);
                    pixels_[dest + 1] = FloatToByte(texel_ptr[1]);
                    pixels_[dest + 2] = FloatToByte(texel_ptr[2]);
                    pixels_[dest + 3] = 255;
                }
            }

            glyph.plane_left = static_cast<f32>(entry_ref.left);
            glyph.plane_bottom = static_cast<f32>(entry_ref.bottom);
            glyph.plane_right = static_cast<f32>(entry_ref.right);
            glyph.plane_top = static_cast<f32>(entry_ref.top);

            const f32 inverse_size = 1.0f / static_cast<f32>(atlas_size);
            glyph.uv_left = static_cast<f32>(origin_x) * inverse_size;
            glyph.uv_top = static_cast<f32>(origin_y) * inverse_size;
            glyph.uv_right = static_cast<f32>(origin_x + entry_ref.width) * inverse_size;
            glyph.uv_bottom = static_cast<f32>(origin_y + entry_ref.height) * inverse_size;
        }

        glyphs_[entry_ref.codepoint] = glyph;
    }

    // --- Kerning ------------------------------------------------------------
    for(const Baked& left : baked)
    {
        for(const Baked& right : baked)
        {
            double kerning = 0.0;
            if(msdfgen::getKerning(kerning, font_ptr, left.codepoint, right.codepoint, msdfgen::FONT_SCALING_EM_NORMALIZED)
                && kerning != 0.0)
            {
                kerning_[KerningKey(left.codepoint, right.codepoint)] = static_cast<f32>(kerning);
            }
        }
    }

    msdfgen::destroyFont(font_ptr);
    msdfgen::deinitializeFreetype(freetype_ptr);
    return true;
}

const Glyph* MsdfAtlas::findGlyph(u32 codepoint) const
{
    auto it = glyphs_.find(codepoint);
    return it == glyphs_.end() ? nullptr : &it->second;
}

f32 MsdfAtlas::getKerning(u32 left, u32 right) const
{
    auto it = kerning_.find(KerningKey(left, right));
    return it == kerning_.end() ? 0.0f : it->second;
}

f32 MsdfAtlas::measureWidth(const std::string& text_ref) const
{
    f32 width = 0.0f;
    u32 previous = 0;
    for(unsigned char character : text_ref)
    {
        const Glyph* glyph_ptr = findGlyph(character);
        if(!glyph_ptr)
        {
            continue;
        }
        if(previous != 0)
        {
            width += getKerning(previous, character);
        }
        width += glyph_ptr->advance;
        previous = character;
    }
    return width;
}

} // namespace Text
