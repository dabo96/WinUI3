#include "core/Renderer.h"
#include "core/Context.h"
#include "core/SharedResourcePool.h"
#include "core/Elevation.h"
#include "Theme/Style.h"
#include <SDL3/SDL.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <filesystem>
#include <system_error>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <cmath>
#include <unordered_set>

#include <ft2build.h>
#include FT_FREETYPE_H

#define STB_IMAGE_IMPLEMENTATION
#include "../external/stb_image.h"

namespace FluentUI {
    bool PathExists(const std::filesystem::path& path)
    {
        std::error_code ec;
        return std::filesystem::exists(path, ec);
    }

    void PushUniquePath(std::vector<std::filesystem::path>& paths, const std::filesystem::path& candidate)
    {
        if (candidate.empty()) return;
        if (std::find(paths.begin(), paths.end(), candidate) == paths.end())
        {
            paths.push_back(candidate);
        }
    }

    std::vector<std::filesystem::path> BuildResourceRoots()
    {
        std::vector<std::filesystem::path> roots;
        try { PushUniquePath(roots, std::filesystem::current_path()); } catch (...) {}
        if (const char* base = SDL_GetBasePath())
        {
            std::filesystem::path basePath(base);
            auto current = basePath;
            while (!current.empty())
            {
                PushUniquePath(roots, current);
                auto parent = current.parent_path();
                if (parent == current) break;
                current = parent;
            }
        }
        return roots;
    }

    std::filesystem::path ResolveResourcePath(const std::string& resource)
    {
        std::filesystem::path candidate(resource);
        if (candidate.is_absolute()) return PathExists(candidate) ? candidate : std::filesystem::path();
        static const std::vector<std::filesystem::path> searchRoots = BuildResourceRoots();
        for (const auto& root : searchRoots)
        {
            auto full = (root / candidate).lexically_normal();
            if (PathExists(full)) return full;
        }
        return {};
    }

    // Issue 8: DecodeUTF8 kept here (also declared in WidgetHelpers.h for shared use)
    std::uint32_t DecodeUTF8(const char*& ptr, const char* end)
    {
        if (ptr >= end) return 0;
        unsigned char first = static_cast<unsigned char>(*ptr++);
        if (first < 0x80) return first;
        else if ((first >> 5) == 0x6) {
            if (ptr >= end) return 0xFFFD;
            unsigned char second = static_cast<unsigned char>(*ptr++);
            if ((second & 0xC0) != 0x80) return 0xFFFD;
            return ((first & 0x1F) << 6) | (second & 0x3F);
        }
        else if ((first >> 4) == 0xE) {
            if (ptr + 1 >= end) return 0xFFFD;
            unsigned char second = static_cast<unsigned char>(*ptr++);
            unsigned char third = static_cast<unsigned char>(*ptr++);
            if ((second & 0xC0) != 0x80 || (third & 0xC0) != 0x80) return 0xFFFD;
            return ((first & 0x0F) << 12) | ((second & 0x3F) << 6) | (third & 0x3F);
        }
        return 0xFFFD;
    }

    Renderer::~Renderer() { Shutdown(); }

    void Renderer::Shutdown() {
        // brief 08 Part C: detach from the shared pool. If we were the device-owner,
        // clear the published handles (they are about to be freed below). The pool
        // object itself is owned and freed by the main context, not here.
        if (sharedPool) {
            if (sharedPool->originRenderer == this) {
                sharedPool->originRenderer = nullptr;
                sharedPool->fontAtlasTexture = nullptr;
                sharedPool->dynamicMSDFAtlasTexture = nullptr;
                sharedPool->msdf = nullptr;
            }
            if (sharedPool->refCount > 0) sharedPool->refCount--;
            sharedPool = nullptr;
        }
        // brief 08 Part C: only the font-resource owner frees the font GPU objects.
        // A secondary Renderer merely referenced the owner's handles — freeing them
        // here would double-free the owner's atlases.
        if (ownsFontResources) {
            // Destroy font objects first while backend is still valid
            fontManager.Shutdown();
            msdfFont.reset();
            msdfGenerator.reset();
            ClearGlyphs();
            if (fontFace) { FT_Done_Face(fontFace); fontFace = nullptr; }
            if (iconFontFace) { FT_Done_Face(iconFontFace); iconFontFace = nullptr; }
            if (ftLibrary) { FT_Done_FreeType(ftLibrary); ftLibrary = nullptr; }
            if (fontAtlasTexture && backend) backend->DeleteTexture(fontAtlasTexture);
            if (dynamicMSDFAtlasTexture && backend) backend->DeleteTexture(dynamicMSDFAtlasTexture);
        }
        // blueNoiseTexture is per-renderer (created lazily by this instance), always ours.
        if (blueNoiseTexture && backend) backend->DeleteTexture(blueNoiseTexture);
        fontAtlasTexture = nullptr; dynamicMSDFAtlasTexture = nullptr; blueNoiseTexture = nullptr;
        backend = nullptr;
    }

    void Renderer::ClearGlyphs() {
        glyphCache.clear();
        dynamicMSDFGlyphCache.clear();
    }

    void Renderer::InitTrigTables() {
        const float pi = 3.14159265f;
        for (int i = 0; i < CORNER_POINTS; ++i) {
            float a = (pi * 0.5f) * i / CORNER_SEGMENTS;
            cosTable[i] = std::cos(a);
            sinTable[i] = std::sin(a);
        }
        trigTablesInitialized = true;
    }

    bool Renderer::Init(RenderBackend* backend, SharedResourcePool* pool) {
        sharedPool = pool;
        // brief 08 Part C: if the pool already has a device-owner, we are a SECONDARY
        // Renderer on the same GL context / Vulkan device. Do NOT bake our own font
        // atlases — adopt the owner's GPU handles and route glyph work through it.
        if (pool && pool->originRenderer && pool->originRenderer != this) {
            bool ok = InitShared(backend);
            if (ok) pool->refCount++;
            return ok;
        }
        bool ok = Init(backend);
        if (ok && sharedPool) {
            // First Renderer bound to the pool becomes the device-owner and publishes
            // its atlas handles so secondary windows can reference the same GPU memory.
            sharedPool->originRenderer = this;
            sharedPool->fontAtlasTexture = fontAtlasTexture;
            sharedPool->dynamicMSDFAtlasTexture = dynamicMSDFAtlasTexture;
            sharedPool->msdf = msdfFont.get();
            sharedPool->refCount++;
        }
        return ok;
    }

    bool Renderer::InitShared(RenderBackend* backend) {
        this->backend = backend;
        if (!this->backend) return false;
        ownsFontResources = false; // we reference the owner's font GPU resources

        // Per-renderer (non-font) setup — identical to the head of Init(backend).
        InitTrigTables();
        memset(cachedProjection, 0, sizeof(cachedProjection));
        cachedProjection[0]  = 2.0f / viewportSize.x;
        cachedProjection[5]  = -2.0f / viewportSize.y;
        cachedProjection[10] = -1.0f;
        cachedProjection[12] = -1.0f;
        cachedProjection[13] = 1.0f;
        cachedProjection[15] = 1.0f;

        // Adopt the owner's shared GPU atlas handles (valid on the shared device /
        // GL context). These are NOT owned here — Shutdown must not free them.
        Renderer* o = sharedPool->originRenderer;
        fontAtlasTexture        = o->fontAtlasTexture;
        dynamicMSDFAtlasTexture = o->dynamicMSDFAtlasTexture;
        atlasWidth = o->atlasWidth; atlasHeight = o->atlasHeight;
        dynamicAtlasWidth = o->dynamicAtlasWidth; dynamicAtlasHeight = o->dynamicAtlasHeight;
        // Mirror bitmap-font metrics so the (delegated) fallback text paths agree.
        fontLoaded      = o->fontLoaded;
        fontPixelHeight = o->fontPixelHeight;
        fontAscent      = o->fontAscent;
        fontDescent     = o->fontDescent;
        fontLineHeight  = o->fontLineHeight;
        iconFontLoaded      = o->iconFontLoaded;
        iconFontPixelHeight = o->iconFontPixelHeight;
        iconFontAscent      = o->iconFontAscent;
        // msdfFont / fontFace / iconFontFace / fontManager stay null/empty here: the
        // owner is the single writer, reached via FontOwner()/ActiveMSDF()/FontMgr().
        return true;
    }

    bool Renderer::Init(RenderBackend* backend) {
        this->backend = backend;
        if (!this->backend) return false;
        if (FT_Init_FreeType(&ftLibrary)) return false;

        // Perf 3.1: Pre-compute trig tables for rounded rects
        InitTrigTables();

        // Perf 1.3: Initialize default projection matrix
        memset(cachedProjection, 0, sizeof(cachedProjection));
        cachedProjection[0] = 2.0f / viewportSize.x;
        cachedProjection[5] = -2.0f / viewportSize.y;
        cachedProjection[10] = -1.0f;
        cachedProjection[12] = -1.0f;
        cachedProjection[13] = 1.0f;
        cachedProjection[15] = 1.0f;

        atlasWidth = 1024; atlasHeight = 1024;
        fontAtlasTexture = backend->CreateTexture(atlasWidth, atlasHeight, nullptr, true);

        dynamicMSDFAtlasTexture = backend->CreateTexture(DYNAMIC_ATLAS_SIZE, DYNAMIC_ATLAS_SIZE, nullptr, false);
        dynamicAtlasNextX = 2; dynamicAtlasNextY = 2;
        dynamicAtlasCurrentRowHeight = 0;

        msdfFont = std::make_unique<FontMSDF>(backend);
        msdfGenerator = std::make_unique<MSDFGenerator>();

        // Initialize FontManager (Phase 5)
        fontManager.Init(backend, ftLibrary);

        InitializeDefaultFont();

        // Phase 5.2: Pre-generate MSDF glyphs for ASCII range
        // Done after font is loaded in InitializeDefaultFont
        if (msdfFont && msdfFont->IsLoaded()) {
            // The MSDF font atlas already has pre-generated glyphs from the atlas.json
            // For dynamic MSDF, pre-generate ASCII + Latin Extended
            if (fontFace) {
                for (uint32_t c = 32; c < 128; ++c) {
                    GetOrGenerateMSDFGlyph(c);
                }
                // Latin Extended (common accented characters)
                for (uint32_t c = 192; c <= 255; ++c) {
                    GetOrGenerateMSDFGlyph(c);
                }
            }
        }

        return true;
    }

