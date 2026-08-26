#include "GpuContext.h"

#include <cstdio>
#include <backends/imgui_impl_wgpu.h>

namespace Gpu {
namespace GpuContextInternal {

    const char* ErrorTypeName(wgpu::ErrorType type)
    {
        switch (type) {
        case wgpu::ErrorType::Validation:
            return "Validation";
        case wgpu::ErrorType::OutOfMemory:
            return "OutOfMemory";
        case wgpu::ErrorType::Internal:
            return "Internal";
        case wgpu::ErrorType::Unknown:
            return "Unknown";
        default:
            return "?";
        }
    }

} // namespace GpuContextInternal
using namespace GpuContextInternal;

Context::~Context()
{
    shutdownResources();
}

bool Context::initializeResources(const ContextDescriptor& descriptor_ref)
{
    width_ = descriptor_ref.width;
    height_ = descriptor_ref.height;
    present_mode_ = descriptor_ref.vsync ? wgpu::PresentMode::Fifo : wgpu::PresentMode::Mailbox;

    wgpu::InstanceDescriptor instance_descriptor {};
    instance_ = wgpu::CreateInstance(&instance_descriptor);
    if (!instance_) {
        last_error_ = "wgpu::CreateInstance returned null";
        return false;
    }

    // Order matters: the adapter is picked to be compatible with the surface we
    // are actually going to render into, so the surface must exist first.
    if (!createSurface(descriptor_ref)) {
        return false;
    }
    if (!requestAdapterAndDevice()) {
        return false;
    }

    queue_ = device_.GetQueue();

    wgpu::SurfaceCapabilities caps {};
    if (surface_.GetCapabilities(adapter_, &caps) == wgpu::Status::Success && caps.formatCount > 0) {
        surface_format_ = caps.formats[0];
    }

    configureSurface();
    return true;
}

bool Context::createSurface(const ContextDescriptor& descriptor_ref)
{
    // Dear ImGui's WebGPU backend already carries the per-platform chained
    // struct plumbing (Metal layer / HWND / xlib / wayland), so we reuse it
    // rather than maintaining an Objective-C++ file of our own.
    ImGui_ImplWGPU_CreateSurfaceInfo surface_information {};
    surface_information.Instance = instance_.Get();
    surface_information.System = descriptor_ref.native_system_ptr;
    surface_information.RawWindow = descriptor_ref.native_window_ptr;
    surface_information.RawDisplay = descriptor_ref.native_display_ptr;
    surface_information.RawSurface = descriptor_ref.native_surface_ptr;

    WGPUSurface raw = ImGui_ImplWGPU_CreateWGPUSurfaceHelper(&surface_information);
    if (!raw) {
        last_error_ = "Failed to create a WebGPU surface for this window";
        return false;
    }
    surface_ = wgpu::Surface::Acquire(raw);
    return true;
}

bool Context::requestAdapterAndDevice()
{
    wgpu::RequestAdapterOptions adapter_options {};
    adapter_options.compatibleSurface = surface_;
    adapter_options.powerPreference = wgpu::PowerPreference::HighPerformance;

    bool adapter_done = false;
    // AllowProcessEvents keeps this portable: no instance features required,
    // we just pump until the callback lands.
    instance_.RequestAdapter(
        &adapter_options, wgpu::CallbackMode::AllowProcessEvents,
        [&](wgpu::RequestAdapterStatus status, wgpu::Adapter adapter, wgpu::StringView message) {
            if (status == wgpu::RequestAdapterStatus::Success) {
                adapter_ = std::move(adapter);
            } else {
                last_error_ = "RequestAdapter failed: " + std::string(std::string_view(message));
            }
            adapter_done = true;
        });

    while (!adapter_done) {
        instance_.ProcessEvents();
    }
    if (!adapter_) {
        return false;
    }

    wgpu::DeviceDescriptor device_descriptor {};
    device_descriptor.label = "DeckCAD Device";
    device_descriptor.SetUncapturedErrorCallback(
        [](const wgpu::Device&, wgpu::ErrorType type, wgpu::StringView message) {
            std::fprintf(stderr, "[WebGPU %s] %.*s\n", ErrorTypeName(type),
                static_cast<int>(std::string_view(message).size()), std::string_view(message).data());
        });
    device_descriptor.SetDeviceLostCallback(
        wgpu::CallbackMode::AllowProcessEvents,
        [](const wgpu::Device&, wgpu::DeviceLostReason reason, wgpu::StringView message) {
            // Destroyed is the normal path during shutdown, so it is not an error.
            if (reason == wgpu::DeviceLostReason::Destroyed) {
                return;
            }
            std::fprintf(stderr, "[WebGPU device lost] %.*s\n",
                static_cast<int>(std::string_view(message).size()), std::string_view(message).data());
        });

    bool device_done = false;
    adapter_.RequestDevice(
        &device_descriptor, wgpu::CallbackMode::AllowProcessEvents,
        [&](wgpu::RequestDeviceStatus status, wgpu::Device device, wgpu::StringView message) {
            if (status == wgpu::RequestDeviceStatus::Success) {
                device_ = std::move(device);
            } else {
                last_error_ = "RequestDevice failed: " + std::string(std::string_view(message));
            }
            device_done = true;
        });

    while (!device_done) {
        instance_.ProcessEvents();
    }
    return static_cast<bool>(device_);
}

void Context::configureSurface()
{
    if (width_ == 0 || height_ == 0) {
        surface_configured_ = false;
        return;
    }

    wgpu::SurfaceConfiguration configuration {};
    configuration.device = device_;
    configuration.format = surface_format_;
    configuration.usage = wgpu::TextureUsage::RenderAttachment;
    configuration.width = width_;
    configuration.height = height_;
    configuration.alphaMode = wgpu::CompositeAlphaMode::Auto;
    configuration.presentMode = present_mode_;

    surface_.Configure(&configuration);
    surface_configured_ = true;
}

void Context::resizeTarget(u32 width, u32 height)
{
    if (width == width_ && height == height_) {
        return;
    }
    width_ = width;
    height_ = height;
    configureSurface();
}

bool Context::beginFrame()
{
    if (!surface_configured_) {
        return false;
    }

    wgpu::SurfaceTexture surface_texture {};
    surface_.GetCurrentTexture(&surface_texture);

    if (ImGui_ImplWGPU_IsSurfaceStatusError(static_cast<WGPUSurfaceGetCurrentTextureStatus>(surface_texture.status))) {
        // Typically the surface went stale behind a resize. Reconfigure and let
        // the caller skip this frame; the next one will pick up the new size.
        configureSurface();
        return false;
    }
    if (!surface_texture.texture) {
        return false;
    }

    backbuffer_ = surface_texture.texture;

    wgpu::TextureViewDescriptor view_descriptor {};
    view_descriptor.label = "Backbuffer";
    view_descriptor.format = surface_format_;
    view_descriptor.dimension = wgpu::TextureViewDimension::e2D;
    view_descriptor.mipLevelCount = 1;
    view_descriptor.arrayLayerCount = 1;
    backbuffer_view_ = backbuffer_.CreateView(&view_descriptor);

    wgpu::CommandEncoderDescriptor encoder_descriptor {};
    encoder_descriptor.label = "Frame Encoder";
    encoder_ = device_.CreateCommandEncoder(&encoder_descriptor);
    return true;
}

void Context::endFrame()
{
    wgpu::CommandBufferDescriptor command_descriptor {};
    command_descriptor.label = "Frame Commands";
    wgpu::CommandBuffer commands = encoder_.Finish(&command_descriptor);
    queue_.Submit(1, &commands);

    surface_.Present();

    encoder_ = nullptr;
    backbuffer_view_ = nullptr;
    backbuffer_ = nullptr;
}

void Context::tickDevice()
{
    if (instance_) {
        instance_.ProcessEvents();
    }
}

wgpu::ShaderModule Context::createShaderModule(const char* label_ptr, const std::string& wgsl_ref) const
{
    wgpu::ShaderSourceWGSL wgsl_descriptor {};
    wgsl_descriptor.code = wgsl_ref.c_str();

    wgpu::ShaderModuleDescriptor descriptor_ref {};
    descriptor_ref.nextInChain = &wgsl_descriptor;
    descriptor_ref.label = label_ptr;

    return device_.CreateShaderModule(&descriptor_ref);
}

void Context::shutdownResources()
{
    encoder_ = nullptr;
    backbuffer_view_ = nullptr;
    backbuffer_ = nullptr;

    if (surface_ && surface_configured_) {
        surface_.Unconfigure();
        surface_configured_ = false;
    }
    surface_ = nullptr;
    queue_ = nullptr;
    device_ = nullptr;
    adapter_ = nullptr;
    instance_ = nullptr;
}

} // namespace Gpu
