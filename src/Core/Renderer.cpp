#include "core/Renderer.h"
#include "Theme/Style.h"
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

// stb_image for PNG loading
#define STB_IMAGE_IMPLEMENTATION
#include "../external/stb_image.h"

// Definir constantes de anisotropía si no están disponibles en GLAD
#ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#endif
#ifndef GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
#endif

namespace FluentUI {
    bool PathExists(const std::filesystem::path& path)
    {
        std::error_code ec;
        return std::filesystem::exists(path, ec);
    }

    void PushUniquePath(std::vector<std::filesystem::path>& paths, const std::filesystem::path& candidate)
    {
        if (candidate.empty())
        {
            return;
        }
        if (std::find(paths.begin(), paths.end(), candidate) == paths.end())
        {
            paths.push_back(candidate);
        }
    }

    std::vector<std::filesystem::path> BuildResourceRoots()
    {
        std::vector<std::filesystem::path> roots;

        try
        {
            PushUniquePath(roots, std::filesystem::current_path());
        }
        catch (...)
        {
        }

        if (const char* base = SDL_GetBasePath())
        {
            std::filesystem::path basePath(base);

            auto current = basePath;
            while (!current.empty())
            {
                PushUniquePath(roots, current);
                auto parent = current.parent_path();
                if (parent == current)
                {
                    break;
                }
                current = parent;
            }
        }

        // Ensure we also consider the parent hierarchy of the already collected paths.
        const std::size_t initialSize = roots.size();
        for (std::size_t i = 0; i < initialSize; ++i)
        {
            auto current = roots[i].parent_path();
            while (!current.empty() && current != current.parent_path())
            {
                PushUniquePath(roots, current);
                current = current.parent_path();
            }
        }

        return roots;
    }

    std::filesystem::path ResolveResourcePath(const std::string& resource)
    {
        static std::unordered_set<std::string> missingOnce;

        std::filesystem::path candidate(resource);
        if (candidate.is_absolute())
        {
            return PathExists(candidate) ? candidate : std::filesystem::path();
        }

        static const std::vector<std::filesystem::path> searchRoots = BuildResourceRoots();
        for (const auto& root : searchRoots)
        {
            auto full = (root / candidate).lexically_normal();
            if (PathExists(full))
            {
                return full;
            }
        }

        if (missingOnce.insert(resource).second)
        {
            std::cerr << "ResolveResourcePath: no se encontró el recurso '" << resource << "' en los directorios conocidos.\n";
        }

        return {};
    }

    std::uint32_t DecodeUTF8(const char*& ptr, const char* end)
    {
        if (ptr >= end)
            return 0;

        unsigned char first = static_cast<unsigned char>(*ptr++);
        if (first < 0x80)
        {
            return first;
        }
        else if ((first >> 5) == 0x6)
        {
            if (ptr >= end)
                return 0xFFFD;
            unsigned char second = static_cast<unsigned char>(*ptr++);
            if ((second & 0xC0) != 0x80)
                return 0xFFFD;
            return ((first & 0x1F) << 6) | (second & 0x3F);
        }
        else if ((first >> 4) == 0xE)
        {
            if (ptr + 1 >= end)
                return 0xFFFD;
            unsigned char second = static_cast<unsigned char>(*ptr++);
            unsigned char third = static_cast<unsigned char>(*ptr++);
            if ((second & 0xC0) != 0x80 || (third & 0xC0) != 0x80)
                return 0xFFFD;
            return ((first & 0x0F) << 12) | ((second & 0x3F) << 6) | (third & 0x3F);
        }
        else if ((first >> 3) == 0x1E)
        {
            if (ptr + 2 >= end)
                return 0xFFFD;
            unsigned char second = static_cast<unsigned char>(*ptr++);
            unsigned char third = static_cast<unsigned char>(*ptr++);
            unsigned char fourth = static_cast<unsigned char>(*ptr++);
            if ((second & 0xC0) != 0x80 || (third & 0xC0) != 0x80 || (fourth & 0xC0) != 0x80)
                return 0xFFFD;
            return ((first & 0x07) << 18) | ((second & 0x3F) << 12) | ((third & 0x3F) << 6) | (fourth & 0x3F);
        }

        return 0xFFFD;
    }

    Renderer::~Renderer()
    {
        Shutdown();
    }

    void Renderer::ClearGlyphs()
    {
        glyphCache.clear();
        if (fontAtlasTexture != 0)
        {
            glDeleteTextures(1, &fontAtlasTexture);
            fontAtlasTexture = 0;
        }
        atlasWidth = atlasHeight = 0;
        atlasNextX = atlasNextY = 0;
        atlasCurrentRowHeight = 0;
        fontLoaded = false;
    }

    void Renderer::SetupTextRendering()
    {
        if (textSystemInitialized)
        {
            return;
        }

        if (FT_Init_FreeType(&ftLibrary))
        {
            std::cerr << "No se pudo inicializar FreeType.\n";
            return;
        }

        try
        {
            textShaderProgram = CreateShaderProgram("shaders/text_vertex.glsl", "shaders/text_fragment.glsl");
        }
        catch (const std::exception& e)
        {
            std::cerr << "Error creando shaders de texto: " << e.what() << std::endl;
            return;
        }

        glUseProgram(textShaderProgram);
        int textureUniform = glGetUniformLocation(textShaderProgram, "uFontAtlas");
        if (textureUniform >= 0)
        {
            glUniform1i(textureUniform, 0);
        }
        textColorUniform = glGetUniformLocation(textShaderProgram, "uTextColor");
        textProjectionUniform = glGetUniformLocation(textShaderProgram, "projection");
        glUseProgram(0);
        projectionDirty = true;

        if (fontAtlasTexture == 0)
        {
            atlasWidth = 1024;
            atlasHeight = 1024;
            atlasNextX = 0;
            atlasNextY = 0;
            atlasCurrentRowHeight = 0;

            GL_CHECK(glGenTextures(1, &fontAtlasTexture));
            glBindTexture(GL_TEXTURE_2D, fontAtlasTexture);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            GL_CHECK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, atlasWidth, atlasHeight, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr));
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            // Usar GL_LINEAR para minificación y GL_LINEAR para magnificación para mejor calidad
            // pero con mejor configuración para texto nítido
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            // Mejorar la nitidez con configuración de aniso (si está disponible)
            // Intentar usar anisotropía si está disponible (manejar error silenciosamente si no)
            GLfloat maxAniso = 1.0f;
            glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAniso);
            if (maxAniso > 1.0f) {
                glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, std::min(maxAniso, 2.0f));
            }
            GLint swizzleMask[] = { GL_RED, GL_RED, GL_RED, GL_RED };
            glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);
            glBindTexture(GL_TEXTURE_2D, 0);
        }

        GL_CHECK(glGenVertexArrays(1, &textVAO));
        GL_CHECK(glGenBuffers(1, &textVBO));

        glBindVertexArray(textVAO);
        glBindBuffer(GL_ARRAY_BUFFER, textVBO);
        GL_CHECK(glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, nullptr, GL_DYNAMIC_DRAW));
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(0));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        textSystemInitialized = true;
    }

    void Renderer::InitializeDefaultFont()
    {
        if (!textSystemInitialized)
        {
            return;
        }

        // Try to load MSDF atlas first (best quality)
        const std::string msdfAtlasImage = "assets/fonts/atlas.png";
        const std::string msdfAtlasJson = "assets/fonts/atlas.json";
        
        msdfFont = std::make_unique<FontMSDF>();
        if (msdfFont->Load(msdfAtlasImage, msdfAtlasJson))
        {
            std::cout << "MSDF font loaded successfully." << std::endl;
            return;
        }
        
        std::cout << "MSDF atlas not found, initializing dynamic MSDF generation from FreeType..." << std::endl;
        
        // Initialize MSDF generator for dynamic generation
        msdfGenerator = std::make_unique<MSDFGenerator>();
        
        // Create dynamic MSDF atlas texture
        if (dynamicMSDFAtlasTexture == 0) {
            GL_CHECK(glGenTextures(1, &dynamicMSDFAtlasTexture));
            glBindTexture(GL_TEXTURE_2D, dynamicMSDFAtlasTexture);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            GL_CHECK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, dynamicAtlasWidth, dynamicAtlasHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr));
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            // Para MSDF, GL_LINEAR es correcto pero con mejor configuración
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            // Mejorar la nitidez con configuración de aniso (si está disponible)
            // Intentar usar anisotropía si está disponible (manejar error silenciosamente si no)
            GLfloat maxAniso = 1.0f;
            glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAniso);
            if (maxAniso > 1.0f) {
                glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, std::min(maxAniso, 2.0f));
            }
            glBindTexture(GL_TEXTURE_2D, 0);
            
            // Initialize packing state for the dynamic MSDF atlas
            dynamicAtlasNextX = 2; // Start with padding
            dynamicAtlasNextY = 2;
            dynamicAtlasCurrentRowHeight = 0;
            
            std::cout << "Dynamic MSDF atlas texture created (" << dynamicAtlasWidth << "x" << dynamicAtlasHeight << ")" << std::endl;
        }


        // Fallback to FreeType rendering
        constexpr int defaultPixelHeight = 14;

        const std::string resourceRoot = "assets/fonts/";
        const std::vector<std::string> candidates = {
            resourceRoot + "SegoeUI.ttf",
            resourceRoot + "SegoeUIVariable.ttf",
            resourceRoot + "SegoeUI-Semibold.ttf",
            "fonts/SegoeUI.ttf",
            "fonts/SegoeUIVariable.ttf",
            "fonts/SegoeUI-Semibold.ttf"
        };

        auto tryLoadCandidates = [this, defaultPixelHeight](const std::vector<std::string>& paths) {
            for (const auto& candidate : paths)
            {
                if (candidate.empty())
                {
                    continue;
                }
                if (LoadFont(candidate, defaultPixelHeight))
                {
                    std::cout << "Fuente cargada: " << candidate << std::endl;
                    return true;
                }
            }
            return false;
        };

        if (tryLoadCandidates(candidates))
        {
            return;
        }

