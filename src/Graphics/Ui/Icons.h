#pragma once
#include "GpuContext.h"
#include "Types.h"
#include <array>
#include <string>

namespace Ui {

// Kept as an X-macro so the enum and the filename table can never drift apart.
#define DECKCAD_ICON_LIST(DO) \
    DO(Check)                 \
    DO(Exit)                  \
    DO(Home)                  \
    DO(Parameters)            \
    DO(Plus)                  \
    DO(Project)               \
    DO(ProjectSettings)       \
    DO(Settings)              \
    DO(Unknown)

#define DECKCAD_ICON_ENUM(NAME) NAME,
enum class IconId : u8 {
    DECKCAD_ICON_LIST(DECKCAD_ICON_ENUM)
        Count
};
#undef DECKCAD_ICON_ENUM

inline constexpr size_t ICON_COUNT = static_cast<size_t>(IconId::Count);

/// Rasterizes the SVG icon set into a single GPU atlas.
///
/// Vector sources are rasterized at the display's real pixel density, so icons
/// stay crisp on HiDPI screens instead of being an upscaled bitmap.
class IconSet {
public:
    /// @param display_scale physical pixels per logical point (2.0 on Retina).
    /// @param logical_size  icon edge length in logical points.
    bool initializeResources(const Gpu::Context& gpu_ref, f32 display_scale, u32 logical_size, std::string& out_error_ref);
    void shutdownResources();

    /// Raw WebGPU texture view, which is what ImGui's WebGPU backend uses as
    /// its texture identifier. Cast through intptr_t at the call site.
    void* getTextureHandle() const;

    /// UV rectangle of one icon within the atlas.
    void getUvRange(IconId icon, f32 out_uv0[2], f32 out_uv1[2]) const;

    u32 getLogicalSize() const { return logical_size_; }
    bool isValid() const { return static_cast<bool>(view_); }

private:
    wgpu::Texture texture_;
    wgpu::TextureView view_;
    u32 logical_size_ { 24 };
    u32 pixel_size_ { 24 };
    u32 atlas_width_ { 0 };
    u32 atlas_height_ { 0 };
};

} // namespace Ui
