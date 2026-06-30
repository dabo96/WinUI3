#include "core/FluentApp.h"
#include "core/Context.h"
#include "core/DockSystem.h"
#include "core/LayoutSerializer.h"
#include "core/OpenGLBackend.h"
#include "UI/Widgets.h"
#include "Theme/FluentTheme.h"
#include <SDL3/SDL_vulkan.h>
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>

namespace FluentUI {

namespace {
// brief 08/09: choose the OS-window creation flag for the active backend.
//   - OpenGL → SDL_WINDOW_OPENGL.
//   - Vulkan → SDL_WINDOW_VULKAN *only* when SDL was built with a Vulkan driver
//     (so SDL_Vulkan_CreateSurface works). When SDL lacks Vulkan, the backend
//     creates a native Win32 surface from the HWND, which needs no Vulkan flag;
//     we keep SDL_WINDOW_OPENGL there to preserve the proven standalone path.
// Mirrors VulkanBackend::CreateSurfaceForWindow's SDL-vs-native gate.
Uint64 BackendWindowFlag() {
    if (GetPreferredBackend() == RenderBackendType::Vulkan) {
        if (SDL_Vulkan_LoadLibrary(nullptr)) {
            uint32_t extCount = 0;
            const char* const* exts = SDL_Vulkan_GetInstanceExtensions(&extCount);
            if (exts && extCount > 0) return SDL_WINDOW_VULKAN;
        }
        // SDL has no Vulkan driver → native HWND surface path.
    }
    return SDL_WINDOW_OPENGL;
}

// brief 13: SDL hit-test callback for borderless windows with a custom title bar.
// Runs on the thread that pumps the OS event queue (the main thread here, during
// SDL_PollEvent). Reads the zones published by FluentUI::TitleBar() under the
// context mutex. `area` is in window-point coordinates, the same space the
// renderer viewport (SDL_GetWindowSize) and the widget rects use, so no DPI
// conversion is needed.
SDL_HitTestResult CustomTitleBarHitTest(SDL_Window* win, const SDL_Point* area,
                                        void* data) {
    UIContext* ctx = static_cast<UIContext*>(data);
    if (!ctx || !area) return SDL_HITTEST_NORMAL;

    int w = 0, h = 0;
    SDL_GetWindowSize(win, &w, &h);

    std::lock_guard<std::mutex> lk(ctx->titleBarHit.mutex);
    TitleBarHitRegions& tb = ctx->titleBarHit;
    float fx = static_cast<float>(area->x), fy = static_cast<float>(area->y);

    // Resize borders take precedence at the window edges.
    if (tb.resizable && tb.resizeBorder > 0.0f) {
        float b = tb.resizeBorder;
        bool left = fx < b, right = fx > (float)w - b;
        bool top = fy < b, bottom = fy > (float)h - b;
        if (top && left) return SDL_HITTEST_RESIZE_TOPLEFT;
        if (top && right) return SDL_HITTEST_RESIZE_TOPRIGHT;
        if (bottom && left) return SDL_HITTEST_RESIZE_BOTTOMLEFT;
        if (bottom && right) return SDL_HITTEST_RESIZE_BOTTOMRIGHT;
        if (top) return SDL_HITTEST_RESIZE_TOP;
        if (bottom) return SDL_HITTEST_RESIZE_BOTTOM;
        if (left) return SDL_HITTEST_RESIZE_LEFT;
        if (right) return SDL_HITTEST_RESIZE_RIGHT;
    }

    // Draggable caption strip, minus the interactive exclusions (caption buttons,
    // center content) which must stay clickable.
    if (tb.active && tb.caption.Contains(Vec2(fx, fy))) {
        for (const Rect& ex : tb.exclusions)
            if (ex.Contains(Vec2(fx, fy))) return SDL_HITTEST_NORMAL;
        return SDL_HITTEST_DRAGGABLE;
    }
    return SDL_HITTEST_NORMAL;
}
} // namespace

// ===================== AppWindow (Secondary Windows) =====================

AppWindow::AppWindow(const std::string& title, int width, int height,
                     SDL_Window* parentWindow, SDL_GLContext parentGLContext,
                     UIContext* parentCtx) {
    // brief 09 Fase 1: create the OS window with the flag matching the active
    // backend (GL → SDL_WINDOW_OPENGL; Vulkan → SDL_WINDOW_VULKAN when SDL has a
    // Vulkan driver, else a native-HWND-surface window). Same gate as the main window.
    window_ = SDL_CreateWindow(title.c_str(), width, height,
                                BackendWindowFlag() | SDL_WINDOW_RESIZABLE);
    if (!window_) {
        Log(LogLevel::Error, "AppWindow: Failed to create window: %s", SDL_GetError());
        return;
    }

    // Save the current global context so we can restore it after.
    UIContext* savedGlobalCtx = GetContext();

    // brief 08/09: share the parent's GPU device / GL context + resource pool.
    // (GL: reuses the SAME SDL_GLContext; Vulkan: surface+swapchain over the shared
    // device.) CreateStandaloneContext makes the new context current as a side effect.
    ctx_ = CreateStandaloneContext(window_, parentCtx, &backend_);
    if (!ctx_) {
        Log(LogLevel::Error, "AppWindow: Failed to create shared context");
        SDL_DestroyWindow(window_);
        window_ = nullptr;
        SetCurrentContext(savedGlobalCtx);
        if (parentGLContext) SDL_GL_MakeCurrent(parentWindow, parentGLContext);
        return;
    }

    // Remember the shared GL context (parent's) for GL resource cleanup later.
    // Null on Vulkan (SDL_GL_GetCurrentContext returns null without a GL context).
    sharedGLContext_ = SDL_GL_GetCurrentContext();

    ctx_->renderer.SetViewport(width, height);
    SDL_StartTextInput(window_);
    open_ = true;
    updateDPIScale();

    // Restore the parent window's GL context and global context so the main window
    // keeps rendering normally.
    if (parentGLContext) SDL_GL_MakeCurrent(parentWindow, parentGLContext);
    SetCurrentContext(savedGlobalCtx);
}

AppWindow::~AppWindow() {
    // brief 09: this window shares the parent's GL context (it does NOT own one).
    // Make that shared context current on THIS window so GL resource cleanup (the
    // window's own atlas textures, buffers) targets the right context. On Vulkan
    // sharedGLContext_ is null and DeleteTexture drains the device itself.
    if (window_ && sharedGLContext_) {
        SDL_GL_MakeCurrent(window_, sharedGLContext_);
    }

    if (ctx_) {
        // Frees the backend (its swapchain/surface on Vulkan; nothing device-wide).
        // Never frees the shared resource pool (owned by the main context).
        DestroyStandaloneContext(ctx_, backend_);
        ctx_ = nullptr;
        backend_ = nullptr;
    } else if (backend_) {
        backend_->Shutdown();
        delete backend_;
        backend_ = nullptr;
    }

    // We never created a GL context, so we never destroy one.
    sharedGLContext_ = nullptr;

    if (window_) {
        SDL_StopTextInput(window_);
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
}

void AppWindow::root(std::function<void(UIBuilder&)> buildFn) {
    rootBuilder_ = std::move(buildFn);
}

void AppWindow::routeEvent(const SDL_Event& e) {
    if (!ctx_) return;

    if (e.type == SDL_EVENT_WINDOW_RESIZED) {
        int w, h;
        SDL_GetWindowSize(window_, &w, &h);
        ctx_->renderer.SetViewport(w, h);
        // Update DPI for this window
        updateDPIScale();
    } else if (e.type == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED ||
               e.type == SDL_EVENT_WINDOW_MOVED) {
        // Window moved to different monitor or display scale changed
        updateDPIScale();
    } else if (e.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
        open_ = false;
    } else {
        ctx_->input.ProcessEvent(e);
    }
}

void AppWindow::updateDPIScale() {
    if (!ctx_ || !window_) return;
    float scale = SDL_GetWindowDisplayScale(window_);
    if (scale <= 0.0f) scale = 1.0f;
    ctx_->dpiScale = scale;
    ctx_->renderer.SetDPIScale(scale); // Phase A4: keep AA fringe at 1 physical pixel
}

void AppWindow::processFrame(float dt, SDL_Window* parentWindow, SDL_GLContext parentGLContext) {
    if (!open_ || !ctx_ || !window_) return;

    // brief 09 Fase 1: backend-agnostic. Make this context active; the backend
    // routes GL to this window inside BeginFrame (GL secondary-window mode) or uses
    // its own swapchain (Vulkan). No raw GL here.
    UIContext* savedCtx = GetContext();
    SetCurrentContext(ctx_);

    ctx_->input.Update(window_);

    NewFrame(dt);

    if (rootBuilder_) {
        UIBuilder builder(ctx_);
        rootBuilder_(builder);
    }

    RenderDeferredDropdowns();
    Render();
    ctx_->renderer.Present(); // GL: SwapWindow(window_); Vulkan: presented in EndFrame

    // Restore the parent's GL context + global context (GL only; null on Vulkan).
    if (parentGLContext) SDL_GL_MakeCurrent(parentWindow, parentGLContext);
    SetCurrentContext(savedCtx);
}

// ===================== FluentApp =====================

namespace {

// Resolve the icon font path: explicit override -> exe_dir/assets/fonts/lucide.ttf
// -> cwd/assets/fonts/lucide.ttf -> $FLUENTUI_ASSETS_DIR/fonts/lucide.ttf.
// Returns empty when nothing exists. We try every location even if some
// std::filesystem queries throw (e.g. permissions) — soft-fail is the contract.
std::string ResolveIconFontPath(const std::string& explicitPath) {
    namespace fs = std::filesystem;
    auto tryFile = [](const fs::path& p) -> std::string {
        std::error_code ec;
        if (fs::exists(p, ec) && fs::is_regular_file(p, ec)) {
            return p.string();
        }
        return {};
    };

    if (!explicitPath.empty()) {
        std::string hit = tryFile(explicitPath);
        if (!hit.empty()) return hit;
        Log(LogLevel::Warning, "FluentApp: iconFontPath '%s' not found, falling back to defaults",
            explicitPath.c_str());
    }

    // Executable directory. SDL3 returns a const char* owned by SDL — do NOT free.
    if (const char* exeBase = SDL_GetBasePath()) {
        fs::path candidate = fs::path(exeBase) / "assets" / "fonts" / "lucide.ttf";
        std::string hit = tryFile(candidate);
        if (!hit.empty()) return hit;
    }

    // Current working directory
    {
        std::error_code ec;
        fs::path candidate = fs::current_path(ec) / "assets" / "fonts" / "lucide.ttf";
        if (!ec) {
            std::string hit = tryFile(candidate);
            if (!hit.empty()) return hit;
        }
    }

    // Environment override
    if (const char* envDir = std::getenv("FLUENTUI_ASSETS_DIR")) {
        fs::path candidate = fs::path(envDir) / "fonts" / "lucide.ttf";
        std::string hit = tryFile(candidate);
        if (!hit.empty()) return hit;
    }

    return {};
}

void AutoLoadIconFont(UIContext* ctx, const std::string& explicitPath, int pixelHeight) {
    if (!ctx) return;
    // The renderer auto-loads a default Lucide font during initialization, so
    // icons already work everywhere. Only act here when the user supplied an
    // explicit path (to override the default) or when nothing got loaded yet.
    if (explicitPath.empty() && ctx->renderer.IsIconFontLoaded()) return;
    std::string path = ResolveIconFontPath(explicitPath);
    if (path.empty()) {
        Log(LogLevel::Warning,
            "FluentApp: lucide.ttf not found — icons will not be drawn. "
            "Place it at <exe>/assets/fonts/lucide.ttf or set FLUENTUI_ASSETS_DIR.");
        return;
    }
    if (!ctx->renderer.LoadIconFont(path, pixelHeight)) {
        Log(LogLevel::Warning, "FluentApp: failed to load icon font from '%s'", path.c_str());
    }
}

} // namespace

FluentApp::FluentApp(const std::string& title, const AppConfig& config)
    : targetFPS_(config.targetFPS), enableDPI_(config.enableDPI) {

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        Log(LogLevel::Error, "FluentApp: SDL_Init failed: %s", SDL_GetError());
        return;
    }

    Uint64 flags = BackendWindowFlag();
    if (config.resizable) flags |= SDL_WINDOW_RESIZABLE;
    flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;
    // brief 13: borderless chrome so the custom TitleBar() drives the window.
    if (config.useCustomTitleBar) flags |= SDL_WINDOW_BORDERLESS;

    window_ = SDL_CreateWindow(title.c_str(), config.width, config.height, flags);
    if (!window_) {
        Log(LogLevel::Error, "FluentApp: SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return;
    }

    ctx_ = CreateContext(window_);
    if (!ctx_) {
        Log(LogLevel::Error, "FluentApp: Failed to create UIContext");
        SDL_DestroyWindow(window_);
        SDL_Quit();
        window_ = nullptr;
        return;
    }

    // Cache the main GL context for later restoration after secondary window ops
    mainGLContext_ = SDL_GL_GetCurrentContext();

    ctx_->style = config.darkMode ? GetDarkFluentStyle() : GetDefaultFluentStyle();

    AutoLoadIconFont(ctx_, config.iconFontPath, config.iconFontSize);

    // brief 13: install the hit-test so TitleBar() can drag/resize the window.
    if (config.useCustomTitleBar) {
        if (!SDL_SetWindowHitTest(window_, CustomTitleBarHitTest, ctx_)) {
            Log(LogLevel::Warning, "FluentApp: SDL_SetWindowHitTest failed: %s",
                SDL_GetError());
        }
    }

    int w, h;
    SDL_GetWindowSize(window_, &w, &h);
    ctx_->renderer.SetViewport(w, h);

    if (enableDPI_) {
        updateDPIScale();
    }

    SDL_StartTextInput(window_);
    lastTime_ = SDL_GetTicks();
    initialized_ = true;
}

FluentApp::FluentApp(const std::string& title, int width, int height)
    : FluentApp(title, AppConfig{width, height}) {}

FluentApp::FluentApp(SDL_Window* externalWindow, SDL_GLContext externalGLContext,
                     bool darkMode, bool enableDPI)
    : enableDPI_(enableDPI), ownsWindow_(false) {

    if (!externalWindow || !externalGLContext) {
        Log(LogLevel::Error, "FluentApp: External window or GL context is null");
        return;
    }

    window_ = externalWindow;
    mainGLContext_ = externalGLContext;

    // Pass the external GL context so the backend reuses it instead of creating a new one
    ctx_ = CreateContext(window_, mainGLContext_);
    if (!ctx_) {
        Log(LogLevel::Error, "FluentApp: Failed to create UIContext");
        window_ = nullptr;
        return;
    }

    ctx_->style = darkMode ? GetDarkFluentStyle() : GetDefaultFluentStyle();

    AutoLoadIconFont(ctx_, /*explicitPath=*/std::string(), /*pixelHeight=*/16);

    int w, h;
    SDL_GetWindowSize(window_, &w, &h);
    ctx_->renderer.SetViewport(w, h);

    if (enableDPI_) {
        updateDPIScale();
    }

    SDL_StartTextInput(window_);
    lastTime_ = SDL_GetTicks();
    initialized_ = true;
}

FluentApp::~FluentApp() {
    // Destroy secondary windows first
    secondaryWindows_.clear();

    if (shutdownFn_) shutdownFn_();
    if (ctx_) DestroyContext();

    if (ownsWindow_) {
        // We created the window and SDL — clean them up
        if (window_) {
            SDL_StopTextInput(window_);
            SDL_DestroyWindow(window_);
        }
        SDL_Quit();
    } else if (window_) {
        // External window: leave window/SDL alone but undo the StartTextInput
        // we issued in the external-window constructor.
        SDL_StopTextInput(window_);
    }
}

void FluentApp::root(std::function<void(UIBuilder&)> buildFn) {
    rootBuilder_ = std::move(buildFn);
}

// Helper: extract SDL window ID from any event type
SDL_WindowID FluentApp::getEventWindowID(const SDL_Event& e) {
    switch (e.type) {
        case SDL_EVENT_WINDOW_RESIZED:
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        case SDL_EVENT_WINDOW_MOVED:
        case SDL_EVENT_WINDOW_FOCUS_GAINED:
        case SDL_EVENT_WINDOW_FOCUS_LOST:
        case SDL_EVENT_WINDOW_SHOWN:
        case SDL_EVENT_WINDOW_HIDDEN:
        case SDL_EVENT_WINDOW_EXPOSED:
        case SDL_EVENT_WINDOW_MINIMIZED:
        case SDL_EVENT_WINDOW_MAXIMIZED:
        case SDL_EVENT_WINDOW_RESTORED:
        case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
            return e.window.windowID;
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
            return e.key.windowID;
        case SDL_EVENT_TEXT_INPUT:
            return e.text.windowID;
        case SDL_EVENT_MOUSE_MOTION:
            return e.motion.windowID;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
            return e.button.windowID;
        case SDL_EVENT_MOUSE_WHEEL:
            return e.wheel.windowID;
        default:
            return 0;
    }
}

void FluentApp::run() {
    if (!initialized_ || !window_ || !ctx_) {
        Log(LogLevel::Error, "FluentApp: Cannot run — initialization failed");
        return;
    }

    if (initFn_) initFn_();
    running_ = true;

    SDL_WindowID mainWindowID = SDL_GetWindowID(window_);

    while (running_) {
        // Ensure main window GL context is current at frame start
        if (mainGLContext_) SDL_GL_MakeCurrent(window_, mainGLContext_); // GL-only; null on Vulkan
        SetCurrentContext(ctx_);

        // Update main window input
        ctx_->input.Update(window_);

        // Poll ALL events and route to correct window
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) {
                running_ = false;
                break;
            }

            SDL_WindowID eventWinID = getEventWindowID(e);

            if (eventWinID == mainWindowID || eventWinID == 0) {
                // Main window or global event
                if (e.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                    // brief 13: custom TitleBar close button posts this for the main
                    // window; the OS X button (non-borderless) sends it too.
                    running_ = false;
                    break;
                } else if (e.type == SDL_EVENT_WINDOW_RESIZED) {
                    int w, h;
                    SDL_GetWindowSize(window_, &w, &h);
                    ctx_->renderer.SetViewport(w, h);
                    if (enableDPI_) updateDPIScale();
                } else if (e.type == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED) {
                    if (enableDPI_) updateDPIScale();
                } else if (e.type == SDL_EVENT_WINDOW_MOVED) {
                    // Window moved — may have crossed to a different monitor with different DPI
                    if (enableDPI_) updateDPIScale();
                } else {
                    ctx_->input.ProcessEvent(e);
                }
            } else {
                // Route to the correct secondary window
                for (auto& win : secondaryWindows_) {
                    if (win->isOpen() && win->window() &&
                        SDL_GetWindowID(win->window()) == eventWinID) {
                        win->routeEvent(e);
                        break;
                    }
                }
            }
        }
        if (!running_) break;

        float dt = computeDelta();

        // Always re-sync viewport with actual window size (guards against
        // native dialogs or other OS events corrupting the cached size)
        {
            int w, h;
            SDL_GetWindowSize(window_, &w, &h);
            Vec2 currentVP = ctx_->renderer.GetViewportSize();
            if (w > 0 && h > 0 && (static_cast<int>(currentVP.x) != w || static_cast<int>(currentVP.y) != h)) {
                ctx_->renderer.SetViewport(w, h);
            }
        }

        if (updateFn_) updateFn_(dt);

        // Dispatch file drop callback
        if (fileDropFn_ && ctx_->input.HasDroppedFiles()) {
            fileDropFn_(ctx_->input.DroppedFiles());
        }

        // Main window frame
        if (mainGLContext_) SDL_GL_MakeCurrent(window_, mainGLContext_); // GL-only; null on Vulkan
        SetCurrentContext(ctx_);

        NewFrame(dt);
        shortcuts.ProcessFrame(ctx_->input);

        if (!ctx_->dockSpace.IsEmpty()) {
            ctx_->dockSpace.HandleInteraction(
                ctx_->input.MouseX(), ctx_->input.MouseY(),
                ctx_->input.IsMouseDown(0), ctx_->input.IsMousePressed(0));
        }

        if (rootBuilder_) {
            UIBuilder builder(ctx_);
            rootBuilder_(builder);

            // Perf Phase C: Draw debug overlay if enabled
            builder.debugOverlay();
        }

        // brief 09 Fase 3: draw the re-dock preview on the main window when a
        // floating panel window hovers one of its dock zones.
        if (autoDetach_ && redockActive_ && redockWin_) {
            Rect tb = redockTargetId_.empty() ? ctx_->dockSpace.GetAvailableArea()
                                              : ctx_->dockSpace.GetPanelBounds(redockTargetId_);
            Rect preview = GetDockZonePreviewRect(tb, redockZone_);
            ctx_->renderer.SetLayer(RenderLayer::Overlay);
            ctx_->renderer.DrawRectFilled(preview.pos, preview.size, Color(0.2f, 0.4f, 0.9f, 0.25f), 4.0f);
            ctx_->renderer.DrawRect(preview.pos, preview.size, Color(0.3f, 0.5f, 1.0f, 0.6f), 4.0f);
            ctx_->renderer.SetLayer(RenderLayer::Default);
        }

        RenderDeferredDropdowns();
        Render();
        // Backend-agnostic present (same path as secondary windows): GL swaps this
        // window's buffers; Vulkan already presented inside Render() (EndFrame).
        ctx_->renderer.Present();

        // brief 09 Fase 3: update re-dock candidate / perform re-dock on release.
        updateFloatingRedock();

        // Process secondary windows
        for (auto it = secondaryWindows_.begin(); it != secondaryWindows_.end(); ) {
            if ((*it)->isOpen()) {
                (*it)->processFrame(dt, window_, mainGLContext_);
                ++it;
            } else {
                // Make sure we're in the right GL context before destroying
                it = secondaryWindows_.erase(it);
            }
        }

        // Restore main GL context after secondary windows
        if (mainGLContext_) SDL_GL_MakeCurrent(window_, mainGLContext_); // GL-only; null on Vulkan
        SetCurrentContext(ctx_);

        // FPS cap
        if (targetFPS_ > 0) {
            uint64_t frameEnd = SDL_GetTicks();
            uint64_t elapsed = frameEnd - (lastTime_ - static_cast<uint64_t>(dt * 1000.0f));
            uint64_t targetMs = 1000 / static_cast<uint64_t>(targetFPS_);
            if (elapsed < targetMs) {
                SDL_Delay(static_cast<Uint32>(targetMs - elapsed));
            }
        }
    }
}

