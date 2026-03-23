#include "core/FluentApp.h"
#include "core/Context.h"
#include "core/DockSystem.h"
#include "core/LayoutSerializer.h"
#include "core/OpenGLBackend.h"
#include "UI/Widgets.h"
#include "Theme/FluentTheme.h"
#include <algorithm>
#include <iostream>

namespace FluentUI {

// ===================== AppWindow (Secondary Windows) =====================

AppWindow::AppWindow(const std::string& title, int width, int height,
                     SDL_Window* parentWindow, SDL_GLContext parentGLContext) {
    // Create the SDL window
    window_ = SDL_CreateWindow(title.c_str(), width, height,
                                SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window_) {
        Log(LogLevel::Error, "AppWindow: Failed to create window: %s", SDL_GetError());
        return;
    }

    // Save the current global context so we can restore it after
    UIContext* savedGlobalCtx = GetContext();

    // IMPORTANT: Set share flag BEFORE CreateStandaloneContext calls
    // OpenGLBackend::Init -> SDL_GL_CreateContext, so the new GL context
    // shares resources (textures, buffers) with the parent.
    SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);

    // Create standalone context (this creates its own OpenGLBackend + GL context)
    // It will change the current GL context as a side effect.
    ctx_ = CreateStandaloneContext(window_, &backend_);
    if (!ctx_) {
        Log(LogLevel::Error, "AppWindow: Failed to create standalone context");
        SDL_DestroyWindow(window_);
        window_ = nullptr;
        SetCurrentContext(savedGlobalCtx);
        SDL_GL_MakeCurrent(parentWindow, parentGLContext);
        return;
    }

    // Grab the GL context that was created by CreateStandaloneContext -> OpenGLBackend::Init
    glContext_ = SDL_GL_GetCurrentContext();

    // Sync viewport
    ctx_->renderer.SetViewport(width, height);

    SDL_StartTextInput(window_);
    open_ = true;

    // Initialize DPI for this window's display
    updateDPIScale();

    // Reset the share attribute for future contexts
    SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 0);

    // CRITICAL: Restore the parent window's GL context and global context
    // so the main window can continue rendering normally
    SDL_GL_MakeCurrent(parentWindow, parentGLContext);
    SetCurrentContext(savedGlobalCtx);
}

AppWindow::~AppWindow() {
    // Make this window's GL context current so GL resource cleanup works
    if (window_ && glContext_) {
        SDL_GL_MakeCurrent(window_, glContext_);
    }

    // Destroy context (renderer first, then backend) while GL context is current
    if (ctx_) {
        DestroyStandaloneContext(ctx_, backend_);
        ctx_ = nullptr;
        backend_ = nullptr;
    } else if (backend_) {
        // Edge case: backend exists without context (partial init failure)
        backend_->Shutdown();
        delete backend_;
        backend_ = nullptr;
    }

    if (glContext_) {
        SDL_GL_DestroyContext(glContext_);
        glContext_ = nullptr;
    }

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
}

void AppWindow::processFrame(float dt, SDL_Window* parentWindow, SDL_GLContext parentGLContext) {
    if (!open_ || !ctx_ || !window_ || !glContext_) return;

    // Switch to this window's GL context
    SDL_GL_MakeCurrent(window_, glContext_);

    // Set this context as the active global context
    UIContext* savedCtx = GetContext();
    SetCurrentContext(ctx_);

    // Update input
    ctx_->input.Update(window_);

    // Begin frame
    NewFrame(dt);

    // Build UI via lambda
    if (rootBuilder_) {
        UIBuilder builder(ctx_);
        rootBuilder_(builder);
    }

    // Render
    RenderDeferredDropdowns();
    Render();
    SDL_GL_SwapWindow(window_);

    // Restore parent window's GL context and global context
    SDL_GL_MakeCurrent(parentWindow, parentGLContext);
    SetCurrentContext(savedCtx);
}

// ===================== FluentApp =====================

FluentApp::FluentApp(const std::string& title, const AppConfig& config)
    : targetFPS_(config.targetFPS), enableDPI_(config.enableDPI) {

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        Log(LogLevel::Error, "FluentApp: SDL_Init failed: %s", SDL_GetError());
        return;
    }

    Uint64 flags = SDL_WINDOW_OPENGL;
    if (config.resizable) flags |= SDL_WINDOW_RESIZABLE;
    flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;

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

    // Ensure the provided GL context is current before creating UIContext
    SDL_GL_MakeCurrent(window_, mainGLContext_);

    ctx_ = CreateContext(window_);
    if (!ctx_) {
        Log(LogLevel::Error, "FluentApp: Failed to create UIContext");
        window_ = nullptr;
        return;
    }

    ctx_->style = darkMode ? GetDarkFluentStyle() : GetDefaultFluentStyle();

    int w, h;
    SDL_GetWindowSize(window_, &w, &h);
    ctx_->renderer.SetViewport(w, h);

    if (enableDPI_) {
        updateDPIScale();
    }

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
    }
    // If !ownsWindow_, the caller owns the window/SDL — don't touch them
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
        SDL_GL_MakeCurrent(window_, mainGLContext_);
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
                if (e.type == SDL_EVENT_WINDOW_RESIZED) {
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
        SDL_GL_MakeCurrent(window_, mainGLContext_);
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

        RenderDeferredDropdowns();
        Render();
        SDL_GL_SwapWindow(window_);

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
        SDL_GL_MakeCurrent(window_, mainGLContext_);
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

    SDL_GL_MakeCurrent(window_, mainGLContext_);
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
    return LayoutSerializer::SaveLayout(filepath, ctx_->dockSpace, ctx_);
}

bool FluentApp::loadLayout(const std::string& filepath) {
    if (!ctx_) return false;
    return LayoutSerializer::LoadLayout(filepath, ctx_->dockSpace, ctx_);
}

// --- Multi-Window ---

AppWindow* FluentApp::createWindow(const std::string& title, int width, int height) {
    // Ensure main GL context is current before creating a child
    SDL_GL_MakeCurrent(window_, mainGLContext_);
    SetCurrentContext(ctx_);

    std::unique_ptr<AppWindow> win(new AppWindow(title, width, height, window_, mainGLContext_));
    if (!win->window()) return nullptr;

    // Double-check main GL context is current after window creation
    SDL_GL_MakeCurrent(window_, mainGLContext_);
    SetCurrentContext(ctx_);

    AppWindow* ptr = win.get();
    secondaryWindows_.push_back(std::move(win));
    return ptr;
}

void FluentApp::closeWindow(AppWindow* win) {
    if (!win) return;
    win->close();
}

// --- DPI Scaling ---

float FluentApp::getDPIScale() const {
    return ctx_ ? ctx_->dpiScale : 1.0f;
}

void FluentApp::setDPIScale(float scale) {
    if (!ctx_) return;
    ctx_->dpiScale = std::max(0.5f, std::min(4.0f, scale));
}

void FluentApp::updateDPIScale() {
    if (!ctx_ || !window_) return;
    float scale = SDL_GetWindowDisplayScale(window_);
    if (scale <= 0.0f) scale = 1.0f;
    ctx_->dpiScale = scale;
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
