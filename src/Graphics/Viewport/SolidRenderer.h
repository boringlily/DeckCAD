#pragma once
#include "GpuContext.h"
#include "DeckMath.h"
#include "Types.h"
#include <string>
#include <vector>

namespace Viewport {

enum class OriginPlane : u8 { XY,
    XZ,
    YZ };

/**
 * @brief Batched flat-shaded triangle renderer.
 * @note Currently draws only the translucent origin planes. It is the seam
 * where solid modelling geometry will plug in, since it already handles
 * batching, blending, and depth for arbitrary triangles.
 */
class SolidRenderer {
public:
    bool initializeResources(const Gpu::Context& gpu_ref,
        wgpu::TextureFormat color_format,
        wgpu::TextureFormat depth_format,
        std::string& out_error_ref);

    void shutdownResources();

    /// Discards the previous frame's geometry.
    void beginBatch();

    void addTriangle(DeckMath::Vector3 a, DeckMath::Vector3 b, DeckMath::Vector3 c, DeckMath::Vector4 color);

    /// Adds a quad as two triangles. Vertices must be given in winding order.
    void addQuad(DeckMath::Vector3 a, DeckMath::Vector3 b, DeckMath::Vector3 c, DeckMath::Vector3 d, DeckMath::Vector4 color);

    /// Adds one of the three origin planes as a square of side 2 * @p half_size.
    void addOriginPlane(OriginPlane plane, f32 half_size, DeckMath::Vector4 color);

    void flushBatch(const Gpu::Context& gpu_ref,
        const wgpu::RenderPassEncoder& pass_ref,
        const DeckMath::Matrix4& view_ref,
        const DeckMath::Matrix4& projection_ref);

    bool isValid() const { return static_cast<bool>(pipeline_); }

private:
    struct Vertex {
        f32 position[3];
        f32 color[4];
    };

    void ensureVertexCapacity(const Gpu::Context& gpu_ref, size_t vertex_count);

    wgpu::RenderPipeline pipeline_;
    wgpu::BindGroup bind_group_;
    wgpu::Buffer uniform_buffer_;
    wgpu::Buffer vertex_buffer_;
    size_t vertex_capacity_ { 0 };
    std::vector<Vertex> vertices_;
};

} // namespace Viewport
