#include "core/Context.h"
#include "core/Renderer.h"
#include "core/OpenGLBackend.h"
#include "core/VulkanBackend.h"
#include "core/SharedResourcePool.h"
#include "Theme/FluentTheme.h"
#include <cassert>
#if defined(_WIN32)
// brief 10 Part B: query the OS "reduce animations" preference without pulling in
// <windows.h> here (it would inject the min/max and DrawText macros that clash with
// std::min/std::max and Renderer::DrawText across this TU). Forward-declare the one
// API we need. SPI_GETCLIENTAREAANIMATION == 0x1042.
extern "C" __declspec(dllimport) int __stdcall SystemParametersInfoA(
    unsigned int uiAction, unsigned int uiParam, void* pvParam, unsigned int fWinIni);
#endif

namespace FluentUI {

    // Logging system
    static LogCallback g_logCallback = nullptr;

    // Backend selection (defaults to OpenGL)
    static RenderBackendType g_preferredBackend = RenderBackendType::OpenGL;

    void SetPreferredBackend(RenderBackendType type) { g_preferredBackend = type; }
    RenderBackendType GetPreferredBackend() { return g_preferredBackend; }

    // Factory: instantiate the configured backend (not yet initialized).
    static RenderBackend* CreateBackendInstance() {
        switch (g_preferredBackend) {
            case RenderBackendType::Vulkan: return new VulkanBackend();
            case RenderBackendType::OpenGL:
            default:                        return new OpenGLBackend();
        }
    }

    void SetLogCallback(LogCallback callback) {
        g_logCallback = std::move(callback);
    }

    LogCallback GetLogCallback() {
        return g_logCallback;
    }

    void Log(LogLevel level, const char* fmt, ...) {
        char buf[1024];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);

