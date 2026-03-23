#pragma once
#include "Math/Vec2.h"
#include "Math/Color.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <cstdint>

namespace FluentUI {

class RenderBackend;
class Renderer;

// Represents a loaded font with its own glyph cache and atlas
struct FontEntry {
    std::string name;
    std::string path;
    int defaultSize = 16;

    // FreeType handles (opaque — stored as void* to avoid FT includes in header)
    void* ftFace = nullptr;

    // Font metrics
    float pixelHeight = 16.0f;
    float ascent = 0.0f;
    float descent = 0.0f;
    float lineHeight = 0.0f;

    // Glyph atlas
    void* atlasTexture = nullptr;
    int atlasWidth = 1024;
    int atlasHeight = 1024;
    int atlasNextX = 0;
    int atlasNextY = 0;
    int atlasCurrentRowHeight = 0;

    struct Glyph {
        Vec2 size;
        Vec2 bearing;
        float advance = 0.0f;
        Vec2 uv0, uv1;
        bool valid = false;
    };
    std::unordered_map<uint32_t, Glyph> glyphs;

    bool loaded = false;

    // Icon font range (0 = not an icon font)
    uint32_t iconRangeStart = 0;
    uint32_t iconRangeEnd = 0;
};

// Manages multiple named fonts
class FontManager {
public:
    FontManager() = default;
    ~FontManager();

    // Initialize with backend and FreeType library
    bool Init(RenderBackend* backend, void* ftLibrary);
    void Shutdown();

    // Load a named font from file
    bool LoadFont(const std::string& name, const std::string& filepath, int defaultSize = 16);

    // Load an icon font (specific Unicode range)
    bool LoadIconFont(const std::string& name, const std::string& filepath,
                      uint32_t rangeStart, uint32_t rangeEnd, int defaultSize = 16);

    // Set the default font (used when no font is specified)
    void SetDefaultFont(const std::string& name);

    // Get a font by name (returns nullptr if not found)
    FontEntry* GetFont(const std::string& name);
    const FontEntry* GetFont(const std::string& name) const;

    // Get the default font
    FontEntry* GetDefaultFont();
    const FontEntry* GetDefaultFont() const;

    // Get a glyph from a specific font (with fallback to other loaded fonts)
    const FontEntry::Glyph* GetGlyph(const std::string& fontName, uint32_t codepoint);
    const FontEntry::Glyph* GetGlyph(FontEntry* font, uint32_t codepoint);

    /// Add a font to the fallback chain (searched in order when a glyph is missing).
    void AddFallbackFont(const std::string& name);

    /// Set the full fallback chain at once.
    void SetFallbackChain(const std::vector<std::string>& names);

    // Measure text with a specific font
    Vec2 MeasureText(const std::string& fontName, const std::string& text, float fontSize);
    Vec2 MeasureText(FontEntry* font, const std::string& text, float fontSize);

    // Get glyph advance for a specific font
    float GetGlyphAdvance(const std::string& fontName, uint32_t codepoint, float fontSize);

    // Get list of loaded font names
    std::vector<std::string> GetFontNames() const;

    // Check if a font is loaded
    bool HasFont(const std::string& name) const;

private:
    bool LoadGlyph(FontEntry& font, uint32_t codepoint);
    bool EnsureAtlasSpace(FontEntry& font, int w, int h, int& outX, int& outY);

    // Try to find a glyph in fallback fonts (skipping skipFont)
    const FontEntry::Glyph* GetGlyphFromFallbacks(uint32_t codepoint, FontEntry* skipFont);

    RenderBackend* backend_ = nullptr;
    void* ftLibrary_ = nullptr; // FT_Library — stored as void* to avoid header dep
    std::unordered_map<std::string, std::unique_ptr<FontEntry>> fonts_;
    std::string defaultFontName_;
    std::vector<std::string> fallbackChain_;
};

} // namespace FluentUI
