#pragma once
#include "core/RenderBackend.h"

// Phase G2 — DirectX 11 backend skeleton.
// NOTE: This file declares the API surface; implementation is deferred and is
// expected to live on a separate branch. Add `Core/DX11Backend.cpp` to the
// build only after the implementation is complete (it is intentionally NOT in
// CMakeLists.txt yet so the main build stays clean).
//
// Recommended scaffolding when implementing:
//   - ID3D11Buffer dynamic with D3D11_USAGE_DYNAMIC + Map(DISCARD) for vertex
//     and index buffers
//   - HLSL shaders mirroring the GLSL ones (Basic.hlsl, Text.hlsl, MSDF.hlsl,
//     Image.hlsl) compiled via D3DCompile or fxc at build time
//   - ID3D11ShaderResourceView per texture handle
//   - ID3D11RasterizerState with scissor enabled, ID3D11BlendState premultiplied

namespace FluentUI {

class DX11Backend : public RenderBackend {
public:
    DX11Backend() = default;
    ~DX11Backend() override = default;

    bool Init(void* windowHandle, void* existingContext = nullptr) override;
    void Shutdown() override;
    void BeginFrame(const Color& clearColor) override;
    void EndFrame() override;
    void SetViewport(int width, int height) override;

    void PushClipRect(int x, int y, int width, int height) override;
    void PopClipRect() override;

    void* CreateTexture(int width, int height, const void* data, bool alphaOnly = false) override;
    void UpdateTexture(void* textureHandle, int x, int y, int width, int height, const void* data) override;
    void DeleteTexture(void* textureHandle) override;

    void DrawBatch(ShaderType type, const RenderVertex* vertices, size_t vertexCount,
                   const unsigned int* indices, size_t indexCount,
                   void* textureHandle, const float* projectionMatrix,
                   const Color& textColor = {1, 1, 1, 1}) override;
    void DrawLines(const RenderVertex* vertices, size_t vertexCount,
                   float width, const float* projectionMatrix) override;
};

} // namespace FluentUI
