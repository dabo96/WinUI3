#pragma once
#include "Math/Color.h"
#include "Math/Vec2.h"
#include "core/RenderBackend.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <cstdint>
#include "core/FontMSDF.h"
#include "core/MSDFGenerator.h"
#include "core/FontManager.h"
#include <memory>
#include <iostream>

namespace FluentUI {

enum class RenderLayer {
  Default = 0,
  Overlay = 1,
  Tooltip = 2,
  Count = 3
};

class Renderer {
public:
  struct ClipRect {
    int x, y, width, height;
  };

  // Estructura para almacenar un batch de dibujo diferido
  struct RenderBatch {
    ShaderType type;
    std::vector<RenderVertex> vertices;
    std::vector<unsigned int> indices;
    void* texture = nullptr;
    float projection[16];
    Color textColor;
    ClipRect clipRect;
    bool hasClip = false;
    float lineWidth = 1.0f;
    bool isLines = false;
  };

  Renderer() = default;
  ~Renderer();

  // Perf Phase C: Optional pointer to performance counters (set by Context)
  struct PerfCounters {
    uint32_t* flushCount = nullptr;
    uint32_t* stateChanges = nullptr;
    uint32_t* batchCount = nullptr;
    uint32_t* drawCalls = nullptr;
    uint32_t* vertexCount = nullptr;
    uint32_t* indexCount = nullptr;
    uint32_t* batchMerges = nullptr;
    uint32_t* clipPushes = nullptr;
  };
  PerfCounters perfCounters;

  bool Init(RenderBackend* backend);
  void BeginFrame(const Color& clearColor = Color(0.13f, 0.13f, 0.13f, 1.0f));
  void EndFrame();
  void Shutdown();

  // Gestión de capas
  void SetLayer(RenderLayer layer) { currentLayer = layer; }
  RenderLayer GetLayer() const { return currentLayer; }

  // Primitivas de dibujo
  void DrawRect(const Vec2 &pos, const Vec2 &size, const Color &color,
                float cornerRadius = 0.0f);
  void DrawRectFilled(const Vec2 &pos, const Vec2 &size, const Color &color,
                      float cornerRadius = 0.0f);
  void DrawRectWithElevation(const Vec2 &pos, const Vec2 &size, const Color &color,
                             float cornerRadius = 0.0f, float elevation = 0.0f);
  // Soft drop shadow approximated by stacked, expanding rounded rects whose
  // alpha accumulates into a smooth penumbra. pos/size/cornerRadius describe the
  // element casting the shadow; blur is the penumbra spread (px); color.a is the
  // peak opacity reached at the element edge; offset shifts the shadow (e.g.
  // downward). Draw this BEFORE the element's own fill.
  void DrawRectShadow(const Vec2 &pos, const Vec2 &size, float cornerRadius,
                      float blur, const Color &color, const Vec2 &offset = Vec2(0.0f, 0.0f));
  // Draw a soft drop shadow derived from a semantic z (depth) level via the
  // unified Elevation curve. Use Elevation::Z::* for the level. opacityScale
  // fades the whole shadow (e.g. for tooltip fade-in). Draw BEFORE the fill.
  void DrawElevationShadow(const Vec2 &pos, const Vec2 &size, float cornerRadius,
                           float z, float opacityScale = 1.0f);
  void DrawRectGradient(const Vec2 &pos, const Vec2 &size,
                        const Color &topLeft, const Color &topRight,
                        const Color &bottomLeft, const Color &bottomRight);
  void DrawRectAcrylic(const Vec2 &pos, const Vec2 &size, const Color &tintColor,
                       float cornerRadius = 0.0f, float opacity = 0.8f, float blurAmount = 0.0f);
  void DrawLine(const Vec2 &start, const Vec2 &end, const Color &color,
                float width = 1.0f);
  void DrawCircle(const Vec2 &center, float radius, const Color &color,
                  bool filled = true);
  void DrawTriangleFilled(const Vec2 &p0, const Vec2 &p1, const Vec2 &p2,
                          const Color &color);
  void DrawRipple(const Vec2 &center, float radius, float opacity);

  // Image drawing
  void DrawImage(const Vec2& pos, const Vec2& size, void* textureHandle,
                 const Vec2& uv0 = Vec2(0,0), const Vec2& uv1 = Vec2(1,1),
                 const Color& tint = Color(1,1,1,1), float cornerRadius = 0.0f);

