#pragma once
#include "GpuContext.h"
#include "DcadMath.h"
#include "MsdfAtlas.h"
#include "Types.h"
#include <string>
#include <vector>

namespace Text
{

enum class AlignHorizontal : u8
{
    Left,
    Center,
    Right
};

enum class AlignVertical : u8
{
    Baseline,
    Bottom,
    Middle,
    Top
};

/**
 * @brief Draws MSDF text anchored to world-space positions inside the 3D viewport.
 * @note Glyphs are batched into a single vertex buffer per frame and issued
 * as one draw call, keeping a viewport full of dimension labels cheap.
 */
class TextRenderer
{
public:
    bool initializeResources(const Gpu::Context& gpu_ref,
        const MsdfAtlas& atlas_ref,
        wgpu::TextureFormat color_format,
        wgpu::TextureFormat depth_format,
        std::string& out_error_ref);

    void shutdownResources();

    /// Discards anything queued by the previous frame.
    void beginBatch();

    /**
     * @brief Queues a single-line label anchored at world_position.
     * @param pixel_size Em height in screen pixels; the label keeps that size
     * regardless of camera distance.
     */
    void addLabel(const std::string& text_ref,
        DcadMath::Vector3 world_position,
        f32 pixel_size,
        DcadMath::Vector4 color,
        AlignHorizontal align_horizontal = AlignHorizontal::Center,
        AlignVertical align_vertical = AlignVertical::Middle);

    /// Uploads the batch and records the draw. Safe to call with nothing queued.
    void flushBatch(const Gpu::Context& gpu_ref,
        const wgpu::RenderPassEncoder& pass_ref,
        const DcadMath::Matrix4& view_projection_ref,
        f32 viewport_width,
        f32 viewport_height);

    bool isValid() const { return static_cast<bool>(pipeline_); }

private:
    struct Vertex
    {
        f32 anchor[3];
        f32 offset[2];
        f32 uv[2];
        f32 color[4];
    };

    void ensureVertexCapacity(const Gpu::Context& gpu_ref, size_t vertex_count);

    const MsdfAtlas* atlas_ptr_ { nullptr };

    wgpu::RenderPipeline pipeline_;
    wgpu::BindGroup bind_group_;
    wgpu::Buffer uniform_buffer_;
    wgpu::Buffer vertex_buffer_;
    size_t vertex_capacity_ { 0 };

    wgpu::Texture atlas_texture_;
    wgpu::TextureView atlas_view_;
    wgpu::Sampler sampler_;

    std::vector<Vertex> vertices_;
};

} // namespace Text
