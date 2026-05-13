#include "GridRenderer.h"
#include "Assets.h"

#include <cstring>

namespace Viewport {
namespace GridRendererInternal {

    // Must mirror the Uniforms struct in shaders/grid.wgsl.
    struct GridUniforms {
        f32 view_projection[16];
        f32 inverse_view_projection[16];
        f32 camera_position[4];
        f32 params[4];
        f32 color_minor[4];
        f32 color_major[4];
        f32 color_axis_x[4];
        f32 color_axis_z[4];
    };

    void CopyVector4(f32* destination_ptr, const DeckMath::Vector4& source_ref)
    {
        destination_ptr[0] = source_ref.x;
        destination_ptr[1] = source_ref.y;
        destination_ptr[2] = source_ref.z;
        destination_ptr[3] = source_ref.w;
    }

} // namespace GridRendererInternal
using namespace GridRendererInternal;

bool GridRenderer::initializeResources(const Gpu::Context& gpu_ref,
    wgpu::TextureFormat color_format,
    wgpu::TextureFormat depth_format,
    std::string& out_error_ref)
{
    std::string wgsl;
    if (!Platform::Assets::ReadTextFile(Platform::Assets::Resolve("shaders/grid.wgsl"), wgsl, out_error_ref)) {
        return false;
    }
    wgpu::ShaderModule shader = gpu_ref.createShaderModule("grid", wgsl);
    if (!shader) {
        out_error_ref = "Failed to compile shaders/grid.wgsl";
        return false;
    }

    wgpu::BufferDescriptor uniform_descriptor {};
    uniform_descriptor.label = "Grid Uniforms";
    uniform_descriptor.size = sizeof(GridUniforms);
    uniform_descriptor.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
    uniform_buffer_ = gpu_ref.getDevice().CreateBuffer(&uniform_descriptor);

    wgpu::BindGroupLayoutEntry layout_entry {};
    layout_entry.binding = 0;
    layout_entry.visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
    layout_entry.buffer.type = wgpu::BufferBindingType::Uniform;
    layout_entry.buffer.minBindingSize = sizeof(GridUniforms);

    wgpu::BindGroupLayoutDescriptor bind_group_layout_descriptor {};
    bind_group_layout_descriptor.label = "Grid BindGroupLayout";
    bind_group_layout_descriptor.entryCount = 1;
    bind_group_layout_descriptor.entries = &layout_entry;
    wgpu::BindGroupLayout bind_group_layout = gpu_ref.getDevice().CreateBindGroupLayout(&bind_group_layout_descriptor);

    wgpu::BindGroupEntry bind_entry {};
    bind_entry.binding = 0;
    bind_entry.buffer = uniform_buffer_;
    bind_entry.size = sizeof(GridUniforms);

    wgpu::BindGroupDescriptor bind_group_descriptor {};
    bind_group_descriptor.label = "Grid BindGroup";
    bind_group_descriptor.layout = bind_group_layout;
    bind_group_descriptor.entryCount = 1;
    bind_group_descriptor.entries = &bind_entry;
    bind_group_ = gpu_ref.getDevice().CreateBindGroup(&bind_group_descriptor);

    wgpu::PipelineLayoutDescriptor pipeline_layout_descriptor {};
    pipeline_layout_descriptor.bindGroupLayoutCount = 1;
    pipeline_layout_descriptor.bindGroupLayouts = &bind_group_layout;
    wgpu::PipelineLayout pipeline_layout = gpu_ref.getDevice().CreatePipelineLayout(&pipeline_layout_descriptor);

    wgpu::BlendState blend {};
    blend.color.operation = wgpu::BlendOperation::Add;
    blend.color.srcFactor = wgpu::BlendFactor::One; // premultiplied
    blend.color.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
    blend.alpha.operation = wgpu::BlendOperation::Add;
    blend.alpha.srcFactor = wgpu::BlendFactor::One;
    blend.alpha.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;

    wgpu::ColorTargetState color_target {};
    color_target.format = color_format;
    color_target.blend = &blend;
    color_target.writeMask = wgpu::ColorWriteMask::All;

    wgpu::FragmentState fragment {};
    fragment.module = shader;
    fragment.entryPoint = "fs_main";
    fragment.targetCount = 1;
    fragment.targets = &color_target;

    // The shader writes true per-pixel depth, so the grid interleaves correctly
    // with scene geometry instead of being a flat overlay.
    wgpu::DepthStencilState depthStencil {};
    depthStencil.format = depth_format;
    depthStencil.depthWriteEnabled = wgpu::OptionalBool::True;
    depthStencil.depthCompare = wgpu::CompareFunction::LessEqual;

    wgpu::RenderPipelineDescriptor pipeline_descriptor {};
    pipeline_descriptor.label = "Grid Pipeline";
    pipeline_descriptor.layout = pipeline_layout;
    pipeline_descriptor.vertex.module = shader;
    pipeline_descriptor.vertex.entryPoint = "vs_main";
    pipeline_descriptor.vertex.bufferCount = 0;
    pipeline_descriptor.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
    pipeline_descriptor.primitive.cullMode = wgpu::CullMode::None;
    pipeline_descriptor.depthStencil = &depthStencil;
    pipeline_descriptor.multisample.count = 1;
    pipeline_descriptor.multisample.mask = ~0u;
    pipeline_descriptor.fragment = &fragment;

    pipeline_ = gpu_ref.getDevice().CreateRenderPipeline(&pipeline_descriptor);
    if (!pipeline_) {
        out_error_ref = "Failed to create the grid render pipeline";
        return false;
    }
    return true;
}

void GridRenderer::shutdownResources()
{
    pipeline_ = nullptr;
    bind_group_ = nullptr;
    uniform_buffer_ = nullptr;
}

void GridRenderer::drawGrid(const Gpu::Context& gpu_ref,
    const wgpu::RenderPassEncoder& pass_ref,
    const DeckMath::Matrix4& view_ref,
    const DeckMath::Matrix4& projection_ref,
    DeckMath::Vector3 camera_position)
{
    if (!pipeline_ || !enabled_) {
        return;
    }

    DeckMath::Matrix4 view_projection = projection_ref * view_ref;
    DeckMath::Matrix4 inverse_view_projection = DeckMath::Inverse(view_projection);

    GridUniforms uniforms {};
    std::memcpy(uniforms.view_projection, &view_projection.columns[0].x, sizeof(uniforms.view_projection));
    std::memcpy(uniforms.inverse_view_projection, &inverse_view_projection.columns[0].x, sizeof(uniforms.inverse_view_projection));

    uniforms.camera_position[0] = camera_position.x;
    uniforms.camera_position[1] = camera_position.y;
    uniforms.camera_position[2] = camera_position.z;
    uniforms.camera_position[3] = 1.0f;

    uniforms.params[0] = style_.minor_spacing;
    uniforms.params[1] = style_.major_every;
    uniforms.params[2] = style_.fade_start;
    uniforms.params[3] = style_.fade_end;

    CopyVector4(uniforms.color_minor, style_.minor_color);
    CopyVector4(uniforms.color_major, style_.major_color);
    CopyVector4(uniforms.color_axis_x, style_.axis_x_color);
    CopyVector4(uniforms.color_axis_z, style_.axis_z_color);

    gpu_ref.getQueue().WriteBuffer(uniform_buffer_, 0, &uniforms, sizeof(uniforms));

    pass_ref.SetPipeline(pipeline_);
    pass_ref.SetBindGroup(0, bind_group_);
    pass_ref.Draw(3, 1, 0, 0); // one full-screen triangle
}

} // namespace Viewport
