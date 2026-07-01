#include <SDL3/SDL.h>
#include "core/OpenGLBackend.h"
#include "core/EmbeddedShaders.h"
#include "core/Context.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace FluentUI {

OpenGLBackend::~OpenGLBackend() {
    Shutdown();
}

void OpenGLBackend::QueryUniforms(GLuint program, ShaderUniforms& uniforms) {
    uniforms.projection = glGetUniformLocation(program, "uProjection");
    uniforms.textColor = glGetUniformLocation(program, "uTextColor");
    uniforms.pxRange = glGetUniformLocation(program, "pxRange");
    uniforms.texture = glGetUniformLocation(program, "uTexture");
    uniforms.reveal = glGetUniformLocation(program, "uReveal");
}

bool OpenGLBackend::Init(void* windowHandle, void* existingGLContext) {
    window = static_cast<SDL_Window*>(windowHandle);

    if (existingGLContext) {
        // Reuse the caller's GL context — do not create a new one
        glContext = static_cast<SDL_GLContext>(existingGLContext);
        ownsGLContext = false;
        SDL_GL_MakeCurrent(static_cast<SDL_Window*>(window), static_cast<SDL_GLContext>(glContext));
    } else {
        // Create our own GL context
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

        glContext = SDL_GL_CreateContext(static_cast<SDL_Window*>(window));
        if (!glContext) {
            Log(LogLevel::Error, "OpenGL Error: Failed to create GL context: %s", SDL_GetError());
            return false;
        }
        ownsGLContext = true;
        SDL_GL_MakeCurrent(static_cast<SDL_Window*>(window), static_cast<SDL_GLContext>(glContext));
    }

    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
        Log(LogLevel::Error, "OpenGL Error: Failed to initialize GLAD");
        return false;
    }

    Log(LogLevel::Info, "OpenGL Backend Initialized: %s", reinterpret_cast<const char*>(glGetString(GL_VERSION)));
    Log(LogLevel::Info, "Vendor: %s", reinterpret_cast<const char*>(glGetString(GL_VENDOR)));
    Log(LogLevel::Info, "Renderer: %s", reinterpret_cast<const char*>(glGetString(GL_RENDERER)));

    SDL_GL_SetSwapInterval(1);

    shaderProgram = CreateShaderProgram(Shaders::VertexShader, Shaders::FragmentShader);
    textShaderProgram = CreateShaderProgram(Shaders::TextVertexShader, Shaders::TextFragmentShader);
    msdfShaderProgram = CreateShaderProgram(Shaders::MSDFVertexShader, Shaders::MSDFFragmentShader);
    imageShaderProgram = CreateShaderProgram(Shaders::ImageVertexShader, Shaders::ImageFragmentShader);
    sdfRectProgram = CreateShaderProgram(Shaders::SDFRectVertexShader, Shaders::SDFRectFragmentShader);

    // Issue 3: Cache uniform locations once after shader creation
    QueryUniforms(shaderProgram, basicUniforms);
    QueryUniforms(textShaderProgram, textUniforms);
    QueryUniforms(msdfShaderProgram, msdfUniforms);
    QueryUniforms(imageShaderProgram, imageUniforms);
    QueryUniforms(sdfRectProgram, sdfRectUniforms);

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);

    // Attribute 0: Position (x, y)
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(RenderVertex), (void*)0);
    glEnableVertexAttribArray(0);
    // Attribute 1: Color (r, g, b, a)
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(RenderVertex), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    // Attribute 2: UV (u, v)
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(RenderVertex), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    // Issue 2: Pre-allocate VBO/EBO with reasonable sizes (~320KB VBO, ~60KB EBO)
    vboCapacity = 10000 * sizeof(RenderVertex);  // ~320KB
    eboCapacity = 15000 * sizeof(unsigned int);    // ~60KB
    glBufferData(GL_ARRAY_BUFFER, vboCapacity, nullptr, GL_DYNAMIC_DRAW);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, eboCapacity, nullptr, GL_DYNAMIC_DRAW);

    glBindVertexArray(0);
    vaoIsBound = false;

    // --- SDF instanced pipeline (brief 01) ---
    // Dedicated VAO: attribute 0 is the static unit quad (divisor 0); attributes
    // 1..6 are per-instance SDFInstance fields (divisor 1).
    {
        static const float quadVerts[8] = {
            -1.0f, -1.0f,   1.0f, -1.0f,   1.0f, 1.0f,   -1.0f, 1.0f
        };
        static const unsigned int quadIdx[6] = { 0, 1, 2, 0, 2, 3 };

        glGenVertexArrays(1, &sdfVao);
        glGenBuffers(1, &sdfQuadVBO);
        glGenBuffers(1, &sdfQuadEBO);
        glGenBuffers(1, &sdfInstanceVBO);

        glBindVertexArray(sdfVao);

        glBindBuffer(GL_ARRAY_BUFFER, sdfQuadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribDivisor(0, 0);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sdfQuadEBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(quadIdx), quadIdx, GL_STATIC_DRAW);

        glBindBuffer(GL_ARRAY_BUFFER, sdfInstanceVBO);
        sdfInstanceCapacity = 256 * sizeof(SDFInstance);
        glBufferData(GL_ARRAY_BUFFER, sdfInstanceCapacity, nullptr, GL_DYNAMIC_DRAW);
        const GLsizei stride = sizeof(SDFInstance);
        // loc 1: iCenter (cx,cy)
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(SDFInstance, cx));
        glEnableVertexAttribArray(1); glVertexAttribDivisor(1, 1);
        // loc 2: iHalf (hx,hy)
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(SDFInstance, hx));
        glEnableVertexAttribArray(2); glVertexAttribDivisor(2, 1);
        // loc 3: iParams (radius,borderWidth,softness,mode)
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(SDFInstance, radius));
        glEnableVertexAttribArray(3); glVertexAttribDivisor(3, 1);
        // loc 4: iFill (rgba)
        glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(SDFInstance, fillR));
        glEnableVertexAttribArray(4); glVertexAttribDivisor(4, 1);
        // loc 5: iBorder (rgba)
        glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(SDFInstance, borderR));
        glEnableVertexAttribArray(5); glVertexAttribDivisor(5, 1);
        // loc 6: iReveal (revealIntensity) — brief 04
        glVertexAttribPointer(6, 1, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(SDFInstance, revealIntensity));
        glEnableVertexAttribArray(6); glVertexAttribDivisor(6, 1);

        glBindVertexArray(0);
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Perf R2: Persistent mapped buffers — disabled pending investigation of text artifacts
    // InitPersistentBuffers();

    return true;
}

