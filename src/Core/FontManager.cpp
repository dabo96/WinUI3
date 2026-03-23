#include "core/FontManager.h"
#include "core/Context.h"
#include "core/RenderBackend.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include <iostream>
#include <algorithm>
#include <cmath>
#include <filesystem>

namespace FluentUI {

// UTF-8 decode (same as in Renderer.cpp)
static uint32_t DecodeUTF8_FM(const char*& ptr, const char* end) {
    if (ptr >= end) return 0;
    unsigned char first = static_cast<unsigned char>(*ptr++);
    if (first < 0x80) return first;
    else if ((first >> 5) == 0x6) {
        if (ptr >= end) return 0xFFFD;
        unsigned char second = static_cast<unsigned char>(*ptr++);
        if ((second & 0xC0) != 0x80) return 0xFFFD;
        return ((first & 0x1F) << 6) | (second & 0x3F);
    } else if ((first >> 4) == 0xE) {
        if (ptr + 1 >= end) return 0xFFFD;
        unsigned char second = static_cast<unsigned char>(*ptr++);
        unsigned char third = static_cast<unsigned char>(*ptr++);
        if ((second & 0xC0) != 0x80 || (third & 0xC0) != 0x80) return 0xFFFD;
        return ((first & 0x0F) << 12) | ((second & 0x3F) << 6) | (third & 0x3F);
    }
    return 0xFFFD;
}

FontManager::~FontManager() {
    Shutdown();
}

bool FontManager::Init(RenderBackend* backend, void* ftLibrary) {
    backend_ = backend;
    ftLibrary_ = ftLibrary;
    return backend_ != nullptr && ftLibrary_ != nullptr;
}

void FontManager::Shutdown() {
    for (auto& [name, entry] : fonts_) {
        if (entry->ftFace) {
            FT_Done_Face(static_cast<FT_Face>(entry->ftFace));
            entry->ftFace = nullptr;
        }
        if (entry->atlasTexture && backend_) {
            backend_->DeleteTexture(entry->atlasTexture);
            entry->atlasTexture = nullptr;
        }
    }
    fonts_.clear();
    defaultFontName_.clear();
}

bool FontManager::LoadFont(const std::string& name, const std::string& filepath, int defaultSize) {
    if (!backend_ || !ftLibrary_) return false;

    // Check if file exists
    std::error_code ec;
    if (!std::filesystem::exists(filepath, ec)) {
        Log(LogLevel::Error, "FontManager: Font file not found: %s", filepath.c_str());
        return false;
    }

    auto entry = std::make_unique<FontEntry>();
    entry->name = name;
    entry->path = filepath;
    entry->defaultSize = defaultSize;

    // Load with FreeType
    FT_Face face = nullptr;
    if (FT_New_Face(static_cast<FT_Library>(ftLibrary_), filepath.c_str(), 0, &face)) {
        Log(LogLevel::Error, "FontManager: Failed to load font: %s", filepath.c_str());
        return false;
    }

    FT_Set_Pixel_Sizes(face, 0, defaultSize);

    entry->ftFace = face;
    entry->pixelHeight = static_cast<float>(defaultSize);
    entry->ascent = static_cast<float>(face->size->metrics.ascender) / 64.0f;
    entry->descent = static_cast<float>(face->size->metrics.descender) / 64.0f;
    entry->lineHeight = static_cast<float>(face->size->metrics.height) / 64.0f;

    // Create glyph atlas
    entry->atlasWidth = 1024;
    entry->atlasHeight = 1024;
    entry->atlasTexture = backend_->CreateTexture(entry->atlasWidth, entry->atlasHeight, nullptr, true);
    entry->loaded = true;

    // Pre-load ASCII glyphs
    for (uint32_t c = 32; c < 128; ++c) {
        LoadGlyph(*entry, c);
    }

    // Set as default if first font loaded
    if (fonts_.empty()) {
        defaultFontName_ = name;
    }

    fonts_[name] = std::move(entry);
    Log(LogLevel::Info, "FontManager: Loaded font '%s' from %s", name.c_str(), filepath.c_str());
    return true;
}

bool FontManager::LoadIconFont(const std::string& name, const std::string& filepath,
                                uint32_t rangeStart, uint32_t rangeEnd, int defaultSize) {
    if (!LoadFont(name, filepath, defaultSize)) return false;

    auto* entry = GetFont(name);
    if (!entry) return false;

    entry->iconRangeStart = rangeStart;
    entry->iconRangeEnd = rangeEnd;

    // Pre-load the icon range
    for (uint32_t c = rangeStart; c <= rangeEnd && c < rangeStart + 512; ++c) {
        LoadGlyph(*entry, c);
    }

    return true;
}

void FontManager::SetDefaultFont(const std::string& name) {
    if (fonts_.count(name)) {
        defaultFontName_ = name;
    }
}

FontEntry* FontManager::GetFont(const std::string& name) {
    auto it = fonts_.find(name);
    return it != fonts_.end() ? it->second.get() : nullptr;
}

const FontEntry* FontManager::GetFont(const std::string& name) const {
    auto it = fonts_.find(name);
    return it != fonts_.end() ? it->second.get() : nullptr;
}

FontEntry* FontManager::GetDefaultFont() {
    return GetFont(defaultFontName_);
}

const FontEntry* FontManager::GetDefaultFont() const {
    return GetFont(defaultFontName_);
}

const FontEntry::Glyph* FontManager::GetGlyph(const std::string& fontName, uint32_t codepoint) {
    auto* font = GetFont(fontName);
    if (!font) return nullptr;
    return GetGlyph(font, codepoint);
}

const FontEntry::Glyph* FontManager::GetGlyph(FontEntry* font, uint32_t codepoint) {
    if (!font || !font->loaded) return nullptr;

    auto it = font->glyphs.find(codepoint);
    if (it != font->glyphs.end() && it->second.valid) return &it->second;

    if (LoadGlyph(*font, codepoint)) return &font->glyphs[codepoint];

    // Glyph not found in this font — try fallback chain
    return GetGlyphFromFallbacks(codepoint, font);
}

const FontEntry::Glyph* FontManager::GetGlyphFromFallbacks(uint32_t codepoint, FontEntry* skipFont) {
    // Search explicit fallback chain first
    for (const auto& fbName : fallbackChain_) {
        auto* fb = GetFont(fbName);
        if (!fb || !fb->loaded || fb == skipFont) continue;

        auto it = fb->glyphs.find(codepoint);
        if (it != fb->glyphs.end() && it->second.valid) return &it->second;
        if (LoadGlyph(*fb, codepoint)) return &fb->glyphs[codepoint];
    }

    // If not found in explicit chain, try all other loaded fonts
    for (auto& [name, entry] : fonts_) {
        if (!entry->loaded || entry.get() == skipFont) continue;
        // Skip icon fonts for general text fallback
        if (entry->iconRangeStart > 0) continue;

        auto it = entry->glyphs.find(codepoint);
        if (it != entry->glyphs.end() && it->second.valid) return &it->second;
        if (LoadGlyph(*entry, codepoint)) return &entry->glyphs[codepoint];
    }

    return nullptr;
}

void FontManager::AddFallbackFont(const std::string& name) {
    // Avoid duplicates
    for (const auto& n : fallbackChain_) {
        if (n == name) return;
    }
    fallbackChain_.push_back(name);
}

void FontManager::SetFallbackChain(const std::vector<std::string>& names) {
    fallbackChain_ = names;
}

Vec2 FontManager::MeasureText(const std::string& fontName, const std::string& text, float fontSize) {
    return MeasureText(GetFont(fontName), text, fontSize);
}

Vec2 FontManager::MeasureText(FontEntry* font, const std::string& text, float fontSize) {
    if (!font || !font->loaded) {
        return {static_cast<float>(text.size()) * fontSize * 0.6f, fontSize};
    }

    float scale = fontSize / font->pixelHeight;
    float currentW = 0.0f, maxW = 0.0f;
    float totalH = (font->lineHeight > 0 ? font->lineHeight : font->pixelHeight) * scale;

    const char* ptr = text.data();
    const char* end = ptr + text.size();
    while (ptr < end) {
        uint32_t cp = DecodeUTF8_FM(ptr, end);
        if (cp == 0) break;
        if (cp == '\n') {
            maxW = std::max(maxW, currentW);
            currentW = 0.0f;
            totalH += font->lineHeight * scale;
            continue;
        }
        auto* g = GetGlyph(font, cp);
        if (g) currentW += g->advance * scale;
    }
    return {std::max(maxW, currentW), totalH};
}

float FontManager::GetGlyphAdvance(const std::string& fontName, uint32_t codepoint, float fontSize) {
    auto* font = GetFont(fontName);
    if (!font || !font->loaded) return fontSize * 0.6f;

    float scale = fontSize / font->pixelHeight;
    auto* g = GetGlyph(font, codepoint);
    if (g) return g->advance * scale;
    return fontSize * 0.6f;
}

std::vector<std::string> FontManager::GetFontNames() const {
    std::vector<std::string> names;
    names.reserve(fonts_.size());
    for (auto& [name, _] : fonts_) {
        names.push_back(name);
    }
    return names;
}

bool FontManager::HasFont(const std::string& name) const {
    return fonts_.count(name) > 0;
}

bool FontManager::LoadGlyph(FontEntry& font, uint32_t cp) {
    FT_Face face = static_cast<FT_Face>(font.ftFace);
    if (!face) return false;

    if (FT_Load_Char(face, cp, FT_LOAD_RENDER | FT_LOAD_TARGET_NORMAL)) return false;

    FT_GlyphSlot slot = face->glyph;
    int w = slot->bitmap.width;
    int h = slot->bitmap.rows;

    int ax, ay;
    if (!EnsureAtlasSpace(font, w, h, ax, ay)) return false;

    if (w > 0 && h > 0 && backend_) {
        backend_->UpdateTexture(font.atlasTexture, ax, ay, w, h, slot->bitmap.buffer);
    }

    auto& g = font.glyphs[cp];
    g.advance = static_cast<float>(slot->advance.x) / 64.0f;
    g.bearing = Vec2(static_cast<float>(slot->bitmap_left), static_cast<float>(slot->bitmap_top));
    g.size = Vec2(static_cast<float>(w), static_cast<float>(h));
    g.uv0 = Vec2(static_cast<float>(ax) / font.atlasWidth, static_cast<float>(ay) / font.atlasHeight);
    g.uv1 = Vec2(static_cast<float>(ax + w) / font.atlasWidth, static_cast<float>(ay + h) / font.atlasHeight);
    g.valid = true;
    return true;
}

bool FontManager::EnsureAtlasSpace(FontEntry& font, int w, int h, int& ox, int& oy) {
    int pad = 2;
    if (font.atlasNextX + w + pad > font.atlasWidth) {
        font.atlasNextX = 0;
        font.atlasNextY += font.atlasCurrentRowHeight + pad;
        font.atlasCurrentRowHeight = 0;
    }
    if (font.atlasNextY + h + pad > font.atlasHeight) {
        // Atlas is full — grow by creating a new larger atlas
        int newHeight = font.atlasHeight * 2;
        if (newHeight > 4096) return false; // Max 4096px

        void* newAtlas = backend_->CreateTexture(font.atlasWidth, newHeight, nullptr, true);
        if (!newAtlas) return false;

        // We can't easily copy the old atlas to the new one via the backend API,
        // so we re-render all existing glyphs. For simplicity, just expand and
        // accept that existing glyphs remain in the old atlas region.
        // A more complete solution would copy the old texture.
        backend_->DeleteTexture(font.atlasTexture);
        font.atlasTexture = newAtlas;
        font.atlasHeight = newHeight;

        // Re-load all existing glyphs into the new atlas
        font.atlasNextX = 0;
        font.atlasNextY = 0;
        font.atlasCurrentRowHeight = 0;
        auto oldGlyphs = std::move(font.glyphs);
        font.glyphs.clear();
        for (auto& [cp, _] : oldGlyphs) {
            LoadGlyph(font, cp);
        }

        // Try again
        if (font.atlasNextX + w + pad > font.atlasWidth) {
            font.atlasNextX = 0;
            font.atlasNextY += font.atlasCurrentRowHeight + pad;
            font.atlasCurrentRowHeight = 0;
        }
        if (font.atlasNextY + h + pad > font.atlasHeight) return false;
    }
    ox = font.atlasNextX + pad;
    oy = font.atlasNextY + pad;
    font.atlasNextX += w + pad;
    font.atlasCurrentRowHeight = std::max(font.atlasCurrentRowHeight, h);
    return true;
}

} // namespace FluentUI