void FluentApp::quit() {
    running_ = false;
}

void FluentApp::onInit(std::function<void()> fn) {
    initFn_ = std::move(fn);
}

void FluentApp::onShutdown(std::function<void()> fn) {
    shutdownFn_ = std::move(fn);
}

void FluentApp::onUpdate(std::function<void(float dt)> fn) {
    updateFn_ = std::move(fn);
}

void FluentApp::onFileDrop(std::function<void(const std::vector<std::string>&)> fn) {
    fileDropFn_ = std::move(fn);
}

void FluentApp::setTheme(const Style& style) {
    if (ctx_) ctx_->style = style;
}

void FluentApp::setTargetFPS(int fps) {
    targetFPS_ = fps;
}

void FluentApp::enableDarkMode(bool dark) {
    if (ctx_) {
        ctx_->style = dark ? GetDarkFluentStyle() : GetDefaultFluentStyle();
    }
}

float FluentApp::computeDelta() {
    uint64_t now = SDL_GetTicks();
    float dt = static_cast<float>(now - lastTime_) / 1000.0f;
    dt = std::min(dt, 0.1f);
    lastTime_ = now;
    return dt;
}

// --- External-window integration ---

void FluentApp::processEvent(const SDL_Event& e) {
    if (!ctx_ || !window_) return;

    if (e.type == SDL_EVENT_WINDOW_RESIZED) {
        int w, h;
        SDL_GetWindowSize(window_, &w, &h);
        ctx_->renderer.SetViewport(w, h);
        if (enableDPI_) updateDPIScale();
    } else if (e.type == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED) {
        if (enableDPI_) updateDPIScale();
    } else if (e.type == SDL_EVENT_WINDOW_MOVED) {
        if (enableDPI_) updateDPIScale();
    } else {
        ctx_->input.ProcessEvent(e);
    }
}

