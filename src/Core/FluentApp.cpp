#include "core/FluentApp.h"
#include "core/Context.h"
#include "core/PlatformBackend.h" // brief 25: platform seam (SDL lives behind this)
#include "core/DockSystem.h"
#include "core/LayoutSerializer.h"
#ifdef FLUENTUI_HAS_GL
#include "core/OpenGLBackend.h"  // GL multi-window path; unused symbols under GL=OFF
#endif
#include "core/Accessibility.h"   // brief 18.3: InitUIAutomation/ShutdownUIAutomation
#include "UI/Widgets.h"
#include "Theme/FluentTheme.h"
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>

namespace FluentUI {

// brief 13/25: neutral hit-test callback for borderless windows with a custom
// title bar. The platform installs it (SetWindowHitTest) and calls it during the
// OS event pump with a point in window (logical) coordinates — the same space the
// renderer viewport and widget rects use, so no DPI conversion is needed. This is
// SDL-free: the platform translates the UIHitTest result to its native form.
// Declared in PlatformBackend.h so standalone SDL hosts can install it too.
UIHitTest CustomTitleBarHitTest(int px, int py, void* user) {
    UIContext* ctx = static_cast<UIContext*>(user);
    if (!ctx) return UIHitTest::Normal;

    // Window size == renderer viewport (kept in sync on resize), same space as the
    // hit-test point. Avoids a platform round-trip from inside the callback.
    Vec2 vp = ctx->renderer.GetViewportSize();
    int w = static_cast<int>(vp.x), h = static_cast<int>(vp.y);

    std::lock_guard<std::mutex> lk(ctx->titleBarHit.mutex);
    TitleBarHitRegions& tb = ctx->titleBarHit;
    float fx = static_cast<float>(px), fy = static_cast<float>(py);

    // Resize borders take precedence at the window edges.
    if (tb.resizable && tb.resizeBorder > 0.0f) {
        float b = tb.resizeBorder;
        bool left = fx < b, right = fx > (float)w - b;
        bool top = fy < b, bottom = fy > (float)h - b;
        if (top && left) return UIHitTest::ResizeTopLeft;
        if (top && right) return UIHitTest::ResizeTopRight;
        if (bottom && left) return UIHitTest::ResizeBottomLeft;
        if (bottom && right) return UIHitTest::ResizeBottomRight;
        if (top) return UIHitTest::ResizeTop;
        if (bottom) return UIHitTest::ResizeBottom;
        if (left) return UIHitTest::ResizeLeft;
        if (right) return UIHitTest::ResizeRight;
    }

    // Draggable caption strip, minus the interactive exclusions (caption buttons,
    // center content) which must stay clickable.
    if (tb.active && tb.caption.Contains(Vec2(fx, fy))) {
        for (const Rect& ex : tb.exclusions)
            if (ex.Contains(Vec2(fx, fy))) return UIHitTest::Normal;
        return UIHitTest::Draggable;
    }
    return UIHitTest::Normal;
}

namespace {

// brief 18.7: deliver positioned OS drops (files / text) to the context's sinks.
// Each context owns its own InputState, so this routes per-window already; the
// drop position is in window coordinates (same space as widget rects).
void dispatchOSDrops(UIContext* ctx) {
    if (!ctx) return;
    Vec2 pos(ctx->input.DropX(), ctx->input.DropY());
    if (ctx->onFilesDropped && ctx->input.HasDroppedFiles()) {
        ctx->onFilesDropped(ctx->input.DroppedFiles(), pos);
    }
    if (ctx->onTextDropped && ctx->input.HasDroppedText()) {
        ctx->onTextDropped(ctx->input.DroppedText(), pos);
    }
}

// brief 18.3/25: attach the platform accessibility provider to an OS window. Pulls
// the native HWND from the platform on Windows (as an opaque void*, so this TU
// needs no <windows.h> and no SDL); no-op elsewhere.
void AttachAccessibility(PlatformBackend* platform, WindowHandle window, UIContext* ctx) {
#ifdef _WIN32
    if (!platform || !window) return;
    void* hwnd = platform->GetNativeWindowHandle(window);
    if (hwnd) InitUIAutomation(hwnd, ctx);
#else
    (void)platform; (void)window; (void)ctx;
#endif
}
} // namespace

// ===================== AppWindow (Secondary Windows) =====================

AppWindow::AppWindow(const std::string& title, int width, int height,
                     WindowHandle parentWindow, void* parentGLContext,
                     UIContext* parentCtx, PlatformBackend* platform)
    : platform_(platform) {
    // brief 09 Fase 1: create the OS window with the flag matching the active
    // backend (chosen internally by the platform). Same gate as the main window.
    window_ = platform_->CreateWindowHandle(title.c_str(), width, height, UIWindow_Resizable);
    if (!window_) {
        Log(LogLevel::Error, "AppWindow: Failed to create window");
        return;
    }

    // Save the current global context so we can restore it after.
    UIContext* savedGlobalCtx = GetContext();

    // brief 08/09: share the parent's GPU device / GL context + resource pool.
    // (GL: reuses the SAME GL context; Vulkan: surface+swapchain over the shared
    // device.) CreateStandaloneContext makes the new context current as a side effect.
    ctx_ = CreateStandaloneContext(window_, parentCtx, &backend_);
    if (ctx_) ctx_->platform = platform_; // brief 26: widgets reach OS services via GetPlatform(ctx)
    if (!ctx_) {
        Log(LogLevel::Error, "AppWindow: Failed to create shared context");
        platform_->DestroyWindowHandle(window_);
        window_ = nullptr;
        SetCurrentContext(savedGlobalCtx);
        platform_->MakeContextCurrent(parentWindow, parentGLContext);
        return;
    }

    // Remember the shared GL context (parent's) for GL resource cleanup later.
    // Null on Vulkan (no current GL context).
    sharedGLContext_ = platform_->GetCurrentGLContext();

    ctx_->renderer.SetViewport(width, height);
    // brief 18.4: IME is now claimed per text field on focus (see TextInput), not
    // globally at startup, so the candidate window only appears over the focused
    // field. Stale StopTextInput in the destructor is harmless if never started.
    open_ = true;
    updateDPIScale();

    // Restore the parent window's GL context and global context so the main window
    // keeps rendering normally.
    platform_->MakeContextCurrent(parentWindow, parentGLContext);
    SetCurrentContext(savedGlobalCtx);
}

AppWindow::~AppWindow() {
    // brief 09: this window shares the parent's GL context (it does NOT own one).
    // Make that shared context current on THIS window so GL resource cleanup (the
    // window's own atlas textures, buffers) targets the right context. On Vulkan
    // sharedGLContext_ is null and DeleteTexture drains the device itself.
    if (window_ && sharedGLContext_) {
        platform_->MakeContextCurrent(window_, sharedGLContext_);
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
        platform_->StopTextInput(window_);
        platform_->DestroyWindowHandle(window_);
        window_ = nullptr;
    }
}

void AppWindow::root(std::function<void(UIBuilder&)> buildFn) {
    rootBuilder_ = std::move(buildFn);
}

void AppWindow::routeEvent(const UIEvent& e) {
    if (!ctx_) return;

    if (e.type == UIEventType::Resize) {
        int w, h;
        platform_->GetFramebufferSize(window_, w, h);
        ctx_->renderer.SetViewport(w, h);
        // Update DPI for this window
        updateDPIScale();
    } else if (e.type == UIEventType::DpiChange) {
        // brief 18.2: window moved/crossed to a different monitor or its display
        // scale changed → recompute the per-monitor DPI for THIS window. MSDF text
        // re-scales without re-rasterizing; immediate-mode re-layouts next frame.
        updateDPIScale();
    } else if (e.type == UIEventType::WindowClose) {
        open_ = false;
    } else {
        ctx_->input.ProcessEvent(e);
    }
}

void AppWindow::updateDPIScale() {
    if (!ctx_ || !window_) return;
    float scale = platform_->GetDpiScale(window_);
    if (scale <= 0.0f) scale = 1.0f;
    ctx_->dpiScale = scale;
    ctx_->renderer.SetDPIScale(scale); // Phase A4: keep AA fringe at 1 physical pixel
}

void AppWindow::processFrame(float dt, WindowHandle parentWindow, void* parentGLContext) {
    if (!open_ || !ctx_ || !window_) return;

    // brief 09 Fase 1: backend-agnostic. Make this context active; the backend
    // routes GL to this window inside BeginFrame (GL secondary-window mode) or uses
    // its own swapchain (Vulkan). No raw GL here.
    UIContext* savedCtx = GetContext();
    SetCurrentContext(ctx_);

    ctx_->input.Update(window_);

    dispatchOSDrops(ctx_); // brief 18.7: route OS drops to this window's sinks

    NewFrame(dt);

    if (rootBuilder_) {
        UIBuilder builder(ctx_);
        rootBuilder_(builder);
    }

    RenderDeferredDropdowns();
    Render();
    ctx_->renderer.Present(); // GL: SwapWindow(window_); Vulkan: presented in EndFrame

    // Restore the parent's GL context + global context (GL only; null on Vulkan).
    platform_->MakeContextCurrent(parentWindow, parentGLContext);
    SetCurrentContext(savedCtx);
}

// ===================== FluentApp =====================

namespace {

// Resolve the icon font path: explicit override -> exe_dir/assets/fonts/lucide.ttf
// -> cwd/assets/fonts/lucide.ttf -> $FLUENTUI_ASSETS_DIR/fonts/lucide.ttf.
// Returns empty when nothing exists. We try every location even if some
// std::filesystem queries throw (e.g. permissions) — soft-fail is the contract.
std::string ResolveIconFontPath(const std::string& explicitPath, PlatformBackend* platform) {
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

    // Executable directory (queried via the platform seam).
    std::string exeBase = platform ? platform->GetBasePath() : std::string();
    if (!exeBase.empty()) {
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

void AutoLoadIconFont(UIContext* ctx, const std::string& explicitPath, int pixelHeight,
                      PlatformBackend* platform) {
    if (!ctx) return;
    // The renderer auto-loads a default Lucide font during initialization, so
    // icons already work everywhere. Only act here when the user supplied an
    // explicit path (to override the default) or when nothing got loaded yet.
    if (explicitPath.empty() && ctx->renderer.IsIconFontLoaded()) return;
    std::string path = ResolveIconFontPath(explicitPath, platform);
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

    // brief 25: create + initialize the default (SDL) platform. This initializes
    // SDL; the platform owns SDL and shuts it down when destroyed (after everything
    // else — see the member declaration order in FluentApp.h).
    platform_ = CreateDefaultPlatform();
    if (!platform_) {
        Log(LogLevel::Error, "FluentApp: platform initialization failed");
        return;
    }

    uint32_t flags = UIWindow_HighDPI;
    if (config.resizable) flags |= UIWindow_Resizable;
    // brief 13: borderless chrome so the custom TitleBar() drives the window.
    if (config.useCustomTitleBar) flags |= UIWindow_Borderless;

    window_ = platform_->CreateWindowHandle(title.c_str(), config.width, config.height, flags);
    if (!window_) {
        Log(LogLevel::Error, "FluentApp: window creation failed");
        return; // platform_ dtor tears down SDL
    }

    ctx_ = CreateContext(window_);
    if (ctx_) ctx_->platform = platform_.get(); // brief 26: widgets reach OS services via GetPlatform(ctx)
    if (!ctx_) {
        Log(LogLevel::Error, "FluentApp: Failed to create UIContext");
        platform_->DestroyWindowHandle(window_);
        window_ = nullptr;
        return; // platform_ dtor tears down SDL
    }

    // Cache the main GL context for later restoration after secondary window ops
    mainGLContext_ = platform_->GetCurrentGLContext();

    ctx_->style = config.darkMode ? GetDarkFluentStyle() : GetDefaultFluentStyle();

    AutoLoadIconFont(ctx_, config.iconFontPath, config.iconFontSize, platform_.get());

    // brief 13: install the hit-test so TitleBar() can drag/resize the window.
    if (config.useCustomTitleBar) {
        platform_->SetWindowHitTest(window_, &CustomTitleBarHitTest, ctx_);
    }

    int w, h;
    platform_->GetFramebufferSize(window_, w, h);
    ctx_->renderer.SetViewport(w, h);

    if (enableDPI_) {
        updateDPIScale();
    }

    // brief 18.4: text input is started per focused field (see TextInput), not globally.
    lastTime_ = platform_->GetTicksMs();
    initialized_ = true;

    // brief 18.3: attach the Windows UI Automation provider to the main window so
    // screen readers (Narrator/NVDA) see a FluentUI automation peer. Opt-in
    // (subclasses the window proc). No-op on non-Windows.
    if (config.enableAccessibility) AttachAccessibility(platform_.get(), window_, ctx_);
}

FluentApp::FluentApp(const std::string& title, int width, int height)
    : FluentApp(title, AppConfig{width, height}) {}

FluentApp::FluentApp(WindowHandle externalWindow, void* externalGLContext,
                     bool darkMode, bool enableDPI)
    : enableDPI_(enableDPI), ownsWindow_(false) {

    if (!externalWindow || !externalGLContext) {
        Log(LogLevel::Error, "FluentApp: External window or GL context is null");
        return;
    }

    // brief 25: host-owned SDL — the engine already initialized SDL and owns the
    // window/context. Use a platform that does NOT own SDL (no init/quit).
    platform_ = CreateHostPlatform();

    window_ = externalWindow;
    mainGLContext_ = externalGLContext;

    // Pass the external GL context so the backend reuses it instead of creating a new one
    ctx_ = CreateContext(window_, mainGLContext_);
    if (ctx_) ctx_->platform = platform_.get(); // brief 26: widgets reach OS services via GetPlatform(ctx)
    if (!ctx_) {
        Log(LogLevel::Error, "FluentApp: Failed to create UIContext");
        window_ = nullptr;
        return;
    }

    ctx_->style = darkMode ? GetDarkFluentStyle() : GetDefaultFluentStyle();

    AutoLoadIconFont(ctx_, /*explicitPath=*/std::string(), /*pixelHeight=*/16, platform_.get());

    int w, h;
    platform_->GetFramebufferSize(window_, w, h);
    ctx_->renderer.SetViewport(w, h);

    if (enableDPI_) {
        updateDPIScale();
    }

    // brief 18.4: text input is started per focused field (see TextInput), not globally.
    lastTime_ = platform_->GetTicksMs();
    initialized_ = true;
}

FluentApp::~FluentApp() {
    // brief 18.3: detach the UIA provider and restore the window proc BEFORE the
    // window/context are torn down.
#ifdef _WIN32
    ShutdownUIAutomation();
#endif

    // Destroy secondary windows first
    secondaryWindows_.clear();

    if (shutdownFn_) shutdownFn_();
    if (ctx_) DestroyContext();

    if (ownsWindow_) {
        // We created the window — destroy it via the platform. SDL shutdown runs when
        // platform_ (declared first, destroyed last) is torn down after this body.
        if (window_ && platform_) {
            platform_->StopTextInput(window_);
            platform_->DestroyWindowHandle(window_);
        }
    } else if (window_ && platform_) {
        // External window: leave window/SDL alone but undo the StartTextInput
        // we issued in the external-window constructor.
        platform_->StopTextInput(window_);
    }
}

void FluentApp::root(std::function<void(UIBuilder&)> buildFn) {
    rootBuilder_ = std::move(buildFn);
}

void FluentApp::run() {
    if (!initialized_ || !window_ || !ctx_) {
        Log(LogLevel::Error, "FluentApp: Cannot run — initialization failed");
        return;
    }

    if (initFn_) initFn_();
    // brief 10 Part B: honor the OS "reduce animations" preference at startup.
    SetCurrentContext(ctx_);
    InitMotionFromOS();
    running_ = true;

    while (running_) {
        // Ensure main window GL context is current at frame start
        platform_->MakeContextCurrent(window_, mainGLContext_); // GL-only; no-op on Vulkan
        SetCurrentContext(ctx_);

        // Update main window input
        ctx_->input.Update(window_);

        // brief 10 Part G: idle/wake. When nothing is animating (and there are no
        // secondary windows whose animations this context can't see), block on the
        // event queue instead of spinning at max FPS. The short cap keeps time-based
        // effects (e.g. text caret blink) ticking. No-op for the embedded render path
        // (hosts drive their own loop and never call run()), and skipped whenever an
        // animation is in flight so motion stays smooth.
        if (secondaryWindows_.empty() && !ctx_->AnyAnimationActive()) {
            platform_->WaitEvents(100); // blocks up to 100ms, re-queues the wake event
        }

        // Poll ALL events (already translated to neutral UIEvents, tagged with the
        // target WindowHandle) and route to the correct window.
        UIEvent ui;
        while (platform_->PollEvent(ui)) {
            if (ui.type == UIEventType::Quit) {
                running_ = false;
                break;
            }

            WindowHandle target = ui.window;

            if (target == window_ || target == nullptr) {
                // Main window or global event
                if (ui.type == UIEventType::WindowClose) {
                    // brief 13: custom TitleBar close button posts this for the main
                    // window; the OS X button (non-borderless) sends it too.
                    running_ = false;
                    break;
                } else if (ui.type == UIEventType::Resize) {
                    int w, h;
                    platform_->GetFramebufferSize(window_, w, h);
                    ctx_->renderer.SetViewport(w, h);
                    if (enableDPI_) updateDPIScale();
                } else if (ui.type == UIEventType::DpiChange) {
                    // brief 18.2: display scale changed / moved to another monitor.
                    if (enableDPI_) updateDPIScale();
                } else {
                    ctx_->input.ProcessEvent(ui);
                }
            } else {
                // Route to the correct secondary window (matched by WindowHandle)
                for (auto& win : secondaryWindows_) {
                    if (win->isOpen() && win->window() && win->window() == target) {
                        win->routeEvent(ui);
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
            platform_->GetFramebufferSize(window_, w, h);
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
        // brief 18.7: dispatch positioned OS drops to the context-level sinks.
        dispatchOSDrops(ctx_);

        // Skip the frame while the main window is minimized (0x0 framebuffer): the
        // backend (esp. Vulkan) stalls on a zero-extent swapchain. Events above were
        // already processed (so a restore is picked up); idle briefly to avoid a busy
        // spin. This is what makes the custom TitleBar's minimize button safe.
        {
            int fbw = 0, fbh = 0;
            platform_->GetFramebufferSize(window_, fbw, fbh);
            if (fbw <= 0 || fbh <= 0) {
                platform_->Delay(16);
                continue;
            }
        }

        // Main window frame
        platform_->MakeContextCurrent(window_, mainGLContext_); // GL-only; no-op on Vulkan
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
        platform_->MakeContextCurrent(window_, mainGLContext_); // GL-only; no-op on Vulkan
        SetCurrentContext(ctx_);

        // FPS cap
        if (targetFPS_ > 0) {
            uint64_t frameEnd = platform_->GetTicksMs();
            uint64_t elapsed = frameEnd - (lastTime_ - static_cast<uint64_t>(dt * 1000.0f));
            uint64_t targetMs = 1000 / static_cast<uint64_t>(targetFPS_);
            if (elapsed < targetMs) {
                platform_->Delay(static_cast<uint32_t>(targetMs - elapsed));
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
    uint64_t now = platform_->GetTicksMs();
    float dt = static_cast<float>(now - lastTime_) / 1000.0f;
    dt = std::min(dt, 0.1f);
    lastTime_ = now;
    return dt;
}

// --- External-window integration ---

void FluentApp::processEvent(const UIEvent& e) {
    if (!ctx_ || !window_) return;

    if (e.type == UIEventType::Resize) {
        int w, h;
        platform_->GetFramebufferSize(window_, w, h);
        ctx_->renderer.SetViewport(w, h);
        if (enableDPI_) updateDPIScale();
    } else if (e.type == UIEventType::DpiChange) {
        if (enableDPI_) updateDPIScale();
    } else {
        ctx_->input.ProcessEvent(e);
    }
}

void FluentApp::beginFrame(float dt) {
    if (!initialized_ || !ctx_ || !window_) return;

    platform_->MakeContextCurrent(window_, mainGLContext_); // GL-only; no-op on Vulkan
    SetCurrentContext(ctx_);

    ctx_->input.Update(window_);

    // Re-sync viewport
    {
        int w, h;
        platform_->GetFramebufferSize(window_, w, h);
        Vec2 vp = ctx_->renderer.GetViewportSize();
        if (w > 0 && h > 0 && (static_cast<int>(vp.x) != w || static_cast<int>(vp.y) != h)) {
            ctx_->renderer.SetViewport(w, h);
        }
    }

    if (updateFn_) updateFn_(dt);

    if (fileDropFn_ && ctx_->input.HasDroppedFiles()) {
        fileDropFn_(ctx_->input.DroppedFiles());
    }
    dispatchOSDrops(ctx_); // brief 18.7

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
    // NOTE: does NOT swap/present buffers — the caller's engine does that.
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
    platform_->MakeContextCurrent(window_, mainGLContext_); // GL-only; no-op on Vulkan
    SetCurrentContext(ctx_);

    std::unique_ptr<AppWindow> win(
        new AppWindow(title, width, height, window_, mainGLContext_, ctx_, platform_.get()));
    if (!win->window()) return nullptr;

    // Double-check main GL context is current after window creation
    platform_->MakeContextCurrent(window_, mainGLContext_); // GL-only; no-op on Vulkan
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
        platform_->SetWindowPosition(win->window(), x, y);
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
    PlatformBackend* plat = platform_.get();
    win->root([rawWin, pid, content, plat](UIBuilder& ui) {
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
            float gx = 0, gy = 0; plat->GetGlobalMousePos(gx, gy);
            int wx = 0, wy = 0; plat->GetWindowPosition(rawWin->window_, wx, wy);
            rawWin->titleDragging_ = true;
            rawWin->titleDragDX_ = static_cast<int>(gx) - wx;
            rawWin->titleDragDY_ = static_cast<int>(gy) - wy;
            c->desiredCursor = UIContext::CursorType::Hand;
        }
        if (rawWin->titleDragging_ && down && rawWin->window_) {
            float gx = 0, gy = 0; plat->GetGlobalMousePos(gx, gy);
            plat->SetWindowPosition(rawWin->window_,
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
        float gx = 0, gy = 0; platform_->GetGlobalMousePos(gx, gy);
        int mwx = 0, mwy = 0; platform_->GetWindowPosition(window_, mwx, mwy);
        int mww = 0, mwh = 0; platform_->GetFramebufferSize(window_, mww, mwh);
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
        platform_->GetWindowPosition(w->window_, x, y);
        platform_->GetFramebufferSize(w->window_, ww, hh);
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
    if (!ctx_ || !window_ || !platform_) return;
    float scale = platform_->GetDpiScale(window_);
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
