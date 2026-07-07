#include "Icons.h"
#include "Assets.h"

// nanosvg is a single-header library; this translation unit owns its one
// implementation definition
#define NANOSVG_IMPLEMENTATION
#include <nanosvg.h>
#define NANOSVGRAST_IMPLEMENTATION
#include <nanosvgrast.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace Ui {
namespace IconsInternal {

#define DECKCAD_ICON_NAME(NAME) #NAME,
    constexpr std::array<const char*, ICON_COUNT> ICON_NAMES { DECKCAD_ICON_LIST(DECKCAD_ICON_NAME) };
#undef DECKCAD_ICON_NAME

} // namespace IconsInternal
using namespace IconsInternal;

bool IconSet::initializeResources(const Gpu::Context& gpu_ref, f32 display_scale, u32 logical_size, std::string& out_error_ref)
{
    logical_size_ = logical_size;
    const f32 scale = display_scale > 0.0f ? display_scale : 1.0f;
    pixel_size_ = static_cast<u32>(std::ceil(static_cast<f32>(logical_size) * scale));

    atlas_width_ = pixel_size_ * static_cast<u32>(ICON_COUNT);
    atlas_height_ = pixel_size_;

    std::vector<u8> atlas(static_cast<size_t>(atlas_width_) * atlas_height_ * 4, 0);

    NSVGrasterizer* rasterizer_ptr = nsvgCreateRasterizer();
    if (!rasterizer_ptr) {
        out_error_ref = "Failed to create the nanosvg rasterizer";
        return false;
    }

    std::vector<u8> scratch(static_cast<size_t>(pixel_size_) * pixel_size_ * 4);

    for (size_t index = 0; index < ICON_COUNT; ++index) {
        const std::string path = Platform::Assets::Resolve(
            std::string("Icon/svg/") + ICON_NAMES[index] + ".svg");

        NSVGimage* image_ptr = nsvgParseFromFile(path.c_str(), "px", 96.0f);
        if (!image_ptr) {
            // missing icon leaves a transparent slot instead of aborting startup
            std::fprintf(stderr, "[icons] could not parse '%s'\n", path.c_str());
            continue;
        }

        std::fill(scratch.begin(), scratch.end(), 0);

        // scale by the SVG's longer dimension to fit its viewBox in the square
        const f32 source_size = std::max(image_ptr->width, image_ptr->height);
        const f32 fit_scale = source_size > 0.0f ? static_cast<f32>(pixel_size_) / source_size : 1.0f;

        nsvgRasterize(rasterizer_ptr, image_ptr, 0.0f, 0.0f, fit_scale,
            scratch.data(), static_cast<int>(pixel_size_), static_cast<int>(pixel_size_),
            static_cast<int>(pixel_size_ * 4));
        nsvgDelete(image_ptr);

        // copy the square into its slot in the atlas
        const u32 origin_x = static_cast<u32>(index) * pixel_size_;
        for (u32 y = 0; y < pixel_size_; ++y) {
            const u8* source_ptr = scratch.data() + static_cast<size_t>(y) * pixel_size_ * 4;
            u8* destination_ptr = atlas.data() + (static_cast<size_t>(y) * atlas_width_ + origin_x) * 4;
            std::memcpy(destination_ptr, source_ptr, static_cast<size_t>(pixel_size_) * 4);
        }
    }

    nsvgDeleteRasterizer(rasterizer_ptr);

    wgpu::TextureDescriptor texture_descriptor {};
    texture_descriptor.label = "Icon Atlas";
    texture_descriptor.dimension = wgpu::TextureDimension::e2D;
    texture_descriptor.size = { atlas_width_, atlas_height_, 1 };
    texture_descriptor.format = wgpu::TextureFormat::RGBA8Unorm;
    texture_descriptor.mipLevelCount = 1;
    texture_descriptor.sampleCount = 1;
    texture_descriptor.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;

    texture_ = gpu_ref.getDevice().CreateTexture(&texture_descriptor);
    if (!texture_) {
        out_error_ref = "Failed to create the icon atlas texture";
        return false;
    }
    view_ = texture_.CreateView();

    wgpu::TexelCopyTextureInfo destination_ptr {};
    destination_ptr.texture = texture_;
    destination_ptr.mipLevel = 0;
    destination_ptr.origin = { 0, 0, 0 };
    destination_ptr.aspect = wgpu::TextureAspect::All;

    wgpu::TexelCopyBufferLayout layout {};
    layout.offset = 0;
    layout.bytesPerRow = atlas_width_ * 4;
    layout.rowsPerImage = atlas_height_;

    wgpu::Extent3D write_size { atlas_width_, atlas_height_, 1 };
    gpu_ref.getQueue().WriteTexture(&destination_ptr, atlas.data(), atlas.size(), &layout, &write_size);
    return true;
}

void IconSet::shutdownResources()
{
    view_ = nullptr;
    texture_ = nullptr;
}

void* IconSet::getTextureHandle() const
{
    return static_cast<void*>(view_.Get());
}

void IconSet::getUvRange(IconId icon, f32 out_uv0[2], f32 out_uv1[2]) const
{
    const size_t index = static_cast<size_t>(icon);
    const f32 span = 1.0f / static_cast<f32>(ICON_COUNT);
    out_uv0[0] = static_cast<f32>(index) * span;
    out_uv0[1] = 0.0f;
    out_uv1[0] = static_cast<f32>(index + 1) * span;
    out_uv1[1] = 1.0f;
}

} // namespace Ui
