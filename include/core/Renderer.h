#pragma once
#include "Math/Color.h"
#include "Math/Vec2.h"
#include <SDL3/SDL.h>
#include <glad/glad.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <cstdint>
#include "core/FontMSDF.h"
#include "core/MSDFGenerator.h"
#include <memory>

namespace FluentUI {


class Renderer {
public:
  Renderer() = default;
  ~Renderer();

  bool Init(SDL_Window *window);
  void BeginFrame();
  void EndFrame();
  void Shutdown();

  // Primitivas de dibujo
  void DrawRect(const Vec2 &pos, const Vec2 &size, const Color &color,
                float cornerRadius = 0.0f);
  void DrawRectFilled(const Vec2 &pos, const Vec2 &size, const Color &color,
                      float cornerRadius = 0.0f);
  void DrawRectWithElevation(const Vec2 &pos, const Vec2 &size, const Color &color,
                             float cornerRadius = 0.0f, float elevation = 0.0f);
  void DrawRectAcrylic(const Vec2 &pos, const Vec2 &size, const Color &tintColor,
                       float cornerRadius = 0.0f, float opacity = 0.8f, float blurAmount = 0.0f);
  void DrawLine(const Vec2 &start, const Vec2 &end, const Color &color,
                float width = 1.0f);
  void DrawCircle(const Vec2 &center, float radius, const Color &color,
                  bool filled = true);
  void DrawRipple(const Vec2 &center, float radius, float opacity);

  // Imágenes
  void DrawImage(uint32_t textureId, const Vec2 &pos, const Vec2 &size, 
                 const Vec2 &uv0 = Vec2(0,0), const Vec2 &uv1 = Vec2(1,1), 
                 const Color &tintColor = Color(1,1,1,1), float opacity = 1.0f);
  
  // Texto básico (usando caracteres ASCII simples)
  void DrawText(const Vec2 &pos, const std::string &text, const Color &color,
                float fontSize = 16.0f);
  bool LoadFont(const std::string &filepath, int pixelHeight);
  bool IsFontLoaded() const { return fontLoaded; }
  Vec2 MeasureText(const std::string &text, float fontSize = 16.0f);

  // Utilidades
  void SetViewport(int width, int height);
  Vec2 GetViewportSize() const { return viewportSize; }

  // Shader utilities
  std::string LoadShaderSource(const std::string &filepath);
  GLuint CompileShader(GLenum type, const std::string &source);
  GLuint CreateShaderProgram(const std::string &vertexSource,
                             const std::string &fragmentSource);
  void FlushBatch();
  void SetupTextRendering();
  void SetupShaders();
private:
  SDL_Window *window = nullptr;
  SDL_GLContext glContext = nullptr;
  GLuint shaderProgram = 0;
  GLuint textShaderProgram = 0;
  GLuint imageShaderProgram = 0;
  GLuint VAO = 0, VBO = 0, EBO = 0;
  GLuint textVAO = 0, textVBO = 0;
  GLuint quadVAO = 0, quadVBO = 0, quadEBO = 0;
  GLuint lineVAO = 0, lineVBO = 0;
  Vec2 viewportSize = {800.0f, 600.0f};
  Vec2 pixelSize = {800.0f, 600.0f};

  struct Glyph {
    Vec2 size;     // size of glyph in pixels
    Vec2 bearing;  // offset from baseline to top-left
    float advance = 0.0f; // horizontal advance in pixels
    Vec2 uv0;
    Vec2 uv1;
    bool valid = false;
  };

  // MSDF Glyph structure for pre-generated atlas


  std::unordered_map<std::uint32_t, Glyph> glyphCache;
  bool fontLoaded = false;
  float fontPixelHeight = 16.0f;
  float fontAscent = 0.0f;
  float fontDescent = 0.0f;
  float fontLineHeight = 0.0f;
  bool textSystemInitialized = false;
  GLint textColorUniform = -1;
  GLint projectionUniform = -1;
  GLint textProjectionUniform = -1;
  GLint rectPosUniform = -1;
  GLint rectSizeUniform = -1;
  GLint cornerRadiusUniform = -1;
  FT_Library ftLibrary = nullptr;
  FT_Face fontFace = nullptr;
  bool projectionDirty = true;

  GLuint fontAtlasTexture = 0;
  int atlasWidth = 0;
  int atlasHeight = 0;
  int atlasNextX = 0;
  int atlasNextY = 0;
  int atlasCurrentRowHeight = 0;

  std::unique_ptr<FontMSDF> msdfFont;
  std::unique_ptr<MSDFGenerator> msdfGenerator;
  
  // Dynamic MSDF atlas for FreeType-generated glyphs
  GLuint dynamicMSDFAtlasTexture = 0;
  int dynamicAtlasWidth = 2048;
  int dynamicAtlasHeight = 2048;
  std::unordered_map<std::uint32_t, Glyph> dynamicMSDFGlyphCache;

  // Helpers internos
  void EnsureBatchResources();
  void InitializeDefaultFont();
  void ClearGlyphs();
  const Glyph* GetGlyph(std::uint32_t codepoint);
  bool LoadGlyph(std::uint32_t codepoint);
  const Glyph* GetOrGenerateMSDFGlyph(std::uint32_t codepoint);
  bool GenerateMSDFGlyph(std::uint32_t codepoint);
  float GetKerning(std::uint32_t left, std::uint32_t right) const;
  void UpdateProjection();
  bool EnsureAtlasSpace(int glyphWidth, int glyphHeight, int& outX, int& outY);
  bool EnsureDynamicMSDFAtlasSpace(int glyphWidth, int glyphHeight, int& outX, int& outY);

  // MSDF methods


  struct Vertex {
    float x;
    float y;
    float r;
    float g;
    float b;
    float a;
  };

  struct ClipRect {
    int x;
    int y;
    int width;
    int height;
  };

  std::vector<Vertex> quadVertices;
  std::vector<unsigned int> quadIndices;
  std::vector<Vertex> lineVertices;
  float lineBatchWidth = 1.0f;
  bool batchResourcesInitialized = false;
  std::vector<ClipRect> clipStack;
  
  // Optimización: límites de batch para evitar reallocaciones grandes
  static constexpr size_t MAX_QUAD_VERTICES = 10000; // ~2500 quads
  static constexpr size_t MAX_QUAD_INDICES = 15000;  // 6 indices por quad
  static constexpr size_t MAX_LINE_VERTICES = 5000;  // ~2500 líneas

public:
  void PushClipRect(const Vec2& pos, const Vec2& size, bool force = false);
  void PopClipRect();
  const std::vector<ClipRect>& GetClipStack() const { return clipStack; }
};

} // namespace FluentUI