#if defined(_WIN32)
        const std::vector<std::string> windowsCandidates = {
            "C:/Windows/Fonts/segoeui.ttf",
            "C:/Windows/Fonts/SegoeUI.ttf",
            "C:/Windows/Fonts/segoeuib.ttf",
            "C:/Windows/Fonts/arial.ttf",
            "C:/Windows/Fonts/calibri.ttf"
        };
        if (tryLoadCandidates(windowsCandidates))
        {
            return;
        }
#elif defined(__APPLE__)
        const std::vector<std::string> appleCandidates = {
            "/Library/Fonts/Segoe UI.ttf",
            "/System/Library/Fonts/Helvetica.ttc",
            "/System/Library/Fonts/SFNS.ttf"
        };
        if (tryLoadCandidates(appleCandidates))
        {
            return;
        }
#else
        const std::vector<std::string> linuxCandidates = {
            "/usr/share/fonts/truetype/msttcorefonts/Segoe_UI.ttf",
            "/usr/share/fonts/truetype/msttcorefonts/Arial.ttf",
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
        };
        if (tryLoadCandidates(linuxCandidates))
        {
            return;
        }
#endif

        std::cerr << "No se pudo encontrar una fuente Segoe UI. Colócala en assets/fonts/SegoeUI.ttf o llama a Renderer::LoadFont manualmente.\n";
    }

    bool Renderer::Init(SDL_Window* window) {
        this->window = window;
        
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

        glContext = SDL_GL_CreateContext(window);
        if (!glContext) {
            std::cerr << "Error creating OpenGL context\n";
            return false;
        }
        SDL_GL_MakeCurrent(window, glContext);

        if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
            std::cerr << "Error inicializando GLAD\n";
            return false;
        }
        
        SDL_GL_SetSwapInterval(1); // vsync

        int width, height;
        SDL_GetWindowSize(window, &width, &height);
        viewportSize = Vec2(static_cast<float>(width), static_cast<float>(height));
        glViewport(0, 0, width, height);
        glClearColor(0.93f, 0.93f, 0.95f, 1.0f);
        SetupShaders();
        SetupTextRendering();
        InitializeDefaultFont();
        
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        return true;
    }

    void Renderer::BeginFrame(const Color& clearColor) {
        if (window) {
            int width, height;
            SDL_GetWindowSize(window, &width, &height);
            viewportSize = Vec2(static_cast<float>(width), static_cast<float>(height));
            glViewport(0, 0, width, height);
        }

        glClearColor(clearColor.r, clearColor.g, clearColor.b, clearColor.a);
        glClear(GL_COLOR_BUFFER_BIT);
        projectionDirty = true;
        quadVertices.clear();
        quadIndices.clear();
        lineVertices.clear();
        lineBatchWidth = 1.0f;
    }

    void Renderer::DrawRect(const Vec2& pos, const Vec2& size, const Color& color, float cornerRadius) {
        // DrawRect should draw only the BORDER, not fill
        float lineWidth = 1.0f;

        if (cornerRadius <= 0.0f || cornerRadius < 0.5f) {
            // Simple quad outline (no rounding)
            // Top
            DrawLine(pos, Vec2(pos.x + size.x, pos.y), color, lineWidth);
            // Right
            DrawLine(Vec2(pos.x + size.x, pos.y), Vec2(pos.x + size.x, pos.y + size.y), color, lineWidth);
            // Bottom
            DrawLine(Vec2(pos.x + size.x, pos.y + size.y), Vec2(pos.x, pos.y + size.y), color, lineWidth);
            // Left
            DrawLine(Vec2(pos.x, pos.y + size.y), pos, color, lineWidth);
            return;
        }

        // Rounded rectangle outline
        const float pi = 3.14159265358979323846f;
        const int segments = 8; // segments per corner arc

        // Clamp cornerRadius so it doesn't exceed half the smallest dimension
        float cr = std::min(cornerRadius, std::min(size.x, size.y) * 0.5f);

        // Corner arc centers
        float cx_left  = pos.x + cr;
        float cx_right = pos.x + size.x - cr;
        float cy_top   = pos.y + cr;
        float cy_bot   = pos.y + size.y - cr;

        // Generate perimeter points around the rounded rectangle
        // Order: top-right corner, right side down, bottom-right corner,
        //        bottom side left, bottom-left corner, left side up, top-left corner, top side right
        std::vector<Vec2> points;
        points.reserve(4 * segments + 4);

        // Top-right corner (arc from -pi/2 to 0, i.e. 270 to 360 degrees)
        for (int i = 0; i <= segments; ++i) {
            float angle = -pi * 0.5f + (pi * 0.5f) * static_cast<float>(i) / static_cast<float>(segments);
            points.push_back(Vec2(cx_right + cr * std::cos(angle), cy_top + cr * std::sin(angle)));
        }

        // Bottom-right corner (arc from 0 to pi/2)
        for (int i = 0; i <= segments; ++i) {
            float angle = (pi * 0.5f) * static_cast<float>(i) / static_cast<float>(segments);
            points.push_back(Vec2(cx_right + cr * std::cos(angle), cy_bot + cr * std::sin(angle)));
        }

        // Bottom-left corner (arc from pi/2 to pi)
        for (int i = 0; i <= segments; ++i) {
            float angle = pi * 0.5f + (pi * 0.5f) * static_cast<float>(i) / static_cast<float>(segments);
            points.push_back(Vec2(cx_left + cr * std::cos(angle), cy_bot + cr * std::sin(angle)));
        }

        // Top-left corner (arc from pi to 3pi/2)
        for (int i = 0; i <= segments; ++i) {
            float angle = pi + (pi * 0.5f) * static_cast<float>(i) / static_cast<float>(segments);
            points.push_back(Vec2(cx_left + cr * std::cos(angle), cy_top + cr * std::sin(angle)));
        }

        // Draw line segments along the perimeter
        for (size_t i = 0; i < points.size(); ++i) {
            size_t next = (i + 1) % points.size();
            DrawLine(points[i], points[next], color, lineWidth);
        }
    }

    void Renderer::DrawRectFilled(const Vec2& pos, const Vec2& size, const Color& color, float cornerRadius) {
        // Fast path: no rounding, draw a simple quad
        if (cornerRadius <= 0.0f || cornerRadius < 0.5f) {
            // Flush if the batch is full
            if (quadVertices.size() + 4 > MAX_QUAD_VERTICES || quadIndices.size() + 6 > MAX_QUAD_INDICES) {
                FlushBatch();
            }

            unsigned int baseIndex = static_cast<unsigned int>(quadVertices.size());
            quadVertices.push_back({ pos.x,             pos.y,              color.r, color.g, color.b, color.a });
            quadVertices.push_back({ pos.x + size.x,    pos.y,              color.r, color.g, color.b, color.a });
            quadVertices.push_back({ pos.x + size.x,    pos.y + size.y,     color.r, color.g, color.b, color.a });
            quadVertices.push_back({ pos.x,             pos.y + size.y,     color.r, color.g, color.b, color.a });

            quadIndices.push_back(baseIndex + 0);
            quadIndices.push_back(baseIndex + 1);
            quadIndices.push_back(baseIndex + 2);
            quadIndices.push_back(baseIndex + 0);
            quadIndices.push_back(baseIndex + 2);
            quadIndices.push_back(baseIndex + 3);
            return;
        }

        // Rounded rectangle filled path using triangle fan from center
        const float pi = 3.14159265358979323846f;
        const int segments = 8; // segments per corner arc

        // Clamp cornerRadius so it doesn't exceed half the smallest dimension
        float cr = std::min(cornerRadius, std::min(size.x, size.y) * 0.5f);

        // Corner arc centers
        float cx_left  = pos.x + cr;
        float cx_right = pos.x + size.x - cr;
        float cy_top   = pos.y + cr;
        float cy_bot   = pos.y + size.y - cr;

        // Generate perimeter vertices around the rounded rectangle
        // Each corner has (segments + 1) points, but adjacent corners share the
        // boundary point, so total perimeter points = 4 * segments
        // We store them in order going clockwise starting from the top-right corner.
        std::vector<Vec2> perim;
        perim.reserve(4 * segments + 4);

        // Top-right corner (arc from -pi/2 to 0)
        for (int i = 0; i <= segments; ++i) {
            float angle = -pi * 0.5f + (pi * 0.5f) * static_cast<float>(i) / static_cast<float>(segments);
            perim.push_back(Vec2(cx_right + cr * std::cos(angle), cy_top + cr * std::sin(angle)));
        }

        // Bottom-right corner (arc from 0 to pi/2)
        for (int i = 0; i <= segments; ++i) {
            float angle = (pi * 0.5f) * static_cast<float>(i) / static_cast<float>(segments);
            perim.push_back(Vec2(cx_right + cr * std::cos(angle), cy_bot + cr * std::sin(angle)));
        }

        // Bottom-left corner (arc from pi/2 to pi)
        for (int i = 0; i <= segments; ++i) {
            float angle = pi * 0.5f + (pi * 0.5f) * static_cast<float>(i) / static_cast<float>(segments);
            perim.push_back(Vec2(cx_left + cr * std::cos(angle), cy_bot + cr * std::sin(angle)));
        }

        // Top-left corner (arc from pi to 3pi/2)
        for (int i = 0; i <= segments; ++i) {
            float angle = pi + (pi * 0.5f) * static_cast<float>(i) / static_cast<float>(segments);
            perim.push_back(Vec2(cx_left + cr * std::cos(angle), cy_top + cr * std::sin(angle)));
        }

        // Total vertices = 1 (center) + perim.size()
        // Total triangles = perim.size() (fan from center, wrapping around)
        // Total indices = perim.size() * 3
        size_t numPerim = perim.size();
        size_t numVerts = 1 + numPerim;
        size_t numIndices = numPerim * 3;

        // Flush if the batch cannot hold this rounded rect
        if (quadVertices.size() + numVerts > MAX_QUAD_VERTICES || quadIndices.size() + numIndices > MAX_QUAD_INDICES) {
            FlushBatch();
        }

        // Center vertex
        unsigned int baseIndex = static_cast<unsigned int>(quadVertices.size());
        float cx = pos.x + size.x * 0.5f;
        float cy = pos.y + size.y * 0.5f;
        quadVertices.push_back({ cx, cy, color.r, color.g, color.b, color.a });

        // Perimeter vertices
        for (size_t i = 0; i < numPerim; ++i) {
            quadVertices.push_back({ perim[i].x, perim[i].y, color.r, color.g, color.b, color.a });
        }

        // Triangle fan indices: center, perim[i], perim[i+1] (wrapping)
        unsigned int centerIdx = baseIndex;
        for (size_t i = 0; i < numPerim; ++i) {
            unsigned int cur  = baseIndex + 1 + static_cast<unsigned int>(i);
            unsigned int next = baseIndex + 1 + static_cast<unsigned int>((i + 1) % numPerim);
            quadIndices.push_back(centerIdx);
            quadIndices.push_back(cur);
            quadIndices.push_back(next);
        }
    }

    void Renderer::DrawRectWithElevation(const Vec2& pos, const Vec2& size, const Color& color, float cornerRadius, float elevation) {
        if (elevation > 0.0f) {
            // Fluent Design multi-layer elevation shadow
            // Layer 1: Ambient shadow (wide, low opacity)
            float ambientOpacity = std::min(0.12f, elevation * 0.006f);
            Vec2 ambientExpand(elevation * 0.8f, elevation * 0.8f);
            Color ambientColor(0.0f, 0.0f, 0.0f, ambientOpacity);
            DrawRectFilled(pos - ambientExpand * 0.5f + Vec2(0.0f, elevation * 0.15f),
                           size + ambientExpand, ambientColor,
                           cornerRadius + elevation * 0.3f);

            // Layer 2: Key shadow (directional, offset downward)
            float keyOpacity = std::min(0.20f, elevation * 0.012f);
            float keyOffsetY = elevation * 0.4f;
            Vec2 keyExpand(elevation * 0.3f, elevation * 0.3f);
            Color keyColor(0.0f, 0.0f, 0.0f, keyOpacity);
            DrawRectFilled(pos - keyExpand * 0.5f + Vec2(0.0f, keyOffsetY),
                           size + keyExpand, keyColor,
                           cornerRadius + elevation * 0.15f);

            // Layer 3: Close shadow (tight, higher opacity for definition)
            if (elevation > 2.0f) {
                float closeOpacity = std::min(0.08f, elevation * 0.004f);
                Color closeColor(0.0f, 0.0f, 0.0f, closeOpacity);
                DrawRectFilled(pos + Vec2(0.0f, 1.0f), size, closeColor, cornerRadius);
            }
        }

        // Dibujar el rectángulo principal
        DrawRectFilled(pos, size, color, cornerRadius);
    }

    void Renderer::DrawRectAcrylic(const Vec2& pos, const Vec2& size, const Color& tintColor, float cornerRadius, float opacity, float blurAmount) {
        // Efecto Acrylic: transparencia + blur simulado + tint
        // Nota: El blur real requeriría framebuffers y shaders más complejos.
        // Por ahora, simulamos el efecto con múltiples capas semi-transparentes y colores.
        
        // Determinar si el tema es oscuro basado en el brillo del color de tint
        float brightness = (tintColor.r + tintColor.g + tintColor.b) / 3.0f;
        bool isDarkTheme = brightness < 0.5f;
        
        // Capa base: fondo blanco/gris semi-transparente (simula el blur del fondo)
        // En Fluent Design, el acrylic tiene un color base que depende del tema
        Color baseColor;
        if (isDarkTheme) {
            // Tema oscuro: base oscura con brillo
            baseColor = Color(0.1f, 0.1f, 0.15f, opacity * 0.5f);
        } else {
            // Tema claro: base clara
            baseColor = Color(0.95f, 0.95f, 0.98f, opacity * 0.4f);
        }
        DrawRectFilled(pos, size, baseColor, cornerRadius);
        
        // Capa de tint: color de acento aplicado con transparencia muy sutil
        Color acrylicTint = tintColor;
        acrylicTint.a = opacity * 0.08f; // Tint muy sutil
        DrawRectFilled(pos, size, acrylicTint, cornerRadius);
        
        // Capa superior: color principal con transparencia (simula el "frosted glass")
        // Mezclar el color de tint con el color base para un efecto más realista
        Color acrylicColor = tintColor;
        if (isDarkTheme) {
            // Mezclar con blanco para tema oscuro (efecto más brillante)
            acrylicColor = Color(
                tintColor.r * 0.7f + 0.2f,
                tintColor.g * 0.7f + 0.2f,
                tintColor.b * 0.7f + 0.25f,
                opacity * 0.75f
            );
        } else {
            // Mezclar con blanco para tema claro
            acrylicColor = Color(
                tintColor.r * 0.3f + 0.7f,
                tintColor.g * 0.3f + 0.7f,
                tintColor.b * 0.3f + 0.72f,
                opacity * 0.8f
            );
        }
        DrawRectFilled(pos, size, acrylicColor, cornerRadius);
        
        // Borde sutil para mejorar la percepción del efecto
        if (cornerRadius > 0.0f) {
            Color borderColor = isDarkTheme ? 
                Color(1.0f, 1.0f, 1.0f, opacity * 0.15f) : 
                Color(0.0f, 0.0f, 0.0f, opacity * 0.1f);
            DrawRect(pos, size, borderColor, cornerRadius);
        }
    }

    void Renderer::DrawLine(const Vec2& start, const Vec2& end, const Color& color, float width) {
        float lineWidth = std::max(width, 1.0f);
        
        // Flush si cambia el grosor de línea o si el batch está lleno
        if (!lineVertices.empty() && std::abs(lineWidth - lineBatchWidth) > 0.1f) {
            FlushBatch();
        }
        if (lineVertices.size() + 2 > MAX_LINE_VERTICES) {
            FlushBatch();
        }
        
        lineBatchWidth = lineWidth;

        lineVertices.push_back({ start.x, start.y, color.r, color.g, color.b, color.a });
        lineVertices.push_back({ end.x,   end.y,   color.r, color.g, color.b, color.a });
    }

    void Renderer::DrawRipple(const Vec2& center, float radius, float opacity) {
        if (radius <= 0.0f || opacity <= 0.0f) {
            return;
        }
        FlushBatch();

        int segments = static_cast<int>(std::max(12.0f, radius * 0.75f));
        std::vector<float> vertices;

        constexpr float twoPi = 6.28318530718f;

        UpdateProjection();

        // Ripple es un círculo con borde y opacidad variable
        vertices.reserve(segments * 6);
        for (int i = 0; i < segments; ++i) {
            float angle1 = (i / static_cast<float>(segments)) * twoPi;
            float angle2 = ((i + 1) / static_cast<float>(segments)) * twoPi;

            float x1 = center.x + std::cos(angle1) * radius;
            float y1 = center.y + std::sin(angle1) * radius;
            float x2 = center.x + std::cos(angle2) * radius;
            float y2 = center.y + std::sin(angle2) * radius;

            // Color blanco con opacidad variable
            Color rippleColor(1.0f, 1.0f, 1.0f, opacity * 0.3f);

            vertices.push_back(center.x);
            vertices.push_back(center.y);
            vertices.push_back(rippleColor.r);
            vertices.push_back(rippleColor.g);
            vertices.push_back(rippleColor.b);
            vertices.push_back(rippleColor.a);

            vertices.push_back(x1);
            vertices.push_back(y1);
            vertices.push_back(rippleColor.r);
            vertices.push_back(rippleColor.g);
            vertices.push_back(rippleColor.b);
            vertices.push_back(rippleColor.a);

            vertices.push_back(x2);
            vertices.push_back(y2);
            vertices.push_back(rippleColor.r);
            vertices.push_back(rippleColor.g);
            vertices.push_back(rippleColor.b);
            vertices.push_back(rippleColor.a);
        }

        EnsureBatchResources();
        glUseProgram(shaderProgram);
        glBindVertexArray(circleVAO);
        glBindBuffer(GL_ARRAY_BUFFER, circleVBO);
        GL_CHECK(glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_DYNAMIC_DRAW));

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        GL_CHECK(glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size() / 6)));

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    void Renderer::DrawCircle(const Vec2& center, float radius, const Color& color, bool filled) {
        if (radius <= 0.0f) {
            return;
        }
        
        // Los círculos usan un renderizado diferente (triangulación en tiempo real)
        // así que siempre flusheamos el batch primero para mantener la consistencia
        // Nota: En el futuro se podría optimizar acumulando círculos con el mismo shader
        FlushBatch();

        int segments = static_cast<int>(std::max(12.0f, radius * 0.75f));
        std::vector<float> vertices;

        constexpr float twoPi = 6.28318530718f;

        UpdateProjection();

        if (filled) {
            vertices.reserve((segments + 2) * 6);
            vertices.push_back(center.x);
            vertices.push_back(center.y);
            vertices.push_back(color.r);
            vertices.push_back(color.g);
            vertices.push_back(color.b);
            vertices.push_back(color.a);

            for (int i = 0; i <= segments; ++i) {
                float angle = twoPi * static_cast<float>(i) / static_cast<float>(segments);
                Vec2 point(center.x + std::cos(angle) * radius,
                           center.y + std::sin(angle) * radius);
                vertices.push_back(point.x);
                vertices.push_back(point.y);
                vertices.push_back(color.r);
                vertices.push_back(color.g);
                vertices.push_back(color.b);
                vertices.push_back(color.a);
            }

            EnsureBatchResources();
            glBindVertexArray(circleVAO);
            glBindBuffer(GL_ARRAY_BUFFER, circleVBO);
            GL_CHECK(glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_DYNAMIC_DRAW));

            glUseProgram(shaderProgram);
            GL_CHECK(glDrawArrays(GL_TRIANGLE_FAN, 0, static_cast<GLsizei>((segments + 2))));

            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindVertexArray(0);
        } else {
            vertices.reserve(segments * 6);
            for (int i = 0; i < segments; ++i) {
                float angle = twoPi * static_cast<float>(i) / static_cast<float>(segments);
                Vec2 point(center.x + std::cos(angle) * radius,
                           center.y + std::sin(angle) * radius);
                vertices.push_back(point.x);
                vertices.push_back(point.y);
                vertices.push_back(color.r);
                vertices.push_back(color.g);
                vertices.push_back(color.b);
                vertices.push_back(color.a);
            }

            EnsureBatchResources();
            glBindVertexArray(circleVAO);
            glBindBuffer(GL_ARRAY_BUFFER, circleVBO);
            GL_CHECK(glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_DYNAMIC_DRAW));

            glUseProgram(shaderProgram);
            GL_CHECK(glDrawArrays(GL_LINE_LOOP, 0, static_cast<GLsizei>(segments)));

            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindVertexArray(0);
        }
    }
    void Renderer::DrawText(const Vec2& pos, const std::string& text, const Color& color, float fontSize) {
        fontSize = std::max(fontSize, 10.0f);
        FlushBatch();

        if (msdfFont && msdfFont->IsLoaded()) {
             // MSDF Path
             if (viewportSize.x <= 0 || viewportSize.y <= 0) {
                 return; // Viewport not ready
             }
             
             // Setup projection matrix
             float left = 0.0f;
             float right = viewportSize.x;
             float top = 0.0f;
             float bottom = viewportSize.y;
             float ortho[16] = {
                2.0f / (right - left), 0.0f,                    0.0f, 0.0f,
                0.0f,                  -2.0f / (bottom - top),  0.0f, 0.0f,
                0.0f,                   0.0f,                  -1.0f, 0.0f,
               -(right + left) / (right - left),
                (bottom + top) / (bottom - top),
                0.0f, 1.0f
            };
            
            // Enable blending for text rendering and disable depth testing
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDisable(GL_DEPTH_TEST);
            
            // Bind MSDF shader - create if needed
            if (!msdfFont) {
                msdfFont = std::make_unique<FontMSDF>();
                msdfFont->BindShaderOnly(ortho);
            } else if (msdfFont->IsLoaded()) {
                msdfFont->Bind(ortho);
            } else {
                msdfFont->BindShaderOnly(ortho);
            }
            
            // Set color
            if (msdfFont) {
                msdfFont->SetColor(color);
            }
            
            // Bind appropriate texture
            if (msdfFont->IsLoaded()) {
                // Pre-generated atlas is bound by FontMSDF::Bind()
            } else if (dynamicMSDFAtlasTexture != 0) {
                // Bind dynamic atlas texture
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, dynamicMSDFAtlasTexture);
                if (msdfFont->GetShaderProgram() != 0) {
                    GLint texUniform = glGetUniformLocation(msdfFont->GetShaderProgram(), "msdfTexture");
                    if (texUniform >= 0) {
                        glUniform1i(texUniform, 0);
                    }
                }
            }
             
             // Calculate scale - use emSize if available, otherwise use fontPixelHeight
             float emSize = msdfFont->GetEmSize();
             if (emSize <= 0 && fontLoaded) {
                 emSize = fontPixelHeight; // Fallback to FreeType pixel height
             }
             if (emSize <= 0) {
                 emSize = fontSize; // Final fallback
             }
             float scale = fontSize / emSize;
             
             // Calculate baseline: pos.y is top-left, we need to offset by ascender
             // Asegurar que haya suficiente espacio para descenders
             float ascender = msdfFont->GetAscender();
             if (ascender <= 0 && fontLoaded) {
                 ascender = fontAscent / emSize; // Use FreeType ascender if available
             }
             if (ascender <= 0) {
                 ascender = 0.8f; // Default ascender ratio
             }
             // Calcular descender para asegurar espacio suficiente
             float descender = 0.0f;
             if (fontLoaded && fontDescent < 0) {
                 descender = std::abs(fontDescent) / emSize; // Descender es negativo en FreeType
             }
             if (descender <= 0) {
                 descender = 0.2f; // Default descender ratio
             }
             // La baseline se calcula desde pos.y (parte superior) más el ascender
             // Los descenders se extienden por debajo de la baseline y están incluidos en el bitmap del glifo
             // Ajustar pos.y para incluir espacio para descenders si es necesario
             float baseline = pos.y + ascender * fontSize;
             
             float xpos = pos.x;
             float lineHeight = fontSize * msdfFont->GetLineHeight();
             
             const char* ptr = text.data();
             const char* end = ptr + text.size();
             
             glBindVertexArray(textVAO);
             glBindBuffer(GL_ARRAY_BUFFER, textVBO);

             // Batch all glyph quads into a single buffer, then draw once
             // Track which atlas is in use to flush when switching
             std::vector<float> batchVertices;
             batchVertices.reserve(text.size() * 6 * 4); // 6 verts * 4 floats per glyph estimate
             bool currentlyUsingDynamic = false;
             bool firstGlyph = true;

             auto flushTextBatch = [&]() {
                 if (batchVertices.empty()) return;
                 GL_CHECK(glBufferData(GL_ARRAY_BUFFER, batchVertices.size() * sizeof(float),
                              batchVertices.data(), GL_DYNAMIC_DRAW));
                 GL_CHECK(glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(batchVertices.size() / 4)));
                 batchVertices.clear();
             };

             while (ptr < end) {
                uint32_t codepoint = DecodeUTF8(ptr, end);
                if (codepoint == 0) break;
                if (codepoint == '\n') {
                    xpos = pos.x;
                    baseline += lineHeight;
                    continue;
                }

                // Try pre-generated MSDF first, then dynamic generation
                const FontMSDF::Glyph* glyph = nullptr;
                const Glyph* dynamicGlyph = nullptr;
                bool useDynamicMSDF = false;

                if (msdfFont && msdfFont->IsLoaded()) {
                    glyph = msdfFont->GetGlyph(codepoint);
                }

                // If not found in pre-generated atlas, try dynamic generation
                if (!glyph && msdfGenerator && fontFace) {
                    dynamicGlyph = GetOrGenerateMSDFGlyph(codepoint);
                    useDynamicMSDF = (dynamicGlyph != nullptr);
                }

                // Handle spaces and missing glyphs: still advance xpos by advance
                float advance = 0.0f;
                if (glyph) {
                    advance = glyph->advance * scale;
                    if (codepoint == 32) { xpos += advance; continue; }
                } else if (dynamicGlyph) {
                    advance = dynamicGlyph->advance * scale;
                    if (codepoint == 32) { xpos += advance; continue; }
                } else {
                    xpos += (codepoint == 32) ? fontSize * 0.3f : fontSize * 0.2f;
                    continue;
                }

                // If atlas type changed, flush the batch
                if (!firstGlyph && useDynamicMSDF != currentlyUsingDynamic) {
                    flushTextBatch();
                }
                if (useDynamicMSDF && (firstGlyph || !currentlyUsingDynamic)) {
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, dynamicMSDFAtlasTexture);
                } else if (!useDynamicMSDF && (firstGlyph || currentlyUsingDynamic)) {
                    // Re-bind pre-generated atlas if we switched from dynamic
                    if (!firstGlyph && currentlyUsingDynamic && msdfFont->IsLoaded()) {
                        glActiveTexture(GL_TEXTURE0);
                        // Pre-generated atlas was bound by msdfFont->Bind()
                    }
                }
                currentlyUsingDynamic = useDynamicMSDF;
                firstGlyph = false;

                // Calculate glyph position and size
                float x0, y0, w, h, u0, v0, u1, v1;

                if (useDynamicMSDF && dynamicGlyph) {
                    x0 = std::round(xpos + dynamicGlyph->bearing.x * scale);
                    y0 = baseline - dynamicGlyph->bearing.y * scale;
                    w = dynamicGlyph->size.x * scale;
                    h = dynamicGlyph->size.y * scale;
                    u0 = dynamicGlyph->uv0.x; v0 = dynamicGlyph->uv0.y;
                    u1 = dynamicGlyph->uv1.x; v1 = dynamicGlyph->uv1.y;
                } else {
                    x0 = std::round(xpos + glyph->bearing.x * scale);
                    y0 = baseline - glyph->bearing.y * scale;
                    w = glyph->planeBounds.x * scale;
                    h = glyph->planeBounds.y * scale;
                    u0 = glyph->uv0.x; v0 = glyph->uv0.y;
                    u1 = glyph->uv1.x; v1 = glyph->uv1.y;
                }

                if (w <= 0 || h <= 0) { xpos += advance; continue; }

                // Add quad vertices to batch (6 vertices, 4 floats each)
                float verts[] = {
                    x0,     y0,       u0, v0,
                    x0 + w, y0 + h,   u1, v1,
                    x0,     y0 + h,   u0, v1,
                    x0,     y0,       u0, v0,
                    x0 + w, y0,       u1, v0,
                    x0 + w, y0 + h,   u1, v1
                };
                batchVertices.insert(batchVertices.end(), std::begin(verts), std::end(verts));

                xpos += advance;
             }

             // Flush remaining batched glyphs
             flushTextBatch();

             glBindBuffer(GL_ARRAY_BUFFER, 0);
             glBindVertexArray(0);
             
             if (msdfFont) {
                 if (msdfFont->IsLoaded()) {
                     msdfFont->Unbind();
                 } else {
                     msdfFont->UnbindShader();
                     glBindTexture(GL_TEXTURE_2D, 0);
                 }
             }
             return;
        }

        // FREETYPE FALLBACK
        if (!textSystemInitialized || !fontLoaded || textShaderProgram == 0 || textVAO == 0) return;

        float scale = fontSize / fontPixelHeight;
        float baseline = pos.y + fontAscent * scale;
        float penX = pos.x;
        float lineHeight = ((fontLineHeight > 0.0f ? fontLineHeight : fontPixelHeight) * scale);
        std::uint32_t prevCodepoint = 0;

        glUseProgram(textShaderProgram);
        UpdateProjection();
        
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        
        if (textColorUniform >= 0) glUniform4f(textColorUniform, color.r, color.g, color.b, color.a);
        
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, fontAtlasTexture);
        glBindVertexArray(textVAO);
        glBindBuffer(GL_ARRAY_BUFFER, textVBO);

        const char* ptr = text.data();
        const char* end = ptr + text.size();

        while (ptr < end) {
            std::uint32_t codepoint = DecodeUTF8(ptr, end);
            if (codepoint == 0) break;
            if (codepoint == '\n') {
                penX = pos.x;
                baseline += lineHeight;
                continue;
            }

            const Glyph* glyph = GetGlyph(codepoint);
            if (!glyph) {
                glyph = GetGlyph('?');
                if (!glyph) continue;
            }

            if (prevCodepoint != 0) {
                penX += GetKerning(prevCodepoint, codepoint) * scale;
            }

            // Alinear solo X a píxeles enteros para mejor nitidez horizontal
            // NO redondear Y para mantener la alineación vertical correcta
            float xpos = std::round(penX + glyph->bearing.x * scale);
            float ypos = baseline - glyph->bearing.y * scale;
            float w = glyph->size.x * scale;
            float h = glyph->size.y * scale;

            if (w <= 0.0f || h <= 0.0f) {
                penX += glyph->advance * scale;
                prevCodepoint = codepoint;
                continue;
            }

            float u0 = glyph->uv0.x; float v0 = glyph->uv0.y;
            float u1 = glyph->uv1.x; float v1 = glyph->uv1.y;
            float vTop = std::max(v0, v1);
            float vBottom = std::min(v0, v1);

            float vertices[6][4] = {
                { xpos,     ypos + h, u0, vTop },
                { xpos,     ypos,     u0, vBottom },
                { xpos + w, ypos,     u1, vBottom },
                { xpos,     ypos + h, u0, vTop },
                { xpos + w, ypos,     u1, vBottom },
                { xpos + w, ypos + h, u1, vTop }
            };

            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
            GL_CHECK(glDrawArrays(GL_TRIANGLES, 0, 6));

            penX += glyph->advance * scale;
            prevCodepoint = codepoint;
        }

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    bool Renderer::LoadFont(const std::string& filepath, int pixelHeight)
    {
        if (!textSystemInitialized || ftLibrary == nullptr) return false;

        std::filesystem::path resolved = ResolveResourcePath(filepath);
        const std::filesystem::path pathToUse = !resolved.empty() ? resolved : std::filesystem::path(filepath);
        const std::string pathString = pathToUse.string();

        FT_Face newFace = nullptr;
        if (FT_New_Face(ftLibrary, pathString.c_str(), 0, &newFace)) {
            std::cerr << "No se pudo cargar la fuente: " << pathString << std::endl;
            return false;
        }
        
        if (FT_Set_Pixel_Sizes(newFace, 0, pixelHeight)) {
             FT_Done_Face(newFace);
             return false;
        }

        if (fontFace) FT_Done_Face(fontFace);
        fontFace = newFace;
        fontPixelHeight = pixelHeight;
        fontAscent = static_cast<float>(fontFace->size->metrics.ascender) / 64.0f;
        fontDescent = static_cast<float>(fontFace->size->metrics.descender) / 64.0f;
        fontLineHeight = static_cast<float>(fontFace->size->metrics.height) / 64.0f;
        
        fontLoaded = true;
        glyphCache.clear();

        for (std::uint32_t c = 32; c < 128; ++c) LoadGlyph(c);

        return true;
    }

    Vec2 Renderer::MeasureText(const std::string& text, float fontSize)
        {
            fontSize = std::max(fontSize, 10.0f);

            if (msdfFont && msdfFont->IsLoaded()) {
                float scale = fontSize / msdfFont->GetEmSize();
                float lineHeight = fontSize * msdfFont->GetLineHeight();
                float currentLineWidth = 0.0f;
                float maxLineWidth = 0.0f;
                float totalHeight = lineHeight;
            
                const char* ptr = text.data();
                const char* end = ptr + text.size();
            
                while (ptr < end) {
                    uint32_t codepoint = DecodeUTF8(ptr, end);
                    if (codepoint == 0) break;
                    if (codepoint == '\n') {
                        maxLineWidth = std::max(maxLineWidth, currentLineWidth);
                        currentLineWidth = 0.0f;
                        totalHeight += lineHeight;
                        continue;
                    }
                    const FontMSDF::Glyph* glyph = msdfFont->GetGlyph(codepoint);
                    if (glyph) {
                        currentLineWidth += glyph->advance * scale;
                    } else {
                        // For missing glyphs, use default advance (same as in DrawText)
                        if (codepoint == 32) { // Space character
                            currentLineWidth += fontSize * 0.3f;
                        } else {
                            currentLineWidth += fontSize * 0.2f;
                        }
                    }
                }
                maxLineWidth = std::max(maxLineWidth, currentLineWidth);
                return Vec2(maxLineWidth, totalHeight);
            }

            if (!fontLoaded || !fontFace) {
                 return Vec2(static_cast<float>(text.size()) * fontSize * 0.6f, fontSize);
            }


        float scale = fontSize / fontPixelHeight;
        float lineHeight = (fontLineHeight > 0.0f ? fontLineHeight : fontPixelHeight) * scale;
        float currentLineWidth = 0.0f;
        float maxLineWidth = 0.0f;
        float totalHeight = lineHeight;
        std::uint32_t prevCodepoint = 0;

        const char* ptr = text.data();
        const char* end = ptr + text.size();

        while (ptr < end)
        {
            std::uint32_t codepoint = DecodeUTF8(ptr, end);
            if (codepoint == 0)
            {
                break;
            }
            if (codepoint == '\r')
            {
                continue;
            }
            if (codepoint == '\n')
            {
                maxLineWidth = std::max(maxLineWidth, currentLineWidth);
                currentLineWidth = 0.0f;
                totalHeight += lineHeight;
                prevCodepoint = 0;
                continue;
            }

            const Glyph* glyph = GetGlyph(codepoint);
            if (!glyph)
            {
                glyph = GetGlyph(static_cast<std::uint32_t>('?'));
                if (!glyph)
                {
                    continue;
                }
            }

            if (prevCodepoint != 0)
            {
                currentLineWidth += GetKerning(prevCodepoint, codepoint) * scale;
            }

            currentLineWidth += glyph->advance * scale;
            prevCodepoint = codepoint;
        }

        maxLineWidth = std::max(maxLineWidth, currentLineWidth);
        return Vec2(maxLineWidth, totalHeight);
    }

    const Renderer::Glyph* Renderer::GetGlyph(std::uint32_t codepoint)
    {
        auto it = glyphCache.find(codepoint);
        if (it != glyphCache.end() && it->second.valid)
        {
            return &it->second;
        }

        if (!fontLoaded || fontFace == nullptr)
        {
            return nullptr;
        }

        if (!LoadGlyph(codepoint))
        {
            return nullptr;
        }

        auto inserted = glyphCache.find(codepoint);
        if (inserted != glyphCache.end() && inserted->second.valid)
        {
            return &inserted->second;
        }

        return nullptr;
    }

    bool Renderer::LoadGlyph(std::uint32_t codepoint)
    {
        if (!fontFace)
        {
            return false;
        }

        auto& glyph = glyphCache[codepoint];
        if (glyph.valid)
        {
            return true;
        }

        // Usar FT_LOAD_TARGET_NORMAL para mejor nitidez y contraste
        // FT_LOAD_TARGET_NORMAL proporciona mejor nitidez que LIGHT
        // FT_LOAD_FORCE_AUTOHINT puede mejorar la legibilidad en tamaños pequeños
        if (FT_Load_Char(fontFace, static_cast<FT_ULong>(codepoint), 
            FT_LOAD_RENDER | FT_LOAD_TARGET_NORMAL | FT_LOAD_FORCE_AUTOHINT))
        {
            std::cerr << "No se pudo cargar el glifo: " << codepoint << std::endl;
            return false;
        }

        const FT_GlyphSlot slot = fontFace->glyph;
        int width = static_cast<int>(slot->bitmap.width);
        int height = static_cast<int>(slot->bitmap.rows);
        const unsigned char* buffer = slot->bitmap.buffer;

        glyph.advance = static_cast<float>(slot->advance.x) / 64.0f;
        glyph.bearing = Vec2(static_cast<float>(slot->bitmap_left),
                             static_cast<float>(slot->bitmap_top));
        glyph.size = Vec2(static_cast<float>(width), static_cast<float>(height));

        if (width == 0 || height == 0 || buffer == nullptr)
        {
            glyph.uv0 = Vec2(0.0f, 0.0f);
            glyph.uv1 = Vec2(0.0f, 0.0f);
            glyph.valid = true;
            return true;
        }

        if (fontAtlasTexture == 0)
        {
            std::cerr << "Font atlas no inicializado.\n";
            return false;
        }

        int atlasX = 0;
        int atlasY = 0;
        if (!EnsureAtlasSpace(width, height, atlasX, atlasY))
        {
            return false;
        }

        glBindTexture(GL_TEXTURE_2D, fontAtlasTexture);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexSubImage2D(GL_TEXTURE_2D, 0, atlasX, atlasY, width, height, GL_RED, GL_UNSIGNED_BYTE, buffer);
        glBindTexture(GL_TEXTURE_2D, 0);

        glyph.uv0 = Vec2(static_cast<float>(atlasX) / static_cast<float>(atlasWidth),
                         static_cast<float>(atlasY) / static_cast<float>(atlasHeight));
        glyph.uv1 = Vec2(static_cast<float>(atlasX + width) / static_cast<float>(atlasWidth),
                         static_cast<float>(atlasY + height) / static_cast<float>(atlasHeight));
        glyph.valid = true;
        return true;
    }

    float Renderer::GetKerning(std::uint32_t left, std::uint32_t right) const
    {
        if (!fontFace || !FT_HAS_KERNING(fontFace))
        {
            return 0.0f;
        }

        FT_UInt leftIndex = FT_Get_Char_Index(fontFace, static_cast<FT_ULong>(left));
        FT_UInt rightIndex = FT_Get_Char_Index(fontFace, static_cast<FT_ULong>(right));
        if (leftIndex == 0 || rightIndex == 0)
        {
            return 0.0f;
        }

        FT_Vector kerning = { 0, 0 };
        if (FT_Get_Kerning(fontFace, leftIndex, rightIndex, FT_KERNING_DEFAULT, &kerning) != 0)
        {
            return 0.0f;
        }

        return static_cast<float>(kerning.x) / 64.0f;
    }

    const Renderer::Glyph* Renderer::GetOrGenerateMSDFGlyph(std::uint32_t codepoint)
    {
        // Check if already generated
        auto it = dynamicMSDFGlyphCache.find(codepoint);
        if (it != dynamicMSDFGlyphCache.end() && it->second.valid)
        {
            return &it->second;
        }

        // Try to generate
        if (GenerateMSDFGlyph(codepoint))
        {
            it = dynamicMSDFGlyphCache.find(codepoint);
            if (it != dynamicMSDFGlyphCache.end() && it->second.valid)
            {
                return &it->second;
            }
        }

        return nullptr;
    }

    bool Renderer::GenerateMSDFGlyph(std::uint32_t codepoint)
    {
        if (!msdfGenerator || !fontFace)
        {
            return false;
        }

        // Get glyph index
        FT_UInt glyphIndex = FT_Get_Char_Index(fontFace, static_cast<FT_ULong>(codepoint));
        if (glyphIndex == 0)
        {
            return false;
        }

        // Generate MSDF
        const int msdfSize = MSDF_GLYPH_SIZE;
        const float pixelRange = 4.0f;
        auto msdfData = msdfGenerator->GenerateFromGlyph(fontFace, glyphIndex, msdfSize, pixelRange, 4);
        
        if (!msdfData)
        {
            return false;
        }

        // Find space in atlas
        int atlasX, atlasY;
        if (!EnsureDynamicMSDFAtlasSpace(msdfData->width, msdfData->height, atlasX, atlasY))
        {
            std::cerr << "Dynamic MSDF atlas full!" << std::endl;
            return false;
        }

        // Upload to texture
        glBindTexture(GL_TEXTURE_2D, dynamicMSDFAtlasTexture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, atlasX, atlasY, msdfData->width, msdfData->height,
                        GL_RGB, GL_UNSIGNED_BYTE, msdfData->pixels.data());
        glBindTexture(GL_TEXTURE_2D, 0);

        // Get glyph metrics from FreeType
        if (FT_Load_Glyph(fontFace, glyphIndex, FT_LOAD_NO_BITMAP))
        {
            return false;
        }

        FT_GlyphSlot slot = fontFace->glyph;
        float scale = 1.0f / 64.0f; // FreeType uses 26.6 fixed point

        // Create glyph entry
        Glyph& glyph = dynamicMSDFGlyphCache[codepoint];
        glyph.size = Vec2(static_cast<float>(msdfData->width), static_cast<float>(msdfData->height));
        glyph.bearing = Vec2(
            static_cast<float>(slot->metrics.horiBearingX) * scale,
            static_cast<float>(slot->metrics.horiBearingY) * scale
        );
        glyph.advance = static_cast<float>(slot->metrics.horiAdvance) * scale;
        glyph.uv0 = Vec2(
            static_cast<float>(atlasX) / static_cast<float>(dynamicAtlasWidth),
            static_cast<float>(atlasY) / static_cast<float>(dynamicAtlasHeight)
        );
        glyph.uv1 = Vec2(
            static_cast<float>(atlasX + msdfData->width) / static_cast<float>(dynamicAtlasWidth),
            static_cast<float>(atlasY + msdfData->height) / static_cast<float>(dynamicAtlasHeight)
        );
        glyph.valid = true;

        return true;
    }

    bool Renderer::EnsureDynamicMSDFAtlasSpace(int glyphWidth, int glyphHeight, int& outX, int& outY)
    {
        // Simple packing algorithm - start new row if needed
        // Uses dedicated dynamic atlas packing state (not shared with FreeType atlas)
        const int padding = 2;

        if (dynamicAtlasNextX + glyphWidth + padding > dynamicAtlasWidth)
        {
            // Move to next row
            dynamicAtlasNextY += dynamicAtlasCurrentRowHeight + padding;
            dynamicAtlasNextX = padding;
            dynamicAtlasCurrentRowHeight = 0;

            if (dynamicAtlasNextY + glyphHeight + padding > dynamicAtlasHeight)
            {
                return false; // Atlas full
            }
        }

        outX = dynamicAtlasNextX;
        outY = dynamicAtlasNextY;

        dynamicAtlasNextX += glyphWidth + padding;
        dynamicAtlasCurrentRowHeight = std::max(dynamicAtlasCurrentRowHeight, glyphHeight);

        return true;
    }

    void Renderer::SetViewport(int width, int height) {
        viewportSize = Vec2(static_cast<float>(width), static_cast<float>(height));
        glViewport(0, 0, width, height);
        projectionDirty = true;
    }

    void Renderer::UpdateProjection()
    {
        if (!projectionDirty)
            return;

        if (viewportSize.x <= 0.0f || viewportSize.y <= 0.0f)
        {
            projectionDirty = false;
            return;
        }

        float left = 0.0f;
        float right = viewportSize.x;
        float top = 0.0f;
        float bottom = viewportSize.y;

        float ortho[16] = {
            2.0f / (right - left), 0.0f,                    0.0f, 0.0f,
            0.0f,                  -2.0f / (bottom - top),  0.0f, 0.0f,
            0.0f,                   0.0f,                  -1.0f, 0.0f,
           -(right + left) / (right - left),
            (bottom + top) / (bottom - top),
            0.0f, 1.0f
        };

        if (shaderProgram != 0 && projectionUniform >= 0)
        {
            glUseProgram(shaderProgram);
            glUniformMatrix4fv(projectionUniform, 1, GL_FALSE, ortho);
        }

        if (textShaderProgram != 0 && textProjectionUniform >= 0)
        {
            glUseProgram(textShaderProgram);
            glUniformMatrix4fv(textProjectionUniform, 1, GL_FALSE, ortho);
        }

        glUseProgram(0);
        projectionDirty = false;
    }

    void Renderer::EndFrame() {
        FlushBatch();
    }

    void Renderer::Shutdown() {
        ClearGlyphs();
        if (fontFace)
        {
            FT_Done_Face(fontFace);
            fontFace = nullptr;
        }
        if (ftLibrary)
        {
            FT_Done_FreeType(ftLibrary);
            ftLibrary = nullptr;
        }
        textSystemInitialized = false;
        textColorUniform = -1;
        if (shaderProgram) {
            glDeleteProgram(shaderProgram);
            shaderProgram = 0;
        }
        if (textShaderProgram)
        {
            glDeleteProgram(textShaderProgram);
            textShaderProgram = 0;
        }
        if (VAO) {
            glDeleteVertexArrays(1, &VAO);
            VAO = 0;
        }
        if (textVAO)
        {
            glDeleteVertexArrays(1, &textVAO);
            textVAO = 0;
        }
        if (VBO) {
            glDeleteBuffers(1, &VBO);
            VBO = 0;
        }
        if (textVBO)
        {
            glDeleteBuffers(1, &textVBO);
            textVBO = 0;
        }
        if (quadVAO)
        {
            glDeleteVertexArrays(1, &quadVAO);
            quadVAO = 0;
        }
        if (quadVBO)
        {
            glDeleteBuffers(1, &quadVBO);
            quadVBO = 0;
        }
        if (quadEBO)
        {
            glDeleteBuffers(1, &quadEBO);
            quadEBO = 0;
        }
        if (lineVAO)
        {
            glDeleteVertexArrays(1, &lineVAO);
            lineVAO = 0;
        }
        if (lineVBO)
        {
            glDeleteBuffers(1, &lineVBO);
            lineVBO = 0;
        }
        if (circleVAO)
        {
            glDeleteVertexArrays(1, &circleVAO);
            circleVAO = 0;
        }
        if (circleVBO)
        {
            glDeleteBuffers(1, &circleVBO);
            circleVBO = 0;
        }
        batchResourcesInitialized = false;
        if (EBO) {
            glDeleteBuffers(1, &EBO);
            EBO = 0;
        }
        // Clean up dynamic MSDF atlas texture
        if (dynamicMSDFAtlasTexture) {
            glDeleteTextures(1, &dynamicMSDFAtlasTexture);
            dynamicMSDFAtlasTexture = 0;
        }
        dynamicMSDFGlyphCache.clear();
        // Clean up font atlas texture
        if (fontAtlasTexture) {
            glDeleteTextures(1, &fontAtlasTexture);
            fontAtlasTexture = 0;
        }
        if (glContext && window) {
            SDL_GL_MakeCurrent(window, nullptr);
            SDL_GL_DestroyContext(glContext);
            glContext = nullptr;
        }
    }

    void Renderer::SetupShaders() {
        shaderProgram = CreateShaderProgram("shaders/vertex.glsl", "shaders/fragment.glsl");
        glUseProgram(shaderProgram);
        projectionUniform = glGetUniformLocation(shaderProgram, "uProjection");
        glUseProgram(0);
        projectionDirty = true;
    }

    void Renderer::EnsureBatchResources()
    {
        if (batchResourcesInitialized)
            return;
        
        // Pre-reservar capacidad para reducir reallocaciones
        quadVertices.reserve(MAX_QUAD_VERTICES);
        quadIndices.reserve(MAX_QUAD_INDICES);
        lineVertices.reserve(MAX_LINE_VERTICES);

        GL_CHECK(glGenVertexArrays(1, &quadVAO));
        GL_CHECK(glGenBuffers(1, &quadVBO));
        GL_CHECK(glGenBuffers(1, &quadEBO));

        glBindVertexArray(quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quadEBO);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(0));
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(2 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        GL_CHECK(glGenVertexArrays(1, &lineVAO));
        GL_CHECK(glGenBuffers(1, &lineVBO));

        glBindVertexArray(lineVAO);
        glBindBuffer(GL_ARRAY_BUFFER, lineVBO);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(0));
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(2 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        GL_CHECK(glGenVertexArrays(1, &circleVAO));
        GL_CHECK(glGenBuffers(1, &circleVBO));

        glBindVertexArray(circleVAO);
        glBindBuffer(GL_ARRAY_BUFFER, circleVBO);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<void*>(0));
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glBindVertexArray(0);
        batchResourcesInitialized = true;
    }

    void Renderer::FlushBatch()
    {
        if (quadVertices.empty() && lineVertices.empty())
            return;

        EnsureBatchResources();
        UpdateProjection();
        glUseProgram(shaderProgram);

        if (!quadVertices.empty())
        {
            glBindVertexArray(quadVAO);
            glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
            GL_CHECK(glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(quadVertices.size() * sizeof(Vertex)), quadVertices.data(), GL_DYNAMIC_DRAW));
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quadEBO);
            GL_CHECK(glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(quadIndices.size() * sizeof(unsigned int)), quadIndices.data(), GL_DYNAMIC_DRAW));
            GL_CHECK(glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(quadIndices.size()), GL_UNSIGNED_INT, nullptr));
        }

        if (!lineVertices.empty())
        {
            glBindVertexArray(lineVAO);
            glBindBuffer(GL_ARRAY_BUFFER, lineVBO);
            GL_CHECK(glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(lineVertices.size() * sizeof(Vertex)), lineVertices.data(), GL_DYNAMIC_DRAW));
            glLineWidth(lineBatchWidth);
            GL_CHECK(glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(lineVertices.size())));
            glLineWidth(1.0f);
        }

        glBindVertexArray(0);
        glUseProgram(0);
        quadVertices.clear();
        quadIndices.clear();
        lineVertices.clear();
        lineBatchWidth = 1.0f;
    }

    bool Renderer::EnsureAtlasSpace(int glyphWidth, int glyphHeight, int& outX, int& outY)
    {
        const int padding = 2;

        if (glyphWidth + padding * 2 > atlasWidth || glyphHeight + padding * 2 > atlasHeight)
        {
            std::cerr << "Glyph too large for font atlas\n";
            return false;
        }

        if (atlasNextX + glyphWidth + padding > atlasWidth)
        {
            atlasNextX = 0;
            atlasNextY += atlasCurrentRowHeight + padding;
            atlasCurrentRowHeight = 0;
        }

        if (atlasNextY + glyphHeight + padding > atlasHeight)
        {
            std::cerr << "Font atlas capacity exceeded\n";
            return false;
        }

        outX = atlasNextX + padding;
        outY = atlasNextY + padding;
        atlasNextX += glyphWidth + padding;
        atlasCurrentRowHeight = std::max(atlasCurrentRowHeight, glyphHeight);
        return true;
    }

    void Renderer::PushClipRect(const Vec2& pos, const Vec2& size)
    {
        FlushBatch();

        ClipRect rect;
        rect.x = static_cast<int>(std::floor(pos.x));
        rect.y = static_cast<int>(std::floor(pos.y));
        rect.width = std::max(0, static_cast<int>(std::ceil(size.x)));
        rect.height = std::max(0, static_cast<int>(std::ceil(size.y)));

        if (!clipStack.empty())
        {
            const ClipRect& top = clipStack.back();
            int left = std::max(top.x, rect.x);
            int topY = std::max(top.y, rect.y);
            int right = std::min(top.x + top.width, rect.x + rect.width);
            int bottom = std::min(top.y + top.height, rect.y + rect.height);
            rect.x = left;
            rect.y = topY;
            rect.width = std::max(0, right - left);
            rect.height = std::max(0, bottom - topY);
        }

        clipStack.push_back(rect);

        if (rect.width <= 0 || rect.height <= 0)
        {
            glEnable(GL_SCISSOR_TEST);
            glScissor(0, 0, 0, 0);
            return;
        }

        int scissorX = rect.x;
        int scissorY = static_cast<int>(viewportSize.y) - (rect.y + rect.height);
        scissorX = std::max(scissorX, 0);
        scissorY = std::max(scissorY, 0);
        int scissorWidth = std::min(rect.width, static_cast<int>(viewportSize.x) - scissorX);
        int scissorHeight = std::min(rect.height, static_cast<int>(viewportSize.y) - scissorY);
        scissorWidth = std::max(scissorWidth, 0);
        scissorHeight = std::max(scissorHeight, 0);

        glEnable(GL_SCISSOR_TEST);
        glScissor(scissorX, scissorY, scissorWidth, scissorHeight);
    }

    void Renderer::PopClipRect()
    {
        if (clipStack.empty())
            return;

        FlushBatch();
        clipStack.pop_back();

        if (clipStack.empty())
        {
            glDisable(GL_SCISSOR_TEST);
            return;
        }

        const ClipRect& rect = clipStack.back();
        if (rect.width <= 0 || rect.height <= 0)
        {
            glEnable(GL_SCISSOR_TEST);
            glScissor(0, 0, 0, 0);
            return;
        }

        int scissorX = rect.x;
        int scissorY = static_cast<int>(viewportSize.y) - (rect.y + rect.height);
        scissorX = std::max(scissorX, 0);
        scissorY = std::max(scissorY, 0);
        int scissorWidth = std::min(rect.width, static_cast<int>(viewportSize.x) - scissorX);
        int scissorHeight = std::min(rect.height, static_cast<int>(viewportSize.y) - scissorY);
        scissorWidth = std::max(scissorWidth, 0);
        scissorHeight = std::max(scissorHeight, 0);

        glEnable(GL_SCISSOR_TEST);
        glScissor(scissorX, scissorY, scissorWidth, scissorHeight);
    }

    std::string Renderer::LoadShaderSource(const std::string& filepath)
    {
        std::filesystem::path resolved = ResolveResourcePath(filepath);
        if (resolved.empty())
        {
            std::cerr << "No se pudo resolver la ruta del shader: " << filepath << std::endl;
            return "";
        }

        std::ifstream file(resolved, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "No se pudo abrir el shader: " << resolved.string() << std::endl;
            return "";
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    GLuint Renderer::CompileShader(GLenum type, const std::string& source)
    {
        GLuint shader = glCreateShader(type);
        const char* src = source.c_str();
        glShaderSource(shader, 1, &src, nullptr);
        glCompileShader(shader);

        GLint success = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            char infoLog[512];
            glGetShaderInfoLog(shader, 512, nullptr, infoLog);
            std::cerr << "Error compilando shader ("
                << (type == GL_VERTEX_SHADER ? "VERTEX" : "FRAGMENT")
                << "):\n" << infoLog << std::endl;
            glDeleteShader(shader);
            return 0;
        }
        return shader;
    }

    GLuint Renderer::CreateShaderProgram(const std::string& vertexSource, const std::string& fragmentSource)
    {
        std::string vertSrc = LoadShaderSource(vertexSource);
        std::string fragSrc = LoadShaderSource(fragmentSource);

        if (vertSrc.empty() || fragSrc.empty()) {
            std::cerr << "No se pudieron cargar los shaders" << std::endl;
            return 0;
        }

        GLuint vertexShader = CompileShader(GL_VERTEX_SHADER, vertSrc);
        GLuint fragmentShader = CompileShader(GL_FRAGMENT_SHADER, fragSrc);

        if (vertexShader == 0 || fragmentShader == 0) {
            if (vertexShader != 0) glDeleteShader(vertexShader);
            if (fragmentShader != 0) glDeleteShader(fragmentShader);
            return 0;
        }

        GLuint program = glCreateProgram();
        glAttachShader(program, vertexShader);
        glAttachShader(program, fragmentShader);
        glLinkProgram(program);

        GLint success = 0;
        glGetProgramiv(program, GL_LINK_STATUS, &success);
        if (!success)
        {
            char infoLog[512];
            glGetProgramInfoLog(program, 512, nullptr, infoLog);
            std::cerr << "Error linkeando el programa de shaders:\n" << infoLog << std::endl;
            glDeleteProgram(program);
            program = 0;
        }

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        return program;
    }

} // namespace FluentUI