        if (g_logCallback) {
            g_logCallback(level, buf);
        } else {
            // Default: print to stderr for warnings/errors, stdout for info/debug
            if (level == LogLevel::Error || level == LogLevel::Warning) {
                std::cerr << buf << std::endl;
            }
        }
    }

    static UIContext* g_ctx = nullptr;
    static RenderBackend* g_backend = nullptr;

    // ─── brief 10 Part B — motion policy ──────────────────────────────────────────
    // Single point that scales / disables animation durations. Called from header
    // inlines (AnimatedValue/SpringValue) so it must tolerate a null context (unit
    // tests build AnimatedValue without ever calling CreateContext).
    float MotionDuration(float base) {
        if (!g_ctx) return base;
        const MotionConfig& m = g_ctx->motion;
        if (!m.enabled || m.reduceMotion) return 0.0f;
        return base * m.durationScale;
    }

    void InitMotionFromOS() {
        if (!g_ctx) return;
#if defined(_WIN32)
        int clientAreaAnim = 1; // default: animations on
        if (SystemParametersInfoA(0x1042 /*SPI_GETCLIENTAREAANIMATION*/, 0,
                                  &clientAreaAnim, 0)) {
            g_ctx->motion.reduceMotion = (clientAreaAnim == 0);
        }
#else
        // No portable SDL query for "reduce motion"; leave the default (false). Hosts
        // can set g_ctx->motion.reduceMotion themselves from their platform hook.
#endif
    }

    // brief 10 Part D: drive a managed overlay's enter/exit transition. The overlay
    // calls this every frame passing active=open; see PresenceResult docs.
    PresenceResult BeginPresence(UIContext* ctx, uint32_t nodeId, bool active,
                                 float enterResponse, float exitResponse) {
        if (!ctx) return { active ? 1.0f : 0.0f, active, false };
        // Avoid creating an entry for an overlay that is closed and has no pending
        // exit fade (the common "exists but not open" case queried every frame).
        if (!active && ctx->presenceStates.find(nodeId) == ctx->presenceStates.end())
            return { 0.0f, false, true };
        auto& ps = ctx->presenceStates[nodeId];
        ps.enterT.Update(ctx->deltaTime);

        if (active) {
            if (ps.exiting || !ps.everActive) {
                if (!ps.everActive) ps.enterT.SetImmediate(0.0f);
                ps.enterT.SetTarget(1.0f, enterResponse, EasingType::Decelerate);
            }
            ps.everActive = true;
            ps.exiting = false;
        } else {
            if (!ps.exiting) {
                ps.enterT.SetTarget(0.0f, exitResponse, EasingType::Accelerate);
                ps.exiting = true;
            }
        }

        float t = ps.enterT.Get();
        bool shouldDraw = active || (t > 0.01f) || ps.enterT.IsAnimating();
        if (!shouldDraw) {
            ctx->presenceStates.erase(nodeId);
            return { 0.0f, false, true };
        }
        return { std::clamp(t, 0.0f, 1.0f), true, ps.exiting };
    }

    // brief 10 Part E: FLIP offset for an opt-in animated layout item.
    Vec2 LayoutFlipOffset(UIContext* ctx, uint32_t itemId, const Vec2& currentPos,
                          float response, float dampingRatio) {
        if (!ctx) return Vec2(0.0f, 0.0f);
        auto& fs = ctx->flipStates[itemId];
        ctx->lastSeenFrame[itemId] = ctx->frame; // keep alive in the GC rotation
        if (!fs.valid) {
            fs.valid = true;
            fs.prevPos = currentPos;
            fs.offset.Configure(response, dampingRatio);
            fs.offset.SetImmediate(Vec2(0.0f, 0.0f));
            return Vec2(0.0f, 0.0f);
        }
        Vec2 delta = fs.prevPos - currentPos; // First - Last (Invert)
        if (std::abs(delta.x) > 0.5f || std::abs(delta.y) > 0.5f) {
            fs.offset.Nudge(delta);                  // seed the visual offset
        }
        fs.offset.Update(ctx->deltaTime);            // Play: decay offset → 0
        fs.prevPos = currentPos;
        // (wake handled by AnyAnimationActive scanning flipStates)
        return fs.offset.Get();
    }

    // brief 10 Part F: stagger helpers.
    float StaggerDelaySeconds(int index, float staggerMs, float capMs) {
        if (index <= 0 || staggerMs <= 0.0f) return 0.0f;
        float ms = std::min((float)index * staggerMs, capMs);
        return ms * 0.001f;
    }

    float StaggeredAppear(UIContext* ctx, uint32_t itemId, int index, float staggerMs,
                          float enterResponse) {
        if (!ctx) return 1.0f;
        auto& a = ctx->floatAnimations[itemId];
        ctx->lastSeenFrame[itemId] = ctx->frame;
        if (!a.IsInitialized()) {
            a.SetImmediate(0.0f);
            a.SetTarget(1.0f, enterResponse, EasingType::Decelerate);
            a.SetDelay(StaggerDelaySeconds(index, staggerMs));
            ctx->NotifyFloatAnimActive(itemId);
        }
        if (a.IsAnimating()) ctx->NotifyFloatAnimActive(itemId);
        return a.Get();
    }

    // brief 10 Part G: union of all active animation sources. Used by the host loop
    // to decide whether to idle (block on events) or render continuously.
    bool UIContext::AnyAnimationActive() const {
        if (!activeColorAnimIds.empty() || !activeFloatAnimIds.empty() ||
            !activeRippleIds.empty() || !activeSpringColorIds.empty() ||
            !activeSpringFloatIds.empty() || widgetTree.HasActiveAnimations())
            return true;
        // brief 10 Part D: a fading-in/out managed overlay keeps the loop awake.
        for (const auto& kv : presenceStates)
            if (kv.second.enterT.IsAnimating()) return true;
        // brief 10 Part E: a sliding FLIP item keeps the loop awake.
        for (const auto& kv : flipStates)
            if (kv.second.offset.IsAnimating()) return true;
        return false;
    }

    // Helper: initialize system cursors for a context
    static void InitCursors(UIContext* ctx) {
        ctx->systemCursors[static_cast<int>(UIContext::CursorType::Arrow)]     = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT);
        ctx->systemCursors[static_cast<int>(UIContext::CursorType::IBeam)]     = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_TEXT);
        ctx->systemCursors[static_cast<int>(UIContext::CursorType::Hand)]      = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_POINTER);
        ctx->systemCursors[static_cast<int>(UIContext::CursorType::ResizeH)]   = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_EW_RESIZE);
        ctx->systemCursors[static_cast<int>(UIContext::CursorType::ResizeV)]   = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NS_RESIZE);
        ctx->systemCursors[static_cast<int>(UIContext::CursorType::ResizeNESW)]= SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NESW_RESIZE);
        ctx->systemCursors[static_cast<int>(UIContext::CursorType::ResizeNWSE)]= SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NWSE_RESIZE);
        ctx->cursorsInitialized = true;
    }

    // Helper: destroy system cursors for a context
    static void DestroyCursors(UIContext* ctx) {
        if (ctx->cursorsInitialized) {
            for (auto& cursor : ctx->systemCursors) {
                if (cursor) { SDL_DestroyCursor(cursor); cursor = nullptr; }
            }
            ctx->cursorsInitialized = false;
        }
    }

    UIContext* CreateContext(SDL_Window* window, void* existingGLContext) {
        if (g_ctx) return g_ctx;

        if (!window) {
            Log(LogLevel::Error, "Window handle is NULL");
            return nullptr;
        }

        g_ctx = new UIContext();
        g_ctx->window = window;

        g_backend = CreateBackendInstance();
        if (!g_backend->Init(window, existingGLContext)) {
            Log(LogLevel::Error, "Failed to initialize render backend");
            if (g_preferredBackend == RenderBackendType::OpenGL) {
                Log(LogLevel::Error, "Hint: if this is a Vulkan window, call "
                    "SetPreferredBackend(RenderBackendType::Vulkan) before CreateContext().");
            }
            delete g_backend;
            delete g_ctx;
            g_backend = nullptr;
            g_ctx = nullptr;
            return nullptr;
        }

        g_ctx->backend = g_backend;

        // brief 08 Part C: the main context owns the per-device shared resource
        // pool. It is injected into the Renderer (which becomes the device-owner
        // and publishes its atlas handles) and reused by every secondary window.
        g_ctx->sharedResources = new SharedResourcePool();
        g_ctx->ownsSharedResources = true;

        if (!g_ctx->renderer.Init(g_backend, g_ctx->sharedResources)) {
            Log(LogLevel::Error, "Failed to initialize Renderer");
            g_backend->Shutdown();
            delete g_backend;
            delete g_ctx->sharedResources;
            delete g_ctx;
            g_backend = nullptr;
            g_ctx = nullptr;
            return nullptr;
        }

        g_ctx->style = GetDarkFluentStyle();

        // Initialize system cursors
        InitCursors(g_ctx);

        g_ctx->initialized = true;
        Log(LogLevel::Info, "FluentUI Context created successfully");
        return g_ctx;
    }

    UIContext* CreateContext(SDL_Window* window, RenderBackendType backend, void* existingContext) {
        // Keep the backend choice and its handle together so they can't desync.
        SetPreferredBackend(backend);
        return CreateContext(window, existingContext);
    }

    UIContext* GetContext() {
        return g_ctx;
    }

    void* RegisterExternalTexture(void* nativeView, void* sampler, int layout) {
        if (!g_backend) {
            Log(LogLevel::Error, "RegisterExternalTexture: no active backend (call CreateContext first)");
            return nullptr;
        }
        void* h = g_backend->RegisterExternalTexture(nativeView, sampler, layout);
        // brief 08 Part C: track in the shared pool — the underlying image lives on
        // the shared device/GL-context, so it is drawable from any window.
        if (h && g_ctx && g_ctx->sharedResources) g_ctx->sharedResources->RegisterSharedTexture(h);
        return h;
    }

    void DestroyExternalTexture(void* handle) {
        if (g_ctx && g_ctx->sharedResources && handle)
            g_ctx->sharedResources->UnregisterSharedTexture(handle);
        if (g_backend && handle) g_backend->DeleteTexture(handle);
    }

    void SetCurrentContext(UIContext* ctx) {
        g_ctx = ctx;
    }

    // brief 18.5: layout direction (RTL) control on the active context.
    void SetLayoutDirection(UIContext::LayoutDirection dir) {
        if (g_ctx) g_ctx->layoutDirection = dir;
    }
    UIContext::LayoutDirection GetLayoutDirection() {
        return g_ctx ? g_ctx->layoutDirection : UIContext::LayoutDirection::LTR;
    }
    bool IsLayoutRTL() {
        return g_ctx && g_ctx->IsRTL();
    }

    UIContext* CreateStandaloneContext(SDL_Window* window, RenderBackend** outBackend) {
        if (!window) {
            Log(LogLevel::Error, "Window handle is NULL");
            return nullptr;
        }

        auto* ctx = new UIContext();
        ctx->window = window;

        auto* backend = CreateBackendInstance();
        if (!backend->Init(window)) {
            Log(LogLevel::Error, "Failed to initialize render backend for secondary window");
            delete backend;
            delete ctx;
            return nullptr;
        }

        if (!ctx->renderer.Init(backend)) {
            Log(LogLevel::Error, "Failed to initialize Renderer for secondary window");
            backend->Shutdown();
            delete backend;
            delete ctx;
            return nullptr;
        }

        ctx->style = GetDarkFluentStyle();
        InitCursors(ctx);
        ctx->initialized = true;

        ctx->backend = backend;

        // Return the backend pointer so the caller can clean it up later
        if (outBackend) *outBackend = backend;

        return ctx;
    }

    UIContext* CreateStandaloneContext(SDL_Window* window, UIContext* shareFrom,
                                       RenderBackend** outBackend) {
        if (!window) {
            Log(LogLevel::Error, "Window handle is NULL");
            return nullptr;
        }
        // No parent to share from → fall back to a fully isolated context.
        if (!shareFrom || !shareFrom->backend) {
            return CreateStandaloneContext(window, outBackend);
        }

        // Build the backend-specific "existing context" handle from the parent.
        void* existing = nullptr;
        VulkanSharedContext vkShared{}; // must outlive backend->Init below
        if (g_preferredBackend == RenderBackendType::OpenGL) {
            auto* gl = static_cast<OpenGLBackend*>(shareFrom->backend);
            existing = gl->GetGLContext(); // reuse the SAME GL context → shared resources
            if (!existing) {
                Log(LogLevel::Warning, "CreateStandaloneContext(share): parent GL context is null; "
                    "creating an isolated context");
                return CreateStandaloneContext(window, outBackend);
            }
        } else { // Vulkan
            auto* vk = static_cast<VulkanBackend*>(shareFrom->backend);
            if (!vk->GetSharedContext(&vkShared)) {
                Log(LogLevel::Warning, "CreateStandaloneContext(share): parent Vulkan device "
                    "unavailable; creating an isolated context");
                return CreateStandaloneContext(window, outBackend);
            }
            existing = &vkShared; // shared device + own swapchain (ownSwapchain=true)
        }

        auto* ctx = new UIContext();
        ctx->window = window;

        auto* backend = CreateBackendInstance();
        if (!backend->Init(window, existing)) {
            Log(LogLevel::Error, "Failed to initialize shared render backend for secondary window");
            delete backend;
            delete ctx;
            return nullptr;
        }
        ctx->backend = backend;

        // brief 08 Part A: route GL to this OS-window each frame and clear/present
        // its own framebuffer (it shares the parent's GL context).
        if (g_preferredBackend == RenderBackendType::OpenGL) {
            static_cast<OpenGLBackend*>(backend)->SetSecondaryWindowMode(true);
        }

        // brief 08 Part C: reference the parent's shared resource pool (not owned).
        ctx->sharedResources = shareFrom->sharedResources;
        ctx->ownsSharedResources = false;

        if (!ctx->renderer.Init(backend, ctx->sharedResources)) {
            Log(LogLevel::Error, "Failed to initialize Renderer for shared secondary window");
            backend->Shutdown();
            delete backend;
            delete ctx;
            return nullptr;
        }

        ctx->style = GetDarkFluentStyle();
        InitCursors(ctx);
        ctx->initialized = true;

        if (outBackend) *outBackend = backend;
        return ctx;
    }

    void DestroyStandaloneContext(UIContext* ctx, RenderBackend* backend) {
        if (!ctx) return;
        DestroyCursors(ctx);
        ctx->renderer.Shutdown();
        if (backend) {
            backend->Shutdown();
            delete backend;
        }
        delete ctx;
    }

    void DestroyContext() {
        if (!g_ctx) return;
        DestroyCursors(g_ctx);
        g_ctx->renderer.Shutdown();
        if (g_backend) {
            g_backend->Shutdown();
            delete g_backend;
            g_backend = nullptr;
        }
        // brief 08 Part C: free the shared resource pool last (owned by the main
        // context). Secondary windows must already be destroyed by this point.
        if (g_ctx->ownsSharedResources && g_ctx->sharedResources) {
            delete g_ctx->sharedResources;
        }
        g_ctx->sharedResources = nullptr;
        delete g_ctx;
        g_ctx = nullptr;
    }

    void NewFrame(float deltaTime) {
        if (!g_ctx || !g_ctx->initialized) return;
        
        // Perf Phase C: Reset performance counters and wire renderer pointers
        g_ctx->perfCounters.Reset();
        g_ctx->renderer.perfCounters.flushCount = &g_ctx->perfCounters.flushCount;
        g_ctx->renderer.perfCounters.stateChanges = &g_ctx->perfCounters.stateChanges;
        g_ctx->renderer.perfCounters.batchCount = &g_ctx->perfCounters.batchCount;
        g_ctx->renderer.perfCounters.drawCalls = &g_ctx->perfCounters.drawCalls;
        g_ctx->renderer.perfCounters.vertexCount = &g_ctx->perfCounters.vertexCount;
        g_ctx->renderer.perfCounters.indexCount = &g_ctx->perfCounters.indexCount;
        g_ctx->renderer.perfCounters.batchMerges = &g_ctx->perfCounters.batchMerges;
        g_ctx->renderer.perfCounters.clipPushes = &g_ctx->perfCounters.clipPushes;

        // Actualizar tiempo
        g_ctx->deltaTime = deltaTime;
        g_ctx->time += deltaTime;

        // brief 13: el TitleBar() volverá a publicar sus zonas este frame; si no se
        // dibuja ninguna, el callback de hit-test no marcará nada como arrastrable.
        {
            std::lock_guard<std::mutex> lk(g_ctx->titleBarHit.mutex);
            g_ctx->titleBarHit.active = false;
        }
        
        // Perf 2.2: Only update active animations (O(active) instead of O(total))
        for (size_t i = 0; i < g_ctx->activeColorAnimIds.size(); ) {
            uint32_t id = g_ctx->activeColorAnimIds[i];
            auto it = g_ctx->colorAnimations.find(id);
            if (it != g_ctx->colorAnimations.end()) {
                it->second.Update(deltaTime);
                if (!it->second.IsAnimating()) {
                    // Swap-and-pop removal (O(1))
                    g_ctx->activeColorAnimIds[i] = g_ctx->activeColorAnimIds.back();
                    g_ctx->activeColorAnimIds.pop_back();
                    continue;
                }
            } else {
                g_ctx->activeColorAnimIds[i] = g_ctx->activeColorAnimIds.back();
                g_ctx->activeColorAnimIds.pop_back();
                continue;
            }
            ++i;
        }
        for (size_t i = 0; i < g_ctx->activeFloatAnimIds.size(); ) {
            uint32_t id = g_ctx->activeFloatAnimIds[i];
            auto it = g_ctx->floatAnimations.find(id);
            if (it != g_ctx->floatAnimations.end()) {
                it->second.Update(deltaTime);
                if (!it->second.IsAnimating()) {
                    g_ctx->activeFloatAnimIds[i] = g_ctx->activeFloatAnimIds.back();
                    g_ctx->activeFloatAnimIds.pop_back();
                    continue;
                }
            } else {
                g_ctx->activeFloatAnimIds[i] = g_ctx->activeFloatAnimIds.back();
                g_ctx->activeFloatAnimIds.pop_back();
                continue;
            }
            ++i;
        }
        // brief 10 Part C: drive the spring maps with the same swap-pop pattern as
        // the tween maps above. Springs are removed from the active list once they
        // settle (IsAnimating()==false).
        for (size_t i = 0; i < g_ctx->activeSpringColorIds.size(); ) {
            uint32_t id = g_ctx->activeSpringColorIds[i];
            auto it = g_ctx->springColors.find(id);
            if (it != g_ctx->springColors.end()) {
                it->second.Update(deltaTime);
                if (!it->second.IsAnimating()) {
                    g_ctx->activeSpringColorIds[i] = g_ctx->activeSpringColorIds.back();
                    g_ctx->activeSpringColorIds.pop_back();
                    continue;
                }
            } else {
                g_ctx->activeSpringColorIds[i] = g_ctx->activeSpringColorIds.back();
                g_ctx->activeSpringColorIds.pop_back();
                continue;
            }
            ++i;
        }
        for (size_t i = 0; i < g_ctx->activeSpringFloatIds.size(); ) {
            uint32_t id = g_ctx->activeSpringFloatIds[i];
            auto it = g_ctx->springFloats.find(id);
            if (it != g_ctx->springFloats.end()) {
                it->second.Update(deltaTime);
                if (!it->second.IsAnimating()) {
                    g_ctx->activeSpringFloatIds[i] = g_ctx->activeSpringFloatIds.back();
                    g_ctx->activeSpringFloatIds.pop_back();
                    continue;
                }
            } else {
                g_ctx->activeSpringFloatIds[i] = g_ctx->activeSpringFloatIds.back();
                g_ctx->activeSpringFloatIds.pop_back();
                continue;
            }
            ++i;
        }
        for (size_t i = 0; i < g_ctx->activeRippleIds.size(); ) {
            uint32_t id = g_ctx->activeRippleIds[i];
            auto it = g_ctx->rippleEffects.find(id);
            if (it != g_ctx->rippleEffects.end()) {
                it->second.Update(deltaTime);
                if (!it->second.IsActive()) {
                    g_ctx->activeRippleIds[i] = g_ctx->activeRippleIds.back();
                    g_ctx->activeRippleIds.pop_back();
                    continue;
                }
            } else {
                g_ctx->activeRippleIds[i] = g_ctx->activeRippleIds.back();
                g_ctx->activeRippleIds.pop_back();
                continue;
            }
            ++i;
        }
        
        // Perf Phase C: Record active animation counts
        g_ctx->perfCounters.activeColorAnims = static_cast<uint32_t>(g_ctx->activeColorAnimIds.size());
        g_ctx->perfCounters.activeFloatAnims = static_cast<uint32_t>(g_ctx->activeFloatAnimIds.size());
        g_ctx->perfCounters.widgetNodeCount = static_cast<uint32_t>(g_ctx->widgetTree.NodeCount());

        // Reset tooltip frame flag
        if (!g_ctx->anyTooltipHoveredThisFrame) {
            g_ctx->tooltipState.hoverTime = 0.0f;
            g_ctx->tooltipState.visible = false;
            g_ctx->tooltipState.lastHoveredWidgetId = 0;
        }
        g_ctx->anyTooltipHoveredThisFrame = false;

        // Reset cursor to arrow at start of frame (widgets will set their desired cursor)
        g_ctx->desiredCursor = UIContext::CursorType::Arrow;

        // Widget tree: reset parent stack and update animations
        g_ctx->widgetTree.ResetParentStack();
        g_ctx->widgetTree.UpdateAnimations(deltaTime);

        g_ctx->renderer.BeginFrame(g_ctx->style.backgroundColor);
        // Reveal highlight (brief 04): feed the cursor position for this frame so SDF
        // rects with revealIntensity>0 light up their edge by proximity. Default radius
        // 120 logical px scaled by DPI.
        g_ctx->renderer.SetRevealCursor(
            Vec2(g_ctx->input.MouseX(), g_ctx->input.MouseY()), 120.0f * g_ctx->dpiScale);
        // Brief 11: feed the themed shadow color so every elevation shadow this
        // frame is tinted by the active theme (light: black; dark: softer black).
        g_ctx->renderer.SetShadowColor(g_ctx->style.shadowColor);
        g_ctx->scrollConsumedThisFrame = false;
        g_ctx->mouseOverAnyWidgetLastFrame = g_ctx->mouseOverAnyWidget;
        g_ctx->mouseOverAnyWidget = false;

        // Phase D: drag-drop per-frame reset (acceptedThisFrame is for current frame)
        g_ctx->dragDrop.acceptedThisFrame = false;
        g_ctx->dragDrop.delivered = false;
        g_ctx->insideContextMenu = false;
        g_ctx->cursorPos = { 20.0f, 20.0f };
        g_ctx->lastItemPos = g_ctx->cursorPos;
        g_ctx->lastItemSize = { 0.0f, 0.0f };
        // brief 21: defensively reset the ID scope stack at frame start so an
        // unbalanced PushID/PopID in one frame can't leak a stale seed into the next.
        g_ctx->idStack.clear();
        if (!g_ctx->input.IsMouseDown(0) && g_ctx->activeWidgetType == ActiveWidgetType::Slider) {
            g_ctx->activeWidgetId = 0;
            g_ctx->activeWidgetType = ActiveWidgetType::None;
        }
        if (!g_ctx->input.IsKeyDown(UIKey::LeftCtrl) && !g_ctx->input.IsKeyDown(UIKey::RightCtrl)) {
            g_ctx->input.anyKeyPressed = false;
        }

        // Manejar navegación con Tab
        if (g_ctx->input.IsKeyPressed(UIKey::Tab)) {
            bool shift = g_ctx->input.IsKeyDown(UIKey::LeftShift) || g_ctx->input.IsKeyDown(UIKey::RightShift);
            if (!g_ctx->focusableWidgets.empty()) {
                if (shift) {
                    // Navegar hacia atrás
                    g_ctx->focusIndex = (g_ctx->focusIndex <= 0) ? 
                        static_cast<int>(g_ctx->focusableWidgets.size() - 1) : g_ctx->focusIndex - 1;
                } else {
                    // Navegar hacia adelante
                    g_ctx->focusIndex = (g_ctx->focusIndex >= static_cast<int>(g_ctx->focusableWidgets.size() - 1)) ? 
                        0 : g_ctx->focusIndex + 1;
                }
                if (g_ctx->focusIndex >= 0 && g_ctx->focusIndex < static_cast<int>(g_ctx->focusableWidgets.size())) {
                    g_ctx->focusedWidgetId = g_ctx->focusableWidgets[g_ctx->focusIndex];
                }
            }
        }
        
        // Perf 3.4: Reserve based on previous frame's count to avoid reallocations
        size_t prevFocusCount = g_ctx->focusableWidgets.size();
        g_ctx->focusableWidgets.clear();
        g_ctx->focusableWidgets.reserve(prevFocusCount > 0 ? prevFocusCount : 64);

        // Phase 6: Clear style stacks at frame start (safety against mismatched push/pop)
        g_ctx->styleStack.clear();
        g_ctx->buttonStyleStack.clear();
        g_ctx->panelStyleStack.clear();
        g_ctx->textColorStack.clear();
        
        // Limpiar stack de menús al inicio de cada frame
        g_ctx->menuIdStack.clear();
        g_ctx->currentMenuItems.clear();
        g_ctx->menuItemStartIndexStack.clear();
        g_ctx->deferredTooltips.clear();
        
        // Issue 12: Smart text cache eviction — only remove stale entries
        g_ctx->textCacheFrame++;
        if (g_ctx->textCacheFrame >= UIContext::TEXT_CACHE_EVICT_INTERVAL) {
            g_ctx->textCacheFrame = 0;
            uint32_t currentFrame = g_ctx->frame;
            for (auto it = g_ctx->textMeasurementCache.begin(); it != g_ctx->textMeasurementCache.end(); ) {
                if ((currentFrame - it->second.lastAccessFrame) > UIContext::TEXT_CACHE_STALE_AGE) {
                    it = g_ctx->textMeasurementCache.erase(it);
                } else {
                    ++it;
                }
            }
        }
        
        // Cerrar context menus y menus si se hace click fuera de ellos
        if (g_ctx->input.IsMousePressed(0) || g_ctx->input.IsMousePressed(1) || g_ctx->input.IsMousePressed(2)) {
            float mouseX = g_ctx->input.MouseX();
            float mouseY = g_ctx->input.MouseY();
            bool clickedOutside = true;
            
            // Verificar context menus
            for (auto& [id, menuState] : g_ctx->contextMenuStates) {
                if (menuState.open) {
                    Vec2 menuPos = menuState.position;
                    Vec2 menuSize = menuState.size;
                    if (mouseX >= menuPos.x && mouseX <= menuPos.x + menuSize.x &&
                        mouseY >= menuPos.y && mouseY <= menuPos.y + menuSize.y) {
                        clickedOutside = false;
                    } else {
                        menuState.open = false;
                    }
                }
            }
            
            // Verificar menus del MenuBar
            for (auto& [id, menuState] : g_ctx->menuStates) {
                if (menuState.open) {
                    Vec2 menuPos = menuState.position;
                    Vec2 menuSize = menuState.size;
                    // Check if click is on the menu button itself
                    bool inButton = (mouseX >= menuPos.x && mouseX <= menuPos.x + menuSize.x &&
                                     mouseY >= menuPos.y && mouseY <= menuPos.y + menuSize.y);
                    // Check if click is in the dropdown area (saved from previous frame)
                    Vec2 dp = menuState.dropdownPos;
                    Vec2 ds = menuState.dropdownSize;
                    bool inDropdown = (ds.x > 0 && ds.y > 0 &&
                                       mouseX >= dp.x && mouseX <= dp.x + ds.x &&
                                       mouseY >= dp.y && mouseY <= dp.y + ds.y);
                    if (inButton || inDropdown) {
                        clickedOutside = false;
                    } else {
                        menuState.open = false;
                    }
                }
            }
            
            // Si se hizo click fuera de todos los menus, cerrar todos
            if (clickedOutside) {
                for (auto& [id, menuState] : g_ctx->contextMenuStates) {
                    menuState.open = false;
                }
                g_ctx->activeContextMenuId = 0;
                for (auto& [id, menuState] : g_ctx->menuStates) {
                    menuState.open = false;
                }
                g_ctx->activeMenuId = 0;
            }
        }
        
        // Issue 11: Amortized GC — rotate through maps, one every GC_ROTATE_INTERVAL frames
        if (g_ctx->frame > 0 && (g_ctx->frame % UIContext::GC_ROTATE_INTERVAL) == 0) {
            uint32_t currentFrame = g_ctx->frame;
            uint32_t threshold = UIContext::GC_MAP_COUNT * UIContext::GC_ROTATE_INTERVAL; // Full rotation cycle
            auto& seen = g_ctx->lastSeenFrame;

            auto gcMap = [&](auto& map) {
                for (auto it = map.begin(); it != map.end(); ) {
                    auto seenIt = seen.find(it->first);
                    if (seenIt == seen.end() || (currentFrame - seenIt->second) > threshold) {
                        it = map.erase(it);
                    } else {
                        ++it;
                    }
                }
            };

            switch (g_ctx->gcMapIndex) {
                case 0:  gcMap(g_ctx->colorAnimations); break;
                case 1:  gcMap(g_ctx->floatAnimations); break;
                case 2:  gcMap(g_ctx->rippleEffects); break;
                case 3:  gcMap(g_ctx->panelStates); break;
                case 4:  gcMap(g_ctx->scrollViewStates); break;
                case 5:  gcMap(g_ctx->tabViewStates); break;
                case 6:  gcMap(g_ctx->listViewStates); break;
                case 7:  gcMap(g_ctx->treeViewStates); break;
                case 8:  gcMap(g_ctx->boolStates); break;
                case 9:  gcMap(g_ctx->floatStates); break;
                case 10: gcMap(g_ctx->intStates); break;
                case 11: gcMap(g_ctx->stringStates); break;
                case 12: gcMap(g_ctx->colorPickerStates); break;
                case 13: gcMap(g_ctx->springColors); break;  // brief 10 Part C
                case 14: gcMap(g_ctx->springFloats); break;  // brief 10 Part C
                case 15: gcMap(g_ctx->flipStates); break;    // brief 10 Part E
            }

            g_ctx->gcMapIndex = (g_ctx->gcMapIndex + 1) % UIContext::GC_MAP_COUNT;

            // Clean up lastSeenFrame once per full rotation
            if (g_ctx->gcMapIndex == 0) {
                for (auto it = seen.begin(); it != seen.end(); ) {
                    if ((currentFrame - it->second) > threshold) {
                        it = seen.erase(it);
                    } else {
                        ++it;
                    }
                }
            }
        }

        // Widget tree: reconcile (remove nodes not seen recently)
        g_ctx->widgetTree.Reconcile(g_ctx->frame, 60); // Keep nodes alive for 60 frames grace period

        g_ctx->frame++;
    }

    void Render() {
        if (!g_ctx || !g_ctx->initialized) return;

        // brief 21: by the time the frame is submitted every PushID must have a
        // matching PopID. A non-empty stack means a container or user scope leaked.
#ifndef NDEBUG
        if (!g_ctx->idStack.empty()) {
            Log(LogLevel::Warning,
                "brief21: ID scope stack not balanced at Render() (depth=%zu); "
                "a PushID is missing its PopID.",
                g_ctx->idStack.size());
            assert(g_ctx->idStack.empty() && "Unbalanced PushID/PopID");
        }
#endif

        // Phase D: render the drag-drop floating preview on the overlay layer
        if (g_ctx->dragDrop.active && g_ctx->dragDrop.previewDrawCtx) {
            auto prevLayer = g_ctx->renderer.GetLayer();
            g_ctx->renderer.SetLayer(RenderLayer::Tooltip);
            Vec2 savedCursor = g_ctx->cursorPos;
            Vec2 mp = g_ctx->dragDrop.currentPos;
            g_ctx->cursorPos = Vec2(mp.x + 12.0f, mp.y + 12.0f);
            auto *fn = static_cast<std::function<void()> *>(g_ctx->dragDrop.previewDrawCtx);
            if (fn && *fn) (*fn)();
            g_ctx->cursorPos = savedCursor;
            g_ctx->renderer.SetLayer(prevLayer);
        }

        // Phase D: when the drag was just delivered or cancelled, clean preview
        if (!g_ctx->input.IsMouseDown(0) && g_ctx->dragDrop.previewDrawCtx) {
            auto *fn = static_cast<std::function<void()> *>(g_ctx->dragDrop.previewDrawCtx);
            delete fn;
            g_ctx->dragDrop.previewDrawCtx = nullptr;
            if (g_ctx->dragDrop.acceptedThisFrame || !g_ctx->dragDrop.active) {
                g_ctx->dragDrop.active = false;
                g_ctx->dragDrop.payloadType.clear();
                g_ctx->dragDrop.payloadBytes.clear();
                g_ctx->dragDrop.sourceWidgetId = 0;
            }
        }

        g_ctx->renderer.EndFrame();

        // Apply mouse cursor at end of frame
        if (g_ctx->cursorsInitialized && g_ctx->desiredCursor != g_ctx->currentCursor) {
            int idx = static_cast<int>(g_ctx->desiredCursor);
            if (idx >= 0 && idx < 7 && g_ctx->systemCursors[idx]) {
                SDL_SetCursor(g_ctx->systemCursors[idx]);
            }
            g_ctx->currentCursor = g_ctx->desiredCursor;
        }
    }

    bool WantCaptureMouse() {
        if (!g_ctx) return false;
        return g_ctx->mouseOverAnyWidgetLastFrame;
    }

    // --- Phase B1: Item-state query implementations ---
    bool IsItemHovered()   { return g_ctx ? g_ctx->lastItem.hovered  : false; }
    bool IsItemActive()    { return g_ctx ? g_ctx->lastItem.active   : false; }
    bool IsItemFocused()   { return g_ctx ? g_ctx->lastItem.focused  : false; }
    bool IsItemEdited()    { return g_ctx ? g_ctx->lastItem.edited   : false; }
    bool IsItemActivated() { return g_ctx ? g_ctx->lastItem.activated : false; }
    bool IsItemDeactivated() { return g_ctx ? g_ctx->lastItem.deactivated : false; }
    bool IsItemDeactivatedAfterEdit() { return g_ctx ? g_ctx->lastItem.deactivatedAfterEdit : false; }

    void GetItemRect(Vec2* outMin, Vec2* outMax) {
        if (!g_ctx) {
            if (outMin) *outMin = Vec2(0, 0);
            if (outMax) *outMax = Vec2(0, 0);
            return;
        }
        if (outMin) *outMin = g_ctx->lastItem.bboxMin;
        if (outMax) *outMax = g_ctx->lastItem.bboxMax;
    }

    void SetLastItem(uint32_t id, const Vec2& bboxMin, const Vec2& bboxMax,
                     bool hovered, bool active, bool focused, bool edited) {
        if (!g_ctx) return;
        UIContext::LastItemData& li = g_ctx->lastItem;
        li.id = id;
        li.bboxMin = bboxMin;
        li.bboxMax = bboxMax;
        li.hovered = hovered;
        li.active = active;
        li.focused = focused;
        li.edited = edited;

        // Track activation transitions
        bool wasActive = false;
        auto itPrev = g_ctx->prevActiveItems.find(id);
        if (itPrev != g_ctx->prevActiveItems.end()) wasActive = itPrev->second;

        li.activated = (active && !wasActive);
        li.deactivated = (!active && wasActive);

        if (li.activated) {
            g_ctx->editedSinceActivate[id] = false;
        }
        if (edited) {
            g_ctx->editedSinceActivate[id] = true;
        }
        if (li.deactivated) {
            auto itEd = g_ctx->editedSinceActivate.find(id);
            li.deactivatedAfterEdit = (itEd != g_ctx->editedSinceActivate.end() && itEd->second);
            g_ctx->editedSinceActivate.erase(id);
        } else {
            li.deactivatedAfterEdit = false;
        }

        // Update prev-frame state for next frame
        g_ctx->prevActiveItems[id] = active;
    }

} // namespace FluentUI