void FluentApp::beginFrame(float dt) {
    if (!initialized_ || !ctx_ || !window_) return;

    if (mainGLContext_) SDL_GL_MakeCurrent(window_, mainGLContext_); // GL-only; null on Vulkan
    SetCurrentContext(ctx_);

    ctx_->input.Update(window_);

    // Re-sync viewport
    {
        int w, h;
        SDL_GetWindowSize(window_, &w, &h);
        Vec2 vp = ctx_->renderer.GetViewportSize();
        if (w > 0 && h > 0 && (static_cast<int>(vp.x) != w || static_cast<int>(vp.y) != h)) {
            ctx_->renderer.SetViewport(w, h);
        }
    }

    if (updateFn_) updateFn_(dt);

    if (fileDropFn_ && ctx_->input.HasDroppedFiles()) {
        fileDropFn_(ctx_->input.DroppedFiles());
    }

    NewFrame(dt);
    shortcuts.ProcessFrame(ctx_->input);

    if (!ctx_->dockSpace.IsEmpty()) {
        ctx_->dockSpace.HandleInteraction(
            ctx_->input.MouseX(), ctx_->input.MouseY(),
            ctx_->input.IsMouseDown(0), ctx_->input.IsMousePressed(0));
    }

    if (rootBuilder_) {
        UIBuilder builder(ctx_);
        rootBuilder_(builder);
        builder.debugOverlay();
    }
}

