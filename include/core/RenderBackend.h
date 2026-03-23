#pragma once

#include "Math/Color.h"
#include "Math/Vec2.h"
#include <string>
#include <vector>

namespace FluentUI {

enum class ShaderType {
    Basic,      // Simple quads/lines
    Text,       // Bitmap text (Red channel only)
    MSDF,       // Multi-channel Signed Distance Field text
    Image       // Textured quad (full RGBA sampling with tint)
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
    virtual bool Init(void* windowHandle) = 0;
    virtual void Shutdown() = 0;
    virtual void BeginFrame(const Color& clearColor) = 0;
    virtual void EndFrame() = 0;
    virtual void SetViewport(int width, int height) = 0;

    // --- State Management ---
    virtual void PushClipRect(int x, int y, int width, int height) = 0;
    virtual void PopClipRect() = 0;
    
    // --- Textures ---
    virtual void* CreateTexture(int width, int height, const void* data, bool alphaOnly = false) = 0;
    virtual void UpdateTexture(void* textureHandle, int x, int y, int width, int height, const void* data) = 0;
    virtual void DeleteTexture(void* textureHandle) = 0;

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
};

} // namespace FluentUI
