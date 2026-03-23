#include "core/FontMSDF.h"
#include "core/Context.h"
#include "core/RenderBackend.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <vector>

#include "../external/stb_image.h" 

namespace FluentUI {

    FontMSDF::FontMSDF(RenderBackend* backend) : backend(backend) {}

    FontMSDF::~FontMSDF() {
        if (textureHandle && backend) {
            backend->DeleteTexture(textureHandle);
            textureHandle = nullptr;
        }
    }

    bool FontMSDF::Load(const std::string& atlasImagePath, const std::string& atlasJsonPath) {
        if (!LoadTexture(atlasImagePath)) {
            return false;
        }
        
        if (!ParseJson(atlasJsonPath)) {
            return false;
        }

        loaded = true;
        return true;
    }

    const FontMSDF::Glyph* FontMSDF::GetGlyph(uint32_t codepoint) const {
        auto it = glyphs.find(codepoint);
        if (it != glyphs.end() && it->second.valid) {
            return &it->second;
        }
        return nullptr;
    }

    bool FontMSDF::LoadTexture(const std::string& imagePath) {
        if (!backend) return false;

        Log(LogLevel::Debug, "Loading MSDF texture: %s", imagePath.c_str());
        
        stbi_set_flip_vertically_on_load(false);
        
        int width, height, channels;
        // Forzar 4 canales (RGBA) para que coincida con lo que el backend espera
        unsigned char* imageData = stbi_load(imagePath.c_str(), &width, &height, &channels, 4);
        
        if (!imageData) {
            Log(LogLevel::Error, "Failed to load MSDF atlas image: %s", imagePath.c_str());
            Log(LogLevel::Error, "Reason: %s", stbi_failure_reason());
            return false;
        }

        Log(LogLevel::Debug, "Texture loaded: %dx%d (original channels: %d)", width, height, channels);

        // Delete old texture if reloading
        if (textureHandle != nullptr) {
            backend->DeleteTexture(textureHandle);
            textureHandle = nullptr;
        }

        textureHandle = backend->CreateTexture(width, height, imageData, false);
        stbi_image_free(imageData);
        
        this->atlasWidth = width;
        this->atlasHeight = height;
        
        return textureHandle != nullptr;
    }