void FluentApp::endFrame() {
    if (!initialized_ || !ctx_) return;

    RenderDeferredDropdowns();
    Render();
    // NOTE: does NOT call SDL_GL_SwapWindow — the caller's engine does that.
}

// --- Layout Serialization ---

bool FluentApp::saveLayout(const std::string& filepath) {
    if (!ctx_) return false;
    // Phase E5: include detached viewports.
    return LayoutSerializer::SaveLayout(filepath, ctx_->dockSpace, ctx_, getViewports());
}

bool FluentApp::loadLayout(const std::string& filepath) {
    if (!ctx_) return false;
    // Phase E5: populate pendingViewports_ for the caller to restore.
    return LayoutSerializer::LoadLayout(filepath, ctx_->dockSpace, ctx_, &pendingViewports_);
}

// --- Multi-Window ---

AppWindow* FluentApp::createWindow(const std::string& title, int width, int height) {
    // Ensure main GL context is current before creating a child
    if (mainGLContext_) SDL_GL_MakeCurrent(window_, mainGLContext_); // GL-only; null on Vulkan
    SetCurrentContext(ctx_);

    std::unique_ptr<AppWindow> win(new AppWindow(title, width, height, window_, mainGLContext_, ctx_));
    if (!win->window()) return nullptr;

    // Double-check main GL context is current after window creation
    if (mainGLContext_) SDL_GL_MakeCurrent(window_, mainGLContext_); // GL-only; null on Vulkan
    SetCurrentContext(ctx_);

    AppWindow* ptr = win.get();
    secondaryWindows_.push_back(std::move(win));
    return ptr;
}