    void Renderer::BeginFrame(const Color& clearColor) {
        if (backend) backend->BeginFrame(clearColor);
        quadVertices.clear(); quadIndices.clear(); lineVertices.clear();
        sdfInstances.clear();
        nextRevealIntensity = 0.0f;
        for (int i = 0; i < (int)RenderLayer::Count; ++i) {
            layerBatches[i].clear();
        }
        currentLayer = RenderLayer::Default;
        // Issue 1: Reset batch state
        currentBatchShader = ShaderType::Basic;
        currentBatchTexture = nullptr;
        currentBatchTextColor = Color(1,1,1,1);

        // Reset clip stack to prevent accumulation from imbalanced Push/Pop
        clipStack.clear();

        // Perf R6: Evict stale dynamic MSDF glyphs periodically
        glyphCacheFrame++;
        if ((glyphCacheFrame % 300) == 0 && dynamicMSDFGlyphCache.size() > MAX_GLYPH_CACHE) {
            for (auto it = dynamicMSDFGlyphCache.begin(); it != dynamicMSDFGlyphCache.end(); ) {
                if ((glyphCacheFrame - it->second.lastAccessFrame) > GLYPH_EVICT_AGE) {
                    it = dynamicMSDFGlyphCache.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }

    void Renderer::EndFrame() {
        FlushBatch(); // Asegurar que el último batch de la capa actual se encola

        if (backend) {
            // Dibujar capas en orden: Default, luego Overlay, luego Tooltip
            for (int i = 0; i < (int)RenderLayer::Count; ++i) {
                for (const auto& batch : layerBatches[i]) {
                    // Perf Phase C: Count batches, draw calls, vertices
                    if (perfCounters.batchCount) (*perfCounters.batchCount)++;
                    if (perfCounters.drawCalls) (*perfCounters.drawCalls)++;
                    if (perfCounters.vertexCount) (*perfCounters.vertexCount) +=
                        static_cast<uint32_t>(batch.isSDF ? batch.instances.size() * 4 : batch.vertices.size());
                    if (perfCounters.indexCount) (*perfCounters.indexCount) +=
                        static_cast<uint32_t>(batch.isSDF ? batch.instances.size() * 6 : batch.indices.size());

                    // Restaurar clipping del batch
                    if (batch.hasClip) {
                        backend->PushClipRect(batch.clipRect.x, batch.clipRect.y, batch.clipRect.width, batch.clipRect.height);
                    }

                    if (batch.isAcrylic) {
                        backend->DrawAcrylicPanel(batch.acrylic, batch.projection);
                    } else if (batch.isSDF) {
                        const float* reveal = (batch.reveal[2] > 0.0f) ? batch.reveal : nullptr;
                        backend->DrawSDFInstances(batch.instances.data(), batch.instances.size(),
                                                  batch.projection, reveal);
                    } else if (batch.isLines) {
                        backend->DrawLines(batch.vertices.data(), batch.vertices.size(), batch.lineWidth, batch.projection);
                    } else {
                        backend->DrawBatch(batch.type, batch.vertices.data(), batch.vertices.size(),
                                           batch.indices.empty() ? nullptr : batch.indices.data(),
                                           batch.indices.size(), batch.texture, batch.projection, batch.textColor);
                    }

                    if (batch.hasClip) {
                        backend->PopClipRect();
                    }
                }
            }
            backend->EndFrame();
        }
    }

    Color Renderer::ReadPixel(int x, int y) {
        FlushBatch();
        if (!backend) return Color(0, 0, 0, 0);
        // Brief 24: gate on the capability so an unsupported backend (e.g. Vulkan,
        // which has no framebuffer readback yet) reports the gap once instead of
        // silently returning a transparent pixel that callers misread as "black".
        if (!backend->Supports(RenderCap::ReadPixel)) {
            static bool warned = false;
            if (!warned) {
                warned = true;
                Log(LogLevel::Warning,
                    "ReadPixel unsupported on the active backend (no RenderCap::ReadPixel); "
                    "eyedropper / pixel readback is disabled.");
            }
            return Color(0, 0, 0, 0);
        }
        return backend->ReadPixel(x, y);
    }

    void Renderer::SetViewport(int width, int height) {
        viewportSize = Vec2(static_cast<float>(width), static_cast<float>(height));
        if (backend) backend->SetViewport(width, height);

        // Perf 1.3: Recompute projection matrix once on viewport change
        float left = 0.0f, right = viewportSize.x;
        float top = 0.0f, bottom = viewportSize.y;
        memset(cachedProjection, 0, sizeof(cachedProjection));
        cachedProjection[0] = 2.0f / (right - left);
        cachedProjection[5] = -2.0f / (bottom - top);
        cachedProjection[10] = -1.0f;
        cachedProjection[12] = -(right + left) / (right - left);
        cachedProjection[13] = (bottom + top) / (bottom - top);
        cachedProjection[15] = 1.0f;
    }

    // Issue 1: EnsureBatchState — only flush when state actually changes
    void Renderer::EnsureBatchState(ShaderType shader, void* texture, const Color& color) {
        bool changed = currentBatchShader != shader || currentBatchTexture != texture ||
                       currentBatchTextColor.r != color.r || currentBatchTextColor.g != color.g ||
                       currentBatchTextColor.b != color.b || currentBatchTextColor.a != color.a;
        // Flush pending content of the OUTGOING state (quads or SDF instances) before
        // switching, so the draw order between SDF rects and Basic/MSDF quads is kept.
        if (changed && (!quadVertices.empty() || !sdfInstances.empty())) {
            FlushBatch(currentBatchShader, currentBatchTexture, currentBatchTextColor);
            // Perf Phase C: Count state changes
            if (perfCounters.stateChanges) (*perfCounters.stateChanges)++;
        }
        currentBatchShader = shader;
        currentBatchTexture = texture;
        currentBatchTextColor = color;
    }

    void Renderer::FlushBatch() {
        FlushBatch(currentBatchShader, currentBatchTexture, currentBatchTextColor);
    }

    void Renderer::FlushBatch(ShaderType type, void* texture, const Color& textColor) {
        if (!backend) return;
        if (quadVertices.empty() && lineVertices.empty() && sdfInstances.empty()) return;

        // Perf Phase C: Count flushes
        if (perfCounters.flushCount) (*perfCounters.flushCount)++;

        // Perf 1.3: Use cached projection matrix (recomputed only in SetViewport)
        const float* projection = cachedProjection;

        if (!quadVertices.empty()) {
            // Merge with last compatible batch only (preserves draw order)
            auto& batches = layerBatches[(int)currentLayer];
            bool merged = false;
            if (!batches.empty()) {
                auto& last = batches.back();
                if (!last.isLines && !last.isSDF && !last.isAcrylic && last.type == type && last.texture == texture &&
                    last.textColor.r == textColor.r && last.textColor.g == textColor.g &&
                    last.textColor.b == textColor.b && last.textColor.a == textColor.a) {
                    bool clipMatch = false;
                    if (!clipStack.empty() && last.hasClip) {
                        clipMatch = (last.clipRect.x == clipStack.back().x &&
                                     last.clipRect.y == clipStack.back().y &&
                                     last.clipRect.width == clipStack.back().width &&
                                     last.clipRect.height == clipStack.back().height);
                    } else if (clipStack.empty() && !last.hasClip) {
                        clipMatch = true;
                    }
                    if (clipMatch) {
                        unsigned int indexOffset = static_cast<unsigned int>(last.vertices.size());
                        last.vertices.insert(last.vertices.end(), quadVertices.begin(), quadVertices.end());
                        last.indices.reserve(last.indices.size() + quadIndices.size());
                        for (auto idx : quadIndices) {
                            last.indices.push_back(idx + indexOffset);
                        }
                        merged = true;
                        if (perfCounters.batchMerges) (*perfCounters.batchMerges)++;
                    }
                }
            }

            if (!merged) {
                RenderBatch batch;
                batch.type = type;
                batch.vertices = std::move(quadVertices);
                batch.indices = std::move(quadIndices);
                batch.texture = texture;
                batch.textColor = textColor;
                memcpy(batch.projection, projection, 16 * sizeof(float));
                batch.isLines = false;

                if (!clipStack.empty()) {
                    batch.clipRect = clipStack.back();
                    batch.hasClip = true;
                } else {
                    batch.hasClip = false;
                }

                batches.push_back(std::move(batch));
            }
            quadVertices.clear();
            quadIndices.clear();
        }

        if (!sdfInstances.empty()) {
            auto& batches = layerBatches[(int)currentLayer];
            bool merged = false;
            if (!batches.empty()) {
                auto& last = batches.back();
                if (last.isSDF) {
                    bool clipMatch = false;
                    if (!clipStack.empty() && last.hasClip) {
                        clipMatch = (last.clipRect.x == clipStack.back().x &&
                                     last.clipRect.y == clipStack.back().y &&
                                     last.clipRect.width == clipStack.back().width &&
                                     last.clipRect.height == clipStack.back().height);
                    } else if (clipStack.empty() && !last.hasClip) {
                        clipMatch = true;
                    }
                    if (clipMatch) {
                        last.instances.insert(last.instances.end(),
                                              sdfInstances.begin(), sdfInstances.end());
                        merged = true;
                        if (perfCounters.batchMerges) (*perfCounters.batchMerges)++;
                    }
                }
            }

            if (!merged) {
                RenderBatch batch;
                batch.type = ShaderType::SDFRect;
                batch.isSDF = true;
                batch.instances = std::move(sdfInstances);
                batch.texture = nullptr;
                memcpy(batch.projection, projection, 16 * sizeof(float));
                batch.reveal[0] = revealCursor[0];
                batch.reveal[1] = revealCursor[1];
                batch.reveal[2] = revealCursor[2];
                if (!clipStack.empty()) {
                    batch.clipRect = clipStack.back();
                    batch.hasClip = true;
                } else {
                    batch.hasClip = false;
                }
                batches.push_back(std::move(batch));
            }
            sdfInstances.clear();
        }

        if (!lineVertices.empty()) {
            RenderBatch batch;
            batch.type = ShaderType::Basic;
            batch.vertices = std::move(lineVertices);
            batch.indices.clear();
            batch.texture = nullptr;
            batch.textColor = Color(1,1,1,1);
            memcpy(batch.projection, projection, 16 * sizeof(float));
            batch.isLines = true;
            batch.lineWidth = lineBatchWidth;

            if (!clipStack.empty()) {
                batch.clipRect = clipStack.back();
                batch.hasClip = true;
            } else {
                batch.hasClip = false;
            }

            layerBatches[(int)currentLayer].push_back(std::move(batch));
            lineVertices.clear();
        }
    }

    void Renderer::DrawLine(const Vec2& start, const Vec2& end, const Color& color, float width) {
        // AA line as a triangulated quad with 1px feather on each side.
        // No more GL_LINES — line width changes don't break batching anymore.
        EnsureBatchState(ShaderType::Basic, nullptr, Color(1,1,1,1));

        float dx = end.x - start.x, dy = end.y - start.y;
        float len = std::sqrt(dx*dx + dy*dy);
        if (len < 1e-6f) return;
        float nx = -dy / len, ny = dx / len; // unit perpendicular

        const float AA_FRINGE = std::max(1.0f, dpiScale); // Phase A4: 1 physical pixel
        float halfCore = std::max(0.5f, width * 0.5f); // core half-width (≥0.5px so 1px lines render)
        float halfOuter = halfCore + AA_FRINGE;

        if (quadVertices.size() + 8 > MAX_QUAD_VERTICES) FlushBatch();
        unsigned int b = static_cast<unsigned int>(quadVertices.size());

        // 0..3 = inner core (alpha=color.a); 4..7 = outer fringe (alpha=0)
        // 0 = start - n*halfCore   1 = start + n*halfCore   2 = end + n*halfCore   3 = end - n*halfCore
        quadVertices.push_back({start.x - nx*halfCore, start.y - ny*halfCore, color.r, color.g, color.b, color.a, 0, 0});
        quadVertices.push_back({start.x + nx*halfCore, start.y + ny*halfCore, color.r, color.g, color.b, color.a, 0, 0});
        quadVertices.push_back({end.x   + nx*halfCore, end.y   + ny*halfCore, color.r, color.g, color.b, color.a, 0, 0});
        quadVertices.push_back({end.x   - nx*halfCore, end.y   - ny*halfCore, color.r, color.g, color.b, color.a, 0, 0});
        // 4 = start - n*halfOuter  5 = start + n*halfOuter  6 = end + n*halfOuter  7 = end - n*halfOuter
        quadVertices.push_back({start.x - nx*halfOuter, start.y - ny*halfOuter, color.r, color.g, color.b, 0.0f, 0, 0});
        quadVertices.push_back({start.x + nx*halfOuter, start.y + ny*halfOuter, color.r, color.g, color.b, 0.0f, 0, 0});
        quadVertices.push_back({end.x   + nx*halfOuter, end.y   + ny*halfOuter, color.r, color.g, color.b, 0.0f, 0, 0});
        quadVertices.push_back({end.x   - nx*halfOuter, end.y   - ny*halfOuter, color.r, color.g, color.b, 0.0f, 0, 0});

        // Core body (2 tris)
        quadIndices.push_back(b+0); quadIndices.push_back(b+1); quadIndices.push_back(b+2);
        quadIndices.push_back(b+0); quadIndices.push_back(b+2); quadIndices.push_back(b+3);
        // Fringe on +n side (between inner 1↔2 and outer 5↔6)
        quadIndices.push_back(b+1); quadIndices.push_back(b+5); quadIndices.push_back(b+6);
        quadIndices.push_back(b+1); quadIndices.push_back(b+6); quadIndices.push_back(b+2);
        // Fringe on -n side (between inner 0↔3 and outer 4↔7)
        quadIndices.push_back(b+4); quadIndices.push_back(b+0); quadIndices.push_back(b+3);
        quadIndices.push_back(b+4); quadIndices.push_back(b+3); quadIndices.push_back(b+7);
    }

    // Brief 02: outline-only rounded rect via one SDF instance. Semantics match the
    // previous StrokePolyline implementation (contour only, no fill); the border band
    // is rendered inside the rect bounds by the SDF shader.
    void Renderer::DrawRect(const Vec2& pos, const Vec2& size, const Color& color, float cornerRadius) {
        if (size.x <= 0.0f || size.y <= 0.0f) return;
        EnsureBatchState(ShaderType::SDFRect, nullptr, Color(1,1,1,1));
        if (sdfInstances.size() >= MAX_SDF_INSTANCES) FlushBatch();

        SDFInstance s{};
        s.cx = pos.x + size.x * 0.5f; s.cy = pos.y + size.y * 0.5f;
        s.hx = size.x * 0.5f;         s.hy = size.y * 0.5f;
        s.radius = std::clamp(cornerRadius, 0.0f, std::min(s.hx, s.hy));
        s.borderWidth = std::max(1.0f, dpiScale); // 1 physical px outline
        s.softness = std::max(1.0f, dpiScale);
        s.mode = 0.0f;
        s.fillA = 0.0f; // transparent fill → outline only
        s.borderR = color.r; s.borderG = color.g; s.borderB = color.b; s.borderA = color.a;
        s.revealIntensity = nextRevealIntensity; nextRevealIntensity = 0.0f;
        sdfInstances.push_back(s);
    }

    void Renderer::DrawRectFilled(const Vec2& pos, const Vec2& size, const Color& color, float cornerRadius) {
        if (size.x <= 0.0f || size.y <= 0.0f) return;
        // Brief 02: every filled rect (sharp or rounded) is one SDF instance — unifies
        // the path and enables reveal/acrylic later. AA is resolved analytically at 1
        // physical px in the fragment shader.
        EnsureBatchState(ShaderType::SDFRect, nullptr, Color(1,1,1,1));
        if (sdfInstances.size() >= MAX_SDF_INSTANCES) FlushBatch();

        SDFInstance s{};
        s.cx = pos.x + size.x * 0.5f; s.cy = pos.y + size.y * 0.5f;
        s.hx = size.x * 0.5f;         s.hy = size.y * 0.5f;
        s.radius = std::clamp(cornerRadius, 0.0f, std::min(s.hx, s.hy));
        s.borderWidth = 0.0f;
        s.softness = std::max(1.0f, dpiScale);
        s.mode = 0.0f;
        s.fillR = color.r; s.fillG = color.g; s.fillB = color.b; s.fillA = color.a;
        s.revealIntensity = nextRevealIntensity; nextRevealIntensity = 0.0f;
        sdfInstances.push_back(s);
    }

    void Renderer::DrawRectWithElevation(const Vec2& pos, const Vec2& size, const Color& color, float cornerRadius, float elevation) {
        DrawElevationShadow(pos, size, cornerRadius, elevation);
        DrawRectFilled(pos, size, color, cornerRadius);
    }

    void Renderer::DrawElevationShadow(const Vec2& pos, const Vec2& size, float cornerRadius, float z, float opacityScale) {
        if (z <= 0.0f || opacityScale <= 0.0f) return;
        // Brief 11: a surface's z (depth) drives TWO shadow layers — a wide soft
        // ambient halo (no offset) and a tense darker key shadow (offset down) —
        // both tinted by the themed shadowColor. Emitted ambient-first so the key
        // sits on top. Both go to the same SDF batch (mode 1) → 2 instances, no
        // extra draws. Drawn BEFORE the element's own fill.
        Elevation::DualShadow ds = Elevation::ParamsDual(z);
        const Color& sc = shadowColor;
        auto emit = [&](const Elevation::ShadowParams& sp) {
            if (sp.opacity <= 0.0f || sp.blur <= 0.0f) return;
            DrawRectShadow(pos, size, cornerRadius, sp.blur,
                           Color(sc.r, sc.g, sc.b, sc.a * sp.opacity * opacityScale),
                           Vec2(0.0f, sp.offsetY));
        };
        emit(ds.ambient);
        emit(ds.key);
    }

    // Brief 11: inner / inset shadow as a SINGLE SDF instance (mode 3). Darkens
    // inside the rect, clipped to the rounded interior. Draw AFTER the fill.
    void Renderer::DrawInsetShadow(const Vec2& pos, const Vec2& size, float cornerRadius,
                                   float sigma, const Color& color) {
        if (color.a <= 0.0f || sigma <= 0.0f || size.x <= 0.0f || size.y <= 0.0f) return;

        EnsureBatchState(ShaderType::SDFRect, nullptr, Color(1,1,1,1));
        if (sdfInstances.size() >= MAX_SDF_INSTANCES) FlushBatch();

        SDFInstance s{};
        s.cx = pos.x + size.x * 0.5f; s.cy = pos.y + size.y * 0.5f;
        s.hx = size.x * 0.5f; s.hy = size.y * 0.5f;
        s.radius = std::clamp(cornerRadius, 0.0f, std::min(s.hx, s.hy));
        s.borderWidth = sigma * dpiScale; // alias = penumbra in physical px
        s.softness = std::max(1.0f, dpiScale);
        s.mode = 3.0f;
        s.fillR = color.r; s.fillG = color.g; s.fillB = color.b; s.fillA = color.a;
        sdfInstances.push_back(s);
    }

    // Brief 03: drop shadow as a SINGLE SDF instance (mode 1) with an exponential
    // falloff in the fragment shader — replaces the previous 16-40 stacked rounded
    // rects. Same batch/accumulator as the fill, so shadow+fill of many widgets
    // collapse to one draw. Draw BEFORE the element's own fill.
    void Renderer::DrawRectShadow(const Vec2& pos, const Vec2& size, float cornerRadius,
                                  float blur, const Color& color, const Vec2& offset) {
        if (color.a <= 0.0f || blur <= 0.0f || size.x <= 0.0f || size.y <= 0.0f) return;

        EnsureBatchState(ShaderType::SDFRect, nullptr, Color(1,1,1,1));
        if (sdfInstances.size() >= MAX_SDF_INSTANCES) FlushBatch();

        SDFInstance s{};
        Vec2 c = pos + size * 0.5f + offset; // offsetY shifts the shadow down (CPU-side)
        s.cx = c.x; s.cy = c.y;
        s.hx = size.x * 0.5f; s.hy = size.y * 0.5f;
        s.radius = std::clamp(cornerRadius, 0.0f, std::min(s.hx, s.hy));
        s.borderWidth = blur * dpiScale; // alias = penumbra in physical px
        s.softness = std::max(1.0f, dpiScale);
        s.mode = 1.0f;
        s.fillR = color.r; s.fillG = color.g; s.fillB = color.b; s.fillA = color.a;
        sdfInstances.push_back(s);
    }

    void Renderer::DrawRectGradient(const Vec2& pos, const Vec2& size,
                                     const Color& topLeft, const Color& topRight,
                                     const Color& bottomLeft, const Color& bottomRight) {
        EnsureBatchState(ShaderType::Basic, nullptr, Color(1,1,1,1));
        if (quadVertices.size() + 4 > MAX_QUAD_VERTICES) FlushBatch();
        unsigned int baseIndex = static_cast<unsigned int>(quadVertices.size());
        // Top-left
        quadVertices.push_back({pos.x, pos.y, topLeft.r, topLeft.g, topLeft.b, topLeft.a, 0, 0});
        // Top-right
        quadVertices.push_back({pos.x + size.x, pos.y, topRight.r, topRight.g, topRight.b, topRight.a, 0, 0});
        // Bottom-right
        quadVertices.push_back({pos.x + size.x, pos.y + size.y, bottomRight.r, bottomRight.g, bottomRight.b, bottomRight.a, 0, 0});
        // Bottom-left
        quadVertices.push_back({pos.x, pos.y + size.y, bottomLeft.r, bottomLeft.g, bottomLeft.b, bottomLeft.a, 0, 0});
        quadIndices.push_back(baseIndex + 0); quadIndices.push_back(baseIndex + 1); quadIndices.push_back(baseIndex + 2);
        quadIndices.push_back(baseIndex + 0); quadIndices.push_back(baseIndex + 2); quadIndices.push_back(baseIndex + 3);
    }

    // Brief 06: 64x64 R8 grain texture. Pseudo-random (hash-based) — visually a fine
    // dither, close enough to true blue noise for the subtle acrylic grain. Created
    // lazily so non-acrylic apps never pay for it.
    void Renderer::EnsureBlueNoise() {
        if (blueNoiseTexture || !backend) return;
        unsigned char data[64 * 64];
        for (int y = 0; y < 64; ++y) {
            for (int x = 0; x < 64; ++x) {
                // Cheap integer hash → byte. Decorrelated enough for grain.
                uint32_t h = static_cast<uint32_t>(x) * 374761393u +
                             static_cast<uint32_t>(y) * 668265263u;
                h = (h ^ (h >> 13)) * 1274126177u;
                data[y * 64 + x] = static_cast<unsigned char>(h >> 24);
            }
        }
        blueNoiseTexture = backend->CreateTexture(64, 64, data, /*alphaOnly=*/true);
    }

    // Brief 06: record a deferred acrylic/mica op into the layer batch list (in draw
    // order). The actual capture+blur+composite runs in EndFrame via the backend, so
    // everything drawn behind the panel is already on the framebuffer (GL).
    void Renderer::EmitAcrylicOp(const Vec2& pos, const Vec2& size, const Color& tintColor,
                                 float cornerRadius, float opacity, float blurAmount, bool mica) {
        if (size.x <= 0.0f || size.y <= 0.0f) return;

        // No real-blur backend → flat fallback (keeps the panel readable everywhere).
        if (!backend || !backend->Supports(RenderCap::Acrylic)) {
            DrawRectAcrylicFallback(pos, size, tintColor, cornerRadius, opacity);
            return;
        }

        EnsureBlueNoise();
        FlushBatch(); // close all prior content into batches so it's behind the panel

        AcrylicParams p{};
        p.x = pos.x; p.y = pos.y; p.w = size.x; p.h = size.y;
        p.cornerRadius = std::clamp(cornerRadius, 0.0f, std::min(size.x, size.y) * 0.5f);
        p.tintR = tintColor.r; p.tintG = tintColor.g; p.tintB = tintColor.b;
        p.dpiScale = dpiScale;
        p.noiseTex = blueNoiseTexture;
        if (mica) {
            // Stronger tint, near-static, slightly wider blur (cheap: backend caches it).
            p.tintOpacity = std::clamp(0.35f + 0.25f * opacity, 0.0f, 1.0f);
            p.luminosityOpacity = 1.0f;
            p.noiseAmount = 0.02f;
            p.blurPasses = 4;
            p.mica = 1;
        } else {
            p.tintOpacity = std::clamp(0.10f + 0.10f * opacity, 0.0f, 1.0f);
            p.luminosityOpacity = 0.6f; // less desaturation → keep more of the backdrop's color
            p.noiseAmount = 0.03f;
            p.blurPasses = (blurAmount > 0.0f)
                         ? std::clamp(static_cast<int>(blurAmount / 10.0f + 0.5f), 1, 5)
                         : 3;
            p.mica = 0;
        }
        // Flat fallback color (used only if the backend bails mid-frame): the tint.
        p.fallbackR = tintColor.r; p.fallbackG = tintColor.g; p.fallbackB = tintColor.b;
        p.fallbackA = std::clamp(opacity, 0.0f, 1.0f);

        RenderBatch batch;
        batch.isAcrylic = true;
        batch.acrylic = p;
        memcpy(batch.projection, cachedProjection, 16 * sizeof(float));
        if (!clipStack.empty()) { batch.clipRect = clipStack.back(); batch.hasClip = true; }
        else                    { batch.hasClip = false; }
        layerBatches[(int)currentLayer].push_back(std::move(batch));
    }

    void Renderer::DrawRectAcrylic(const Vec2& pos, const Vec2& size, const Color& tintColor,
                                   float cornerRadius, float opacity, float blurAmount) {
        EmitAcrylicOp(pos, size, tintColor, cornerRadius, opacity, blurAmount, /*mica=*/false);
    }

    void Renderer::DrawRectMica(const Vec2& pos, const Vec2& size, const Color& tintColor,
                                float cornerRadius, float opacity) {
        EmitAcrylicOp(pos, size, tintColor, cornerRadius, opacity, /*blurAmount=*/0.0f, /*mica=*/true);
    }

    // Flat tinted "fake acrylic" — the pre-brief-06 look, kept as a fallback for
    // backends that can't blur (e.g. Vulkan shared mode). Uses SDF fills (DrawRectFilled
    // handles rounded corners analytically), so no tessellation tables are needed.
    void Renderer::DrawRectAcrylicFallback(const Vec2& pos, const Vec2& size, const Color& tintColor,
                                           float cornerRadius, float opacity) {
        bool isDark = (tintColor.r + tintColor.g + tintColor.b) / 3.0f < 0.5f;
        Color base = isDark ? Color(0.1f, 0.1f, 0.15f, opacity * 0.5f) : Color(0.95f, 0.95f, 0.98f, opacity * 0.4f);
        Color tint = Color(tintColor.r, tintColor.g, tintColor.b, opacity * 0.08f);
        Color acrylic = isDark ? Color(tintColor.r*0.7f+0.2f, tintColor.g*0.7f+0.2f, tintColor.b*0.7f+0.25f, opacity*0.75f)
                               : Color(tintColor.r*0.3f+0.7f, tintColor.g*0.3f+0.7f, tintColor.b*0.7f+0.72f, opacity*0.8f);
        DrawRectFilled(pos, size, base, cornerRadius);
        DrawRectFilled(pos, size, tint, cornerRadius);
        DrawRectFilled(pos, size, acrylic, cornerRadius);
        DrawRect(pos, size, isDark ? Color(1,1,1,opacity*0.15f) : Color(0,0,0,opacity*0.1f), cornerRadius);
    }

    void Renderer::DrawCircle(const Vec2& center, float radius, const Color& color, bool filled) {
        if (radius <= 0.0f) return;
        // Issue 1: Ensure Basic batch state
        EnsureBatchState(ShaderType::Basic, nullptr, Color(1,1,1,1));

        const float pi = 3.14159265f; int segments = static_cast<int>(std::max(12.0f, radius * 0.75f));
        if (filled) {
            // AA fringe: inner ring at radius (alpha=color.a) + outer ring at radius+1px (alpha=0)
            const float AA_FRINGE = std::max(1.0f, dpiScale); // Phase A4: 1 physical pixel
            if (quadVertices.size() + 1 + 2 * static_cast<size_t>(segments) > MAX_QUAD_VERTICES) FlushBatch();
            unsigned int centerIdx = static_cast<unsigned int>(quadVertices.size());
            quadVertices.push_back({center.x, center.y, color.r, color.g, color.b, color.a, 0, 0});
            for (int i = 0; i < segments; ++i) {
                float a = 2.0f * pi * i / segments;
                float cx = std::cos(a), cy = std::sin(a);
                quadVertices.push_back({center.x + cx * radius, center.y + cy * radius, color.r, color.g, color.b, color.a, 0, 0});
            }
            for (int i = 0; i < segments; ++i) {
                float a = 2.0f * pi * i / segments;
                float cx = std::cos(a), cy = std::sin(a);
                quadVertices.push_back({center.x + cx * (radius + AA_FRINGE), center.y + cy * (radius + AA_FRINGE), color.r, color.g, color.b, 0.0f, 0, 0});
            }
            unsigned int innerBase = centerIdx + 1;
            unsigned int outerBase = centerIdx + 1 + static_cast<unsigned int>(segments);
            for (int i = 0; i < segments; ++i) {
                unsigned int next = static_cast<unsigned int>((i + 1) % segments);
                // Inner fan triangle
                quadIndices.push_back(centerIdx);
                quadIndices.push_back(innerBase + static_cast<unsigned int>(i));
                quadIndices.push_back(innerBase + next);
                // Fringe quad
                quadIndices.push_back(innerBase + static_cast<unsigned int>(i));
                quadIndices.push_back(innerBase + next);
                quadIndices.push_back(outerBase + next);
                quadIndices.push_back(innerBase + static_cast<unsigned int>(i));
                quadIndices.push_back(outerBase + next);
                quadIndices.push_back(outerBase + static_cast<unsigned int>(i));
            }
        } else {
            for (int i = 0; i < segments; ++i) {
                float a1 = 2.0f * pi * i / segments; float a2 = 2.0f * pi * (i+1) / segments;
                DrawLine({center.x + std::cos(a1) * radius, center.y + std::sin(a1) * radius}, {center.x + std::cos(a2) * radius, center.y + std::sin(a2) * radius}, color);
            }
        }
    }

    void Renderer::DrawTriangleFilled(const Vec2& p0, const Vec2& p1, const Vec2& p2, const Color& color) {
        EnsureBatchState(ShaderType::Basic, nullptr, Color(1,1,1,1));
        if (quadVertices.size() + 3 > MAX_QUAD_VERTICES) FlushBatch();
        unsigned int base = static_cast<unsigned int>(quadVertices.size());
        quadVertices.push_back({p0.x, p0.y, color.r, color.g, color.b, color.a, 0, 0});
        quadVertices.push_back({p1.x, p1.y, color.r, color.g, color.b, color.a, 0, 0});
        quadVertices.push_back({p2.x, p2.y, color.r, color.g, color.b, color.a, 0, 0});
        quadIndices.push_back(base);
        quadIndices.push_back(base + 1);
        quadIndices.push_back(base + 2);
    }

    void Renderer::DrawRipple(const Vec2& center, float radius, float opacity) {
        if (radius <= 0.0f || opacity <= 0.0f) return;
        DrawCircle(center, radius, Color(1, 1, 1, opacity * 0.3f), true);
    }

    void Renderer::DrawImage(const Vec2& pos, const Vec2& size, void* textureHandle,
                             const Vec2& uv0, const Vec2& uv1,
                             const Color& tint, float cornerRadius) {
        if (!backend || !textureHandle) return;
        if (size.x <= 0.0f || size.y <= 0.0f) return;

        // Use Image shader which samples full RGBA texture and multiplies by vertex color
        EnsureBatchState(ShaderType::Image, textureHandle, Color(1,1,1,1));

        if (cornerRadius < 0.5f) {
            // Simple quad — 4 vertices, 6 indices
            if (quadVertices.size() + 4 > MAX_QUAD_VERTICES) FlushBatch();
            unsigned int base = static_cast<unsigned int>(quadVertices.size());
            quadVertices.push_back({pos.x,          pos.y,          tint.r, tint.g, tint.b, tint.a, uv0.x, uv0.y});
            quadVertices.push_back({pos.x + size.x, pos.y,          tint.r, tint.g, tint.b, tint.a, uv1.x, uv0.y});
            quadVertices.push_back({pos.x + size.x, pos.y + size.y, tint.r, tint.g, tint.b, tint.a, uv1.x, uv1.y});
            quadVertices.push_back({pos.x,          pos.y + size.y, tint.r, tint.g, tint.b, tint.a, uv0.x, uv1.y});
            quadIndices.push_back(base + 0); quadIndices.push_back(base + 1); quadIndices.push_back(base + 2);
            quadIndices.push_back(base + 0); quadIndices.push_back(base + 2); quadIndices.push_back(base + 3);
        } else {
            // Perf 3.1: Use pre-computed trig tables for rounded image corners
            float cr = std::min(cornerRadius, std::min(size.x, size.y) * 0.5f);
            float cx_left = pos.x + cr;
            float cx_right = pos.x + size.x - cr;
            float cy_top = pos.y + cr;
            float cy_bot = pos.y + size.y - cr;

            constexpr int PERIM_SIZE = CORNER_POINTS * 4;
            Vec2 perim[PERIM_SIZE];
            Vec2 normals[PERIM_SIZE];
            int pi = 0;
            for (int i = 0; i < CORNER_POINTS; ++i) {
                normals[pi] = {sinTable[i], -cosTable[i]};
                perim[pi++]  = {cx_right + cr * sinTable[i], cy_top - cr * cosTable[i]};
            }
            for (int i = 0; i < CORNER_POINTS; ++i) {
                normals[pi] = {cosTable[i], sinTable[i]};
                perim[pi++]  = {cx_right + cr * cosTable[i], cy_bot + cr * sinTable[i]};
            }
            for (int i = 0; i < CORNER_POINTS; ++i) {
                normals[pi] = {-sinTable[i], cosTable[i]};
                perim[pi++]  = {cx_left - cr * sinTable[i], cy_bot + cr * cosTable[i]};
            }
            for (int i = 0; i < CORNER_POINTS; ++i) {
                normals[pi] = {-cosTable[i], -sinTable[i]};
                perim[pi++]  = {cx_left - cr * cosTable[i], cy_top - cr * sinTable[i]};
            }

            const float AA_FRINGE = std::max(1.0f, dpiScale); // Phase A4: 1 physical pixel
            if (quadVertices.size() + 1 + 2 * PERIM_SIZE > MAX_QUAD_VERTICES) FlushBatch();

            float invW = 1.0f / size.x;
            float invH = 1.0f / size.y;
            auto posToUV = [&](float px, float py) -> Vec2 {
                float t = (px - pos.x) * invW;
                float s = (py - pos.y) * invH;
                return Vec2(uv0.x + (uv1.x - uv0.x) * t, uv0.y + (uv1.y - uv0.y) * s);
            };

            unsigned int centerIdx = static_cast<unsigned int>(quadVertices.size());
            Vec2 centerPos(pos.x + size.x * 0.5f, pos.y + size.y * 0.5f);
            Vec2 centerUV = posToUV(centerPos.x, centerPos.y);
            quadVertices.push_back({centerPos.x, centerPos.y, tint.r, tint.g, tint.b, tint.a, centerUV.x, centerUV.y});

            // Inner ring: full alpha, sample inside texture
            for (int i = 0; i < PERIM_SIZE; ++i) {
                Vec2 uv = posToUV(perim[i].x, perim[i].y);
                quadVertices.push_back({perim[i].x, perim[i].y, tint.r, tint.g, tint.b, tint.a, uv.x, uv.y});
            }
            // Outer ring: alpha=0 (texture sample doesn't matter — kept at edge UV to stay clamped)
            for (int i = 0; i < PERIM_SIZE; ++i) {
                Vec2 uv = posToUV(perim[i].x, perim[i].y);
                quadVertices.push_back({perim[i].x + normals[i].x * AA_FRINGE, perim[i].y + normals[i].y * AA_FRINGE, tint.r, tint.g, tint.b, 0.0f, uv.x, uv.y});
            }

            unsigned int innerBase = centerIdx + 1;
            unsigned int outerBase = centerIdx + 1 + PERIM_SIZE;
            for (int i = 0; i < PERIM_SIZE; ++i) {
                unsigned int next = static_cast<unsigned int>((i + 1) % PERIM_SIZE);
                quadIndices.push_back(centerIdx);
                quadIndices.push_back(innerBase + static_cast<unsigned int>(i));
                quadIndices.push_back(innerBase + next);
                quadIndices.push_back(innerBase + static_cast<unsigned int>(i));
                quadIndices.push_back(innerBase + next);
                quadIndices.push_back(outerBase + next);
                quadIndices.push_back(innerBase + static_cast<unsigned int>(i));
                quadIndices.push_back(outerBase + next);
                quadIndices.push_back(outerBase + static_cast<unsigned int>(i));
            }
        }
    }

    void Renderer::DrawText(const Vec2& pos, const std::string& text, const Color& color, float fontSize) {
        if (!backend) return;

        FontMSDF* mf = ActiveMSDF(); // shared static-MSDF atlas (owner's in multi-window)
        if (mf && mf->IsLoaded()) {
            // Issue 1: Only flush if shader/texture/color state actually changes
            EnsureBatchState(ShaderType::MSDF, mf->GetTextureHandle(), color);

            float scale = fontSize / mf->GetEmSize();
            float baseline = pos.y + mf->GetAscender() * fontSize;
            if (mf->GetAscender() <= 0.0f) baseline = pos.y + fontSize * 0.8f;

            float xpos = pos.x;
            const char* ptr = text.data(); const char* end = ptr + text.size();

            while (ptr < end) {
                uint32_t cp = DecodeUTF8(ptr, end);
                if (cp == 0) break;
                if (cp == '\n') { xpos = pos.x; baseline += fontSize * mf->GetLineHeight(); continue; }
                const FontMSDF::Glyph* g = mf->GetGlyph(cp);

                if (g) {
                    float x0 = std::round(xpos + g->bearing.x * scale); float y0 = baseline - g->bearing.y * scale;
                    float w = g->planeBounds.x * scale; float h = g->planeBounds.y * scale;
                    if (w > 0 && h > 0) {
                        // Issue 5: Use 4 vertices + 6 indices instead of 6 vertices
                        if (quadVertices.size() + 4 > MAX_QUAD_VERTICES - 4) FlushBatch(ShaderType::MSDF, mf->GetTextureHandle(), color);
                        unsigned int base = static_cast<unsigned int>(quadVertices.size());
                        quadVertices.push_back({x0,y0,1,1,1,1,g->uv0.x,g->uv0.y});
                        quadVertices.push_back({x0+w,y0,1,1,1,1,g->uv1.x,g->uv0.y});
                        quadVertices.push_back({x0+w,y0+h,1,1,1,1,g->uv1.x,g->uv1.y});
                        quadVertices.push_back({x0,y0+h,1,1,1,1,g->uv0.x,g->uv1.y});
                        quadIndices.push_back(base); quadIndices.push_back(base+1); quadIndices.push_back(base+2);
                        quadIndices.push_back(base); quadIndices.push_back(base+2); quadIndices.push_back(base+3);
                    }
                    xpos += g->advance * scale;
                } else xpos += fontSize * 0.3f;
            }
            // Don't flush at end — let the batch continue for next DrawText or DrawRectFilled
            return;
        }

        if (fontLoaded && fontAtlasTexture) {
            // Issue 1: Only flush if shader/texture/color state actually changes
            EnsureBatchState(ShaderType::Text, fontAtlasTexture, color);

            float scale = fontSize / fontPixelHeight; float baseline = pos.y + fontAscent * scale; float penX = pos.x;
            const char* ptr = text.data(); const char* end = ptr + text.size();
            while (ptr < end) {
                uint32_t cp = DecodeUTF8(ptr, end);
                if (cp == 0) break;
                if (cp == '\n') { penX = pos.x; baseline += fontLineHeight * scale; continue; }
                const Glyph* g = GetGlyph(cp);
                if (!g) g = GetGlyph('?');
                if (g) {
                    float x0 = std::round(penX + g->bearing.x * scale); float y0 = baseline - g->bearing.y * scale;
                    float w = g->size.x * scale; float h = g->size.y * scale;
                    if (w > 0 && h > 0) {
                        // Issue 5: Use 4 vertices + 6 indices instead of 6 vertices
                        if (quadVertices.size() + 4 > MAX_QUAD_VERTICES - 4) FlushBatch(ShaderType::Text, fontAtlasTexture, color);
                        unsigned int base = static_cast<unsigned int>(quadVertices.size());
                        quadVertices.push_back({x0,y0,1,1,1,1,g->uv0.x,g->uv1.y});
                        quadVertices.push_back({x0+w,y0,1,1,1,1,g->uv1.x,g->uv1.y});
                        quadVertices.push_back({x0+w,y0+h,1,1,1,1,g->uv1.x,g->uv0.y});
                        quadVertices.push_back({x0,y0+h,1,1,1,1,g->uv0.x,g->uv0.y});
                        quadIndices.push_back(base); quadIndices.push_back(base+1); quadIndices.push_back(base+2);
                        quadIndices.push_back(base); quadIndices.push_back(base+2); quadIndices.push_back(base+3);
                    }
                    penX += g->advance * scale;
                }
            }
            // Don't flush at end — let the batch continue
        }
    }

    bool Renderer::LoadFont(const std::string& filepath, int pixelHeight) {
        // Secondary renderer: load into the owner's shared atlas instead.
        if (!ownsFontResources) return FontOwner()->LoadFont(filepath, pixelHeight);
        std::filesystem::path resolved = ResolveResourcePath(filepath);
        if (resolved.empty()) return false;
        if (FT_New_Face(ftLibrary, resolved.string().c_str(), 0, &fontFace)) return false;
        FT_Set_Pixel_Sizes(fontFace, 0, pixelHeight);
        fontPixelHeight = static_cast<float>(pixelHeight);
        fontAscent = static_cast<float>(fontFace->size->metrics.ascender) / 64.0f;
        fontDescent = static_cast<float>(fontFace->size->metrics.descender) / 64.0f;
        fontLineHeight = static_cast<float>(fontFace->size->metrics.height) / 64.0f;
        fontLoaded = true; ClearGlyphs();
        for (uint32_t c = 32; c < 128; ++c) LoadGlyph(c);
        return true;
    }

    Vec2 Renderer::MeasureText(const std::string& text, float fontSize) {
        FontMSDF* mf = ActiveMSDF();
        if (mf && mf->IsLoaded()) {
            float scale = fontSize / mf->GetEmSize(); float currentW = 0.0f, maxW = 0.0f;
            float totalH = fontSize * mf->GetLineHeight();
            const char* ptr = text.data(); const char* end = ptr + text.size();
            while (ptr < end) {
                uint32_t cp = DecodeUTF8(ptr, end);
                if (cp == 0) break;
                if (cp == '\n') { maxW = std::max(maxW, currentW); currentW = 0.0f; totalH += fontSize * mf->GetLineHeight(); continue; }
                const FontMSDF::Glyph* g = mf->GetGlyph(cp);
                if (g) currentW += g->advance * scale; else currentW += fontSize * 0.3f;
            }
            return {std::max(maxW, currentW), totalH};
        }
        if (!fontLoaded) return {static_cast<float>(text.size()) * fontSize * 0.6f, fontSize};
        float scale = fontSize / fontPixelHeight; float currentW = 0.0f, maxW = 0.0f;
        float totalH = (fontLineHeight > 0 ? fontLineHeight : fontPixelHeight) * scale;
        const char* ptr = text.data(); const char* end = ptr + text.size();
        while (ptr < end) {
            uint32_t cp = DecodeUTF8(ptr, end);
            if (cp == 0) break;
            if (cp == '\n') { maxW = std::max(maxW, currentW); currentW = 0.0f; totalH += (fontLineHeight * scale); continue; }
            const Glyph* g = GetGlyph(cp);
            if (g) currentW += g->advance * scale;
        }
        return {std::max(maxW, currentW), totalH};
    }

    // Vertical advance per line, consistent with DrawText's multiline stepping
    // and MeasureText's per-line height. Routes through ActiveMSDF() so secondary
    // renderers (shared-device, brief 08) use the owner's font metrics.
    float Renderer::LineAdvancePx(float fontSize) {
        FontMSDF* mf = ActiveMSDF();
        if (mf && mf->IsLoaded()) return fontSize * mf->GetLineHeight();
        if (fontLoaded) {
            float scale = fontSize / fontPixelHeight;
            return (fontLineHeight > 0 ? fontLineHeight : fontPixelHeight) * scale;
        }
        return fontSize;
    }

    // Break text into lines, wrapping by word within maxWidth and honoring
    // explicit '\n'. Reuses MeasureText/GetGlyphAdvance for sizing (no rasterizing
    // here). outMaxWidth receives the widest resulting line.
    std::vector<std::string> Renderer::WrapTextLines(const std::string& text, float maxWidth,
                                                     float fontSize, float& outMaxWidth) {
        std::vector<std::string> lines;
        outMaxWidth = 0.0f;
        const float spaceW = GetGlyphAdvance(' ', fontSize);
        size_t start = 0;
        while (start <= text.size()) {
            size_t nl = text.find('\n', start);
            std::string paragraph = (nl == std::string::npos)
                                        ? text.substr(start)
                                        : text.substr(start, nl - start);
            std::string line;
            float lineW = 0.0f;
            bool anyWord = false;
            size_t wstart = 0;
            while (wstart <= paragraph.size()) {
                size_t sp = paragraph.find(' ', wstart);
                std::string word = (sp == std::string::npos)
                                       ? paragraph.substr(wstart)
                                       : paragraph.substr(wstart, sp - wstart);
                if (!word.empty()) {
                    float wordW = MeasureText(word, fontSize).x;
                    if (!anyWord) {
                        line = word; lineW = wordW; anyWord = true;
                    } else if (lineW + spaceW + wordW <= maxWidth) {
                        line += ' '; line += word; lineW += spaceW + wordW;
                    } else {
                        lines.push_back(line);
                        outMaxWidth = std::max(outMaxWidth, lineW);
                        line = word; lineW = wordW;
                    }
                }
                if (sp == std::string::npos) break;
                wstart = sp + 1;
            }
            lines.push_back(line);
            outMaxWidth = std::max(outMaxWidth, lineW);
            if (nl == std::string::npos) break;
            start = nl + 1;
        }
        return lines;
    }

    Vec2 Renderer::MeasureTextWrapped(const std::string& text, float maxWidth, float fontSize) {
        if (fontSize <= 0.0f) fontSize = 16.0f;
        float maxW = 0.0f;
        std::vector<std::string> lines = WrapTextLines(text, maxWidth, fontSize, maxW);
        return { maxW, LineAdvancePx(fontSize) * static_cast<float>(lines.size()) };
    }

    void Renderer::DrawTextWrapped(const Vec2& pos, const std::string& text, const Color& color,
                                   float maxWidth, float fontSize) {
        if (fontSize <= 0.0f) fontSize = 16.0f;
        float maxW = 0.0f;
        std::vector<std::string> lines = WrapTextLines(text, maxWidth, fontSize, maxW);
        float lineH = LineAdvancePx(fontSize);
        float y = pos.y;
        for (const std::string& ln : lines) {
            if (!ln.empty()) DrawText(Vec2(pos.x, y), ln, color, fontSize);
            y += lineH;
        }
    }

    float Renderer::GetFontAscender() const {
        FontMSDF* mf = ActiveMSDF();
        if (mf && mf->IsLoaded()) {
            return mf->GetAscender();
        }
        // Fallback razonable para fonts típicos (sans-serif estándar).
        return 0.8f;
    }

    // Issue 8: Public glyph advance accessor
    float Renderer::GetGlyphAdvance(uint32_t codepoint, float fontSize) {
        FontMSDF* mf = ActiveMSDF();
        if (mf && mf->IsLoaded()) {
            float scale = fontSize / mf->GetEmSize();
            const FontMSDF::Glyph* g = mf->GetGlyph(codepoint);
            if (g) return g->advance * scale;
            return fontSize * 0.3f;
        }
        if (!fontLoaded) return fontSize * 0.6f;
        float scale = fontSize / fontPixelHeight;
        const Glyph* g = GetGlyph(codepoint);
        if (g) return g->advance * scale;
        return fontSize * 0.6f;
    }

    const Renderer::Glyph* Renderer::GetGlyph(uint32_t cp) {
        // Secondary renderer: the owner is the sole writer of the shared bitmap atlas.
        if (!ownsFontResources) return FontOwner()->GetGlyph(cp);
        auto it = glyphCache.find(cp); if (it != glyphCache.end() && it->second.valid) return &it->second;
        if (LoadGlyph(cp)) return &glyphCache[cp];
        return nullptr;
    }

    bool Renderer::LoadGlyph(uint32_t cp) {
        if (!fontFace) return false;
        if (FT_Load_Char(fontFace, cp, FT_LOAD_RENDER | FT_LOAD_TARGET_NORMAL)) return false;
        FT_GlyphSlot slot = fontFace->glyph;
        int w = slot->bitmap.width, h = slot->bitmap.rows;
        int ax, ay; if (!EnsureAtlasSpace(w, h, ax, ay)) return false;
        if (w > 0 && h > 0) backend->UpdateTexture(fontAtlasTexture, ax, ay, w, h, slot->bitmap.buffer);
        auto& g = glyphCache[cp];
        g.advance = static_cast<float>(slot->advance.x) / 64.0f;
        g.bearing = Vec2(static_cast<float>(slot->bitmap_left), static_cast<float>(slot->bitmap_top));
        g.size = Vec2(static_cast<float>(w), static_cast<float>(h));
        g.uv0 = Vec2(static_cast<float>(ax) / atlasWidth, static_cast<float>(ay) / atlasHeight);
        g.uv1 = Vec2(static_cast<float>(ax + w) / atlasWidth, static_cast<float>(ay + h) / atlasHeight);
        g.valid = true; return true;
    }

    // ─── Icon Font (secondary) ─────────────────────────────────────────────
    bool Renderer::LoadIconFont(const std::string& filepath, int pixelHeight) {
        // Secondary renderer: load into the owner's shared atlas instead.
        if (!ownsFontResources) return FontOwner()->LoadIconFont(filepath, pixelHeight);
        std::filesystem::path resolved = ResolveResourcePath(filepath);
        if (resolved.empty()) {
            Log(LogLevel::Error, "Icon font not found: %s", filepath.c_str());
            return false;
        }
        if (FT_New_Face(ftLibrary, resolved.string().c_str(), 0, &iconFontFace)) {
            Log(LogLevel::Error, "Failed to load icon font: %s", resolved.string().c_str());
            return false;
        }
        FT_Set_Pixel_Sizes(iconFontFace, 0, pixelHeight);
        iconFontPixelHeight = static_cast<float>(pixelHeight);
        iconFontAscent = static_cast<float>(iconFontFace->size->metrics.ascender) / 64.0f;
        iconFontLoaded = true;
        Log(LogLevel::Info, "Icon font loaded: %s (%dpx)", resolved.string().c_str(), pixelHeight);
        return true;
    }

    bool Renderer::LoadIconGlyph(uint32_t cp) {
        if (!iconFontFace) return false;
        if (FT_Load_Char(iconFontFace, cp, FT_LOAD_RENDER | FT_LOAD_TARGET_NORMAL)) return false;
        FT_GlyphSlot slot = iconFontFace->glyph;
        int w = slot->bitmap.width, h = slot->bitmap.rows;
        int ax, ay; if (!EnsureAtlasSpace(w, h, ax, ay)) return false;
        if (w > 0 && h > 0) backend->UpdateTexture(fontAtlasTexture, ax, ay, w, h, slot->bitmap.buffer);
        auto& g = iconGlyphCache[cp];
        g.advance = static_cast<float>(slot->advance.x) / 64.0f;
        g.bearing = Vec2(static_cast<float>(slot->bitmap_left), static_cast<float>(slot->bitmap_top));
        g.size = Vec2(static_cast<float>(w), static_cast<float>(h));
        g.uv0 = Vec2(static_cast<float>(ax) / atlasWidth, static_cast<float>(ay) / atlasHeight);
        g.uv1 = Vec2(static_cast<float>(ax + w) / atlasWidth, static_cast<float>(ay + h) / atlasHeight);
        g.valid = true; return true;
    }

    const Renderer::Glyph* Renderer::GetIconGlyph(uint32_t cp) {
        // Secondary renderer: icon glyphs live in the owner's shared bitmap atlas.
        if (!ownsFontResources) return FontOwner()->GetIconGlyph(cp);
        auto it = iconGlyphCache.find(cp);
        if (it != iconGlyphCache.end() && it->second.valid) return &it->second;
        if (LoadIconGlyph(cp)) return &iconGlyphCache[cp];
        return nullptr;
    }

    void Renderer::DrawIconGlyph(const Vec2& pos, uint32_t codepoint, const Color& color, float fontSize) {
        if (!iconFontLoaded) return;
        const Glyph* g = GetIconGlyph(codepoint);
        if (!g) return;

        float scale = fontSize / iconFontPixelHeight;
        float x0 = std::round(pos.x + g->bearing.x * scale);
        float y0 = std::round(pos.y + (iconFontAscent - g->bearing.y) * scale);
        float w = g->size.x * scale;
        float h = g->size.y * scale;

        EnsureBatchState(ShaderType::Text, fontAtlasTexture, color);
        if (quadVertices.size() + 4 > MAX_QUAD_VERTICES) FlushBatch();

        unsigned int base = static_cast<unsigned int>(quadVertices.size());
        quadVertices.push_back({x0,     y0,     color.r, color.g, color.b, color.a, g->uv0.x, g->uv0.y});
        quadVertices.push_back({x0 + w, y0,     color.r, color.g, color.b, color.a, g->uv1.x, g->uv0.y});
        quadVertices.push_back({x0 + w, y0 + h, color.r, color.g, color.b, color.a, g->uv1.x, g->uv1.y});
        quadVertices.push_back({x0,     y0 + h, color.r, color.g, color.b, color.a, g->uv0.x, g->uv1.y});
        quadIndices.push_back(base); quadIndices.push_back(base+1); quadIndices.push_back(base+2);
        quadIndices.push_back(base); quadIndices.push_back(base+2); quadIndices.push_back(base+3);
    }

    bool Renderer::EnsureAtlasSpace(int w, int h, int& ox, int& oy) {
        int pad = 2;
        if (atlasNextX + w + pad > atlasWidth) { atlasNextX = 0; atlasNextY += atlasCurrentRowHeight + pad; atlasCurrentRowHeight = 0; }
        if (atlasNextY + h + pad > atlasHeight) return false;
        ox = atlasNextX + pad; oy = atlasNextY + pad;
        atlasNextX += w + pad; atlasCurrentRowHeight = std::max(atlasCurrentRowHeight, h);
        return true;
    }

    const Renderer::Glyph* Renderer::GetOrGenerateMSDFGlyph(uint32_t cp) {
        // Secondary renderer: the owner is the sole writer of the shared dynamic atlas.
        if (!ownsFontResources) return FontOwner()->GetOrGenerateMSDFGlyph(cp);
        auto it = dynamicMSDFGlyphCache.find(cp);
        if (it != dynamicMSDFGlyphCache.end() && it->second.valid) {
            it->second.lastAccessFrame = glyphCacheFrame; // Perf R6: touch for LRU
            return &it->second;
        }
        if (GenerateMSDFGlyph(cp)) {
            dynamicMSDFGlyphCache[cp].lastAccessFrame = glyphCacheFrame;
            return &dynamicMSDFGlyphCache[cp];
        }
        return nullptr;
    }

    bool Renderer::GenerateMSDFGlyph(uint32_t cp) {
        if (!msdfGenerator || !fontFace) return false;
        FT_UInt idx = FT_Get_Char_Index(fontFace, cp); if (idx == 0) return false;
        auto data = msdfGenerator->GenerateFromGlyph(fontFace, idx, MSDF_GLYPH_SIZE, 4.0f, 4);
        if (!data) return false;
        int ax, ay; if (!EnsureDynamicMSDFAtlasSpace(data->width, data->height, ax, ay)) return false;
        backend->UpdateTexture(dynamicMSDFAtlasTexture, ax, ay, data->width, data->height, data->pixels.data());
        FT_Load_Glyph(fontFace, idx, FT_LOAD_NO_BITMAP);
        auto& g = dynamicMSDFGlyphCache[cp];
        g.size = Vec2(static_cast<float>(data->width), static_cast<float>(data->height));
        g.bearing = Vec2(static_cast<float>(fontFace->glyph->metrics.horiBearingX)/64.0f, static_cast<float>(fontFace->glyph->metrics.horiBearingY)/64.0f);
        g.advance = static_cast<float>(fontFace->glyph->metrics.horiAdvance)/64.0f;
        g.uv0 = Vec2(static_cast<float>(ax)/DYNAMIC_ATLAS_SIZE, static_cast<float>(ay)/DYNAMIC_ATLAS_SIZE);
        g.uv1 = Vec2(static_cast<float>(ax+data->width)/DYNAMIC_ATLAS_SIZE, static_cast<float>(ay+data->height)/DYNAMIC_ATLAS_SIZE);
        g.valid = true; return true;
    }

    bool Renderer::EnsureDynamicMSDFAtlasSpace(int w, int h, int& ox, int& oy) {
        int pad = 2;
        if (dynamicAtlasNextX + w + pad > dynamicAtlasWidth) {
            dynamicAtlasNextX = pad;
            dynamicAtlasNextY += dynamicAtlasCurrentRowHeight + pad;
            dynamicAtlasCurrentRowHeight = 0;
        }
        if (dynamicAtlasNextY + h + pad > dynamicAtlasHeight) {
            // Dynamic Atlas Growth — create a larger atlas and re-generate glyphs
            int newHeight = dynamicAtlasHeight * 2;
            if (newHeight > 8192) return false; // Max 8192px

            void* newAtlas = backend->CreateTexture(dynamicAtlasWidth, newHeight, nullptr, false);
            if (!newAtlas) return false;

            backend->DeleteTexture(dynamicMSDFAtlasTexture);
            dynamicMSDFAtlasTexture = newAtlas;
            dynamicAtlasHeight = newHeight;

            dynamicAtlasNextX = pad;
            dynamicAtlasNextY = pad;
            dynamicAtlasCurrentRowHeight = 0;

            auto oldGlyphs = std::move(dynamicMSDFGlyphCache);
            dynamicMSDFGlyphCache.clear();
            for (auto& [cp, _] : oldGlyphs) {
                GenerateMSDFGlyph(cp);
            }

            if (dynamicAtlasNextX + w + pad > dynamicAtlasWidth) {
                dynamicAtlasNextX = pad;
                dynamicAtlasNextY += dynamicAtlasCurrentRowHeight + pad;
                dynamicAtlasCurrentRowHeight = 0;
            }
            if (dynamicAtlasNextY + h + pad > dynamicAtlasHeight) return false;
        }
        ox = dynamicAtlasNextX; oy = dynamicAtlasNextY;
        dynamicAtlasNextX += w + pad; dynamicAtlasCurrentRowHeight = std::max(dynamicAtlasCurrentRowHeight, h);
        return true;
    }

    void Renderer::InitializeDefaultFont() {
        std::filesystem::path atlasPng = ResolveResourcePath("assets/fonts/atlas.png");
        std::filesystem::path atlasJson = ResolveResourcePath("assets/fonts/atlas.json");

        bool textReady = false;
        if (!atlasPng.empty() && !atlasJson.empty()) {
            if (msdfFont->Load(atlasPng.string(), atlasJson.string())) {
                Log(LogLevel::Info, "MSDF Font loaded successfully from: %s", atlasPng.string().c_str());
                textReady = true;
            }
        }

        if (!textReady) {
            Log(LogLevel::Error, "Could not load MSDF font, falling back to System font.");
#if defined(_WIN32)
            LoadFont("C:/Windows/Fonts/segoeui.ttf", 14);
#elif defined(__APPLE__)
            LoadFont("/System/Library/Fonts/SFNS.ttf", 14);
#else
            LoadFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 14);
#endif
        }

        // Auto-load the Lucide icon font here, at the renderer level, so icon
        // glyphs render on EVERY entry point — FluentApp, the standalone gallery,
        // or an external-GL host — not only when FluentApp's constructor runs.
        // FluentApp may still override this with an explicit path afterwards.
        if (!iconFontLoaded) {
            LoadIconFont("assets/fonts/lucide.ttf", 32);
        }
    }

    void Renderer::PushClipRect(const Vec2& pos, const Vec2& size) {
        // Perf Phase C
        if (perfCounters.clipPushes) (*perfCounters.clipPushes)++;
        // Perf 2.5: Only flush if clip rect differs from current batch's clip
        // The batch system already stores clip per batch, so we only need to flush
        // when there are pending vertices with a different clip state
        ClipRect rect = { static_cast<int>(pos.x), static_cast<int>(pos.y), static_cast<int>(size.x), static_cast<int>(size.y) };
        if (!clipStack.empty()) {
            auto& top = clipStack.back();
            int l = std::max(top.x, rect.x); int t = std::max(top.y, rect.y);
            int r = std::min(top.x + top.width, rect.x + rect.width); int b = std::min(top.y + top.height, rect.y + rect.height);
            rect = { l, t, std::max(0, r-l), std::max(0, b-t) };
        }
        // Perf R4: Skip flush if new clip rect matches current
        if (!clipStack.empty()) {
            auto& cur = clipStack.back();
            if (cur.x == rect.x && cur.y == rect.y &&
                cur.width == rect.width && cur.height == rect.height) {
                // Identical clip — push duplicate without flushing
                clipStack.push_back(rect);
                if (backend) backend->PushClipRect(rect.x, rect.y, rect.width, rect.height);
                return;
            }
        }
        // Only flush if we have pending vertices (changing clip mid-batch)
        if (!quadVertices.empty() || !lineVertices.empty()) FlushBatch();
        clipStack.push_back(rect);
        if (backend) backend->PushClipRect(rect.x, rect.y, rect.width, rect.height);
    }

    void Renderer::PopClipRect() {
        if (clipStack.empty()) return;
        // Only flush if we have pending vertices
        if (!quadVertices.empty() || !lineVertices.empty()) FlushBatch();
        clipStack.pop_back();
        if (backend) backend->PopClipRect();
    }

    // --- Path API (Phase A1) ---
    void Renderer::PathClear() {
        pathPoints.clear();
    }

    void Renderer::PathLineTo(const Vec2& p) {
        pathPoints.push_back(p);
    }

    void Renderer::PathArcTo(const Vec2& center, float radius, float a_min, float a_max, int num_segments) {
        if (radius <= 0.0f) {
            pathPoints.push_back(center);
            return;
        }
        if (num_segments <= 0) {
            // Auto: ~12 segments per quarter at radius~32, scale by arc length
            float arc = std::abs(a_max - a_min);
            num_segments = std::max(2, static_cast<int>(std::ceil(arc * radius * 0.25f)));
            num_segments = std::min(num_segments, 64);
        }
        pathPoints.reserve(pathPoints.size() + static_cast<size_t>(num_segments + 1));
        for (int i = 0; i <= num_segments; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(num_segments);
            float a = a_min + (a_max - a_min) * t;
            pathPoints.push_back({center.x + std::cos(a) * radius, center.y + std::sin(a) * radius});
        }
    }

    void Renderer::PathArcToFast(const Vec2& center, float radius, int a_min12, int a_max12) {
        // 12 steps == 90° (a quarter), so 48 steps == 360°. We use CORNER_POINTS-1 segments per quarter
        // sampled from the precomputed cosTable/sinTable; for arbitrary 12-step ranges we compute the angle.
        if (radius <= 0.0f) { pathPoints.push_back(center); return; }
        if (a_min12 == a_max12) return;
        const float pi = 3.14159265358979323846f;
        int steps = std::abs(a_max12 - a_min12);
        int num_segments = std::max(2, steps * 3); // ~3 sub-steps per "12-unit"
        pathPoints.reserve(pathPoints.size() + static_cast<size_t>(num_segments + 1));
        for (int i = 0; i <= num_segments; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(num_segments);
            float step = static_cast<float>(a_min12) + (static_cast<float>(a_max12 - a_min12)) * t;
            float a = (step / 48.0f) * 2.0f * pi; // 48 steps = 360°
            pathPoints.push_back({center.x + std::cos(a) * radius, center.y + std::sin(a) * radius});
        }
    }

    void Renderer::PathBezierCubicCurveTo(const Vec2& p2, const Vec2& p3, const Vec2& p4, int segments) {
        if (pathPoints.empty()) return;
        if (segments <= 0) segments = 16;
        Vec2 p1 = pathPoints.back();
        for (int i = 1; i <= segments; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(segments);
            float u = 1.0f - t;
            float u2 = u * u, u3 = u2 * u;
            float t2 = t * t, t3 = t2 * t;
            pathPoints.push_back({
                u3 * p1.x + 3.0f * u2 * t * p2.x + 3.0f * u * t2 * p3.x + t3 * p4.x,
                u3 * p1.y + 3.0f * u2 * t * p2.y + 3.0f * u * t2 * p3.y + t3 * p4.y
            });
        }
    }

    void Renderer::PathRect(const Vec2& p_min, const Vec2& p_max, float rounding) {
        float w = p_max.x - p_min.x;
        float h = p_max.y - p_min.y;
        float r = std::min(rounding, std::min(w, h) * 0.5f);
        if (r < 0.5f) {
            pathPoints.push_back(p_min);
            pathPoints.push_back({p_max.x, p_min.y});
            pathPoints.push_back(p_max);
            pathPoints.push_back({p_min.x, p_max.y});
            return;
        }
        // CCW order matching how DrawRectFilled rounded constructs perimeters:
        // top-right corner, bottom-right, bottom-left, top-left.
        const float pi = 3.14159265358979323846f;
        Vec2 c_tr(p_max.x - r, p_min.y + r);
        Vec2 c_br(p_max.x - r, p_max.y - r);
        Vec2 c_bl(p_min.x + r, p_max.y - r);
        Vec2 c_tl(p_min.x + r, p_min.y + r);
        // Each arc spans 90° (pi/2). Use CORNER_SEGMENTS (8) per corner.
        PathArcTo(c_tr, r, -pi * 0.5f, 0.0f,    CORNER_SEGMENTS);
        PathArcTo(c_br, r,  0.0f,       pi * 0.5f, CORNER_SEGMENTS);
        PathArcTo(c_bl, r,  pi * 0.5f,  pi,        CORNER_SEGMENTS);
        PathArcTo(c_tl, r,  pi,         pi * 1.5f, CORNER_SEGMENTS);
    }

    // Compute outward unit normals at each path point as the bisector of adjacent edges.
    // Assumes a CCW (counter-clockwise) polygon, where outward = right-perpendicular of the
    // forward edge direction. For CW polygons the result will be inward — caller must orient.
    static void ComputeOutwardNormals(const Vec2* points, int count, std::vector<Vec2>& normals) {
        normals.resize(count);
        if (count < 2) {
            for (int i = 0; i < count; ++i) normals[i] = Vec2(0, 0);
            return;
        }
        // First pass: per-edge unit normals (right-perp of forward direction)
        std::vector<Vec2> edgeN(count);
        for (int i = 0; i < count; ++i) {
            int j = (i + 1) % count;
            Vec2 d = points[j] - points[i];
            float len = std::sqrt(d.x * d.x + d.y * d.y);
            if (len < 1e-6f) { edgeN[i] = Vec2(0, 0); continue; }
            // Right-perpendicular for CCW polygon points outward: (dy, -dx)/len
            edgeN[i] = Vec2(d.y / len, -d.x / len);
        }
        // Second pass: average of incoming and outgoing edge normals, scaled by miter factor
        for (int i = 0; i < count; ++i) {
            int prev = (i + count - 1) % count;
            Vec2 n = edgeN[prev] + edgeN[i];
            float lenSq = n.x * n.x + n.y * n.y;
            if (lenSq < 1e-6f) { normals[i] = edgeN[i]; continue; }
            float invLen = 1.0f / std::sqrt(lenSq);
            n.x *= invLen; n.y *= invLen;
            // Miter scale: 1/cos(half-angle) so that perpendicular distance to each edge = AA_FRINGE
            float dotV = n.x * edgeN[i].x + n.y * edgeN[i].y;
            float scale = (dotV > 0.5f) ? (1.0f / dotV) : 2.0f; // clamp at 2x to avoid spikes
            normals[i] = Vec2(n.x * scale, n.y * scale);
        }
    }

    void Renderer::EmitConvexFanWithFringe(const Vec2* points, int count, const Color& color) {
        if (count < 3) return;
        EnsureBatchState(ShaderType::Basic, nullptr, Color(1,1,1,1));
        const float AA_FRINGE = std::max(1.0f, dpiScale); // Phase A4: 1 physical pixel
        if (quadVertices.size() + 1 + 2 * static_cast<size_t>(count) > MAX_QUAD_VERTICES) FlushBatch();

        std::vector<Vec2> normals;
        ComputeOutwardNormals(points, count, normals);

        // Compute centroid for fan center
        float cx = 0.0f, cy = 0.0f;
        for (int i = 0; i < count; ++i) { cx += points[i].x; cy += points[i].y; }
        cx /= count; cy /= count;

        unsigned int centerIdx = static_cast<unsigned int>(quadVertices.size());
        quadVertices.push_back({cx, cy, color.r, color.g, color.b, color.a, 0, 0});
        for (int i = 0; i < count; ++i)
            quadVertices.push_back({points[i].x, points[i].y, color.r, color.g, color.b, color.a, 0, 0});
        for (int i = 0; i < count; ++i)
            quadVertices.push_back({points[i].x + normals[i].x * AA_FRINGE,
                                     points[i].y + normals[i].y * AA_FRINGE,
                                     color.r, color.g, color.b, 0.0f, 0, 0});

        unsigned int innerBase = centerIdx + 1;
        unsigned int outerBase = centerIdx + 1 + static_cast<unsigned int>(count);
        for (int i = 0; i < count; ++i) {
            unsigned int next = static_cast<unsigned int>((i + 1) % count);
            quadIndices.push_back(centerIdx);
            quadIndices.push_back(innerBase + static_cast<unsigned int>(i));
            quadIndices.push_back(innerBase + next);
            quadIndices.push_back(innerBase + static_cast<unsigned int>(i));
            quadIndices.push_back(innerBase + next);
            quadIndices.push_back(outerBase + next);
            quadIndices.push_back(innerBase + static_cast<unsigned int>(i));
            quadIndices.push_back(outerBase + next);
            quadIndices.push_back(outerBase + static_cast<unsigned int>(i));
        }
    }

    void Renderer::PathFillConvex(const Color& color) {
        if (pathPoints.size() < 3) { pathPoints.clear(); return; }
        EmitConvexFanWithFringe(pathPoints.data(), static_cast<int>(pathPoints.size()), color);
        pathPoints.clear();
    }

    void Renderer::StrokePolyline(const Vec2* points, int count, bool closed,
                                    const Color& color, float thickness) {
        if (count < 2) return;
        EnsureBatchState(ShaderType::Basic, nullptr, Color(1,1,1,1));

        const float AA_FRINGE = std::max(1.0f, dpiScale); // Phase A4: 1 physical pixel
        float halfCore = std::max(0.5f, thickness * 0.5f);
        float halfOuter = halfCore + AA_FRINGE;

        // Per-edge unit normal (left-perp of forward direction).
        // Number of edges is count-1 (open) or count (closed).
        int numEdges = closed ? count : count - 1;
        std::vector<Vec2> edgeN(numEdges);
        for (int i = 0; i < numEdges; ++i) {
            int j = (i + 1) % count;
            Vec2 d = points[j] - points[i];
            float len = std::sqrt(d.x * d.x + d.y * d.y);
            if (len < 1e-6f) { edgeN[i] = Vec2(0, 0); continue; }
            edgeN[i] = Vec2(-d.y / len, d.x / len);
        }

        // Per-vertex bisector with miter scale (1/cos(half_angle)) clamped to avoid spikes.
        std::vector<Vec2> bisector(count);
        for (int i = 0; i < count; ++i) {
            Vec2 nIn, nOut;
            int eOut = i;             // outgoing edge index = i
            int eIn = i - 1;          // incoming edge index = i-1

            if (closed) {
                if (eIn < 0) eIn = numEdges - 1;
                if (eOut >= numEdges) eOut = 0;
                nIn = edgeN[eIn];
                nOut = edgeN[eOut];
            } else {
                // Open: first vertex has no incoming edge, last has no outgoing edge
                if (i == 0) { nIn = edgeN[0]; nOut = edgeN[0]; }
                else if (i == count - 1) { nIn = edgeN[numEdges - 1]; nOut = edgeN[numEdges - 1]; }
                else { nIn = edgeN[eIn]; nOut = edgeN[eOut]; }
            }

            Vec2 b = nIn + nOut;
            float lenSq = b.x * b.x + b.y * b.y;
            if (lenSq < 1e-6f) {
                bisector[i] = nOut;
            } else {
                float invLen = 1.0f / std::sqrt(lenSq);
                b.x *= invLen; b.y *= invLen;
                float dotV = b.x * nOut.x + b.y * nOut.y;
                float scale = (dotV > 0.5f) ? (1.0f / dotV) : 2.0f;
                bisector[i] = Vec2(b.x * scale, b.y * scale);
            }
        }

        // Emit 4 vertices per path point: outer-left, inner-left, inner-right, outer-right.
        // Triangulate as 3 quad strips (outer-left fringe, core, outer-right fringe) between
        // each consecutive pair of path points.
        if (quadVertices.size() + static_cast<size_t>(count) * 4 > MAX_QUAD_VERTICES) FlushBatch();

        unsigned int base = static_cast<unsigned int>(quadVertices.size());
        for (int i = 0; i < count; ++i) {
            const Vec2& p = points[i];
            const Vec2& b = bisector[i];
            // 0: outer-left fringe (alpha 0)
            quadVertices.push_back({p.x + b.x * halfOuter, p.y + b.y * halfOuter,
                                     color.r, color.g, color.b, 0.0f, 0, 0});
            // 1: inner-left core
            quadVertices.push_back({p.x + b.x * halfCore, p.y + b.y * halfCore,
                                     color.r, color.g, color.b, color.a, 0, 0});
            // 2: inner-right core
            quadVertices.push_back({p.x - b.x * halfCore, p.y - b.y * halfCore,
                                     color.r, color.g, color.b, color.a, 0, 0});
            // 3: outer-right fringe (alpha 0)
            quadVertices.push_back({p.x - b.x * halfOuter, p.y - b.y * halfOuter,
                                     color.r, color.g, color.b, 0.0f, 0, 0});
        }

        int segCount = closed ? count : count - 1;
        for (int s = 0; s < segCount; ++s) {
            unsigned int a = base + static_cast<unsigned int>(s) * 4;
            unsigned int c = base + static_cast<unsigned int>((s + 1) % count) * 4;
            // Outer-left fringe quad: a+0, a+1, c+1, c+0
            quadIndices.push_back(a + 0); quadIndices.push_back(a + 1); quadIndices.push_back(c + 1);
            quadIndices.push_back(a + 0); quadIndices.push_back(c + 1); quadIndices.push_back(c + 0);
            // Core quad: a+1, a+2, c+2, c+1
            quadIndices.push_back(a + 1); quadIndices.push_back(a + 2); quadIndices.push_back(c + 2);
            quadIndices.push_back(a + 1); quadIndices.push_back(c + 2); quadIndices.push_back(c + 1);
            // Outer-right fringe quad: a+2, a+3, c+3, c+2
            quadIndices.push_back(a + 2); quadIndices.push_back(a + 3); quadIndices.push_back(c + 3);
            quadIndices.push_back(a + 2); quadIndices.push_back(c + 3); quadIndices.push_back(c + 2);
        }
    }

    void Renderer::PathStroke(const Color& color, bool closed, float thickness) {
        if (pathPoints.size() < 2) { pathPoints.clear(); return; }
        StrokePolyline(pathPoints.data(), static_cast<int>(pathPoints.size()),
                       closed, color, thickness);
        pathPoints.clear();
    }

    // --- Bezier Curve (Phase 5) ---
    void Renderer::DrawBezier(const Vec2& p0, const Vec2& p1, const Vec2& p2, const Vec2& p3,
                               const Color& color, float thickness, int segments) {
        if (segments < 2) segments = 2;
        // Tessellate into a polyline and emit as a single continuous stroke (Phase A2).
        std::vector<Vec2> pts;
        pts.reserve(static_cast<size_t>(segments) + 1);
        pts.push_back(p0);
        for (int i = 1; i <= segments; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(segments);
            float u = 1.0f - t;
            float u2 = u * u;
            float u3 = u2 * u;
            float t2 = t * t;
            float t3 = t2 * t;
            pts.push_back({
                u3 * p0.x + 3.0f * u2 * t * p1.x + 3.0f * u * t2 * p2.x + t3 * p3.x,
                u3 * p0.y + 3.0f * u2 * t * p1.y + 3.0f * u * t2 * p2.y + t3 * p3.y
            });
        }
        StrokePolyline(pts.data(), static_cast<int>(pts.size()), false, color, thickness);
    }

    // --- Multi-font DrawText (Phase 5) ---
    void Renderer::DrawTextWithFont(const Vec2& pos, const std::string& text, const Color& color,
                                     const std::string& fontName, float fontSize) {
        auto* font = FontMgr().GetFont(fontName);
        if (!font || !font->loaded || !font->atlasTexture) {
            // Fallback to default rendering
            DrawText(pos, text, color, fontSize);
            return;
        }

        EnsureBatchState(ShaderType::Text, font->atlasTexture, color);

        float scale = fontSize / font->pixelHeight;
        float baseline = pos.y + font->ascent * scale;
        float penX = pos.x;

        const char* ptr = text.data();
        const char* end = ptr + text.size();
        while (ptr < end) {
            uint32_t cp = DecodeUTF8(ptr, end);
            if (cp == 0) break;
            if (cp == '\n') {
                penX = pos.x;
                baseline += font->lineHeight * scale;
                continue;
            }
            auto* g = FontMgr().GetGlyph(font, cp);
            if (!g) {
                penX += fontSize * 0.3f;
                continue;
            }
            float x0 = std::round(penX + g->bearing.x * scale);
            float y0 = baseline - g->bearing.y * scale;
            float w = g->size.x * scale;
            float h = g->size.y * scale;
            if (w > 0 && h > 0) {
                if (quadVertices.size() + 4 > MAX_QUAD_VERTICES - 4)
                    FlushBatch(ShaderType::Text, font->atlasTexture, color);
                unsigned int base = static_cast<unsigned int>(quadVertices.size());
                quadVertices.push_back({x0, y0, 1,1,1,1, g->uv0.x, g->uv1.y});
                quadVertices.push_back({x0+w, y0, 1,1,1,1, g->uv1.x, g->uv1.y});
                quadVertices.push_back({x0+w, y0+h, 1,1,1,1, g->uv1.x, g->uv0.y});
                quadVertices.push_back({x0, y0+h, 1,1,1,1, g->uv0.x, g->uv0.y});
                quadIndices.push_back(base); quadIndices.push_back(base+1); quadIndices.push_back(base+2);
                quadIndices.push_back(base); quadIndices.push_back(base+2); quadIndices.push_back(base+3);
            }
            penX += g->advance * scale;
        }
    }

    Vec2 Renderer::MeasureTextWithFont(const std::string& text, const std::string& fontName, float fontSize) {
        return FontMgr().MeasureText(fontName, text, fontSize);
    }

} // namespace FluentUI