void OpenGLBackend::InitPersistentBuffers() {
    // Check if GL_ARB_buffer_storage is available (requires GL 4.4+)
    // We already require GL 4.5, so this should always work
    GLint majorVer = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &majorVer);
    if (majorVer < 4) return;

    size_t totalVBOSize = PERSISTENT_VBO_SIZE * BUFFER_FRAMES;
    size_t totalEBOSize = PERSISTENT_EBO_SIZE * BUFFER_FRAMES;

    GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;

    glGenBuffers(1, &persistentVBO);
    glBindBuffer(GL_ARRAY_BUFFER, persistentVBO);
    glBufferStorage(GL_ARRAY_BUFFER, totalVBOSize, nullptr, flags);
    mappedVBO = glMapBufferRange(GL_ARRAY_BUFFER, 0, totalVBOSize, flags);

    glGenBuffers(1, &persistentEBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, persistentEBO);
    glBufferStorage(GL_ELEMENT_ARRAY_BUFFER, totalEBOSize, nullptr, flags);
    mappedEBO = glMapBufferRange(GL_ELEMENT_ARRAY_BUFFER, 0, totalEBOSize, flags);

    if (mappedVBO && mappedEBO) {
        usePersistentMapping = true;
        Log(LogLevel::Info, "Persistent mapped buffers enabled (triple-buffered)");
    } else {
        // Fallback: clean up and use traditional buffers
        if (mappedVBO) { glUnmapBuffer(GL_ARRAY_BUFFER); mappedVBO = nullptr; }
        if (mappedEBO) { glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER); mappedEBO = nullptr; }
        glDeleteBuffers(1, &persistentVBO); persistentVBO = 0;
        glDeleteBuffers(1, &persistentEBO); persistentEBO = 0;
    }

    currentBufferRegion = 0;
    for (int i = 0; i < BUFFER_FRAMES; ++i) bufferFences[i] = nullptr;
}

void OpenGLBackend::WaitForBufferRegion(int region) {
    if (bufferFences[region]) {
        GLenum result = glClientWaitSync(bufferFences[region], GL_SYNC_FLUSH_COMMANDS_BIT, 1000000000);
        if (result == GL_TIMEOUT_EXPIRED) {
            Log(LogLevel::Warning, "GPU fence wait timed out for buffer region %d", region);
        } else if (result == GL_WAIT_FAILED) {
            Log(LogLevel::Error, "GPU fence wait failed for buffer region %d", region);
        }
        glDeleteSync(bufferFences[region]);
        bufferFences[region] = nullptr;
    }
}

void OpenGLBackend::Shutdown() {
    // Clean up render targets
    for (auto* rt : renderTargets) {
        if (rt->fbo) glDeleteFramebuffers(1, &rt->fbo);
        if (rt->colorTexture) glDeleteTextures(1, &rt->colorTexture);
        if (rt->depthRenderbuffer) glDeleteRenderbuffers(1, &rt->depthRenderbuffer);
        delete rt;
    }
    renderTargets.clear();

    // Perf 1.5: Cleanup persistent buffers
    if (usePersistentMapping) {
        for (int i = 0; i < BUFFER_FRAMES; ++i) {
            if (bufferFences[i]) { glDeleteSync(bufferFences[i]); bufferFences[i] = nullptr; }
        }
        if (persistentVBO) {
            glBindBuffer(GL_ARRAY_BUFFER, persistentVBO);
            glUnmapBuffer(GL_ARRAY_BUFFER);
            glDeleteBuffers(1, &persistentVBO);
            persistentVBO = 0; mappedVBO = nullptr;
        }
        if (persistentEBO) {
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, persistentEBO);
            glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
            glDeleteBuffers(1, &persistentEBO);
            persistentEBO = 0; mappedEBO = nullptr;
        }
        usePersistentMapping = false;
    }

    if (shaderProgram) { glDeleteProgram(shaderProgram); shaderProgram = 0; }
    if (textShaderProgram) { glDeleteProgram(textShaderProgram); textShaderProgram = 0; }
    if (msdfShaderProgram) { glDeleteProgram(msdfShaderProgram); msdfShaderProgram = 0; }
    if (imageShaderProgram) { glDeleteProgram(imageShaderProgram); imageShaderProgram = 0; }
    if (sdfRectProgram) { glDeleteProgram(sdfRectProgram); sdfRectProgram = 0; }
    if (vao) { glDeleteVertexArrays(1, &vao); vao = 0; }
    if (vbo) { glDeleteBuffers(1, &vbo); vbo = 0; }
    if (ebo) { glDeleteBuffers(1, &ebo); ebo = 0; }
    if (sdfVao) { glDeleteVertexArrays(1, &sdfVao); sdfVao = 0; }
    if (sdfQuadVBO) { glDeleteBuffers(1, &sdfQuadVBO); sdfQuadVBO = 0; }
    if (sdfQuadEBO) { glDeleteBuffers(1, &sdfQuadEBO); sdfQuadEBO = 0; }
    if (sdfInstanceVBO) { glDeleteBuffers(1, &sdfInstanceVBO); sdfInstanceVBO = 0; }

    // Acrylic / Mica resources (brief 06)
    DestroyBlurChain();
    if (kawaseDownProgram) { glDeleteProgram(kawaseDownProgram); kawaseDownProgram = 0; }
    if (kawaseUpProgram) { glDeleteProgram(kawaseUpProgram); kawaseUpProgram = 0; }
    if (acrylicCompositeProgram) { glDeleteProgram(acrylicCompositeProgram); acrylicCompositeProgram = 0; }
    if (blurVao) { glDeleteVertexArrays(1, &blurVao); blurVao = 0; }
    if (blurVbo) { glDeleteBuffers(1, &blurVbo); blurVbo = 0; }
    acrylicResourcesReady = false;

    if (glContext && ownsGLContext) {
        SDL_GL_DestroyContext(static_cast<SDL_GLContext>(glContext));
    }
    glContext = nullptr;
    textureIsAlphaOnly.clear();
}

