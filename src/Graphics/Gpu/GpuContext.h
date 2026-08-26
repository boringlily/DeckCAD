#pragma once
#include "Types.h"
#include <dawn/webgpu_cpp.h>
#include <string>

namespace Gpu {

/// Everything needed to attach WebGPU to an already-created OS window.
struct ContextDescriptor {
    const char* native_system_ptr { nullptr }; // "cocoa" | "win32" | "wayland" | "x11"
    void* native_window_ptr { nullptr };
    void* native_display_ptr { nullptr };
    void* native_surface_ptr { nullptr };
    u32 width { 0 };
    u32 height { 0 };
    bool vsync { true };
};

/// Owns the WebGPU instance, device, queue and swap surface.
///
/// Deliberately the *only* owner of GPU state in the app. Core logic never
/// holds a device or a pipeline, which is what keeps the door open to moving
/// Core back behind a hot-reload boundary later.
class Context {
public:
    Context() = default;
    ~Context();

    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;

    bool initializeResources(const ContextDescriptor& descriptor_ref);
    void shutdownResources();

    /// Reconfigures the surface. Cheap to call with an unchanged size.
    void resizeTarget(u32 width, u32 height);

    /// Acquires the next backbuffer and opens a command encoder.
    /// Returns false when the frame should be skipped (e.g. surface lost while
    /// resizing); the caller must not call endFrame() in that case.
    bool beginFrame();

    /// Submits the frame's commands and presents.
    void endFrame();

    /// Drives Dawn's callback queue. Call once per frame.
    void tickDevice();

    const wgpu::Instance& getInstance() const { return instance_; }
    const wgpu::Device& getDevice() const { return device_; }
    const wgpu::Queue& getQueue() const { return queue_; }
    wgpu::TextureFormat getSurfaceFormat() const { return surface_format_; }

    /// Valid only between beginFrame() and endFrame().
    const wgpu::TextureView& getBackbufferView() const { return backbuffer_view_; }
    const wgpu::CommandEncoder& getEncoder() const { return encoder_; }

    u32 getWidth() const { return width_; }
    u32 getHeight() const { return height_; }

    /// Compiles a WGSL source string into a shader module.
    /// Returns a null module on failure; the message goes to the error callback.
    wgpu::ShaderModule createShaderModule(const char* label_ptr, const std::string& wgsl_ref) const;

    const std::string& getLastError() const { return last_error_; }

private:
    bool createSurface(const ContextDescriptor& descriptor_ref);
    bool requestAdapterAndDevice();
    void configureSurface();

    wgpu::Instance instance_;
    wgpu::Adapter adapter_;
    wgpu::Device device_;
    wgpu::Queue queue_;
    wgpu::Surface surface_;

    wgpu::TextureFormat surface_format_ { wgpu::TextureFormat::BGRA8Unorm };
    wgpu::PresentMode present_mode_ { wgpu::PresentMode::Fifo };

    // Per-frame, reset in endFrame().
    wgpu::Texture backbuffer_;
    wgpu::TextureView backbuffer_view_;
    wgpu::CommandEncoder encoder_;

    u32 width_ { 0 };
    u32 height_ { 0 };
    bool surface_configured_ { false };
    std::string last_error_;
};

} // namespace Gpu
