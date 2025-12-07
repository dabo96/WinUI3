#pragma once
#include "Math/Vec2.h"
#include "Math/Color.h"
#include <SDL3/SDL.h>
#include <glad/glad.h>
#include <string>
#include <unordered_map>
#include <memory>
#include <iostream>

namespace FluentUI {

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

        FontMSDF() = default;
        ~FontMSDF();

        bool Load(const std::string& atlasImagePath, const std::string& atlasJsonPath);
        const Glyph* GetGlyph(uint32_t codepoint) const;
        
        // Bind texture and shader for rendering
        // Returns true if successfully bound
        bool Bind(const float* projectionMatrix);
        void Unbind();
        
        // Set text color (must be called after Bind)
        void SetColor(const Color& color);

        // Getters for font metrics
        float GetLineHeight() const { return lineHeight; }
        float GetEmSize() const { return emSize; }
        float GetFontSize() const { return fontSize; }
        float GetPixelRange() const { return pixelRange; }
        float GetAscender() const { return ascender; }
        bool IsLoaded() const { return loaded; }

    private:
        bool LoadTexture(const std::string& imagePath);
        bool ParseJson(const std::string& jsonPath);
        void CreateShader();
        std::string LoadShaderSource(const std::string& filepath);
        GLuint CompileShader(GLenum type, const std::string& source);
        GLuint CreateShaderProgram(const std::string& vertexSource, const std::string& fragmentSource);

    private:
        bool loaded = false;
        
        // OpenGL resources
        GLuint textureID = 0;
        GLuint shaderProgram = 0;
        
        // Uniform locations
        GLint projectionUniform = -1;
        GLint colorUniform = -1;
        GLint pxRangeUniform = -1;
        GLint textureUniform = -1;

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