void OpenGLBackend::BeginFrame(const Color& clearColor) {
    // brief 08 Part A: a secondary window shares the main GL context but is a real
    // OS-window we drive. Route GL to it before recording this frame. (Single
    // window / engine-embedded cases keep the context the caller already made
    // current.)
    if (secondaryWindow && window && glContext) {
        SDL_GL_MakeCurrent(static_cast<SDL_Window*>(window), static_cast<SDL_GLContext>(glContext));
    }

    // "present" = we own this window's default framebuffer (own context, or our
    // own secondary window on a shared context). Only the engine-embedded case
    // (external context, NOT a secondary window) must preserve engine GL state.
    const bool present = ownsGLContext || secondaryWindow;

    // When embedded in an external engine context, save its state so we can
    // restore it after UI rendering.
    if (!present) {
        SaveState();
    }

    // Set GL state required for UI rendering
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Clear when we drive the framebuffer; with an external engine context the
    // engine already rendered its scene and we only overlay the UI.
    if (present) {
        glClearColor(clearColor.r, clearColor.g, clearColor.b, clearColor.a);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    // Reset clip stack from previous frame
    clipStack.clear();

    // Issue 4: Reset cached GL state at frame start
    lastBoundProgram = 0;
    lastBoundTexture = 0;
    vaoIsBound = false;
    lastBoundVBO = 0;
    lastBoundEBO = 0;
    // Perf 3.5: Reset uniform cache
    projectionDirty = true;
    lastTextColor = {-1,-1,-1,-1};
    lastPxRange = -1.0f;

    // Perf 1.5: Advance buffer region, reset offsets, and wait for fence
    if (usePersistentMapping) {
        currentBufferRegion = (currentBufferRegion + 1) % BUFFER_FRAMES;
        persistentVBOOffset = 0;
        persistentEBOOffset = 0;
        WaitForBufferRegion(currentBufferRegion);
    }
}

void OpenGLBackend::EndFrame() {
    // Perf 1.5: Insert fence for current buffer region
    if (usePersistentMapping) {
        bufferFences[currentBufferRegion] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    }

    // When embedded in an external engine context (not our own secondary window),
    // restore the engine's GL state.
    if (!ownsGLContext && !secondaryWindow) {
        RestoreState();
    }
}

void OpenGLBackend::Present() {
    // Swap this window's buffers. Used by the backend-agnostic multi-window path
    // (brief 09 AppWindow). The main FluentApp loop still swaps directly.
    if (window) SDL_GL_SwapWindow(static_cast<SDL_Window*>(window));
}

void OpenGLBackend::SetViewport(int width, int height) {
    viewportSize = Vec2(static_cast<float>(width), static_cast<float>(height));
    glViewport(0, 0, width, height);
}

// Issue 9: Remove double intersection — rect already comes pre-intersected from Renderer
void OpenGLBackend::PushClipRect(int x, int y, int width, int height) {
    ClipRect rect = { x, y, width, height };
    clipStack.push_back(rect);
    UpdateClipScissor();
}

void OpenGLBackend::PopClipRect() {
    if (!clipStack.empty()) {
        clipStack.pop_back();
        UpdateClipScissor();
    }
}

void OpenGLBackend::UpdateClipScissor() {
    if (clipStack.empty()) {
        glDisable(GL_SCISSOR_TEST);
    } else {
        const ClipRect& rect = clipStack.back();
        int scissorX = rect.x;
        int scissorY = static_cast<int>(viewportSize.y) - (rect.y + rect.height);
        glEnable(GL_SCISSOR_TEST);
        glScissor(std::max(0, scissorX), std::max(0, scissorY), std::max(0, rect.width), std::max(0, rect.height));
    }
}

// Issue 10: Store texture format on creation
void* OpenGLBackend::CreateTexture(int width, int height, const void* data, bool alphaOnly) {
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    GLint internalFormat = alphaOnly ? GL_RED : GL_RGBA8;
    GLenum format = alphaOnly ? GL_RED : GL_RGBA;

    Log(LogLevel::Info, "Creating GL Texture: %dx%d %s", width, height, alphaOnly ? "(Alpha)" : "(RGBA)");

    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, GL_UNSIGNED_BYTE, data);

    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        Log(LogLevel::Error, "OpenGL Error after glTexImage2D: %u", err);
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    if (alphaOnly) {
        GLint swizzleMask[] = { GL_RED, GL_RED, GL_RED, GL_RED };
        glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);
    }

    // Issue 10: Cache the format
    textureIsAlphaOnly[tex] = alphaOnly;

    // Issue 4: Invalidate cached texture binding since we bound a new one
    lastBoundTexture = tex;

    return (void*)(uintptr_t)tex;
}

