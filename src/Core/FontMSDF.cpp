#include "core/FontMSDF.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <vector>
#include <SDL3/SDL.h>

// stb_image is included in Renderer.cpp with implementation. 
// We should ideally move implementation or include header here. 
// For now, assuming we link against Renderer, but we need the header declarations.
// Actually, STB_IMAGE_IMPLEMENTATION should be in ONE cpp file. 
// It is in Renderer.cpp. So here we just include header.
#include "../../external/stb_image.h" 

namespace FluentUI {

    FontMSDF::~FontMSDF() {
        if (textureID != 0) {
            glDeleteTextures(1, &textureID);
            textureID = 0;
        }
        if (shaderProgram != 0) {
            glDeleteProgram(shaderProgram);
            shaderProgram = 0;
        }
    }

    bool FontMSDF::Load(const std::string& atlasImagePath, const std::string& atlasJsonPath) {
        if (!LoadTexture(atlasImagePath)) {
            return false;
        }
        
        if (!ParseJson(atlasJsonPath)) {
            return false;
        }

        CreateShader();
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

        bool FontMSDF::Bind(const float* projectionMatrix) {
        if (!loaded || shaderProgram == 0) return false;

        glUseProgram(shaderProgram);
        
        if (projectionUniform >= 0) {
            glUniformMatrix4fv(projectionUniform, 1, GL_FALSE, projectionMatrix);
        }
        
        if (pxRangeUniform >= 0) {
            glUniform1f(pxRangeUniform, pixelRange);
        }

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureID);
        
        if (textureUniform >= 0) {
            glUniform1i(textureUniform, 0);
        }
        
        // Note: color uniform is set by Renderer after Bind() is called

        return true;
    }
    
    void FontMSDF::SetColor(const Color& color) {
        if (colorUniform >= 0) {
            glUniform4f(colorUniform, color.r, color.g, color.b, color.a);
        }
    }

    void FontMSDF::Unbind() {
        glBindTexture(GL_TEXTURE_2D, 0);
        glUseProgram(0);
    }

    void FontMSDF::BindShaderOnly(const float* projectionMatrix) {
        if (shaderProgram == 0) {
            CreateShader(); // Create shader if not already created
        }
        if (shaderProgram == 0) return;

        glUseProgram(shaderProgram);
        
        if (projectionUniform >= 0) {
            glUniformMatrix4fv(projectionUniform, 1, GL_FALSE, projectionMatrix);
        }
        
        // Set pixel range - use stored value or default to 4.0 for dynamic MSDF
        // Dynamic MSDF uses 4.0 pixel range by default
        float rangeToUse = pixelRange > 0 ? pixelRange : 4.0f;
        if (pxRangeUniform >= 0) {
            glUniform1f(pxRangeUniform, rangeToUse);
        }
    }

    void FontMSDF::UnbindShader() {
        glUseProgram(0);
    }

