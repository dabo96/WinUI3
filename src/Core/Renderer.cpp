#include "core/Renderer.h"
#include "core/Context.h"
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
        // Destroy font objects first while backend is still valid
        fontManager.Shutdown();
        msdfFont.reset();
        msdfGenerator.reset();
        ClearGlyphs();
        if (fontFace) { FT_Done_Face(fontFace); fontFace = nullptr; }
        if (ftLibrary) { FT_Done_FreeType(ftLibrary); ftLibrary = nullptr; }
        if (fontAtlasTexture && backend) backend->DeleteTexture(fontAtlasTexture);
        if (dynamicMSDFAtlasTexture && backend) backend->DeleteTexture(dynamicMSDFAtlasTexture);
        fontAtlasTexture = nullptr; dynamicMSDFAtlasTexture = nullptr;
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
        for (int i = 0; i < (int)RenderLayer::Count; ++i) {
            layerBatches[i].clear();
        }
        currentLayer = RenderLayer::Default;
        // Issue 1: Reset batch state
        currentBatchShader = ShaderType::Basic;
        currentBatchTexture = nullptr;
        currentBatchTextColor = Color(1,1,1,1);

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
                    if (perfCounters.vertexCount) (*perfCounters.vertexCount) += static_cast<uint32_t>(batch.vertices.size());
                    if (perfCounters.indexCount) (*perfCounters.indexCount) += static_cast<uint32_t>(batch.indices.size());

                    // Restaurar clipping del batch
                    if (batch.hasClip) {
                        backend->PushClipRect(batch.clipRect.x, batch.clipRect.y, batch.clipRect.width, batch.clipRect.height);
                    }

                    if (batch.isLines) {
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
        if (!quadVertices.empty()) {
            if (currentBatchShader != shader || currentBatchTexture != texture ||
                currentBatchTextColor.r != color.r || currentBatchTextColor.g != color.g ||
                currentBatchTextColor.b != color.b || currentBatchTextColor.a != color.a) {
                FlushBatch(currentBatchShader, currentBatchTexture, currentBatchTextColor);
                // Perf Phase C: Count state changes
                if (perfCounters.stateChanges) (*perfCounters.stateChanges)++;
            }
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
        if (quadVertices.empty() && lineVertices.empty()) return;

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
                if (!last.isLines && last.type == type && last.texture == texture &&
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
        // Issue 1: Ensure we're in Basic batch state for lines
        EnsureBatchState(ShaderType::Basic, nullptr, Color(1,1,1,1));
        if (!lineVertices.empty() && std::abs(width - lineBatchWidth) > 0.01f) FlushBatch();
        if (lineVertices.size() + 2 > MAX_LINE_VERTICES) FlushBatch();
        lineBatchWidth = width;
        lineVertices.push_back({ start.x, start.y, color.r, color.g, color.b, color.a, 0, 0 });
        lineVertices.push_back({ end.x,   end.y,   color.r, color.g, color.b, color.a, 0, 0 });
    }

    void Renderer::DrawRect(const Vec2& pos, const Vec2& size, const Color& color, float cornerRadius) {
        float lineWidth = 1.0f;
        if (cornerRadius < 0.5f) {
            DrawLine(pos, Vec2(pos.x + size.x, pos.y), color, lineWidth);
            DrawLine(Vec2(pos.x + size.x, pos.y), Vec2(pos.x + size.x, pos.y + size.y), color, lineWidth);
            DrawLine(Vec2(pos.x + size.x, pos.y + size.y), Vec2(pos.x, pos.y + size.y), color, lineWidth);
            DrawLine(Vec2(pos.x, pos.y + size.y), pos, color, lineWidth);
        } else {
            // Perf 3.1: Use pre-computed trig tables
            float cr = std::min(cornerRadius, std::min(size.x, size.y) * 0.5f);
            float cx_left = pos.x + cr; float cx_right = pos.x + size.x - cr;
            float cy_top = pos.y + cr; float cy_bot = pos.y + size.y - cr;
            constexpr int PTS = CORNER_POINTS * 4;
            Vec2 points[PTS];
            int pi = 0;
            for (int i = 0; i < CORNER_POINTS; ++i)
                points[pi++] = {cx_right + cr * sinTable[i], cy_top - cr * cosTable[i]};
            for (int i = 0; i < CORNER_POINTS; ++i)
                points[pi++] = {cx_right + cr * cosTable[i], cy_bot + cr * sinTable[i]};
            for (int i = 0; i < CORNER_POINTS; ++i)
                points[pi++] = {cx_left - cr * sinTable[i], cy_bot + cr * cosTable[i]};
            for (int i = 0; i < CORNER_POINTS; ++i)
                points[pi++] = {cx_left - cr * cosTable[i], cy_top - cr * sinTable[i]};
            for (int i = 0; i < PTS; ++i) {
                DrawLine(points[i], points[(i + 1) % PTS], color, lineWidth);
            }
        }
    }

    void Renderer::DrawRectFilled(const Vec2& pos, const Vec2& size, const Color& color, float cornerRadius) {
        // Issue 1: Ensure Basic batch state
        EnsureBatchState(ShaderType::Basic, nullptr, Color(1,1,1,1));

        if (cornerRadius < 0.5f) {
            if (quadVertices.size() + 4 > MAX_QUAD_VERTICES) FlushBatch();
            unsigned int baseIndex = static_cast<unsigned int>(quadVertices.size());
            quadVertices.push_back({pos.x, pos.y, color.r, color.g, color.b, color.a, 0, 0});
            quadVertices.push_back({pos.x + size.x, pos.y, color.r, color.g, color.b, color.a, 0, 0});
            quadVertices.push_back({pos.x + size.x, pos.y + size.y, color.r, color.g, color.b, color.a, 0, 0});
            quadVertices.push_back({pos.x, pos.y + size.y, color.r, color.g, color.b, color.a, 0, 0});
            quadIndices.push_back(baseIndex + 0); quadIndices.push_back(baseIndex + 1); quadIndices.push_back(baseIndex + 2);
            quadIndices.push_back(baseIndex + 0); quadIndices.push_back(baseIndex + 2); quadIndices.push_back(baseIndex + 3);
        } else {
            // Perf 3.1: Use pre-computed trig tables instead of cos/sin per vertex
            // cosTable[i] = cos(i * pi/2 / N), sinTable[i] = sin(i * pi/2 / N)
            // where i goes from 0..N (N=CORNER_SEGMENTS=8)
            // cos goes 1→0, sin goes 0→1 over the quarter circle
            float cr = std::min(cornerRadius, std::min(size.x, size.y) * 0.5f);
            float cx_left = pos.x + cr; float cx_right = pos.x + size.x - cr;
            float cy_top = pos.y + cr; float cy_bot = pos.y + size.y - cr;
            // Perf R6: Fixed-size array instead of thread_local vector
            constexpr int PERIM_SIZE = CORNER_POINTS * 4; // 36 points
            Vec2 perim[PERIM_SIZE];
            int pi = 0;
            for (int i = 0; i < CORNER_POINTS; ++i)
                perim[pi++] = {cx_right + cr * sinTable[i], cy_top - cr * cosTable[i]};
            for (int i = 0; i < CORNER_POINTS; ++i)
                perim[pi++] = {cx_right + cr * cosTable[i], cy_bot + cr * sinTable[i]};
            for (int i = 0; i < CORNER_POINTS; ++i)
                perim[pi++] = {cx_left - cr * sinTable[i], cy_bot + cr * cosTable[i]};
            for (int i = 0; i < CORNER_POINTS; ++i)
                perim[pi++] = {cx_left - cr * cosTable[i], cy_top - cr * sinTable[i]};
            if (quadVertices.size() + PERIM_SIZE + 1 > MAX_QUAD_VERTICES) FlushBatch();
            unsigned int centerIdx = static_cast<unsigned int>(quadVertices.size());
            quadVertices.push_back({pos.x + size.x * 0.5f, pos.y + size.y * 0.5f, color.r, color.g, color.b, color.a, 0, 0});
            for (int i = 0; i < PERIM_SIZE; ++i) quadVertices.push_back({perim[i].x, perim[i].y, color.r, color.g, color.b, color.a, 0, 0});
            for (int i = 0; i < PERIM_SIZE; ++i) {
                quadIndices.push_back(centerIdx);
                quadIndices.push_back(centerIdx + 1 + static_cast<unsigned int>(i));
                quadIndices.push_back(centerIdx + 1 + static_cast<unsigned int>((i + 1) % PERIM_SIZE));
            }
        }
    }

    void Renderer::DrawRectWithElevation(const Vec2& pos, const Vec2& size, const Color& color, float cornerRadius, float elevation) {
        if (elevation > 0.0f) {
            float ambientOpacity = std::min(0.12f, elevation * 0.006f);
            Vec2 ambientExpand(elevation * 0.8f, elevation * 0.8f);
            DrawRectFilled(pos - ambientExpand * 0.5f + Vec2(0.0f, elevation * 0.15f), size + ambientExpand, Color(0,0,0,ambientOpacity), cornerRadius + elevation * 0.3f);
            float keyOpacity = std::min(0.20f, elevation * 0.012f);
            Vec2 keyExpand(elevation * 0.3f, elevation * 0.3f);
            DrawRectFilled(pos - keyExpand * 0.5f + Vec2(0.0f, elevation * 0.4f), size + keyExpand, Color(0,0,0,keyOpacity), cornerRadius + elevation * 0.15f);
        }
        DrawRectFilled(pos, size, color, cornerRadius);
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

    // Issue 15: Helper that calculates perimeter once and generates geometry N times
    void Renderer::DrawMultipleFilledRoundedRects(const Vec2& pos, const Vec2& size, float cornerRadius,
                                                   const Color* colors, int count) {
        // Perf 3.1: Use pre-computed trig tables
        float cr = std::min(cornerRadius, std::min(size.x, size.y) * 0.5f);
        float cx_left = pos.x + cr; float cx_right = pos.x + size.x - cr;
        float cy_top = pos.y + cr; float cy_bot = pos.y + size.y - cr;

        constexpr int PERIM_SIZE = CORNER_POINTS * 4;
        Vec2 perim[PERIM_SIZE];
        int pi = 0;
        for (int i = 0; i < CORNER_POINTS; ++i)
            perim[pi++] = {cx_right + cr * sinTable[i], cy_top - cr * cosTable[i]};
        for (int i = 0; i < CORNER_POINTS; ++i)
            perim[pi++] = {cx_right + cr * cosTable[i], cy_bot + cr * sinTable[i]};
        for (int i = 0; i < CORNER_POINTS; ++i)
            perim[pi++] = {cx_left - cr * sinTable[i], cy_bot + cr * cosTable[i]};
        for (int i = 0; i < CORNER_POINTS; ++i)
            perim[pi++] = {cx_left - cr * cosTable[i], cy_top - cr * sinTable[i]};

        EnsureBatchState(ShaderType::Basic, nullptr, Color(1,1,1,1));

        float centerX = pos.x + size.x * 0.5f;
        float centerY = pos.y + size.y * 0.5f;

        for (int c = 0; c < count; ++c) {
            const Color& color = colors[c];
            if (quadVertices.size() + PERIM_SIZE + 1 > MAX_QUAD_VERTICES) FlushBatch();
            unsigned int centerIdx = static_cast<unsigned int>(quadVertices.size());
            quadVertices.push_back({centerX, centerY, color.r, color.g, color.b, color.a, 0, 0});
            for (int i = 0; i < PERIM_SIZE; ++i) quadVertices.push_back({perim[i].x, perim[i].y, color.r, color.g, color.b, color.a, 0, 0});
            for (int i = 0; i < PERIM_SIZE; ++i) {
                quadIndices.push_back(centerIdx);
                quadIndices.push_back(centerIdx + 1 + static_cast<unsigned int>(i));
                quadIndices.push_back(centerIdx + 1 + static_cast<unsigned int>((i + 1) % PERIM_SIZE));
            }
        }
    }

    void Renderer::DrawRectAcrylic(const Vec2& pos, const Vec2& size, const Color& tintColor, float cornerRadius, float opacity, float blurAmount) {
        bool isDark = (tintColor.r + tintColor.g + tintColor.b) / 3.0f < 0.5f;
        Color base = isDark ? Color(0.1f, 0.1f, 0.15f, opacity * 0.5f) : Color(0.95f, 0.95f, 0.98f, opacity * 0.4f);
        Color tint = Color(tintColor.r, tintColor.g, tintColor.b, opacity * 0.08f);
        Color acrylic = isDark ? Color(tintColor.r*0.7f+0.2f, tintColor.g*0.7f+0.2f, tintColor.b*0.7f+0.25f, opacity*0.75f)
                               : Color(tintColor.r*0.3f+0.7f, tintColor.g*0.3f+0.7f, tintColor.b*0.7f+0.72f, opacity*0.8f);

        // Issue 15: For rounded corners, calculate perimeter once and draw 3 fills
        if (cornerRadius > 0.0f) {
            Color colors[3] = { base, tint, acrylic };
            DrawMultipleFilledRoundedRects(pos, size, cornerRadius, colors, 3);
            DrawRect(pos, size, isDark ? Color(1,1,1,opacity*0.15f) : Color(0,0,0,opacity*0.1f), cornerRadius);
        } else {
            DrawRectFilled(pos, size, base, 0.0f);
            DrawRectFilled(pos, size, tint, 0.0f);
            DrawRectFilled(pos, size, acrylic, 0.0f);
        }
    }

    void Renderer::DrawCircle(const Vec2& center, float radius, const Color& color, bool filled) {
        if (radius <= 0.0f) return;
        // Issue 1: Ensure Basic batch state
        EnsureBatchState(ShaderType::Basic, nullptr, Color(1,1,1,1));

        const float pi = 3.14159265f; int segments = static_cast<int>(std::max(12.0f, radius * 0.75f));
        if (filled) {
            if (quadVertices.size() + segments + 1 > MAX_QUAD_VERTICES) FlushBatch();
            unsigned int centerIdx = static_cast<unsigned int>(quadVertices.size());
            quadVertices.push_back({center.x, center.y, color.r, color.g, color.b, color.a, 0, 0});
            for (int i = 0; i <= segments; ++i) {
                float a = 2.0f * pi * i / segments;
                quadVertices.push_back({center.x + std::cos(a) * radius, center.y + std::sin(a) * radius, color.r, color.g, color.b, color.a, 0, 0});
                if (i > 0) {
                    quadIndices.push_back(centerIdx);
                    quadIndices.push_back(centerIdx + static_cast<unsigned int>(i));
                    quadIndices.push_back(centerIdx + static_cast<unsigned int>(i + 1));
                }
            }
        } else {
            for (int i = 0; i < segments; ++i) {
                float a1 = 2.0f * pi * i / segments; float a2 = 2.0f * pi * (i+1) / segments;
                DrawLine({center.x + std::cos(a1) * radius, center.y + std::sin(a1) * radius}, {center.x + std::cos(a2) * radius, center.y + std::sin(a2) * radius}, color);
            }
        }
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
            int pi = 0;
            for (int i = 0; i < CORNER_POINTS; ++i)
                perim[pi++] = {cx_right + cr * sinTable[i], cy_top - cr * cosTable[i]};
            for (int i = 0; i < CORNER_POINTS; ++i)
                perim[pi++] = {cx_right + cr * cosTable[i], cy_bot + cr * sinTable[i]};
            for (int i = 0; i < CORNER_POINTS; ++i)
                perim[pi++] = {cx_left - cr * sinTable[i], cy_bot + cr * cosTable[i]};
            for (int i = 0; i < CORNER_POINTS; ++i)
                perim[pi++] = {cx_left - cr * cosTable[i], cy_top - cr * sinTable[i]};

            if (quadVertices.size() + PERIM_SIZE + 1 > MAX_QUAD_VERTICES) FlushBatch();

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

            for (int i = 0; i < PERIM_SIZE; ++i) {
                Vec2 uv = posToUV(perim[i].x, perim[i].y);
                quadVertices.push_back({perim[i].x, perim[i].y, tint.r, tint.g, tint.b, tint.a, uv.x, uv.y});
            }

            for (int i = 0; i < PERIM_SIZE; ++i) {
                quadIndices.push_back(centerIdx);
                quadIndices.push_back(centerIdx + 1 + static_cast<unsigned int>(i));
                quadIndices.push_back(centerIdx + 1 + static_cast<unsigned int>((i + 1) % PERIM_SIZE));
            }
        }
    }

    void Renderer::DrawText(const Vec2& pos, const std::string& text, const Color& color, float fontSize) {
        if (!backend) return;

        if (msdfFont && msdfFont->IsLoaded()) {
            // Issue 1: Only flush if shader/texture/color state actually changes
            EnsureBatchState(ShaderType::MSDF, msdfFont->GetTextureHandle(), color);

            float scale = fontSize / msdfFont->GetEmSize();
            float baseline = pos.y + msdfFont->GetAscender() * fontSize;
            if (msdfFont->GetAscender() <= 0.0f) baseline = pos.y + fontSize * 0.8f;

            float xpos = pos.x;
            const char* ptr = text.data(); const char* end = ptr + text.size();

            while (ptr < end) {
                uint32_t cp = DecodeUTF8(ptr, end);
                if (cp == 0) break;
                if (cp == '\n') { xpos = pos.x; baseline += fontSize * msdfFont->GetLineHeight(); continue; }
                const FontMSDF::Glyph* g = msdfFont->GetGlyph(cp);

                if (g) {
                    float x0 = std::round(xpos + g->bearing.x * scale); float y0 = baseline - g->bearing.y * scale;
                    float w = g->planeBounds.x * scale; float h = g->planeBounds.y * scale;
                    if (w > 0 && h > 0) {
                        // Issue 5: Use 4 vertices + 6 indices instead of 6 vertices
                        if (quadVertices.size() + 4 > MAX_QUAD_VERTICES - 4) FlushBatch(ShaderType::MSDF, msdfFont->GetTextureHandle(), color);
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
        if (msdfFont && msdfFont->IsLoaded()) {
            float scale = fontSize / msdfFont->GetEmSize(); float currentW = 0.0f, maxW = 0.0f;
            float totalH = fontSize * msdfFont->GetLineHeight();
            const char* ptr = text.data(); const char* end = ptr + text.size();
            while (ptr < end) {
                uint32_t cp = DecodeUTF8(ptr, end);
                if (cp == 0) break;
                if (cp == '\n') { maxW = std::max(maxW, currentW); currentW = 0.0f; totalH += fontSize * msdfFont->GetLineHeight(); continue; }
                const FontMSDF::Glyph* g = msdfFont->GetGlyph(cp);
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

    // Issue 8: Public glyph advance accessor
    float Renderer::GetGlyphAdvance(uint32_t codepoint, float fontSize) {
        if (msdfFont && msdfFont->IsLoaded()) {
            float scale = fontSize / msdfFont->GetEmSize();
            const FontMSDF::Glyph* g = msdfFont->GetGlyph(codepoint);
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

    bool Renderer::EnsureAtlasSpace(int w, int h, int& ox, int& oy) {
        int pad = 2;
        if (atlasNextX + w + pad > atlasWidth) { atlasNextX = 0; atlasNextY += atlasCurrentRowHeight + pad; atlasCurrentRowHeight = 0; }
        if (atlasNextY + h + pad > atlasHeight) return false;
        ox = atlasNextX + pad; oy = atlasNextY + pad;
        atlasNextX += w + pad; atlasCurrentRowHeight = std::max(atlasCurrentRowHeight, h);
        return true;
    }

    const Renderer::Glyph* Renderer::GetOrGenerateMSDFGlyph(uint32_t cp) {
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

        if (!atlasPng.empty() && !atlasJson.empty()) {
            if (msdfFont->Load(atlasPng.string(), atlasJson.string())) {
                Log(LogLevel::Info, "MSDF Font loaded successfully from: %s", atlasPng.string().c_str());
                return;
            }
        }

        Log(LogLevel::Error, "Could not load MSDF font, falling back to System font.");
#if defined(_WIN32)
        LoadFont("C:/Windows/Fonts/segoeui.ttf", 14);
#elif defined(__APPLE__)
        LoadFont("/System/Library/Fonts/SFNS.ttf", 14);
#else
        LoadFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 14);
#endif
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

    // --- Bezier Curve (Phase 5) ---
    // Perf 3.3: Generate all line vertices directly without per-segment DrawLine overhead
    void Renderer::DrawBezier(const Vec2& p0, const Vec2& p1, const Vec2& p2, const Vec2& p3,
                               const Color& color, float thickness, int segments) {
        if (segments < 2) segments = 2;
        EnsureBatchState(ShaderType::Basic, nullptr, Color(1,1,1,1));
        if (!lineVertices.empty() && std::abs(thickness - lineBatchWidth) > 0.01f) FlushBatch();
        if (lineVertices.size() + segments * 2 > MAX_LINE_VERTICES) FlushBatch();
        lineBatchWidth = thickness;

        Vec2 prev = p0;
        for (int i = 1; i <= segments; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(segments);
            float u = 1.0f - t;
            float u2 = u * u;
            float u3 = u2 * u;
            float t2 = t * t;
            float t3 = t2 * t;
            Vec2 point = {
                u3 * p0.x + 3.0f * u2 * t * p1.x + 3.0f * u * t2 * p2.x + t3 * p3.x,
                u3 * p0.y + 3.0f * u2 * t * p1.y + 3.0f * u * t2 * p2.y + t3 * p3.y
            };
            lineVertices.push_back({prev.x, prev.y, color.r, color.g, color.b, color.a, 0, 0});
            lineVertices.push_back({point.x, point.y, color.r, color.g, color.b, color.a, 0, 0});
            prev = point;
        }
    }

    // --- Multi-font DrawText (Phase 5) ---
    void Renderer::DrawTextWithFont(const Vec2& pos, const std::string& text, const Color& color,
                                     const std::string& fontName, float fontSize) {
        auto* font = fontManager.GetFont(fontName);
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
            auto* g = fontManager.GetGlyph(font, cp);
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
        return fontManager.MeasureText(fontName, text, fontSize);
    }

} // namespace FluentUI
