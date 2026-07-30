#include "TextRenderer.h"
#include "Assets.h"

#include <algorithm>
#include <cstddef>
#include <cstring>

namespace Text
{
namespace TextRendererInternal
{

    struct TextUniforms
    {
        f32 view_projection[16];
        f32 viewport_size[4]; // xy = pixels, z = pixel range
    };

} // namespace TextRendererInternal
using namespace TextRendererInternal;

bool TextRenderer::initializeResources(const Gpu::Context& gpu_ref,
    const MsdfAtlas& atlas_ref,
    wgpu::TextureFormat color_format,
    wgpu::TextureFormat depth_format,
    std::string& out_error_ref)
{
    if(!atlas_ref.isValid())
    {
        out_error_ref = "Text atlas is empty";
        return false;
    }
    atlas_ptr_ = &atlas_ref;

    std::string wgsl;
    if(!Platform::Assets::ReadTextFile(Platform::Assets::Resolve("shaders/msdf_text.wgsl"), wgsl, out_error_ref))
    {
        return false;
    }
    wgpu::ShaderModule shader = gpu_ref.createShaderModule("msdf_text", wgsl);
    if(!shader)
    {
        out_error_ref = "Failed to compile shaders/msdf_text.wgsl";
        return false;
    }

    // --- Atlas texture ------------------------------------------------------
    wgpu::TextureDescriptor texture_descriptor {};
    texture_descriptor.label = "MSDF Atlas";
    texture_descriptor.dimension = wgpu::TextureDimension::e2D;
    texture_descriptor.size = { atlas_ref.getWidth(), atlas_ref.getHeight(), 1 };
    // deliberately linear, not Srgb: channels hold distances, not colour;
    // gamma conversion would corrupt the field
    texture_descriptor.format = wgpu::TextureFormat::RGBA8Unorm;
    texture_descriptor.mipLevelCount = 1;
    texture_descriptor.sampleCount = 1;
    texture_descriptor.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;

    atlas_texture_ = gpu_ref.getDevice().CreateTexture(&texture_descriptor);
    if(!atlas_texture_)
    {
        out_error_ref = "Failed to create the MSDF atlas texture";
        return false;
    }
    atlas_view_ = atlas_texture_.CreateView();

    wgpu::TexelCopyTextureInfo destination {};
    destination.texture = atlas_texture_;
    destination.mipLevel = 0;
    destination.origin = { 0, 0, 0 };
    destination.aspect = wgpu::TextureAspect::All;

    wgpu::TexelCopyBufferLayout layout {};
    layout.offset = 0;
    layout.bytesPerRow = atlas_ref.getWidth() * 4;
    layout.rowsPerImage = atlas_ref.getHeight();

    wgpu::Extent3D write_size { atlas_ref.getWidth(), atlas_ref.getHeight(), 1 };
    gpu_ref.getQueue().WriteTexture(&destination, atlas_ref.getPixels().data(), atlas_ref.getPixels().size(), &layout, &write_size);

    // bilinear, clamped: shader reconstructs sharp edges from the field;
    // smooth interpolation between texels is what it needs
    wgpu::SamplerDescriptor sampler_descriptor {};
    sampler_descriptor.label = "MSDF Sampler";
    sampler_descriptor.addressModeU = wgpu::AddressMode::ClampToEdge;
    sampler_descriptor.addressModeV = wgpu::AddressMode::ClampToEdge;
    sampler_descriptor.addressModeW = wgpu::AddressMode::ClampToEdge;
    sampler_descriptor.magFilter = wgpu::FilterMode::Linear;
    sampler_descriptor.minFilter = wgpu::FilterMode::Linear;
    sampler_descriptor.mipmapFilter = wgpu::MipmapFilterMode::Linear;
    sampler_descriptor.maxAnisotropy = 1;
    sampler_ = gpu_ref.getDevice().CreateSampler(&sampler_descriptor);

    // --- Uniforms and bindings ---------------------------------------------
    wgpu::BufferDescriptor uniform_descriptor {};
    uniform_descriptor.label = "Text Uniforms";
    uniform_descriptor.size = sizeof(TextUniforms);
    uniform_descriptor.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
    uniform_buffer_ = gpu_ref.getDevice().CreateBuffer(&uniform_descriptor);

    wgpu::BindGroupLayoutEntry layout_entries[3] {};
    layout_entries[0].binding = 0;
    layout_entries[0].visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
    layout_entries[0].buffer.type = wgpu::BufferBindingType::Uniform;
    layout_entries[0].buffer.minBindingSize = sizeof(TextUniforms);

    layout_entries[1].binding = 1;
    layout_entries[1].visibility = wgpu::ShaderStage::Fragment;
    layout_entries[1].texture.sampleType = wgpu::TextureSampleType::Float;
    layout_entries[1].texture.viewDimension = wgpu::TextureViewDimension::e2D;

    layout_entries[2].binding = 2;
    layout_entries[2].visibility = wgpu::ShaderStage::Fragment;
    layout_entries[2].sampler.type = wgpu::SamplerBindingType::Filtering;

    wgpu::BindGroupLayoutDescriptor bind_group_layout_descriptor {};
    bind_group_layout_descriptor.label = "Text BindGroupLayout";
    bind_group_layout_descriptor.entryCount = 3;
    bind_group_layout_descriptor.entries = layout_entries;
    wgpu::BindGroupLayout bind_group_layout = gpu_ref.getDevice().CreateBindGroupLayout(&bind_group_layout_descriptor);

    wgpu::BindGroupEntry bind_entries[3] {};
    bind_entries[0].binding = 0;
    bind_entries[0].buffer = uniform_buffer_;
    bind_entries[0].size = sizeof(TextUniforms);
    bind_entries[1].binding = 1;
    bind_entries[1].textureView = atlas_view_;
    bind_entries[2].binding = 2;
    bind_entries[2].sampler = sampler_;

    wgpu::BindGroupDescriptor bind_group_descriptor {};
    bind_group_descriptor.label = "Text BindGroup";
    bind_group_descriptor.layout = bind_group_layout;
    bind_group_descriptor.entryCount = 3;
    bind_group_descriptor.entries = bind_entries;
    bind_group_ = gpu_ref.getDevice().CreateBindGroup(&bind_group_descriptor);

    // --- Pipeline -----------------------------------------------------------
    wgpu::PipelineLayoutDescriptor pipeline_layout_descriptor {};
    pipeline_layout_descriptor.bindGroupLayoutCount = 1;
    pipeline_layout_descriptor.bindGroupLayouts = &bind_group_layout;
    wgpu::PipelineLayout pipeline_layout = gpu_ref.getDevice().CreatePipelineLayout(&pipeline_layout_descriptor);

    wgpu::VertexAttribute attributes[4] {};
    attributes[0].format = wgpu::VertexFormat::Float32x3;
    attributes[0].offset = offsetof(Vertex, anchor);
    attributes[0].shaderLocation = 0;
    attributes[1].format = wgpu::VertexFormat::Float32x2;
    attributes[1].offset = offsetof(Vertex, offset);
    attributes[1].shaderLocation = 1;
    attributes[2].format = wgpu::VertexFormat::Float32x2;
    attributes[2].offset = offsetof(Vertex, uv);
    attributes[2].shaderLocation = 2;
    attributes[3].format = wgpu::VertexFormat::Float32x4;
    attributes[3].offset = offsetof(Vertex, color);
    attributes[3].shaderLocation = 3;

    wgpu::VertexBufferLayout vertex_layout {};
    vertex_layout.arrayStride = sizeof(Vertex);
    vertex_layout.stepMode = wgpu::VertexStepMode::Vertex;
    vertex_layout.attributeCount = 4;
    vertex_layout.attributes = attributes;

    // Premultiplied alpha, matching what the fragment shader emits.
    wgpu::BlendState blend {};
    blend.color.operation = wgpu::BlendOperation::Add;
    blend.color.srcFactor = wgpu::BlendFactor::One;
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

    // depth-tested against the scene, letting geometry occlude labels; not
    // depth-written, or overlapping translucent glyphs would clip each other
    wgpu::DepthStencilState depth_stencil {};
    depth_stencil.format = depth_format;
    depth_stencil.depthWriteEnabled = wgpu::OptionalBool::False;
    depth_stencil.depthCompare = wgpu::CompareFunction::LessEqual;

    wgpu::RenderPipelineDescriptor pipeline_descriptor {};
    pipeline_descriptor.label = "Text Pipeline";
    pipeline_descriptor.layout = pipeline_layout;
    pipeline_descriptor.vertex.module = shader;
    pipeline_descriptor.vertex.entryPoint = "vs_main";
    pipeline_descriptor.vertex.bufferCount = 1;
    pipeline_descriptor.vertex.buffers = &vertex_layout;
    pipeline_descriptor.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
    pipeline_descriptor.primitive.cullMode = wgpu::CullMode::None;
    pipeline_descriptor.depthStencil = &depth_stencil;
    pipeline_descriptor.multisample.count = 1;
    pipeline_descriptor.multisample.mask = ~0u;
    pipeline_descriptor.fragment = &fragment;

    pipeline_ = gpu_ref.getDevice().CreateRenderPipeline(&pipeline_descriptor);
    if(!pipeline_)
    {
        out_error_ref = "Failed to create the text render pipeline";
        return false;
    }
    return true;
}

void TextRenderer::shutdownResources()
{
    pipeline_ = nullptr;
    bind_group_ = nullptr;
    uniform_buffer_ = nullptr;
    vertex_buffer_ = nullptr;
    atlas_view_ = nullptr;
    atlas_texture_ = nullptr;
    sampler_ = nullptr;
    vertex_capacity_ = 0;
    vertices_.clear();
    atlas_ptr_ = nullptr;
}

void TextRenderer::beginBatch()
{
    vertices_.clear();
}

void TextRenderer::addLabel(const std::string& text_ref,
    DcadMath::Vector3 world_position,
    f32 pixel_size,
    DcadMath::Vector4 color,
    AlignHorizontal align_horizontal,
    AlignVertical align_vertical)
{
    if(!atlas_ptr_ || text_ref.empty())
    {
        return;
    }

    const AtlasMetrics& metrics = atlas_ptr_->getMetrics();

    // Horizontal pivot, in pixels.
    f32 origin_x = 0.0f;
    if(align_horizontal != AlignHorizontal::Left)
    {
        f32 width = atlas_ptr_->measureWidth(text_ref) * pixel_size;
        origin_x = (align_horizontal == AlignHorizontal::Center) ? -width * 0.5f : -width;
    }

    // Vertical pivot, in pixels. Screen Y grows downward, font Y grows
    // upward: a shift "up" on screen is a negative offset.
    f32 origin_y = 0.0f;
    switch(align_vertical)
    {
    case AlignVertical::Baseline:
        origin_y = 0.0f;
        break;
    case AlignVertical::Bottom:
        origin_y = metrics.descender * pixel_size;
        break;
    case AlignVertical::Middle:
        origin_y = (metrics.ascender + metrics.descender) * 0.5f * pixel_size;
        break;
    case AlignVertical::Top:
        origin_y = metrics.ascender * pixel_size;
        break;
    }

    const f32 anchor[3] = { world_position.x, world_position.y, world_position.z };
    const f32 rgba[4] = { color.x, color.y, color.z, color.w };

    f32 pen_x = origin_x;
    u32 previous = 0;

    for(unsigned char character : text_ref)
    {
        const Glyph* glyph_ptr = atlas_ptr_->findGlyph(character);
        if(!glyph_ptr)
        {
            continue;
        }
        if(previous != 0)
        {
            pen_x += atlas_ptr_->getKerning(previous, character) * pixel_size;
        }
        previous = character;

        if(glyph_ptr->has_geometry)
        {
            // Font space is Y-up; negate to land in Y-down screen offsets.
            const f32 left = pen_x + glyph_ptr->plane_left * pixel_size;
            const f32 right = pen_x + glyph_ptr->plane_right * pixel_size;
            const f32 top = origin_y - glyph_ptr->plane_top * pixel_size;
            const f32 bottom = origin_y - glyph_ptr->plane_bottom * pixel_size;

            const f32 uv_left = glyph_ptr->uv_left;
            const f32 uv_right = glyph_ptr->uv_right;
            const f32 uv_top = glyph_ptr->uv_top;
            const f32 uv_bottom = glyph_ptr->uv_bottom;

            auto push = [&](f32 x, f32 y, f32 u, f32 v)
            {
                Vertex vertex {};
                std::memcpy(vertex.anchor, anchor, sizeof(anchor));
                vertex.offset[0] = x;
                vertex.offset[1] = y;
                vertex.uv[0] = u;
                vertex.uv[1] = v;
                std::memcpy(vertex.color, rgba, sizeof(rgba));
                vertices_.push_back(vertex);
            };

            push(left, top, uv_left, uv_top);
            push(left, bottom, uv_left, uv_bottom);
            push(right, bottom, uv_right, uv_bottom);

            push(left, top, uv_left, uv_top);
            push(right, bottom, uv_right, uv_bottom);
            push(right, top, uv_right, uv_top);
        }

        pen_x += glyph_ptr->advance * pixel_size;
    }
}

void TextRenderer::ensureVertexCapacity(const Gpu::Context& gpu_ref, size_t vertex_count)
{
    if(vertex_count <= vertex_capacity_ && vertex_buffer_)
    {
        return;
    }

    // grows geometrically: a viewport that gains labels frame by frame does
    // not reallocate every frame
    size_t capacity = std::max<size_t>(vertex_capacity_ * 2, 1024);
    while(capacity < vertex_count)
    {
        capacity *= 2;
    }

    wgpu::BufferDescriptor descriptor {};
    descriptor.label = "Text Vertices";
    descriptor.size = capacity * sizeof(Vertex);
    descriptor.usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst;
    vertex_buffer_ = gpu_ref.getDevice().CreateBuffer(&descriptor);
    vertex_capacity_ = capacity;
}

void TextRenderer::flushBatch(const Gpu::Context& gpu_ref,
    const wgpu::RenderPassEncoder& pass_ref,
    const DcadMath::Matrix4& view_projection_ref,
    f32 viewport_width,
    f32 viewport_height)
{
    if(!pipeline_ || vertices_.empty())
    {
        return;
    }

    ensureVertexCapacity(gpu_ref, vertices_.size());

    TextUniforms uniforms {};
    std::memcpy(uniforms.view_projection, &view_projection_ref.columns[0].x, sizeof(uniforms.view_projection));
    uniforms.viewport_size[0] = viewport_width;
    uniforms.viewport_size[1] = viewport_height;
    uniforms.viewport_size[2] = atlas_ptr_ ? atlas_ptr_->getMetrics().pixel_range : 4.0f;
    uniforms.viewport_size[3] = 0.0f;

    gpu_ref.getQueue().WriteBuffer(uniform_buffer_, 0, &uniforms, sizeof(uniforms));
    gpu_ref.getQueue().WriteBuffer(vertex_buffer_, 0, vertices_.data(), vertices_.size() * sizeof(Vertex));

    pass_ref.SetPipeline(pipeline_);
    pass_ref.SetBindGroup(0, bind_group_);
    pass_ref.SetVertexBuffer(0, vertex_buffer_, 0, vertices_.size() * sizeof(Vertex));
    pass_ref.Draw(static_cast<u32>(vertices_.size()), 1, 0, 0);
}

} // namespace Text
