#pragma once
#include "GpuContext.h"
#include "DcadMath.h"
#include "Types.h"
#include <string>

namespace Viewport
{

/**
 * @brief Draws the analytic infinite ground grid.
 * @note Holds no geometry: a single full-screen triangle plus one uniform
 * buffer, with all the work done per pixel in shaders/grid.wgsl.
 */
class GridRenderer
{
public:
    struct Style
    {
        DcadMath::Vector4 minor_color { 0.62f, 0.62f, 0.66f, 0.45f };
        DcadMath::Vector4 major_color { 0.42f, 0.42f, 0.48f, 0.75f };
        DcadMath::Vector4 axis_x_color { 0.85f, 0.25f, 0.30f, 0.95f };
        DcadMath::Vector4 axis_z_color { 0.22f, 0.45f, 0.90f, 0.95f };
        f32 minor_spacing { 1.0f }; // world units between minor lines
        f32 major_every { 10.0f }; // a major line every N minor lines
        f32 fade_start { 40.0f }; // distance where the grid begins to fade
        f32 fade_end { 160.0f }; // distance where it is fully gone
    };

    bool initializeResources(const Gpu::Context& gpu_ref,
        wgpu::TextureFormat color_format,
        wgpu::TextureFormat depth_format,
        std::string& out_error_ref);

    void shutdownResources();

    void drawGrid(const Gpu::Context& gpu_ref,
        const wgpu::RenderPassEncoder& pass_ref,
        const DcadMath::Matrix4& view_ref,
        const DcadMath::Matrix4& projection_ref,
        DcadMath::Vector3 camera_position);

    Style& getMutableStyle() { return style_; }
    const Style& getStyle() const { return style_; }

    void setEnabled(bool enabled) { enabled_ = enabled; }
    bool isEnabled() const { return enabled_; }

    bool isValid() const { return static_cast<bool>(pipeline_); }

private:
    wgpu::RenderPipeline pipeline_;
    wgpu::BindGroup bind_group_;
    wgpu::Buffer uniform_buffer_;
    Style style_ {};
    bool enabled_ { true };
};

} // namespace Viewport