void FluentApp::closeWindow(AppWindow* win) {
    if (!win) return;
    win->close();
}

// Phase E: Multi-viewport — detach a docked panel into its own OS-window.
AppWindow* FluentApp::detachPanelToWindow(const std::string& panelId,
                                          std::function<void(UIBuilder&)> buildFn,
                                          int x, int y, int width, int height) {
    if (!ctx_) return nullptr;
    // Remove from main dock layout (UndockPanel restores it to floating; we then
    // remove from the dock root entirely by making the dock space drop the leaf).
    if (ctx_->dockSpace.IsPanelDocked(panelId)) {
        ctx_->dockSpace.UndockPanel(panelId);
    }

    AppWindow* win = createWindow(panelId, width, height);
    if (!win) return nullptr;
    if (win->window()) {
        SDL_SetWindowPosition(win->window(), x, y);
    }
    // Phase E5: tag the window with its panel id so it can be serialized later.
    win->panelId_ = panelId;
    win->root(std::move(buildFn));
    return win;
}

void FluentApp::setOnPanelDragOut(std::function<void(const std::string&, int, int)> cb) {
    onPanelDragOut_ = std::move(cb);
    if (ctx_) ctx_->dockDrag.onPanelDragOut = onPanelDragOut_;
}

// brief 09 Fase 2 — panel builder registry + auto-detach orchestration.

