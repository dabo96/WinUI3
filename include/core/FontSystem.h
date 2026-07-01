#pragma once
#include "Math/Color.h"
#include "Math/Vec2.h"
#include "core/FontMSDF.h"
#include "core/MSDFGenerator.h"
#include "core/FontManager.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <cstdint>
#include <ft2build.h>
#include FT_FREETYPE_H

namespace FluentUI {

class RenderBackend;

// Font subsystem extracted from Renderer (brief 23, phase 1). Owns FreeType, the
// bitmap glyph atlas, the static + dynamic MSDF atlases and the multi-font
// manager. It is a DEVICE-level resource: in a multi-window shared-device setup
// (brief 08) the first window's FontSystem OWNS the GPU atlases and every
// secondary window's FontSystem routes every glyph lookup/generation to that
// owner (via owner_), so the atlas is never rebuilt per window.
//
// Renderer keeps the text DRAW methods (they emit quads into its batch); this
// class only provides glyph data, atlas handles and text metrics. The public API
// (LoadFont/MeasureText/…) mirrors what Renderer used to expose, so the Renderer
// facade delegates without changing widget-facing behavior.
class FontSystem {
public:
  struct Glyph {
    Vec2 size;     // glyph size in pixels
    Vec2 bearing;  // offset from baseline to top-left
    float advance = 0.0f;  // horizontal advance in pixels
    Vec2 uv0;
    Vec2 uv1;
    bool valid = false;
    uint32_t lastAccessFrame = 0;  // Perf R6: for LRU eviction
  };

  FontSystem() = default;
  ~FontSystem() { Shutdown(); }

  // Owner init: FreeType + atlas textures + MSDF font/generator + default font.
  bool Init(RenderBackend* backend);
  // Secondary init (brief 08): reference the device-owner's FontSystem; no baking.
  // All reads route to `owner`, so this instance holds no font GPU resources.
  bool InitShared(RenderBackend* backend, FontSystem* owner);
  // Frees the GPU/FreeType resources — only when this instance is the owner.
  // Idempotent (safe to call from Renderer::Shutdown and again from ~FontSystem).
  void Shutdown();
  // Per-frame housekeeping: bump the frame counter and LRU-evict stale dynamic
  // MSDF glyphs. Call once per frame from Renderer::BeginFrame.
  void NewFrame();

  bool LoadFont(const std::string& filepath, int pixelHeight);
  bool LoadIconFont(const std::string& filepath, int pixelHeight);

  // ── Queries used by Renderer's text-draw methods (route to owner if shared) ──
  FontMSDF* ActiveMSDF() const { return Owner()->msdfFont.get(); }
  FontManager& Manager() { return Owner()->fontManager; }
  void* BitmapAtlasTexture() const { return Owner()->fontAtlasTexture; }
  void* DynamicMSDFAtlasTexture() const { return Owner()->dynamicMSDFAtlasTexture; }

  bool IsFontLoaded() const { return Owner()->fontLoaded; }
  bool IsIconFontLoaded() const { return Owner()->iconFontLoaded; }
  float FontPixelHeight() const { return Owner()->fontPixelHeight; }
  float FontAscent() const { return Owner()->fontAscent; }
  float FontLineHeight() const { return Owner()->fontLineHeight; }
  float IconFontPixelHeight() const { return Owner()->iconFontPixelHeight; }
  float IconFontAscent() const { return Owner()->iconFontAscent; }

  const Glyph* GetGlyph(uint32_t cp);
  const Glyph* GetIconGlyph(uint32_t cp);
  const Glyph* GetOrGenerateMSDFGlyph(uint32_t cp);

  // ── Metrics / measuring ──
  Vec2 MeasureText(const std::string& text, float fontSize);
  Vec2 MeasureTextWrapped(const std::string& text, float maxWidth, float fontSize);
  // Break text into lines, wrapping by word within maxWidth and honoring '\n'.
  std::vector<std::string> WrapTextLines(const std::string& text, float maxWidth,
                                         float fontSize, float& outMaxWidth);
  // Vertical advance per line, consistent with DrawText's multiline stepping.
  float LineAdvancePx(float fontSize);
  float GetFontAscender() const;
  float GetGlyphAdvance(uint32_t codepoint, float fontSize);
  Vec2 MeasureTextWithFont(const std::string& text, const std::string& fontName, float fontSize);

private:
  // The FontSystem that physically owns the GPU/FreeType resources: `this` when
  // standalone / device-owner, or the origin's FontSystem for a secondary window.
  FontSystem* Owner() { return owner_ ? owner_ : this; }
  const FontSystem* Owner() const { return owner_ ? owner_ : this; }
  bool ownsResources() const { return owner_ == nullptr; }

  void InitializeDefaultFont();
  void ClearGlyphs();
  bool LoadGlyph(uint32_t cp);
  bool LoadIconGlyph(uint32_t cp);
  bool GenerateMSDFGlyph(uint32_t cp);
  bool EnsureAtlasSpace(int glyphWidth, int glyphHeight, int& outX, int& outY);
  bool EnsureDynamicMSDFAtlasSpace(int glyphWidth, int glyphHeight, int& outX, int& outY);

  RenderBackend* backend = nullptr;
  // null => this instance owns the resources; else routes reads to the owner.
  FontSystem* owner_ = nullptr;

  // ── Bitmap (FreeType) font + atlas ──
  uint32_t glyphCacheFrame = 0;
  static constexpr size_t MAX_GLYPH_CACHE = 2048;
  static constexpr uint32_t GLYPH_EVICT_AGE = 600;  // Evict after 10s at 60fps
  std::unordered_map<std::uint32_t, Glyph> glyphCache;
  bool fontLoaded = false;
  float fontPixelHeight = 16.0f;
  float fontAscent = 0.0f;
  float fontDescent = 0.0f;
  float fontLineHeight = 0.0f;
  FT_Library ftLibrary = nullptr;
  FT_Face fontFace = nullptr;

  void* fontAtlasTexture = nullptr;
  int atlasWidth = 0;
  int atlasHeight = 0;
  int atlasNextX = 0;
  int atlasNextY = 0;
  int atlasCurrentRowHeight = 0;

  // ── Icon font (secondary bitmap font, shares the bitmap atlas) ──
  FT_Face iconFontFace = nullptr;
  bool iconFontLoaded = false;
  float iconFontPixelHeight = 16.0f;
  float iconFontAscent = 0.0f;
  std::unordered_map<uint32_t, Glyph> iconGlyphCache;

  // ── MSDF (static + dynamic atlas) ──
  std::unique_ptr<FontMSDF> msdfFont;
  std::unique_ptr<MSDFGenerator> msdfGenerator;
  static constexpr int DYNAMIC_ATLAS_SIZE = 2048;
  static constexpr int MSDF_GLYPH_SIZE = 64;
  void* dynamicMSDFAtlasTexture = nullptr;
  int dynamicAtlasWidth = DYNAMIC_ATLAS_SIZE;
  int dynamicAtlasHeight = DYNAMIC_ATLAS_SIZE;
  int dynamicAtlasNextX = 0;
  int dynamicAtlasNextY = 0;
  int dynamicAtlasCurrentRowHeight = 0;
  std::unordered_map<std::uint32_t, Glyph> dynamicMSDFGlyphCache;

  // ── Multi-font manager (Phase 5) ──
  FontManager fontManager;
};

} // namespace FluentUI