  // Bezier curve (Phase 5) — cubic bezier tessellated into line segments
  void DrawBezier(const Vec2& p0, const Vec2& p1, const Vec2& p2, const Vec2& p3,
                  const Color& color, float thickness = 1.0f, int segments = 32);

  // --- Path API (Phase A1) ---
  // Accumulate a polyline/polygon and stroke or fill it as a single AA-fringed entity.
  // Pattern: PathClear(); PathLineTo(...); PathArcTo(...); ...; PathFillConvex(color);
  void PathClear();
  void PathLineTo(const Vec2& p);
  void PathArcTo(const Vec2& center, float radius, float a_min, float a_max,
                 int num_segments = 0);
  // Fast variant: a_min12/a_max12 are integer steps where 12 == full 360° (uses cosTable/sinTable).
  void PathArcToFast(const Vec2& center, float radius, int a_min12, int a_max12);
  void PathBezierCubicCurveTo(const Vec2& p2, const Vec2& p3, const Vec2& p4,
                              int segments = 0);
  void PathRect(const Vec2& p_min, const Vec2& p_max, float rounding = 0.0f);
  void PathFillConvex(const Color& color);
  void PathStroke(const Color& color, bool closed, float thickness = 1.0f);
  const std::vector<Vec2>& PathPoints() const { return pathPoints; }

  // Texto básico
  void DrawText(const Vec2 &pos, const std::string &text, const Color &color,
                float fontSize = 16.0f);
  bool LoadFont(const std::string &filepath, int pixelHeight);
  bool IsFontLoaded() const { return fontLoaded; }
  Vec2 MeasureText(const std::string &text, float fontSize = 16.0f);
  // Métricas del font activo (en proporción al fontSize). Se usan para
  // centrar verticalmente texto dentro de un campo: el ascender devuelve
  // el offset desde pos.y hasta la baseline, así el centro visual del
  // texto = pos.y + ascender * fontSize / 2 (aprox., sin descenders).
  float GetFontAscender() const;

  // Icon font (secondary font for UI icons like Lucide)
  bool LoadIconFont(const std::string &filepath, int pixelHeight);
  void DrawIconGlyph(const Vec2 &pos, uint32_t codepoint, const Color &color, float fontSize = 16.0f);
  bool IsIconFontLoaded() const { return iconFontLoaded; }

  // Issue 8: Public glyph advance accessor for FindCaretPosition
  float GetGlyphAdvance(uint32_t codepoint, float fontSize);

  // Multi-font support (Phase 5) — draw text with a named font
  void DrawTextWithFont(const Vec2& pos, const std::string& text, const Color& color,
                        const std::string& fontName, float fontSize = 16.0f);
  Vec2 MeasureTextWithFont(const std::string& text, const std::string& fontName, float fontSize = 16.0f);

  // Font manager access
  FontManager& GetFontManager() { return fontManager; }

  // Utilidades
  void SetViewport(int width, int height);
  Vec2 GetViewportSize() const { return viewportSize; }

  // Phase A4: DPI scale propagated from UIContext so the AA fringe stays at 1 physical
  // pixel (instead of 1 logical pixel) on Hi-DPI displays.
  void SetDPIScale(float scale) { dpiScale = std::max(0.5f, std::min(4.0f, scale)); }
  float GetDPIScale() const { return dpiScale; }

  // Phase C6 (eyedropper): read a single pixel from the framebuffer.
  Color ReadPixel(int x, int y);

  void FlushBatch();
  void FlushBatch(ShaderType type, void* texture = nullptr, const Color& textColor = Color(1,1,1,1));

private:
  RenderBackend* backend = nullptr;
  Vec2 viewportSize = {800.0f, 600.0f};
  RenderLayer currentLayer = RenderLayer::Default;
  // Phase A4: DPI scale used to size the AA fringe (1 physical pixel always).
  float dpiScale = 1.0f;

  // Perf 1.3: Cached projection matrix — recomputed only on SetViewport()
  float cachedProjection[16] = {};

  // Issue 1: Batch state tracking to avoid unnecessary flushes
  ShaderType currentBatchShader = ShaderType::Basic;
  void* currentBatchTexture = nullptr;
  Color currentBatchTextColor{1,1,1,1};
  void EnsureBatchState(ShaderType shader, void* texture, const Color& color);

  std::vector<RenderBatch> layerBatches[(int)RenderLayer::Count];