void FluentApp::registerPanelBuilder(const std::string& panelId,
                                     std::function<void(UIBuilder&)> builder) {
    panelBuilders_[panelId] = std::move(builder);
}

AppWindow* FluentApp::autoDetachPanel(const std::string& panelId, int screenX, int screenY) {
    auto it = panelBuilders_.find(panelId);
    if (it == panelBuilders_.end()) {
        Log(LogLevel::Warning, "autoDetachPanel: no builder registered for panel '%s' — "
            "call registerPanelBuilder() first", panelId.c_str());
        return nullptr;
    }
    // Size: keep the panel's docked size if known, else a sensible default.
    Rect bounds = ctx_ ? ctx_->dockSpace.GetPanelBounds(panelId) : Rect{};
    int w = bounds.size.x > 32 ? static_cast<int>(bounds.size.x) : 480;
    int h = bounds.size.y > 32 ? static_cast<int>(bounds.size.y) : 360;
    // Place the window so the (custom) titlebar sits under the cursor.
    int x = screenX - 24;
    int y = screenY - 12;

    std::function<void(UIBuilder&)> content = it->second;

    AppWindow* win = detachPanelToWindow(panelId, /*placeholder*/ content, x, y, w, h);
    if (!win) return nullptr;

    // Wrap the content with a draggable custom titlebar so the floating window can
    // be moved and dropped back onto a dock zone (re-dock handled in the main loop).
    AppWindow* rawWin = win;
    std::string pid = panelId;
    win->root([rawWin, pid, content](UIBuilder& ui) {
        UIContext* c = rawWin->context();
        if (!c) return;
        const float vw = c->renderer.GetViewportSize().x;
        const float titleH = 28.0f;

        // Titlebar background + label.
        c->renderer.DrawRectFilled({0.0f, 0.0f}, {vw, titleH},
                                   c->style.panel.headerBackground);
        c->renderer.DrawText({8.0f, 6.0f}, pid, c->style.panel.headerText.color,
                             c->style.panel.headerText.fontSize);

        // Titlebar drag → move the OS window following the global mouse and flag the
        // drag so the main loop can test re-dock zones.
        const float mx = c->input.MouseX(), my = c->input.MouseY();
        const bool overTitle = (my >= 0.0f && my <= titleH && mx >= 0.0f && mx <= vw);
        const bool pressed = c->input.IsMousePressed(0);
        const bool down = c->input.IsMouseDown(0);
        if (overTitle && pressed && rawWin->window_) {
            float gx = 0, gy = 0; SDL_GetGlobalMouseState(&gx, &gy);
            int wx = 0, wy = 0; SDL_GetWindowPosition(rawWin->window_, &wx, &wy);
            rawWin->titleDragging_ = true;
            rawWin->titleDragDX_ = static_cast<int>(gx) - wx;
            rawWin->titleDragDY_ = static_cast<int>(gy) - wy;
            c->desiredCursor = UIContext::CursorType::Hand;
        }
        if (rawWin->titleDragging_ && down && rawWin->window_) {
            float gx = 0, gy = 0; SDL_GetGlobalMouseState(&gx, &gy);
            SDL_SetWindowPosition(rawWin->window_,
                                  static_cast<int>(gx) - rawWin->titleDragDX_,
                                  static_cast<int>(gy) - rawWin->titleDragDY_);
            c->desiredCursor = UIContext::CursorType::Hand;
        }
        if (!down) rawWin->titleDragging_ = false;

        // Content below the titlebar.
        c->cursorPos = {8.0f, titleH + 6.0f};
        if (content) content(ui);
    });
    return win;
}

