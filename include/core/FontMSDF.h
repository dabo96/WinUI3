#pragma once
#include "Math/Vec2.h"
#include "Math/Color.h"
#include <string>
#include <unordered_map>
#include <memory>

namespace FluentUI {

    class RenderBackend;

    class FontMSDF {
    public:
        struct Glyph {
            Vec2 planeBounds;  // Size in font units
            Vec2 atlasBounds;  // Size in atlas pixels
            Vec2 bearing;      // Offset from baseline
            float advance;     // Horizontal advance
            Vec2 uv0, uv1;     // UV coordinates in atlas
            bool valid = false;
        };

        FontMSDF(RenderBackend* backend = nullptr);
        ~FontMSDF();

        bool Load(const std::string& atlasImagePath, const std::string& atlasJsonPath);
        const Glyph* GetGlyph(uint32_t codepoint) const;
        
        // Metrics
        float GetLineHeight() const { return lineHeight; }
        float GetEmSize() const { return emSize; }
        float GetFontSize() const { return fontSize; }
        float GetPixelRange() const { return pixelRange; }
        float GetAscender() const { return ascender; }
        bool IsLoaded() const { return loaded; }
        
        void* GetTextureHandle() const { return textureHandle; }

    private:
        bool LoadTexture(const std::string& imagePath);
        bool ParseJson(const std::string& jsonPath);

    private:
        RenderBackend* backend = nullptr;
        bool loaded = false;
        
        void* textureHandle = nullptr;

        // Metrics
        float emSize = 1.0f;
        float lineHeight = 1.0f;
        float fontSize = 32.0f;
        float pixelRange = 4.0f;
        float ascender = 1.0f;  // Ascender in font units
        int atlasWidth = 0;
        int atlasHeight = 0;

        std::unordered_map<uint32_t, Glyph> glyphs;
    };

} // namespace FluentUI