  struct Glyph {
    Vec2 size;     // size of glyph in pixels
    Vec2 bearing;  // offset from baseline to top-left
    float advance = 0.0f; // horizontal advance in pixels
    Vec2 uv0;
    Vec2 uv1;
    bool valid = false;
    uint32_t lastAccessFrame = 0;  // Perf R6: for LRU eviction
  };
  uint32_t glyphCacheFrame = 0;
  static constexpr size_t MAX_GLYPH_CACHE = 2048;
  static constexpr uint32_t GLYPH_EVICT_AGE = 600; // Evict after 10s at 60fps

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

  // Icon font (secondary)
  FT_Face iconFontFace = nullptr;
  bool iconFontLoaded = false;
  float iconFontPixelHeight = 16.0f;
  float iconFontAscent = 0.0f;
  std::unordered_map<uint32_t, Glyph> iconGlyphCache;
  bool LoadIconGlyph(uint32_t cp);
  const Glyph* GetIconGlyph(uint32_t cp);

  std::unique_ptr<FontMSDF> msdfFont;
  std::unique_ptr<MSDFGenerator> msdfGenerator;

  // Multi-font manager (Phase 5)
  FontManager fontManager;

  static constexpr int DYNAMIC_ATLAS_SIZE = 2048;
  static constexpr int MSDF_GLYPH_SIZE = 64;
  void* dynamicMSDFAtlasTexture = nullptr;
  int dynamicAtlasWidth = DYNAMIC_ATLAS_SIZE;
  int dynamicAtlasHeight = DYNAMIC_ATLAS_SIZE;
  int dynamicAtlasNextX = 0;
  int dynamicAtlasNextY = 0;
  int dynamicAtlasCurrentRowHeight = 0;
  std::unordered_map<std::uint32_t, Glyph> dynamicMSDFGlyphCache;

  // Helpers internos
  void InitializeDefaultFont();
  void ClearGlyphs();
  const Glyph* GetGlyph(std::uint32_t codepoint);
  bool LoadGlyph(std::uint32_t codepoint);
  const Glyph* GetOrGenerateMSDFGlyph(std::uint32_t codepoint);
  bool GenerateMSDFGlyph(std::uint32_t codepoint);
  float GetKerning(std::uint32_t left, std::uint32_t right) const;
  bool EnsureAtlasSpace(int glyphWidth, int glyphHeight, int& outX, int& outY);
  bool EnsureDynamicMSDFAtlasSpace(int glyphWidth, int glyphHeight, int& outX, int& outY);

  // Issue 15: Helper for acrylic multi-fill optimization
  void DrawMultipleFilledRoundedRects(const Vec2& pos, const Vec2& size, float cornerRadius,
                                       const Color* colors, int count);

  // Phase A1: Path state and fan-with-fringe emitter (Basic shader, untextured)
  std::vector<Vec2> pathPoints;
  void EmitConvexFanWithFringe(const Vec2* points, int count, const Color& color);
  // Phase A2: continuous AA stroke with miter joins (Basic shader, untextured)
  void StrokePolyline(const Vec2* points, int count, bool closed,
                      const Color& color, float thickness);

  std::vector<RenderVertex> quadVertices;
  std::vector<unsigned int> quadIndices;
  std::vector<RenderVertex> lineVertices;
  float lineBatchWidth = 1.0f;

  // Optimización: límites de batch para evitar reallocaciones grandes
  static constexpr size_t MAX_QUAD_VERTICES = 10000; // ~2500 quads
  static constexpr size_t MAX_QUAD_INDICES = 15000;  // 6 indices por quad
  static constexpr size_t MAX_LINE_VERTICES = 5000;  // ~2500 líneas

  // Perf 3.1: Pre-computed trig lookup table for rounded rects (8 segments per corner)
  static constexpr int CORNER_SEGMENTS = 8;
  static constexpr int CORNER_POINTS = CORNER_SEGMENTS + 1; // 9 points per quarter
  float cosTable[CORNER_POINTS];
  float sinTable[CORNER_POINTS];
  bool trigTablesInitialized = false;
  void InitTrigTables();

  std::vector<ClipRect> clipStack;

public:
  void PushClipRect(const Vec2& pos, const Vec2& size);
  void PopClipRect();
  const std::vector<ClipRect>& GetClipStack() const { return clipStack; }
};

} // namespace FluentUI
