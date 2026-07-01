#include "core/FontSystem.h"
#include "core/RenderBackend.h"
#include "core/Context.h"      // Log / LogLevel
#include "UI/WidgetHelpers.h"  // DecodeUTF8
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace FluentUI {

// Defined at namespace scope in Renderer.cpp (external linkage). Resolves a
// resource path against the executable/current-dir search roots.
std::filesystem::path ResolveResourcePath(const std::string& resource);

// ─── Lifecycle ──────────────────────────────────────────────────────────────

bool FontSystem::Init(RenderBackend* backend) {
    this->backend = backend;
    owner_ = nullptr; // this instance owns the resources
    if (!this->backend) return false;
    if (FT_Init_FreeType(&ftLibrary)) return false;

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

    // Phase 5.2: Pre-generate MSDF glyphs for ASCII range once the font is loaded.
    if (msdfFont && msdfFont->IsLoaded()) {
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

bool FontSystem::InitShared(RenderBackend* backend, FontSystem* owner) {
    this->backend = backend;
    owner_ = owner; // reference the owner's resources; hold none of our own
    // Every glyph lookup / metric read routes through Owner(), so no baking and no
    // atlas handles are needed here (brief 08 shared-device text).
    return this->backend != nullptr && owner_ != nullptr;
}

void FontSystem::Shutdown() {
    // Only the resource owner frees the font GPU/FreeType objects. Guarding on a
    // live backend makes this idempotent (Renderer::Shutdown then ~FontSystem).
    if (ownsResources() && backend) {
        fontManager.Shutdown();
        msdfFont.reset();
        msdfGenerator.reset();
        ClearGlyphs();
        if (fontFace) { FT_Done_Face(fontFace); fontFace = nullptr; }
        if (iconFontFace) { FT_Done_Face(iconFontFace); iconFontFace = nullptr; }
        if (ftLibrary) { FT_Done_FreeType(ftLibrary); ftLibrary = nullptr; }
        if (fontAtlasTexture) backend->DeleteTexture(fontAtlasTexture);
        if (dynamicMSDFAtlasTexture) backend->DeleteTexture(dynamicMSDFAtlasTexture);
    }
    fontAtlasTexture = nullptr;
    dynamicMSDFAtlasTexture = nullptr;
    backend = nullptr;
}

void FontSystem::NewFrame() {
    // Perf R6: Evict stale dynamic MSDF glyphs periodically. On a secondary
    // FontSystem the dynamic cache lives in the owner, so this is a no-op there.
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

void FontSystem::ClearGlyphs() {
    glyphCache.clear();
    dynamicMSDFGlyphCache.clear();
}

// ─── Bitmap font ────────────────────────────────────────────────────────────

bool FontSystem::LoadFont(const std::string& filepath, int pixelHeight) {
    // Secondary renderer: load into the owner's shared atlas instead.
    if (!ownsResources()) return Owner()->LoadFont(filepath, pixelHeight);
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

const FontSystem::Glyph* FontSystem::GetGlyph(uint32_t cp) {
    // Secondary renderer: the owner is the sole writer of the shared bitmap atlas.
    if (!ownsResources()) return Owner()->GetGlyph(cp);
    auto it = glyphCache.find(cp); if (it != glyphCache.end() && it->second.valid) return &it->second;
    if (LoadGlyph(cp)) return &glyphCache[cp];
    return nullptr;
}

bool FontSystem::LoadGlyph(uint32_t cp) {
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

// ─── Icon Font (secondary bitmap font) ──────────────────────────────────────

bool FontSystem::LoadIconFont(const std::string& filepath, int pixelHeight) {
    // Secondary renderer: load into the owner's shared atlas instead.
    if (!ownsResources()) return Owner()->LoadIconFont(filepath, pixelHeight);
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

bool FontSystem::LoadIconGlyph(uint32_t cp) {
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

const FontSystem::Glyph* FontSystem::GetIconGlyph(uint32_t cp) {
    // Secondary renderer: icon glyphs live in the owner's shared bitmap atlas.
    if (!ownsResources()) return Owner()->GetIconGlyph(cp);
    auto it = iconGlyphCache.find(cp);
    if (it != iconGlyphCache.end() && it->second.valid) return &it->second;
    if (LoadIconGlyph(cp)) return &iconGlyphCache[cp];
    return nullptr;
}

// ─── Atlas packing ──────────────────────────────────────────────────────────

bool FontSystem::EnsureAtlasSpace(int w, int h, int& ox, int& oy) {
    int pad = 2;
    if (atlasNextX + w + pad > atlasWidth) { atlasNextX = 0; atlasNextY += atlasCurrentRowHeight + pad; atlasCurrentRowHeight = 0; }
    if (atlasNextY + h + pad > atlasHeight) return false;
    ox = atlasNextX + pad; oy = atlasNextY + pad;
    atlasNextX += w + pad; atlasCurrentRowHeight = std::max(atlasCurrentRowHeight, h);
    return true;
}

// ─── Dynamic MSDF glyphs ────────────────────────────────────────────────────

const FontSystem::Glyph* FontSystem::GetOrGenerateMSDFGlyph(uint32_t cp) {
    // Secondary renderer: the owner is the sole writer of the shared dynamic atlas.
    if (!ownsResources()) return Owner()->GetOrGenerateMSDFGlyph(cp);
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

bool FontSystem::GenerateMSDFGlyph(uint32_t cp) {
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

bool FontSystem::EnsureDynamicMSDFAtlasSpace(int w, int h, int& ox, int& oy) {
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

void FontSystem::InitializeDefaultFont() {
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

    // Auto-load the Lucide icon font here so icon glyphs render on EVERY entry
    // point — FluentApp, the standalone gallery, or an external-GL host — not only
    // when FluentApp's constructor runs. FluentApp may override the path afterwards.
    if (!iconFontLoaded) {
        LoadIconFont("assets/fonts/lucide.ttf", 32);
    }
}

// ─── Metrics / measuring ────────────────────────────────────────────────────

Vec2 FontSystem::MeasureText(const std::string& text, float fontSize) {
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
    const FontSystem* o = Owner();
    if (!o->fontLoaded) return {static_cast<float>(text.size()) * fontSize * 0.6f, fontSize};
    float scale = fontSize / o->fontPixelHeight; float currentW = 0.0f, maxW = 0.0f;
    float totalH = (o->fontLineHeight > 0 ? o->fontLineHeight : o->fontPixelHeight) * scale;
    const char* ptr = text.data(); const char* end = ptr + text.size();
    while (ptr < end) {
        uint32_t cp = DecodeUTF8(ptr, end);
        if (cp == 0) break;
        if (cp == '\n') { maxW = std::max(maxW, currentW); currentW = 0.0f; totalH += (o->fontLineHeight * scale); continue; }
        const Glyph* g = GetGlyph(cp);
        if (g) currentW += g->advance * scale;
    }
    return {std::max(maxW, currentW), totalH};
}

float FontSystem::LineAdvancePx(float fontSize) {
    FontMSDF* mf = ActiveMSDF();
    if (mf && mf->IsLoaded()) return fontSize * mf->GetLineHeight();
    const FontSystem* o = Owner();
    if (o->fontLoaded) {
        float scale = fontSize / o->fontPixelHeight;
        return (o->fontLineHeight > 0 ? o->fontLineHeight : o->fontPixelHeight) * scale;
    }
    return fontSize;
}

std::vector<std::string> FontSystem::WrapTextLines(const std::string& text, float maxWidth,
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

Vec2 FontSystem::MeasureTextWrapped(const std::string& text, float maxWidth, float fontSize) {
    if (fontSize <= 0.0f) fontSize = 16.0f;
    float maxW = 0.0f;
    std::vector<std::string> lines = WrapTextLines(text, maxWidth, fontSize, maxW);
    return { maxW, LineAdvancePx(fontSize) * static_cast<float>(lines.size()) };
}

float FontSystem::GetFontAscender() const {
    FontMSDF* mf = ActiveMSDF();
    if (mf && mf->IsLoaded()) {
        return mf->GetAscender();
    }
    // Fallback razonable para fonts típicos (sans-serif estándar).
    return 0.8f;
}

float FontSystem::GetGlyphAdvance(uint32_t codepoint, float fontSize) {
    FontMSDF* mf = ActiveMSDF();
    if (mf && mf->IsLoaded()) {
        float scale = fontSize / mf->GetEmSize();
        const FontMSDF::Glyph* g = mf->GetGlyph(codepoint);
        if (g) return g->advance * scale;
        return fontSize * 0.3f;
    }
    const FontSystem* o = Owner();
    if (!o->fontLoaded) return fontSize * 0.6f;
    float scale = fontSize / o->fontPixelHeight;
    const Glyph* g = GetGlyph(codepoint);
    if (g) return g->advance * scale;
    return fontSize * 0.6f;
}

Vec2 FontSystem::MeasureTextWithFont(const std::string& text, const std::string& fontName, float fontSize) {
    return Manager().MeasureText(fontName, text, fontSize);
}

} // namespace FluentUI