    bool FontMSDF::ParseJson(const std::string& jsonPath) {
        std::ifstream file(jsonPath);
        if (!file.is_open()) {
            Log(LogLevel::Error, "Failed to open MSDF JSON: %s", jsonPath.c_str());
            return false;
        }

        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();

        // Metrics parsing
        size_t metricsPos = content.find("\"metrics\"");
        if (metricsPos != std::string::npos) {
            auto findValue = [&](const std::string& key, float& outVal) {
                size_t keyPos = content.find("\"" + key + "\"", metricsPos);
                if (keyPos != std::string::npos) {
                    size_t colonPos = content.find(":", keyPos);
                    size_t commaPos = content.find_first_of(",}", colonPos);
                    std::string valStr = content.substr(colonPos + 1, commaPos - colonPos - 1);
                    try { outVal = std::stof(valStr); } catch(...) {}
                }
            };
            findValue("emSize", emSize);
            findValue("lineHeight", lineHeight);
            findValue("ascender", ascender);
        }

        // Default values if missing or invalid
        if (emSize <= 0.0f) emSize = 32.0f; // Typical MSDF emSize
        if (lineHeight <= 0.0f) lineHeight = 1.2f;
        if (ascender <= 0.0f) ascender = 0.8f;

        // Atlas Info parsing
        size_t atlasPos = content.find("\"atlas\"");
        if (atlasPos != std::string::npos) {
            auto findValue = [&](const std::string& key, float& outVal) {
                size_t keyPos = content.find("\"" + key + "\"", atlasPos);
                if (keyPos != std::string::npos) {
                    size_t colonPos = content.find(":", keyPos);
                    size_t commaPos = content.find_first_of(",}", colonPos);
                    std::string valStr = content.substr(colonPos + 1, commaPos - colonPos - 1);
                    try { outVal = std::stof(valStr); } catch(...) {}
                }
            };
            findValue("size", fontSize);
            findValue("distanceRange", pixelRange);
        }

        if (pixelRange <= 0.0f) pixelRange = 4.0f;

        // Glyphs parsing
        size_t glyphsPos = content.find("\"glyphs\"");
        if (glyphsPos == std::string::npos) return false;

        size_t arrayStart = content.find("[", glyphsPos);
        size_t arrayEnd = content.find("]", arrayStart);
        if (arrayStart == std::string::npos || arrayEnd == std::string::npos) return false;

        size_t pos = arrayStart + 1;
        while (pos < arrayEnd) {
            size_t objStart = content.find("{", pos);
            if (objStart >= arrayEnd) break;
            
            int braceCount = 1;
            size_t objEnd = objStart + 1;
            while (objEnd < arrayEnd && braceCount > 0) {
                if (content[objEnd] == '{') braceCount++;
                else if (content[objEnd] == '}') braceCount--;
                objEnd++;
            }
            objEnd--;

            std::string glyphObj = content.substr(objStart, objEnd - objStart + 1);

            size_t unicodePos = glyphObj.find("\"unicode\"");
            if (unicodePos == std::string::npos) {
                pos = objEnd + 1;
                continue;
            }

            size_t colonPos = glyphObj.find(":", unicodePos);
            size_t commaPos = glyphObj.find_first_of(",}", colonPos);
            uint32_t unicode = 0;
            try { unicode = std::stoul(glyphObj.substr(colonPos + 1, commaPos - colonPos - 1)); } catch(...) {}

            Glyph glyph;
            glyph.valid = true;

            size_t advancePos = glyphObj.find("\"advance\"");
            if (advancePos != std::string::npos) {
                colonPos = glyphObj.find(":", advancePos);
                commaPos = glyphObj.find_first_of(",}", colonPos);
                try { glyph.advance = std::stof(glyphObj.substr(colonPos + 1, commaPos - colonPos - 1)); } catch(...) {}
            }

            auto parseRect = [&](const std::string& key, Vec2& outSize, Vec2& outOrigin, bool isAtlas) {
                size_t keyPos = glyphObj.find("\"" + key + "\"");
                if (keyPos != std::string::npos) {
                    size_t leftPos = glyphObj.find("\"left\"", keyPos);
                    size_t bottomPos = glyphObj.find("\"bottom\"", keyPos);
                    size_t rightPos = glyphObj.find("\"right\"", keyPos);
                    size_t topPos = glyphObj.find("\"top\"", keyPos);
                    
                    if (leftPos != std::string::npos && rightPos != std::string::npos &&
                        bottomPos != std::string::npos && topPos != std::string::npos) {
                        
                        float l=0, r=0, b=0, t=0;
                        auto extract = [&](size_t p) {
                            size_t c = glyphObj.find(":", p);
                            size_t cm = glyphObj.find_first_of(",}", c);
                            return std::stof(glyphObj.substr(c + 1, cm - c - 1));
                        };
                        try {
                            l = extract(leftPos); r = extract(rightPos);
                            b = extract(bottomPos); t = extract(topPos);
                        } catch(...) {}

                        outSize = Vec2(r - l, t - b);
                        if (!isAtlas) {
                            outOrigin = Vec2(l, t);
                        } else {
                            glyph.uv0 = Vec2(l / static_cast<float>(atlasWidth), 1.0f - (t / static_cast<float>(atlasHeight)));
                            glyph.uv1 = Vec2(r / static_cast<float>(atlasWidth), 1.0f - (b / static_cast<float>(atlasHeight)));
                        }
                    }
                }
            };

            parseRect("planeBounds", glyph.planeBounds, glyph.bearing, false);
            parseRect("atlasBounds", glyph.atlasBounds, glyph.atlasBounds, true);

            glyphs[unicode] = glyph;
            pos = objEnd + 1;
        }

        return !glyphs.empty();
    }

} // namespace FluentUI