void FluentApp::enableAutoDetach(bool enable) {
    autoDetach_ = enable;
    if (!enable) {
        setOnPanelDragOut(nullptr);
        return;
    }
    // Wire the drag-out callback to spawn a floating window from the registry.
    setOnPanelDragOut([this](const std::string& panelId, int sx, int sy) {
        autoDetachPanel(panelId, sx, sy);
    });
}

void FluentApp::updateFloatingRedock() {
    if (!autoDetach_ || !ctx_ || !window_) return;

    // Find a floating panel window currently being titlebar-dragged.
    AppWindow* dragging = nullptr;
    for (auto& w : secondaryWindows_) {
        if (w && w->open_ && !w->panelId_.empty() && w->titleDragging_) {
            dragging = w.get();
            break;
        }
    }

    if (dragging) {
        float gx = 0, gy = 0; SDL_GetGlobalMouseState(&gx, &gy);
        int mwx = 0, mwy = 0; SDL_GetWindowPosition(window_, &mwx, &mwy);
        int mww = 0, mwh = 0; SDL_GetWindowSize(window_, &mww, &mwh);
        const float lx = gx - mwx, ly = gy - mwy;

        redockWin_ = dragging;
        redockActive_ = false;
        redockZone_ = DockPosition::Float;
        redockTargetId_.clear();

        const bool insideMain = (lx >= 0 && ly >= 0 && lx <= mww && ly <= mwh);
        if (insideMain) {
            auto panels = ctx_->dockSpace.GetDockedPanels();
            for (auto& pid : panels) {
                if (pid == dragging->panelId_) continue;
                Rect b = ctx_->dockSpace.GetPanelBounds(pid);
                DockPosition z = HitTestDockZones(b, lx, ly);
                if (z != DockPosition::Float) {
                    redockZone_ = z;
                    redockTargetId_ = pid;
                    redockActive_ = true;
                    break;
                }
            }
            // Empty dock space: any drop inside re-docks as the root panel.
            if (!redockActive_ && panels.empty()) {
                redockZone_ = DockPosition::Center;
                redockActive_ = true;
            }
        }
        return;
    }

    // Not dragging this frame: if we had an active zone candidate, the user just
    // released over it → re-dock and destroy the floating window. (Skip if the
    // window was closed via its X in the meantime.)
    if (redockWin_ && redockActive_ && redockWin_->open_) {
        std::string pid = redockWin_->panelId_;
        ctx_->dockSpace.DockPanel(pid, redockZone_, redockTargetId_);
        Rect area = ctx_->dockSpace.GetAvailableArea();
        if (area.size.x > 0 && area.size.y > 0) ctx_->dockSpace.ComputeLayout(area);
        // Re-registering a builder is already done; the panel now renders in-dock.
        redockWin_->close(); // erased by the main loop's cleanup pass
    }
    redockWin_ = nullptr;
    redockActive_ = false;
}