// Issue 10: Use cached format instead of glGetTexLevelParameteriv
void OpenGLBackend::UpdateTexture(void* textureHandle, int x, int y, int width, int height, const void* data) {
    GLuint tex = (GLuint)(uintptr_t)textureHandle;
    glBindTexture(GL_TEXTURE_2D, tex);
    lastBoundTexture = tex;
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    // Use cached format instead of querying GL
    auto it = textureIsAlphaOnly.find(tex);
    GLenum glFormat = (it != textureIsAlphaOnly.end() && it->second) ? GL_RED : GL_RGBA;

    glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, width, height, glFormat, GL_UNSIGNED_BYTE, data);
}

// Perf R3: Copy texture content via FBO blit
void OpenGLBackend::CopyTexture(void* src, void* dst, int width, int height) {
    GLuint srcTex = (GLuint)(uintptr_t)src;
    GLuint dstTex = (GLuint)(uintptr_t)dst;

    GLuint fbos[2] = {};
    glGenFramebuffers(2, fbos);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbos[0]);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, srcTex, 0);
    if (glCheckFramebufferStatus(GL_READ_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        Log(LogLevel::Error, "CopyTexture: source FBO incomplete");
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(2, fbos);
        return;
    }

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbos[1]);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dstTex, 0);
    if (glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        Log(LogLevel::Error, "CopyTexture: destination FBO incomplete");
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(2, fbos);
        return;
    }

    glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(2, fbos);
}

// Issue 10: Clean up cache entry
void OpenGLBackend::DeleteTexture(void* textureHandle) {
    GLuint tex = (GLuint)(uintptr_t)textureHandle;
    textureIsAlphaOnly.erase(tex);
    if (lastBoundTexture == tex) lastBoundTexture = 0;
    glDeleteTextures(1, &tex);
}

void OpenGLBackend::DrawBatch(ShaderType type, const RenderVertex* vertices, size_t vertexCount,
                             const unsigned int* indices, size_t indexCount,
                             void* textureHandle, const float* projectionMatrix,
                             const Color& textColor) {
    if (vertexCount == 0) return;

    GLuint program = 0;
    ShaderUniforms* uniforms = nullptr;
    switch (type) {
        case ShaderType::Basic: program = shaderProgram; uniforms = &basicUniforms; break;
        case ShaderType::Text:  program = textShaderProgram; uniforms = &textUniforms; break;
        case ShaderType::MSDF:  program = msdfShaderProgram; uniforms = &msdfUniforms; break;
        case ShaderType::Image: program = imageShaderProgram; uniforms = &imageUniforms; break;
    }

    // Issue 4: Only bind program if changed
    bool programChanged = (lastBoundProgram != program);
    if (programChanged) {
        glUseProgram(program);
        lastBoundProgram = program;
        // Perf 3.5: Force re-upload all uniforms when program changes
        projectionDirty = true;
        lastTextColor = {-1,-1,-1,-1};
        lastPxRange = -1.0f;
    }

    // Perf 3.5: Only upload uniforms when values change
    if (uniforms->projection != -1) {
        if (projectionDirty || std::memcmp(lastProjection, projectionMatrix, 16 * sizeof(float)) != 0) {
            glUniformMatrix4fv(uniforms->projection, 1, GL_FALSE, projectionMatrix);
            std::memcpy(lastProjection, projectionMatrix, 16 * sizeof(float));
            projectionDirty = false;
        }
    }
    if (uniforms->textColor != -1) {
        if (lastTextColor.r != textColor.r || lastTextColor.g != textColor.g ||
            lastTextColor.b != textColor.b || lastTextColor.a != textColor.a) {
            glUniform4f(uniforms->textColor, textColor.r, textColor.g, textColor.b, textColor.a);
            lastTextColor = textColor;
        }
    }
    if (uniforms->pxRange != -1 && lastPxRange != 4.0f) {
        glUniform1f(uniforms->pxRange, 4.0f);
        lastPxRange = 4.0f;
    }

    if (textureHandle) {
        GLuint texId = (GLuint)(uintptr_t)textureHandle;
        if (uniforms->texture != -1) {
            glUniform1i(uniforms->texture, 0);
            // Issue 4: Only bind texture if changed
            if (lastBoundTexture != texId) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, texId);
                lastBoundTexture = texId;
            }
        }
    }

    // Issue 4: Only bind VAO if not already bound
    if (!vaoIsBound) {
        glBindVertexArray(vao);
        vaoIsBound = true;
    }

    // Perf 1.5: Use persistent mapped buffers if available, else fallback to glBufferSubData
    size_t vboNeeded = vertexCount * sizeof(RenderVertex);
    size_t eboNeeded = (indices && indexCount > 0) ? indexCount * sizeof(unsigned int) : 0;

    bool usePersistent = usePersistentMapping
                         && vboNeeded <= PERSISTENT_VBO_SIZE
                         && eboNeeded <= PERSISTENT_EBO_SIZE;

    if (usePersistent) {
        // Perf R2: Write at incrementing offset within the current region (no overwrites)
        size_t regionBase = currentBufferRegion * PERSISTENT_VBO_SIZE;
        size_t vboOffset = regionBase + persistentVBOOffset;

        // Check if we have space left in this region
        if (persistentVBOOffset + vboNeeded > PERSISTENT_VBO_SIZE ||
            persistentEBOOffset + eboNeeded > PERSISTENT_EBO_SIZE) {
            // Region full — fall through to traditional path
            usePersistent = false;
            goto traditional_path;
        }

        std::memcpy(static_cast<char*>(mappedVBO) + vboOffset, vertices, vboNeeded);
        persistentVBOOffset += vboNeeded;

        if (lastBoundVBO != persistentVBO) {
            glBindBuffer(GL_ARRAY_BUFFER, persistentVBO);
            lastBoundVBO = persistentVBO;
        }

        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(RenderVertex), (void*)vboOffset);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(RenderVertex), (void*)(vboOffset + 2 * sizeof(float)));
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(RenderVertex), (void*)(vboOffset + 6 * sizeof(float)));

        if (eboNeeded > 0) {
            size_t eboRegionBase = currentBufferRegion * PERSISTENT_EBO_SIZE;
            size_t eboOffset = eboRegionBase + persistentEBOOffset;
            std::memcpy(static_cast<char*>(mappedEBO) + eboOffset, indices, eboNeeded);
            persistentEBOOffset += eboNeeded;

            if (lastBoundEBO != persistentEBO) {
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, persistentEBO);
                lastBoundEBO = persistentEBO;
            }
            glDrawElements(GL_TRIANGLES, (GLsizei)indexCount, GL_UNSIGNED_INT, (void*)eboOffset);
        } else {
            glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertexCount);
        }
    } else { traditional_path:
        // Traditional path: glBufferSubData
        if (lastBoundVBO != vbo) {
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            lastBoundVBO = vbo;
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(RenderVertex), (void*)0);
            glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(RenderVertex), (void*)(2 * sizeof(float)));
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(RenderVertex), (void*)(6 * sizeof(float)));
        }
        if (vboNeeded > vboCapacity) {
            vboCapacity = vboNeeded * 2;
            glBufferData(GL_ARRAY_BUFFER, vboCapacity, nullptr, GL_DYNAMIC_DRAW);
        }
        glBufferSubData(GL_ARRAY_BUFFER, 0, vboNeeded, vertices);

        if (eboNeeded > 0) {
            if (lastBoundEBO != ebo) {
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
                lastBoundEBO = ebo;
            }
            if (eboNeeded > eboCapacity) {
                eboCapacity = eboNeeded * 2;
                glBufferData(GL_ELEMENT_ARRAY_BUFFER, eboCapacity, nullptr, GL_DYNAMIC_DRAW);
            }
            glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, eboNeeded, indices);
            glDrawElements(GL_TRIANGLES, (GLsizei)indexCount, GL_UNSIGNED_INT, nullptr);
        } else {
            glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertexCount);
        }
    }

    // Issue 4: Don't unbind VAO/program at end — they stay bound
}