    bool FontMSDF::LoadTexture(const std::string& imagePath) {
        std::cout << "Loading MSDF texture: " << imagePath << std::endl;
        
        // We use stbi_load. IMPORTANT: maintain flip settings consistent with JSON parsing.
        stbi_set_flip_vertically_on_load(false);
        
        int width, height, channels;
        unsigned char* imageData = stbi_load(imagePath.c_str(), &width, &height, &channels, 0);
        
        if (!imageData) {
            std::cerr << "Failed to load MSDF atlas image: " << imagePath << std::endl;
            return false;
        }

        GLenum format = (channels == 4) ? GL_RGBA : GL_RGB;

        // Delete old texture if reloading
        if (textureID != 0) {
            glDeleteTextures(1, &textureID);
            textureID = 0;
        }

        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);
        
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, imageData);
        
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        // Use linear filtering for MSDF - it works well with distance fields
        // The shader will handle the sharpness through proper screen-space calculations
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        
        glBindTexture(GL_TEXTURE_2D, 0);
        stbi_image_free(imageData);
        
        this->atlasWidth = width;
        this->atlasHeight = height;
        
        return true;
    }

    bool FontMSDF::ParseJson(const std::string& jsonPath) {
        std::ifstream file(jsonPath);
        if (!file.is_open()) {
            std::cerr << "Failed to open MSDF JSON: " << jsonPath << std::endl;
            return false;
        }

        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();

        // Basic manual JSON parsing to avoid pulling in a heavy JSON library dependency
        // Similar logic to what was in Renderer.cpp

        // Parse Metrics
        size_t metricsPos = content.find("\"metrics\"");
        if (metricsPos != std::string::npos) {
            auto findValue = [&](const std::string& key, float& outVal) {
                size_t keyPos = content.find("\"" + key + "\"", metricsPos);
                if (keyPos != std::string::npos) {
                    size_t colonPos = content.find(":", keyPos);
                    size_t commaPos = content.find_first_of(",}", colonPos);
                    std::string valStr = content.substr(colonPos + 1, commaPos - colonPos - 1);
                    try { outVal = std::stof(valStr); } catch(const std::exception& e) {
                        std::cerr << "Warning: failed to parse " << key << ": " << e.what() << std::endl;
                    }
                }
            };
            findValue("emSize", emSize);
            findValue("lineHeight", lineHeight);
            findValue("ascender", ascender);
        }

        // Validate parsed metrics
        if (emSize <= 0.0f) emSize = 1.0f;

        // Parse Atlas Info
        size_t atlasPos = content.find("\"atlas\"");
        if (atlasPos != std::string::npos) {
            auto findValue = [&](const std::string& key, float& outVal) {
                size_t keyPos = content.find("\"" + key + "\"", atlasPos);
                if (keyPos != std::string::npos) {
                    size_t colonPos = content.find(":", keyPos);
                    size_t commaPos = content.find_first_of(",}", colonPos);
                    std::string valStr = content.substr(colonPos + 1, commaPos - colonPos - 1);
                    try { outVal = std::stof(valStr); } catch(const std::exception& e) {
                        std::cerr << "Warning: failed to parse " << key << ": " << e.what() << std::endl;
                    }
                }
            };
            findValue("size", fontSize);
            findValue("distanceRange", pixelRange);
        }

        // Validate atlas metrics
        if (pixelRange <= 0.0f) pixelRange = 4.0f;

        // Parse Glyphs
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

            // Extract unicode
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

            // Extract advance
            size_t advancePos = glyphObj.find("\"advance\"");
            if (advancePos != std::string::npos) {
                colonPos = glyphObj.find(":", advancePos);
                commaPos = glyphObj.find_first_of(",}", colonPos);
                try { glyph.advance = std::stof(glyphObj.substr(colonPos + 1, commaPos - colonPos - 1)); } catch(...) {}
            }

            // Helper to parse rects
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
                            l = extract(leftPos);
                            r = extract(rightPos);
                            b = extract(bottomPos);
                            t = extract(topPos);
                        } catch(...) {}

                        outSize = Vec2(r - l, t - b);
                        if (!isAtlas) {
                            outOrigin = Vec2(l, t); // Bearing: Left, Top
                        } else {
                            // Atlas UV calculation
                            // Invert Y for OpenGL (Bottom-Origin) vs Image Load (Top-Down)
                            // JSON Origin: Bottom
                            // Image Data: Inverted relative to JSON if loaded without flip? 
                            // Verified Fix: V = 1.0 - (Y / H)
                            
                            glyph.uv0 = Vec2(
                                l / static_cast<float>(atlasWidth),
                                1.0f - (t / static_cast<float>(atlasHeight))
                            );
                            glyph.uv1 = Vec2(
                                r / static_cast<float>(atlasWidth),
                                1.0f - (b / static_cast<float>(atlasHeight))
                            );
                        }
                    }
                }
            };

            parseRect("planeBounds", glyph.planeBounds, glyph.bearing, false);
            parseRect("atlasBounds", glyph.atlasBounds, glyph.atlasBounds, true); // Dummy for origin

            glyphs[unicode] = glyph;
            pos = objEnd + 1;
        }

        std::cout << "Loaded " << glyphs.size() << " glyphs from " << jsonPath << std::endl;
        return !glyphs.empty();
    }

    void FontMSDF::CreateShader() {
         try {
             std::string vertexSrc = LoadShaderSource("shaders/text_vertex_msdf.glsl");
             std::string fragmentSrc = LoadShaderSource("shaders/text_fragment_msdf.glsl");
             
             if (vertexSrc.empty() || fragmentSrc.empty()) {
                 std::cerr << "Failed to load MSDF shader sources" << std::endl;
                 return;
             }
             
             shaderProgram = CreateShaderProgram(vertexSrc, fragmentSrc);
             
             if (shaderProgram == 0) {
                 std::cerr << "Failed to create MSDF shader program" << std::endl;
                 return;
             }
             
             glUseProgram(shaderProgram);
             projectionUniform = glGetUniformLocation(shaderProgram, "uProjection");
             colorUniform = glGetUniformLocation(shaderProgram, "uTextColor1");
             pxRangeUniform = glGetUniformLocation(shaderProgram, "pxRange");
             textureUniform = glGetUniformLocation(shaderProgram, "msdfTexture");
             
             if (textureUniform >= 0) glUniform1i(textureUniform, 0);
             glUseProgram(0);
             
             std::cout << "MSDF shader created successfully" << std::endl;

         } catch (const std::exception& e) {
             std::cerr << "Error creating MSDF shader: " << e.what() << std::endl;
         }
    }

    std::string FontMSDF::LoadShaderSource(const std::string& filepath) {
        // Try to resolve path using the same logic as Renderer
        std::filesystem::path candidate(filepath);
        
        // Try absolute path first
        if (candidate.is_absolute() && std::filesystem::exists(candidate)) {
            std::ifstream file(candidate, std::ios::binary);
            if (file.is_open()) {
                std::stringstream buffer;
                buffer << file.rdbuf();
                return buffer.str();
            }
        }
        
        // Try relative paths
        std::vector<std::filesystem::path> searchPaths = {
            std::filesystem::current_path(),
            std::filesystem::current_path() / "..",
            std::filesystem::current_path() / "../.."
        };
        
        if (const char* base = SDL_GetBasePath()) {
            searchPaths.push_back(std::filesystem::path(base));
            searchPaths.push_back(std::filesystem::path(base) / "..");
        }
        
        for (const auto& root : searchPaths) {
            auto full = (root / candidate).lexically_normal();
            if (std::filesystem::exists(full)) {
                std::ifstream file(full, std::ios::binary);
                if (file.is_open()) {
                    std::stringstream buffer;
                    buffer << file.rdbuf();
                    return buffer.str();
                }
            }
        }
        
        std::cerr << "Failed to load shader: " << filepath << std::endl;
        return "";
    }
    
    GLuint FontMSDF::CompileShader(GLenum type, const std::string& source) {
        if (source.empty()) {
            std::cerr << "Empty shader source" << std::endl;
            return 0;
        }
        
        GLuint shader = glCreateShader(type);
        const char* src = source.c_str();
        glShaderSource(shader, 1, &src, nullptr);
        glCompileShader(shader);
        
        GLint success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetShaderInfoLog(shader, 512, nullptr, infoLog);
            std::cerr << "Shader compile error (" << (type == GL_VERTEX_SHADER ? "vertex" : "fragment") << "): " << infoLog << std::endl;
            glDeleteShader(shader);
            return 0;
        }
        return shader;
    }

    GLuint FontMSDF::CreateShaderProgram(const std::string& vertexSource, const std::string& fragmentSource) {
        GLuint vertexShader = CompileShader(GL_VERTEX_SHADER, vertexSource);
        GLuint fragmentShader = CompileShader(GL_FRAGMENT_SHADER, fragmentSource);
        
        if (vertexShader == 0 || fragmentShader == 0) {
            if (vertexShader != 0) glDeleteShader(vertexShader);
            if (fragmentShader != 0) glDeleteShader(fragmentShader);
            return 0;
        }
        
        GLuint program = glCreateProgram();
        glAttachShader(program, vertexShader);
        glAttachShader(program, fragmentShader);
        glLinkProgram(program);
        
        GLint success;
        glGetProgramiv(program, GL_LINK_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetProgramInfoLog(program, 512, nullptr, infoLog);
            std::cerr << "Shader program link error: " << infoLog << std::endl;
            glDeleteProgram(program);
            program = 0;
        }
        
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        
        return program;
    }

} // namespace FluentUI
