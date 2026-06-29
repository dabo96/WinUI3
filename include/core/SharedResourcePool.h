#pragma once
#include <vector>
#include <cstdint>

namespace FluentUI {

class FontMSDF;
class Renderer;

// Per-device / per-shared-GL-context resource pool (brief 08, Part C).
//
// In a multi-window setup every secondary window shares the SAME GPU device
// (Vulkan) or the SAME GL context (OpenGL) as the main "device-owner" context.
// Because of that, GPU objects created by the owner — font atlases, the MSDF
// atlas, render targets and engine-registered textures — are valid handles in
// every window and must NOT be duplicated.
//
// The pool is created once next to the device-owner (the context made with
// CreateContext) and injected by pointer into every Renderer created for a
// secondary window via CreateStandaloneContext(window, shareFrom). It is owned
// by the device-owner and freed only when the main context is destroyed;
// DestroyStandaloneContext never frees it.
struct SharedResourcePool {
    // The Renderer that physically created the device-side resources (the
    // device-owner). Secondary Renderers reference its handles instead of
    // allocating their own.
    Renderer* originRenderer = nullptr;

    // Shared font atlas handles, published by the origin Renderer after its font
    // subsystem is initialized. Opaque GPU handles (GLuint-wrapped pointer in GL,
    // VulkanTexture* in Vulkan) — valid in every window of the shared device.
    void* fontAtlasTexture = nullptr;          // bitmap (R8) glyph atlas
    void* dynamicMSDFAtlasTexture = nullptr;   // runtime MSDF atlas
    FontMSDF* msdf = nullptr;                   // static MSDF atlas (read-only after load)

    // Registry of cross-window shared textures / render targets (e.g. an engine
    // viewport registered via RegisterExternalTexture, or a brief-05 render
    // target). Drawable as Image() in ANY window because they live on the shared
    // device/context. Stored only for bookkeeping/inspection; ownership stays
    // with whoever created them.
    std::vector<void*> sharedTextures;

    // Number of live contexts referencing this pool (the owner + every secondary
    // window). The owner frees the pool when this drops back to its own usage.
    int refCount = 0;

    void RegisterSharedTexture(void* tex) {
        if (!tex) return;
        for (void* t : sharedTextures) if (t == tex) return;
        sharedTextures.push_back(tex);
    }
    void UnregisterSharedTexture(void* tex) {
        for (size_t i = 0; i < sharedTextures.size(); ++i) {
            if (sharedTextures[i] == tex) {
                sharedTextures[i] = sharedTextures.back();
                sharedTextures.pop_back();
                return;
            }
        }
    }
};

} // namespace FluentUI