void OpenGLBackend::DrawLines(const RenderVertex* vertices, size_t vertexCount,
                             float width, const float* projectionMatrix) {
    if (vertexCount == 0) return;

    // Issue 4: Only bind program if changed
    if (lastBoundProgram != shaderProgram) {
        glUseProgram(shaderProgram);
        lastBoundProgram = shaderProgram;
    }

    // Issue 3: Use cached uniform location
    if (basicUniforms.projection != -1)
        glUniformMatrix4fv(basicUniforms.projection, 1, GL_FALSE, projectionMatrix);

    glLineWidth(width);

    // Issue 4: Only bind VAO if not already bound
    if (!vaoIsBound) {
        glBindVertexArray(vao);
        vaoIsBound = true;
    }

    // Issue 2: Use glBufferSubData
    // Perf 2.4: Only bind if not already bound
    size_t vboNeeded = vertexCount * sizeof(RenderVertex);
    if (lastBoundVBO != vbo) {
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        lastBoundVBO = vbo;
    }
    if (vboNeeded > vboCapacity) {
        vboCapacity = vboNeeded * 2;
        glBufferData(GL_ARRAY_BUFFER, vboCapacity, nullptr, GL_DYNAMIC_DRAW);
    }
    glBufferSubData(GL_ARRAY_BUFFER, 0, vboNeeded, vertices);

    glDrawArrays(GL_LINES, 0, (GLsizei)vertexCount);

    glLineWidth(1.0f);
    // Issue 4: Don't unbind VAO/program
}

void OpenGLBackend::DrawSDFInstances(const SDFInstance* instances, size_t count,
                                     const float* projectionMatrix,
                                     const float* revealCursor) {
    if (count == 0 || !instances) return;

    // Bind the SDF program (respect the program cache).
    if (lastBoundProgram != sdfRectProgram) {
        glUseProgram(sdfRectProgram);
        lastBoundProgram = sdfRectProgram;
        projectionDirty = true; // force projection re-upload after a program switch
    }

    // Upload projection (reuse the projectionDirty cache pattern).
    if (sdfRectUniforms.projection != -1) {
        if (projectionDirty || std::memcmp(lastProjection, projectionMatrix, 16 * sizeof(float)) != 0) {
            glUniformMatrix4fv(sdfRectUniforms.projection, 1, GL_FALSE, projectionMatrix);
            std::memcpy(lastProjection, projectionMatrix, 16 * sizeof(float));
            projectionDirty = false;
        }
    }

    // Reveal cursor (brief 04). Uploaded per draw; cheap and the value can change
    // each frame. {0,0,0} (radius 0) disables the effect in the shader.
    if (sdfRectUniforms.reveal != -1) {
        if (revealCursor) glUniform3f(sdfRectUniforms.reveal, revealCursor[0], revealCursor[1], revealCursor[2]);
        else              glUniform3f(sdfRectUniforms.reveal, 0.0f, 0.0f, 0.0f);
    }

    // The SDF VAO carries its own quad/instance buffer bindings; binding it sets the
    // current ARRAY_BUFFER/ELEMENT_ARRAY_BUFFER, so invalidate the quad-path caches.
    glBindVertexArray(sdfVao);
    vaoIsBound = false;       // quad path must rebind its own VAO afterwards
    lastBoundVBO = 0;
    lastBoundEBO = 0;

    // Stream the instances into the dynamic instance buffer.
    size_t needed = count * sizeof(SDFInstance);
    glBindBuffer(GL_ARRAY_BUFFER, sdfInstanceVBO);
    if (needed > sdfInstanceCapacity) {
        sdfInstanceCapacity = needed * 2;
        glBufferData(GL_ARRAY_BUFFER, sdfInstanceCapacity, nullptr, GL_DYNAMIC_DRAW);
    }
    glBufferSubData(GL_ARRAY_BUFFER, 0, needed, instances);

    glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0, static_cast<GLsizei>(count));

    // Leave the default quad VAO unbound; the next DrawBatch rebinds `vao`.
    glBindVertexArray(0);
}