std::vector<ViewportInfo> FluentApp::getViewports() const {
    std::vector<ViewportInfo> out;
    out.reserve(secondaryWindows_.size());
    for (const auto& w : secondaryWindows_) {
        if (!w || w->panelId().empty() || !w->window_) continue;
        ViewportInfo v;
        v.panelId = w->panelId();
        int x = 0, y = 0, ww = 0, hh = 0;
        SDL_GetWindowPosition(w->window_, &x, &y);
        SDL_GetWindowSize(w->window_, &ww, &hh);
        v.x = x; v.y = y;
        v.width = ww > 0 ? ww : v.width;
        v.height = hh > 0 ? hh : v.height;
        out.push_back(v);
    }
    return out;
}

void FluentApp::restoreViewports(
    std::function<std::function<void(UIBuilder&)>(const std::string&)> factory) {
    if (!factory) return;
    for (const auto& v : pendingViewports_) {
        auto buildFn = factory(v.panelId);
        if (!buildFn) continue;
        detachPanelToWindow(v.panelId, std::move(buildFn), v.x, v.y, v.width, v.height);
    }
    pendingViewports_.clear();
}

// --- DPI Scaling ---

float FluentApp::getDPIScale() const {
    return ctx_ ? ctx_->dpiScale : 1.0f;
}

void FluentApp::setDPIScale(float scale) {
    if (!ctx_) return;
    ctx_->dpiScale = std::max(0.5f, std::min(4.0f, scale));
    ctx_->renderer.SetDPIScale(ctx_->dpiScale); // Phase A4
}

void FluentApp::updateDPIScale() {
    if (!ctx_ || !window_) return;
    float scale = SDL_GetWindowDisplayScale(window_);
    if (scale <= 0.0f) scale = 1.0f;
    ctx_->dpiScale = scale;
    ctx_->renderer.SetDPIScale(scale); // Phase A4
}

// --- Dock Space Access ---

DockSpace& FluentApp::dockSpace() {
    return ctx_->dockSpace;
}

// --- Debug Overlay (Phase C) ---

void FluentApp::showDebugOverlay(bool show) {
    if (ctx_) ctx_->showDebugOverlay = show;
}

bool FluentApp::isDebugOverlayVisible() const {
    return ctx_ ? ctx_->showDebugOverlay : false;
}

} // namespace FluentUI
