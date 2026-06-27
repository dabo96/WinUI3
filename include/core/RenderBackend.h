#pragma once

#include "Math/Color.h"
#include "Math/Vec2.h"
#include <string>
#include <vector>
#include <cstdint>

namespace FluentUI {

enum class ShaderType {
    Basic,      // Simple quads/lines
    Text,       // Bitmap text (Red channel only)
    MSDF,       // Multi-channel Signed Distance Field text
    Image       // Textured quad (full RGBA sampling with tint)
};

/// Objects shared by an external Vulkan engine when FluentUI renders inside its
/// frame (the Vulkan analogue of passing an existing GL context). Pass a pointer
/// to a *fully populated* instance as the `existingContext` argument of
/// RenderBackend::Init to enable shared mode.
///
/// To run STANDALONE (FluentUI owns its instance/device/swapchain/render pass),
/// pass `existingContext == nullptr`. A non-null pointer with missing required
/// handles (device/physicalDevice/graphicsQueue/renderPass) is treated as an
/// error, NOT as a request for standalone — so don't pass a half-filled struct.
///
/// Handles are typed as void*/uint64_t so this public header never needs to pull
/// in <vulkan/vulkan.h>. The real Vulkan types are shown in the comments; the
/// VulkanBackend reinterpret_casts them internally.
struct VulkanSharedContext {
    void*    instance        = nullptr; // VkInstance
    void*    physicalDevice  = nullptr; // VkPhysicalDevice
    void*    device          = nullptr; // VkDevice
    void*    graphicsQueue   = nullptr; // VkQueue
    uint32_t queueFamilyIndex = 0;      // graphics+present family
    uint64_t renderPass      = 0;       // VkRenderPass the engine renders into (ignored if dynamicRendering)
    uint32_t colorFormat     = 0;       // VkFormat of the color attachment (REQUIRED for dynamic rendering)
    uint32_t sampleCount     = 1;       // VkSampleCountFlagBits of the render pass (1,2,4,8...)

    // Dynamic rendering (VK_KHR_dynamic_rendering / Vulkan 1.3). Set this when the
    // engine renders with vkCmdBeginRendering/vkCmdEndRendering instead of a
    // VkRenderPass. The UI pipelines are then built with VkPipelineRenderingCreateInfo
    // (renderPass = VK_NULL_HANDLE), so `renderPass` above may be 0. The engine's
    // device must have the dynamicRendering feature enabled. `colorFormat` is then
    // required; depth/stencil formats are only needed if the engine's rendering
    // info binds those attachments (UI itself never depth-tests).
    bool     dynamicRendering = false;
    uint32_t depthFormat     = 0;       // VkFormat of depth attachment (0 = none)
    uint32_t stencilFormat   = 0;       // VkFormat of stencil attachment (0 = none)
};

struct RenderVertex {
    float x, y;
    float r, g, b, a;
    float u = 0.0f, v = 0.0f; // Added UVs for general use
};

class RenderBackend {
public:
    virtual ~RenderBackend() = default;

    // --- Initialization and Frame ---
    // The meaning of `existingContext` depends on the backend:
    //   - OpenGL: an existing SDL_GLContext to reuse (null = create our own).
    //   - Vulkan: a VulkanSharedContext* describing the engine's device/render
    //             pass (null = standalone; creates its own device/swapchain).
    virtual bool Init(void* windowHandle, void* existingContext = nullptr) = 0;
    virtual void Shutdown() = 0;
    virtual void BeginFrame(const Color& clearColor) = 0;
    virtual void EndFrame() = 0;
    virtual void SetViewport(int width, int height) = 0;

    // --- External frame command buffer (Vulkan shared mode) ---
    // Supply the command buffer the engine is currently recording into. The
    // backend records its UI draws onto it (no submit/present). Must be called
    // each frame before BeginFrame while inside the engine's active render pass.
    // No-op on backends that don't need it (e.g. OpenGL).
    //   cmdBuffer: VkCommandBuffer (already in the recording state).
    virtual void SetFrameCommandBuffer(void* cmdBuffer) { (void)cmdBuffer; }

    // --- State Management ---
    virtual void PushClipRect(int x, int y, int width, int height) = 0;
    virtual void PopClipRect() = 0;
    
    // --- Textures ---
    virtual void* CreateTexture(int width, int height, const void* data, bool alphaOnly = false) = 0;
    virtual void UpdateTexture(void* textureHandle, int x, int y, int width, int height, const void* data) = 0;
    virtual void DeleteTexture(void* textureHandle) = 0;

    // Wrap a texture the host already owns (e.g. an engine-rendered viewport image)
    // so it can be drawn with Image()/DrawImage(). Returns an opaque handle for
    // those APIs, or nullptr if unsupported. The backend does NOT take ownership of
    // the underlying image/memory — pass the returned handle to DeleteTexture() to
    // release only the wrapper (descriptor). The host must keep the image alive and
    // in the sampled layout while the handle is in use.
    //   Vulkan: nativeView = VkImageView; sampler = VkSampler (null → backend's
    //           default linear sampler); layout = VkImageLayout the image is in when
    //           sampled (0 → VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL).
    virtual void* RegisterExternalTexture(void* nativeView, void* sampler = nullptr, int layout = 0) {
        (void)nativeView; (void)sampler; (void)layout; return nullptr;
    }

    // --- Drawing ---
    // vertices: pointer to array of RenderVertex
    // indices: pointer to array of unsigned int
    // textureHandle: pointer returned by CreateTexture (can be null for Basic shader)
    virtual void DrawBatch(ShaderType type, const RenderVertex* vertices, size_t vertexCount, 
                           const unsigned int* indices, size_t indexCount, 
                           void* textureHandle, const float* projectionMatrix,
                           const Color& textColor = {1,1,1,1}) = 0;
    
    // Special case for lines because of line width
    virtual void DrawLines(const RenderVertex* vertices, size_t vertexCount,
                           float width, const float* projectionMatrix) = 0;

    // --- Render Targets / FBO (Phase 5) ---
    // Returns an opaque handle to the render target
    virtual void* CreateRenderTarget(int width, int height) { return nullptr; }
    // Set active render target (nullptr = default framebuffer)
    virtual void SetRenderTarget(void* target) {}
    // Get the color texture of a render target (for drawing as image)
    virtual void* GetRenderTargetTexture(void* target) { return nullptr; }
    // Delete a render target
    virtual void DeleteRenderTarget(void* target) {}

    // --- Texture Copy (Perf R3) ---
    // Copy src texture content to dst texture (for atlas growth without regeneration)
    virtual void CopyTexture(void* src, void* dst, int width, int height) {}

    // --- GL State Save/Restore (Phase 5) ---
    // Save the current graphics state (blend, scissor, viewport, programs, textures)
    virtual void SaveState() {}
    // Restore the previously saved state
    virtual void RestoreState() {}

    /// Phase C6: read a single pixel from the default framebuffer (eyedropper).
    /// Returns {0,0,0,0} if unsupported. Coordinates are in framebuffer pixels
    /// (origin top-left).
    virtual Color ReadPixel(int x, int y) { (void)x; (void)y; return Color(0, 0, 0, 0); }
};

} // namespace FluentUI