GLuint OpenGLBackend::CompileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        Log(LogLevel::Error, "Shader compilation error: %s", infoLog);
        return 0;
    }
    return shader;
}

GLuint OpenGLBackend::CreateShaderProgram(const char* vertexSource, const char* fragmentSource) {
    GLuint vs = CompileShader(GL_VERTEX_SHADER, vertexSource);
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fragmentSource);
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        Log(LogLevel::Error, "Shader linking error: %s", infoLog);
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

// --- Render Targets / FBO (Phase 5) ---

void* OpenGLBackend::CreateRenderTarget(int width, int height) {
    auto* rt = new RenderTargetData();
    rt->width = width;
    rt->height = height;

    // Create FBO
    glGenFramebuffers(1, &rt->fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, rt->fbo);

    // Create color texture attachment
    glGenTextures(1, &rt->colorTexture);
    glBindTexture(GL_TEXTURE_2D, rt->colorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt->colorTexture, 0);

    // Create depth/stencil renderbuffer
    glGenRenderbuffers(1, &rt->depthRenderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, rt->depthRenderbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rt->depthRenderbuffer);

    // Check completeness
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        Log(LogLevel::Error, "OpenGL Error: Framebuffer is not complete");
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        DeleteRenderTarget(rt);
        return nullptr;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    lastBoundTexture = 0; // Invalidate texture cache

    renderTargets.push_back(rt);
    return rt;
}

void OpenGLBackend::SetRenderTarget(void* target) {
    if (target) {
        auto* rt = static_cast<RenderTargetData*>(target);
        glBindFramebuffer(GL_FRAMEBUFFER, rt->fbo);
        glViewport(0, 0, rt->width, rt->height);
    } else {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, static_cast<int>(viewportSize.x), static_cast<int>(viewportSize.y));
    }
}

void* OpenGLBackend::GetRenderTargetTexture(void* target) {
    if (!target) return nullptr;
    auto* rt = static_cast<RenderTargetData*>(target);
    return (void*)(uintptr_t)rt->colorTexture;
}

void OpenGLBackend::DeleteRenderTarget(void* target) {
    if (!target) return;
    auto* rt = static_cast<RenderTargetData*>(target);

    if (rt->fbo) glDeleteFramebuffers(1, &rt->fbo);
    if (rt->colorTexture) glDeleteTextures(1, &rt->colorTexture);
    if (rt->depthRenderbuffer) glDeleteRenderbuffers(1, &rt->depthRenderbuffer);

    // Remove from tracking list
    auto it = std::find(renderTargets.begin(), renderTargets.end(), rt);
    if (it != renderTargets.end()) renderTargets.erase(it);

    delete rt;
}

// --- GL State Save/Restore (Phase 5) ---

void OpenGLBackend::SaveState() {
    glGetIntegerv(GL_VIEWPORT, savedState.viewport);
    glGetIntegerv(GL_SCISSOR_BOX, savedState.scissorBox);
    savedState.blendEnabled = glIsEnabled(GL_BLEND);
    savedState.scissorEnabled = glIsEnabled(GL_SCISSOR_TEST);
    savedState.depthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
    glGetIntegerv(GL_BLEND_SRC_RGB, &savedState.blendSrcRGB);
    glGetIntegerv(GL_BLEND_DST_RGB, &savedState.blendDstRGB);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &savedState.blendSrcAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &savedState.blendDstAlpha);
    glGetIntegerv(GL_CURRENT_PROGRAM, &savedState.currentProgram);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &savedState.boundTexture);
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &savedState.boundFBO);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &savedState.boundVAO);
    savedState.saved = true;
}

void OpenGLBackend::RestoreState() {
    if (!savedState.saved) return;

    glViewport(savedState.viewport[0], savedState.viewport[1],
               savedState.viewport[2], savedState.viewport[3]);

    if (savedState.blendEnabled) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    if (savedState.scissorEnabled) {
        glEnable(GL_SCISSOR_TEST);
        glScissor(savedState.scissorBox[0], savedState.scissorBox[1],
                  savedState.scissorBox[2], savedState.scissorBox[3]);
    } else {
        glDisable(GL_SCISSOR_TEST);
    }
    if (savedState.depthTestEnabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);

    glBlendFuncSeparate(savedState.blendSrcRGB, savedState.blendDstRGB,
                        savedState.blendSrcAlpha, savedState.blendDstAlpha);

    glUseProgram(savedState.currentProgram);
    glBindTexture(GL_TEXTURE_2D, savedState.boundTexture);
    glBindFramebuffer(GL_FRAMEBUFFER, savedState.boundFBO);
    glBindVertexArray(savedState.boundVAO);

    // Invalidate our caches since external state was restored
    lastBoundProgram = savedState.currentProgram;
    lastBoundTexture = savedState.boundTexture;
    vaoIsBound = (savedState.boundVAO != 0);

    savedState.saved = false;
}

// ───────────────────────────────────────────────────────────────────────────
// Acrylic / Mica backdrop (brief 06)
// ───────────────────────────────────────────────────────────────────────────

