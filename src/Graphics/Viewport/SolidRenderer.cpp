#include "SolidRenderer.h"
#include "Assets.h"

#include <algorithm>
#include <cstddef>
#include <cstring>

namespace Viewport {
namespace SolidRendererInternal {

    struct SolidUniforms {
        f32 view_projection[16];
    };

} // namespace SolidRendererInternal
using namespace SolidRendererInternal;

bool SolidRenderer::initializeResources(const Gpu::Context& gpu_ref,
    wgpu::TextureFormat color_format,
    wgpu::TextureFormat depth_format,
    std::string& out_error_ref)
{
    std::string wgsl;
    if (!Platform::Assets::ReadTextFile(Platform::Assets::Resolve("shaders/solid.wgsl"), wgsl, out_error_ref)) {
        return false;
    }
    wgpu::ShaderModule shader = gpu_ref.createShaderModule("solid", wgsl);
    if (!shader) {
        out_error_ref = "Failed to compile shaders/solid.wgsl";
        return false;
    }

    wgpu::BufferDescriptor uniform_descriptor {};
    uniform_descriptor.label = "Solid Uniforms";
    uniform_descriptor.size = sizeof(SolidUniforms);
    uniform_descriptor.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
    uniform_buffer_ = gpu_ref.getDevice().CreateBuffer(&uniform_descriptor);

    wgpu::BindGroupLayoutEntry layout_entry {};
    layout_entry.binding = 0;
    layout_entry.visibility = wgpu::ShaderStage::Vertex;
    layout_entry.buffer.type = wgpu::BufferBindingType::Uniform;
    layout_entry.buffer.minBindingSize = sizeof(SolidUniforms);

    wgpu::BindGroupLayoutDescriptor bind_group_layout_descriptor {};
    bind_group_layout_descriptor.label = "Solid BindGroupLayout";
    bind_group_layout_descriptor.entryCount = 1;
    bind_group_layout_descriptor.entries = &layout_entry;
    wgpu::BindGroupLayout bind_group_layout = gpu_ref.getDevice().CreateBindGroupLayout(&bind_group_layout_descriptor);

    wgpu::BindGroupEntry bind_entry {};
    bind_entry.binding = 0;
    bind_entry.buffer = uniform_buffer_;
    bind_entry.size = sizeof(SolidUniforms);

    wgpu::BindGroupDescriptor bind_group_descriptor {};
    bind_group_descriptor.label = "Solid BindGroup";
    bind_group_descriptor.layout = bind_group_layout;
    bind_group_descriptor.entryCount = 1;
    bind_group_descriptor.entries = &bind_entry;
    bind_group_ = gpu_ref.getDevice().CreateBindGroup(&bind_group_descriptor);

    wgpu::PipelineLayoutDescriptor pipeline_layout_descriptor {};
    pipeline_layout_descriptor.bindGroupLayoutCount = 1;
    pipeline_layout_descriptor.bindGroupLayouts = &bind_group_layout;
    wgpu::PipelineLayout pipeline_layout = gpu_ref.getDevice().CreatePipelineLayout(&pipeline_layout_descriptor);

    wgpu::VertexAttribute attributes[2] {};
    attributes[0].format = wgpu::VertexFormat::Float32x3;
    attributes[0].offset = offsetof(Vertex, position);
    attributes[0].shaderLocation = 0;
    attributes[1].format = wgpu::VertexFormat::Float32x4;
    attributes[1].offset = offsetof(Vertex, color);
    attributes[1].shaderLocation = 1;

    wgpu::VertexBufferLayout vertex_layout {};
    vertex_layout.arrayStride = sizeof(Vertex);
    vertex_layout.stepMode = wgpu::VertexStepMode::Vertex;
    vertex_layout.attributeCount = 2;
    vertex_layout.attributes = attributes;

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

    // Translucent planes read depth but must not write it, so two overlapping
    // planes both stay visible where they cross.
    wgpu::DepthStencilState depthStencil {};
    depthStencil.format = depth_format;
    depthStencil.depthWriteEnabled = wgpu::OptionalBool::False;
    depthStencil.depthCompare = wgpu::CompareFunction::LessEqual;

    wgpu::RenderPipelineDescriptor pipeline_descriptor {};
    pipeline_descriptor.label = "Solid Pipeline";
    pipeline_descriptor.layout = pipeline_layout;
    pipeline_descriptor.vertex.module = shader;
    pipeline_descriptor.vertex.entryPoint = "vs_main";
    pipeline_descriptor.vertex.bufferCount = 1;
    pipeline_descriptor.vertex.buffers = &vertex_layout;
    pipeline_descriptor.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
    // Planes are viewed from both sides, so culling stays off.
    pipeline_descriptor.primitive.cullMode = wgpu::CullMode::None;
    pipeline_descriptor.depthStencil = &depthStencil;
    pipeline_descriptor.multisample.count = 1;
    pipeline_descriptor.multisample.mask = ~0u;
    pipeline_descriptor.fragment = &fragment;

    pipeline_ = gpu_ref.getDevice().CreateRenderPipeline(&pipeline_descriptor);
    if (!pipeline_) {
        out_error_ref = "Failed to create the solid render pipeline";
        return false;
    }
    return true;
}

void SolidRenderer::shutdownResources()
{
    pipeline_ = nullptr;
    bind_group_ = nullptr;
    uniform_buffer_ = nullptr;
    vertex_buffer_ = nullptr;
    vertex_capacity_ = 0;
    vertices_.clear();
}

void SolidRenderer::beginBatch()
{
    vertices_.clear();
}

void SolidRenderer::addTriangle(DeckMath::Vector3 a, DeckMath::Vector3 b, DeckMath::Vector3 c, DeckMath::Vector4 color)
{
    auto push = [&](DeckMath::Vector3 position) {
        Vertex vertex {};
        vertex.position[0] = position.x;
        vertex.position[1] = position.y;
        vertex.position[2] = position.z;
        vertex.color[0] = color.x;
        vertex.color[1] = color.y;
        vertex.color[2] = color.z;
        vertex.color[3] = color.w;
        vertices_.push_back(vertex);
    };
    push(a);
    push(b);
    push(c);
}

void SolidRenderer::addQuad(DeckMath::Vector3 a, DeckMath::Vector3 b, DeckMath::Vector3 c, DeckMath::Vector3 d, DeckMath::Vector4 color)
{
    addTriangle(a, b, c, color);
    addTriangle(a, c, d, color);
}

void SolidRenderer::addOriginPlane(OriginPlane plane, f32 half_size, DeckMath::Vector4 color)
{
    const f32 s = half_size;
    switch (plane) {
    case OriginPlane::XY:
        addQuad({ -s, -s, 0 }, { s, -s, 0 }, { s, s, 0 }, { -s, s, 0 }, color);
        break;
    case OriginPlane::XZ:
        addQuad({ -s, 0, -s }, { s, 0, -s }, { s, 0, s }, { -s, 0, s }, color);
        break;
    case OriginPlane::YZ:
        addQuad({ 0, -s, -s }, { 0, s, -s }, { 0, s, s }, { 0, -s, s }, color);
        break;
    }
}

void SolidRenderer::ensureVertexCapacity(const Gpu::Context& gpu_ref, size_t vertex_count)
{
    if (vertex_count <= vertex_capacity_ && vertex_buffer_) {
        return;
    }

    size_t capacity = std::max<size_t>(vertex_capacity_ * 2, 512);
    while (capacity < vertex_count) {
        capacity *= 2;
    }

    wgpu::BufferDescriptor descriptor {};
    descriptor.label = "Solid Vertices";
    descriptor.size = capacity * sizeof(Vertex);
    descriptor.usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst;
    vertex_buffer_ = gpu_ref.getDevice().CreateBuffer(&descriptor);
    vertex_capacity_ = capacity;
}

void SolidRenderer::flushBatch(const Gpu::Context& gpu_ref,
    const wgpu::RenderPassEncoder& pass_ref,
    const DeckMath::Matrix4& view_ref,
    const DeckMath::Matrix4& projection_ref)
{
    if (!pipeline_ || vertices_.empty()) {
        return;
    }

    ensureVertexCapacity(gpu_ref, vertices_.size());

    DeckMath::Matrix4 view_projection = projection_ref * view_ref;
    SolidUniforms uniforms {};
    std::memcpy(uniforms.view_projection, &view_projection.columns[0].x, sizeof(uniforms.view_projection));

    gpu_ref.getQueue().WriteBuffer(uniform_buffer_, 0, &uniforms, sizeof(uniforms));
    gpu_ref.getQueue().WriteBuffer(vertex_buffer_, 0, vertices_.data(), vertices_.size() * sizeof(Vertex));

    pass_ref.SetPipeline(pipeline_);
    pass_ref.SetBindGroup(0, bind_group_);
    pass_ref.SetVertexBuffer(0, vertex_buffer_, 0, vertices_.size() * sizeof(Vertex));
    pass_ref.Draw(static_cast<u32>(vertices_.size()), 1, 0, 0);
}

} // namespace Viewport
