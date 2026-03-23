#pragma once

#include "RenderBackend.h"
#include <glad/glad.h>
#include <SDL3/SDL.h>
#include <unordered_map>
#include <vector>

namespace FluentUI {

class OpenGLBackend : public RenderBackend {
public:
    OpenGLBackend() = default;
    ~OpenGLBackend() override;

    bool Init(void* windowHandle) override;
    void Shutdown() override;
    void BeginFrame(const Color& clearColor) override;
    void EndFrame() override;
    void SetViewport(int width, int height) override;

    void PushClipRect(int x, int y, int width, int height) override;
    void PopClipRect() override;

    void* CreateTexture(int width, int height, const void* data, bool alphaOnly = false) override;
    void UpdateTexture(void* textureHandle, int x, int y, int width, int height, const void* data) override;
    void DeleteTexture(void* textureHandle) override;
    void CopyTexture(void* src, void* dst, int width, int height) override;

    void DrawBatch(ShaderType type, const RenderVertex* vertices, size_t vertexCount,
                   const unsigned int* indices, size_t indexCount,
                   void* textureHandle, const float* projectionMatrix,
                   const Color& textColor = {1,1,1,1}) override;

    void DrawLines(const RenderVertex* vertices, size_t vertexCount,
                   float width, const float* projectionMatrix) override;

    // --- Render Targets / FBO (Phase 5) ---
    void* CreateRenderTarget(int width, int height) override;
    void SetRenderTarget(void* target) override;
    void* GetRenderTargetTexture(void* target) override;
    void DeleteRenderTarget(void* target) override;

    // --- GL State Save/Restore (Phase 5) ---
    void SaveState() override;
    void RestoreState() override;

private:
    SDL_Window* window = nullptr;
    SDL_GLContext glContext = nullptr;
    GLuint shaderProgram = 0;
    GLuint textShaderProgram = 0;
    GLuint msdfShaderProgram = 0;
    GLuint imageShaderProgram = 0;
    GLuint vao = 0, vbo = 0, ebo = 0;
    Vec2 viewportSize = {800.0f, 600.0f};

    // Issue 3: Cached uniform locations
    struct ShaderUniforms {
        GLint projection = -1;
        GLint textColor = -1;
        GLint pxRange = -1;
        GLint texture = -1;
    };
    ShaderUniforms basicUniforms;
    ShaderUniforms textUniforms;
    ShaderUniforms msdfUniforms;
    ShaderUniforms imageUniforms;

    // Issue 2: Pre-allocated VBO/EBO capacity
    size_t vboCapacity = 0;
    size_t eboCapacity = 0;

    // Perf 3.5: Cached uniform values to avoid redundant uploads
    bool projectionDirty = true;
    float lastProjection[16] = {};
    Color lastTextColor{-1,-1,-1,-1}; // Invalid initial value to force first upload
    float lastPxRange = -1.0f;

    // Issue 4: GL state caching
    GLuint lastBoundProgram = 0;
    GLuint lastBoundTexture = 0;
    bool vaoIsBound = false;

    // Perf 2.4: Buffer bind caching
    GLuint lastBoundVBO = 0;
    GLuint lastBoundEBO = 0;

    // Perf 1.5: Persistent mapped buffers with triple-buffering
    bool usePersistentMapping = false;
    static constexpr int BUFFER_FRAMES = 3;
    static constexpr size_t PERSISTENT_VBO_SIZE = 1024 * 1024;  // 1MB per region
    static constexpr size_t PERSISTENT_EBO_SIZE = 512 * 1024;   // 512KB per region
    GLuint persistentVBO = 0;
    GLuint persistentEBO = 0;
    void* mappedVBO = nullptr;
    void* mappedEBO = nullptr;
    int currentBufferRegion = 0;
    size_t persistentVBOOffset = 0;  // Current write offset within region
    size_t persistentEBOOffset = 0;  // Current write offset within region
    GLsync bufferFences[BUFFER_FRAMES] = {};
    void InitPersistentBuffers();
    void WaitForBufferRegion(int region);

    // Issue 10: Texture format cache
    std::unordered_map<GLuint, bool> textureIsAlphaOnly;

    struct ClipRect {
        int x, y, width, height;
    };
    std::vector<ClipRect> clipStack;

    // FBO render targets (Phase 5)
    struct RenderTargetData {
        GLuint fbo = 0;
        GLuint colorTexture = 0;
        GLuint depthRenderbuffer = 0;
        int width = 0;
        int height = 0;
    };
    std::vector<RenderTargetData*> renderTargets;

    // Saved GL state (Phase 5)
    struct SavedGLState {
        GLint viewport[4] = {};
        GLint scissorBox[4] = {};
        GLboolean blendEnabled = GL_FALSE;
        GLboolean scissorEnabled = GL_FALSE;
        GLboolean depthTestEnabled = GL_FALSE;
        GLint blendSrcRGB = 0, blendDstRGB = 0;
        GLint blendSrcAlpha = 0, blendDstAlpha = 0;
        GLint currentProgram = 0;
        GLint boundTexture = 0;
        GLint boundFBO = 0;
        GLint boundVAO = 0;
        bool saved = false;
    };
    SavedGLState savedState;

    GLuint CompileShader(GLenum type, const char* source);
    GLuint CreateShaderProgram(const char* vertexSource, const char* fragmentSource);
    void UpdateClipScissor();
    void QueryUniforms(GLuint program, ShaderUniforms& uniforms);
};

} // namespace FluentUI
