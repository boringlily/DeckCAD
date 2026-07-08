#pragma once
#include "Camera.h"
#include "GpuContext.h"
#include "GridRenderer.h"
#include "MsdfAtlas.h"
#include "SolidRenderer.h"
#include "TextRenderer.h"
#include "Types.h"
#include <string>

namespace Viewport {

/**
 * @brief Bundles an offscreen colour+depth target, the renderers that draw
 * into it, and the camera plumbing for the 3D viewport.
 * @note Rendering offscreen, rather than straight to the backbuffer, lets the
 * viewport live inside a dockable ImGui panel and be resized freely.
 *
 * The target is allocated in rounded blocks and the scene is drawn into a
 * sub-rectangle of it. Dragging a splitter changes the requested size every
 * frame, and ImGui's WebGPU backend permanently caches a bind group per
 * texture view it sees; reallocating on each pixel change would leak a
 * texture per frame.
 */
class Viewport {
public:
    static constexpr wgpu::TextureFormat COLOR_FORMAT = wgpu::TextureFormat::RGBA8Unorm;
    static constexpr wgpu::TextureFormat DEPTH_FORMAT = wgpu::TextureFormat::Depth24Plus;

    /// Allocation granularity, in pixels.
    static constexpr u32 SIZE_GRANULARITY = 128;

    bool initializeResources(const Gpu::Context& gpu_ref, const Text::MsdfAtlas& atlas_ref, std::string& out_error_ref);
    void shutdownResources();

    /// Sets the area the scene is rendered into, reallocating the backing
    /// texture only when the rounded-up size actually needs to change.
    void resizeTarget(const Gpu::Context& gpu_ref, u32 width, u32 height);

    /// Clears the per-frame geometry batches. Call before queuing anything.
    void beginScene();

    /// Records the viewport render pass into the frame's command encoder.
    void renderFrame(const Gpu::Context& gpu_ref, const Camera& camera_ref);

    /// Handle for ImGui::Image. Null until the first successful resizeTarget().
    const wgpu::TextureView& getColorTextureView() const { return color_view_; }

    /// Bottom-right UV of the live sub-rectangle; the image must be drawn with
    /// uv0 = (0,0) and uv1 = this, since the texture is larger than the scene.
    void getContentUvMax(f32& out_u_ref, f32& out_v_ref) const;

    u32 getWidth() const { return width_; }
    u32 getHeight() const { return height_; }
    f32 getAspectRatio() const { return height_ > 0 ? static_cast<f32>(width_) / static_cast<f32>(height_) : 1.0f; }

    SolidRenderer& getSolids() { return solids_; }
    Text::TextRenderer& getLabels() { return text_; }
    GridRenderer& getGrid() { return grid_; }

    DeckMath::Vector4& getBackgroundColor() { return background_; }

    bool isReady() const { return static_cast<bool>(color_view_) && width_ > 0 && height_ > 0; }

private:
    void releaseTargets();
    void allocateTargets(const Gpu::Context& gpu_ref, u32 width, u32 height);

    wgpu::Texture color_texture_;
    wgpu::TextureView color_view_;
    wgpu::Texture depth_texture_;
    wgpu::TextureView depth_view_;

    GridRenderer grid_;
    SolidRenderer solids_;
    Text::TextRenderer text_;

    /// Live scene area, in pixels.
    u32 width_ { 0 };
    u32 height_ { 0 };
    /// Allocated texture size, always >= the live area and a multiple of the granularity.
    u32 texture_width_ { 0 };
    u32 texture_height_ { 0 };

    DeckMath::Vector4 background_ { 0.16f, 0.17f, 0.20f, 1.0f };
};

} // namespace Viewport