void OpenGLBackend::EnsureAcrylicResources() {
    if (acrylicResourcesReady) return;
    kawaseDownProgram = CreateShaderProgram(Shaders::BlurVertexShader, Shaders::KawaseDownFragment);
    kawaseUpProgram   = CreateShaderProgram(Shaders::BlurVertexShader, Shaders::KawaseUpFragment);
    acrylicCompositeProgram = CreateShaderProgram(Shaders::AcrylicCompositeVertexShader,
                                                  Shaders::AcrylicCompositeFragmentShader);

    uKawaseDownTex       = glGetUniformLocation(kawaseDownProgram, "uTex");
    uKawaseDownHalfpixel = glGetUniformLocation(kawaseDownProgram, "uHalfpixel");
    uKawaseUpTex         = glGetUniformLocation(kawaseUpProgram, "uTex");
    uKawaseUpHalfpixel   = glGetUniformLocation(kawaseUpProgram, "uHalfpixel");

    uCmpProjection   = glGetUniformLocation(acrylicCompositeProgram, "uProjection");
    uCmpCenter       = glGetUniformLocation(acrylicCompositeProgram, "uCenter");
    uCmpHalf         = glGetUniformLocation(acrylicCompositeProgram, "uHalf");
    uCmpSoft         = glGetUniformLocation(acrylicCompositeProgram, "uSoft");
    uCmpBlur         = glGetUniformLocation(acrylicCompositeProgram, "uBlur");
    uCmpNoise        = glGetUniformLocation(acrylicCompositeProgram, "uNoiseTex");
    uCmpScreenSize   = glGetUniformLocation(acrylicCompositeProgram, "uScreenSize");
    uCmpRadius       = glGetUniformLocation(acrylicCompositeProgram, "uRadius");
    uCmpTint         = glGetUniformLocation(acrylicCompositeProgram, "uTint");
    uCmpTintOpacity  = glGetUniformLocation(acrylicCompositeProgram, "uTintOpacity");
    uCmpLumOpacity   = glGetUniformLocation(acrylicCompositeProgram, "uLuminosityOpacity");
    uCmpNoiseAmount  = glGetUniformLocation(acrylicCompositeProgram, "uNoiseAmount");

    // Fullscreen quad (pos.xy, uv.xy) for the blur passes — triangle strip.
    static const float quad[16] = {
        -1.f, -1.f, 0.f, 0.f,
         1.f, -1.f, 1.f, 0.f,
        -1.f,  1.f, 0.f, 1.f,
         1.f,  1.f, 1.f, 1.f,
    };
    glGenVertexArrays(1, &blurVao);
    glGenBuffers(1, &blurVbo);
    glBindVertexArray(blurVao);
    glBindBuffer(GL_ARRAY_BUFFER, blurVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    acrylicResourcesReady = true;
}

static GLuint MakeColorRT(int w, int h, GLuint& outFbo) {
    GLuint tex = 0, fbo = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    outFbo = fbo;
    return tex;
}

void OpenGLBackend::DestroyBlurChain() {
    for (auto& lv : blurChain) {
        if (lv.fbo) glDeleteFramebuffers(1, &lv.fbo);
        if (lv.tex) glDeleteTextures(1, &lv.tex);
    }
    blurChain.clear();
    if (micaCache.fbo) { glDeleteFramebuffers(1, &micaCache.fbo); micaCache.fbo = 0; }
    if (micaCache.tex) { glDeleteTextures(1, &micaCache.tex); micaCache.tex = 0; }
    micaCacheValid = false;
}

void OpenGLBackend::EnsureBlurChain(int fbW, int fbH, int passes) {
    int needLevels = passes + 1; // level 0 = 1/2 res, then halve per level
    if (blurChainFbW == fbW && blurChainFbH == fbH &&
        static_cast<int>(blurChain.size()) >= needLevels) {
        return; // reuse
    }
    DestroyBlurChain();
    blurChain.reserve(needLevels);
    for (int i = 0; i < needLevels; ++i) {
        int w = std::max(1, fbW >> (i + 1));
        int h = std::max(1, fbH >> (i + 1));
        BlurLevel lv; lv.w = w; lv.h = h;
        lv.tex = MakeColorRT(w, h, lv.fbo);
        blurChain.push_back(lv);
    }
    blurChainFbW = fbW; blurChainFbH = fbH;
}

void OpenGLBackend::CaptureAndBlur(int fbW, int fbH, int passes) {
    // 1. Downscale-capture the default framebuffer into level 0 (1/2 res).
    BlurLevel& l0 = blurChain[0];
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, l0.fbo);
    glBlitFramebuffer(0, 0, fbW, fbH, 0, 0, l0.w, l0.h, GL_COLOR_BUFFER_BIT, GL_LINEAR);

    glDisable(GL_BLEND);
    glBindVertexArray(blurVao);

    // 2. Downsample chain: level[k] -> level[k+1].
    glUseProgram(kawaseDownProgram);
    glUniform1i(uKawaseDownTex, 0);
    glActiveTexture(GL_TEXTURE0);
    for (int k = 0; k < passes; ++k) {
        BlurLevel& src = blurChain[k];
        BlurLevel& dst = blurChain[k + 1];
        glBindFramebuffer(GL_FRAMEBUFFER, dst.fbo);
        glViewport(0, 0, dst.w, dst.h);
        glBindTexture(GL_TEXTURE_2D, src.tex);
        glUniform2f(uKawaseDownHalfpixel, 0.5f / src.w, 0.5f / src.h);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    // 3. Upsample chain: level[k] -> level[k-1].
    glUseProgram(kawaseUpProgram);
    glUniform1i(uKawaseUpTex, 0);
    for (int k = passes; k >= 1; --k) {
        BlurLevel& src = blurChain[k];
        BlurLevel& dst = blurChain[k - 1];
        glBindFramebuffer(GL_FRAMEBUFFER, dst.fbo);
        glViewport(0, 0, dst.w, dst.h);
        glBindTexture(GL_TEXTURE_2D, src.tex);
        glUniform2f(uKawaseUpHalfpixel, 0.5f / src.w, 0.5f / src.h);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }
    // Final blurred backdrop now lives in blurChain[0] (1/2 res, full screen extent).
}

void OpenGLBackend::DrawAcrylicPanel(const AcrylicParams& p, const float* projectionMatrix) {
    if (p.w <= 0.0f || p.h <= 0.0f) return;
    EnsureAcrylicResources();

    const int fbW = std::max(1, static_cast<int>(viewportSize.x));
    const int fbH = std::max(1, static_cast<int>(viewportSize.y));
    int passes = std::clamp(p.blurPasses, 1, 5);
    EnsureBlurChain(fbW, fbH, passes);

    // Save scissor state (EndFrame set it for the panel's clip). Blur passes must
    // not be scissored (they fill offscreen RTs); the composite restores it.
    GLboolean scissorWasEnabled = glIsEnabled(GL_SCISSOR_TEST);
    GLint scBox[4]; glGetIntegerv(GL_SCISSOR_BOX, scBox);
    glDisable(GL_SCISSOR_TEST);

    GLuint blurredTex;
    if (p.mica && micaCacheValid && micaCache.w == fbW && micaCache.h == fbH) {
        // Mica: reuse the cached blurred backdrop (near-static).
        blurredTex = micaCache.tex;
    } else {
        CaptureAndBlur(fbW, fbH, passes);
        blurredTex = blurChain[0].tex;
        if (p.mica) {
            // Cache the blurred result at full-screen-relative size for reuse.
            if (!micaCache.tex || micaCache.w != fbW || micaCache.h != fbH) {
                if (micaCache.fbo) glDeleteFramebuffers(1, &micaCache.fbo);
                if (micaCache.tex) glDeleteTextures(1, &micaCache.tex);
                micaCache.tex = MakeColorRT(blurChain[0].w, blurChain[0].h, micaCache.fbo);
                micaCache.w = fbW; micaCache.h = fbH;
            }
            // Copy blurChain[0] -> micaCache via blit.
            glBindFramebuffer(GL_READ_FRAMEBUFFER, blurChain[0].fbo);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, micaCache.fbo);
            glBlitFramebuffer(0, 0, blurChain[0].w, blurChain[0].h,
                              0, 0, blurChain[0].w, blurChain[0].h,
                              GL_COLOR_BUFFER_BIT, GL_NEAREST);
            blurredTex = micaCache.tex;
            micaCacheValid = true;
        }
    }

    // Composite onto the default framebuffer over the panel rect.
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, fbW, fbH);
    if (scissorWasEnabled) {
        glEnable(GL_SCISSOR_TEST);
        glScissor(scBox[0], scBox[1], scBox[2], scBox[3]);
    }
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(acrylicCompositeProgram);
    glUniformMatrix4fv(uCmpProjection, 1, GL_FALSE, projectionMatrix);
    glUniform2f(uCmpCenter, p.x + p.w * 0.5f, p.y + p.h * 0.5f);
    glUniform2f(uCmpHalf, p.w * 0.5f, p.h * 0.5f);
    glUniform1f(uCmpSoft, std::max(1.0f, p.dpiScale));
    glUniform1f(uCmpRadius, p.cornerRadius);
    glUniform2f(uCmpScreenSize, static_cast<float>(fbW), static_cast<float>(fbH));
    glUniform3f(uCmpTint, p.tintR, p.tintG, p.tintB);
    glUniform1f(uCmpTintOpacity, p.tintOpacity);
    glUniform1f(uCmpLumOpacity, p.luminosityOpacity);
    glUniform1f(uCmpNoiseAmount, p.noiseAmount);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, blurredTex);
    glUniform1i(uCmpBlur, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, (GLuint)(uintptr_t)p.noiseTex);
    glUniform1i(uCmpNoise, 1);
    glActiveTexture(GL_TEXTURE0);

    glBindVertexArray(blurVao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    // Invalidate our state caches — we changed program/texture/VAO/FBO behind the
    // batch renderer's back; the next DrawBatch/DrawSDFInstances rebinds everything.
    lastBoundProgram = 0;
    lastBoundTexture = 0;
    vaoIsBound = false;
    lastBoundVBO = 0;
    lastBoundEBO = 0;
    projectionDirty = true;
}

// Brief 24: the GL backend implements every optional feature except external
// texture wrapping (RegisterExternalTexture is Vulkan-only so far).
uint32_t OpenGLBackend::Capabilities() const {
    return static_cast<uint32_t>(RenderCap::RenderTargets)
         | static_cast<uint32_t>(RenderCap::SaveRestore)
         | static_cast<uint32_t>(RenderCap::CopyTexture)
         | static_cast<uint32_t>(RenderCap::ReadPixel)
         | static_cast<uint32_t>(RenderCap::Instancing)
         | static_cast<uint32_t>(RenderCap::Acrylic);
}

// Phase C6: read a single pixel from the default framebuffer.
// Note: glReadPixels uses bottom-up Y; we flip from top-down widget Y.
Color OpenGLBackend::ReadPixel(int x, int y) {
    GLint vp[4];
    glGetIntegerv(GL_VIEWPORT, vp);
    int fbHeight = vp[3];
    int flippedY = fbHeight - 1 - y;
    unsigned char rgba[4] = {0, 0, 0, 0};
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(x, flippedY, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    return Color(rgba[0] / 255.0f, rgba[1] / 255.0f, rgba[2] / 255.0f, rgba[3] / 255.0f);
}

} // namespace FluentUI
