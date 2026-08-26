#include "Viewport.h"
#include <algorithm>

namespace Viewport {
namespace ViewportInternal {

    u32 RoundUpTo(u32 value, u32 granularity)
    {
        if (value == 0) {
            return granularity;
        }
        return ((value + granularity - 1) / granularity) * granularity;
    }

} // namespace ViewportInternal
using namespace ViewportInternal;

bool Viewport::initializeResources(const Gpu::Context& gpu_ref, const Text::MsdfAtlas& atlas_ref, std::string& out_error_ref)
{
    if (!grid_.initializeResources(gpu_ref, COLOR_FORMAT, DEPTH_FORMAT, out_error_ref)) {
        return false;
    }
    if (!solids_.initializeResources(gpu_ref, COLOR_FORMAT, DEPTH_FORMAT, out_error_ref)) {
        return false;
    }
    // Text is optional: a missing or unbakeable font must not stop the viewport
    // from rendering, it just means no in-viewport labels.
    if (atlas_ref.isValid() && !text_.initializeResources(gpu_ref, atlas_ref, COLOR_FORMAT, DEPTH_FORMAT, out_error_ref)) {
        return false;
    }
    return true;
}

void Viewport::shutdownResources()
{
    releaseTargets();
    text_.shutdownResources();
    solids_.shutdownResources();
    grid_.shutdownResources();
}

void Viewport::releaseTargets()
{
    color_view_ = nullptr;
    color_texture_ = nullptr;
    depth_view_ = nullptr;
    depth_texture_ = nullptr;
    texture_width_ = 0;
    texture_height_ = 0;
}

void Viewport::allocateTargets(const Gpu::Context& gpu_ref, u32 width, u32 height)
{
    releaseTargets();
    texture_width_ = width;
    texture_height_ = height;

    wgpu::TextureDescriptor color_descriptor {};
    color_descriptor.label = "Viewport Color";
    color_descriptor.dimension = wgpu::TextureDimension::e2D;
    color_descriptor.size = { width, height, 1 };
    color_descriptor.format = COLOR_FORMAT;
    color_descriptor.mipLevelCount = 1;
    color_descriptor.sampleCount = 1;
    // TextureBinding so ImGui can sample it back as an image.
    color_descriptor.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding;
    color_texture_ = gpu_ref.getDevice().CreateTexture(&color_descriptor);
    color_view_ = color_texture_.CreateView();

    wgpu::TextureDescriptor depth_descriptor {};
    depth_descriptor.label = "Viewport Depth";
    depth_descriptor.dimension = wgpu::TextureDimension::e2D;
    depth_descriptor.size = { width, height, 1 };
    depth_descriptor.format = DEPTH_FORMAT;
    depth_descriptor.mipLevelCount = 1;
    depth_descriptor.sampleCount = 1;
    depth_descriptor.usage = wgpu::TextureUsage::RenderAttachment;
    depth_texture_ = gpu_ref.getDevice().CreateTexture(&depth_descriptor);
    depth_view_ = depth_texture_.CreateView();
}

void Viewport::resizeTarget(const Gpu::Context& gpu_ref, u32 width, u32 height)
{
    if (width == 0 || height == 0) {
        return;
    }

    width_ = width;
    height_ = height;

    const u32 required_width = RoundUpTo(width, SIZE_GRANULARITY);
    const u32 required_height = RoundUpTo(height, SIZE_GRANULARITY);

    if (!color_view_) {
        allocateTargets(gpu_ref, required_width, required_height);
        return;
    }

    const bool too_small = required_width > texture_width_ || required_height > texture_height_;
    // Only hand memory back once the panel has shrunk well past the current
    // allocation, so nudging a splitter back and forth does not thrash.
    const bool much_too_large = texture_width_ >= required_width * 2 && texture_height_ >= required_height * 2;

    if (much_too_large) {
        allocateTargets(gpu_ref, required_width, required_height);
    } else if (too_small) {
        // Grow on the axis that needs it and keep the other, so resizing in one
        // direction does not repeatedly reallocate the whole target.
        allocateTargets(gpu_ref,
            std::max(required_width, texture_width_),
            std::max(required_height, texture_height_));
    }
}

void Viewport::getContentUvMax(f32& out_u_ref, f32& out_v_ref) const
{
    out_u_ref = texture_width_ > 0 ? static_cast<f32>(width_) / static_cast<f32>(texture_width_) : 1.0f;
    out_v_ref = texture_height_ > 0 ? static_cast<f32>(height_) / static_cast<f32>(texture_height_) : 1.0f;
}

void Viewport::beginScene()
{
    solids_.beginBatch();
    text_.beginBatch();
}

void Viewport::renderFrame(const Gpu::Context& gpu_ref, const Camera& camera_ref)
{
    if (!isReady()) {
        return;
    }

    wgpu::RenderPassColorAttachment color_attachment {};
    color_attachment.view = color_view_;
    color_attachment.loadOp = wgpu::LoadOp::Clear;
    color_attachment.storeOp = wgpu::StoreOp::Store;
    color_attachment.clearValue = { background_.x, background_.y, background_.z, background_.w };

    wgpu::RenderPassDepthStencilAttachment depth_attachment {};
    depth_attachment.view = depth_view_;
    depth_attachment.depthLoadOp = wgpu::LoadOp::Clear;
    depth_attachment.depthStoreOp = wgpu::StoreOp::Store;
    // 1.0 is the far plane under WebGPU's [0, 1] depth range.
    depth_attachment.depthClearValue = 1.0f;

    wgpu::RenderPassDescriptor pass_descriptor {};
    pass_descriptor.label = "Viewport Pass";
    pass_descriptor.colorAttachmentCount = 1;
    pass_descriptor.colorAttachments = &color_attachment;
    pass_descriptor.depthStencilAttachment = &depth_attachment;

    wgpu::RenderPassEncoder pass_ref = gpu_ref.getEncoder().BeginRenderPass(&pass_descriptor);

    // Confine drawing to the live sub-rect; the rest of the texture is padding
    // that the shader-side full-screen triangle must not touch.
    pass_ref.SetViewport(0.0f, 0.0f, static_cast<f32>(width_), static_cast<f32>(height_), 0.0f, 1.0f);
    pass_ref.SetScissorRect(0, 0, width_, height_);

    const DeckMath::Matrix4 view = camera_ref.getViewMatrix();
    const DeckMath::Matrix4 projection = camera_ref.getProjectionMatrix(getAspectRatio());

    // Order matters: the grid writes depth first so translucent planes and
    // labels behind it are correctly occluded.
    grid_.drawGrid(gpu_ref, pass_ref, view, projection, camera_ref.getPosition());
    solids_.flushBatch(gpu_ref, pass_ref, view, projection);
    text_.flushBatch(gpu_ref, pass_ref, projection * view,
        static_cast<f32>(width_), static_cast<f32>(height_));

    pass_ref.End();
}

} // namespace Viewport
