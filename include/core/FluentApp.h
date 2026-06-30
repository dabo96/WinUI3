#pragma once
#include "core/UIBuilder.h"
#include "core/ShortcutRegistry.h"
#include "core/UndoSystem.h"
#include "core/LayoutSerializer.h"
#include "Theme/Style.h"
#include <SDL3/SDL.h>
#include <string>
#include <functional>
#include <cstdint>
#include <vector>
#include <memory>
#include <unordered_map>

namespace FluentUI {

// Forward declarations
struct UIContext;
class DockSpace;
class RenderBackend;
enum class DockPosition; // defined in DockSystem.h

/// Application configuration passed to FluentApp constructor.
struct AppConfig {
    int width = 1280;        ///< Initial window width in pixels.
    int height = 720;        ///< Initial window height in pixels.
    bool resizable = true;   ///< Allow window resizing.
    bool darkMode = true;    ///< Start with dark Fluent theme.
    int targetFPS = 60;      ///< Frame rate cap (0 = vsync only).
    bool enableDPI = true;   ///< Auto-detect and apply DPI scaling.

    /// brief 13: borderless window with a custom title bar. When true, the window
    /// is created with SDL_WINDOW_BORDERLESS and a hit-test is installed so the
    /// FluentUI::TitleBar() widget drives drag/resize/caption-buttons. Opt-in so
    /// the default window chrome is preserved for existing apps. The app MUST draw
    /// a TitleBar() each frame (otherwise the window has no draggable region).
    bool useCustomTitleBar = false;

    /// Optional override for the icon font path. When empty (default),
    /// FluentApp searches for `assets/fonts/lucide.ttf` next to the
    /// executable, the current working directory, and the
    /// FLUENTUI_ASSETS_DIR environment variable, in that order.
    std::string iconFontPath;
    int iconFontSize = 16;   ///< Pixel size used to bake the icon atlas.
};

/// Secondary window with its own UIContext, RenderBackend, and GL context.
/// Created via FluentApp::createWindow(). Destroyed automatically when closed.
class AppWindow {
public:
    ~AppWindow();

    // Non-copyable
    AppWindow(const AppWindow&) = delete;
    AppWindow& operator=(const AppWindow&) = delete;

    // Set the root builder for this window
    void root(std::function<void(UIBuilder&)> buildFn);

    // Access
    SDL_Window* window() { return window_; }
    UIContext* context() { return ctx_; }
    bool isOpen() const { return open_; }
    void close() { open_ = false; }

    /// Phase E5: panel id when this window hosts a detached dock panel.
    /// Empty for non-dock secondary windows. Set by detachPanelToWindow.
    const std::string& panelId() const { return panelId_; }

private:
    friend class FluentApp; // Only FluentApp can construct

    // brief 09 Fase 1: backend-agnostic. The window owns a UIContext created with
    // CreateStandaloneContext(window, parentCtx) so it SHARES the parent's GPU
    // device / GL context and resource pool (brief 08). No own GL context.
    AppWindow(const std::string& title, int width, int height,
              SDL_Window* parentWindow, SDL_GLContext parentGLContext,
              UIContext* parentCtx);

    // Process one frame (called by FluentApp in the main loop). Sets this context
    // current, builds + renders the panel, and presents via the backend. For GL it
    // restores the parent context afterwards (parentGLContext may be null on Vulkan).
    void processFrame(float dt, SDL_Window* parentWindow, SDL_GLContext parentGLContext);

    // Route an SDL event to this window's input system
    void routeEvent(const SDL_Event& e);

    // Update DPI scale from current display
    void updateDPIScale();

    SDL_Window* window_ = nullptr;
    RenderBackend* backend_ = nullptr;   // Own backend (for cleanup); shares parent device
    UIContext* ctx_ = nullptr;
    // brief 09: the SHARED GL context (parent's) this window renders with. Stored
    // only so GL resource cleanup can be made current at destruction. Null on Vulkan.
    SDL_GLContext sharedGLContext_ = nullptr;
    bool open_ = true;
    std::function<void(UIBuilder&)> rootBuilder_;
    std::string panelId_; // Phase E5: set when hosting a detached dock panel
    // brief 09 Fase 3: custom-titlebar drag state for re-dock detection.
    bool titleDragging_ = false;
    int  titleDragDX_ = 0, titleDragDY_ = 0; // cursor offset from window origin
};

/// Main application class. Owns the SDL window, GL context, and UIContext.
/// Usage: construct, call root() to set the UI builder, then run().
///
/// @code
/// FluentApp app("My App", {1280, 720});
/// app.root([](UIBuilder& ui) {
///     ui.button("Hello", [] { /* clicked */ });
/// });
/// app.run();
/// @endcode
class FluentApp {
public:
    /// Construct with full config — creates and owns the SDL window.
    FluentApp(const std::string& title, const AppConfig& config = {});
    /// Convenience: construct with just width/height (uses default config).
    FluentApp(const std::string& title, int width, int height);

    /// Construct from an **external** SDL window + GL context.
    /// FluentApp will NOT create, destroy, or manage the window/context.
    /// Use beginFrame()/endFrame() instead of run().
    ///
    /// @code
    /// // Your engine already has a window and GL context:
    /// SDL_Window* win = ...;
    /// SDL_GLContext gl = SDL_GL_GetCurrentContext();
    ///
    /// FluentApp ui(win, gl);
    /// ui.root([](UIBuilder& b) { b.button("Hello"); });
    ///
    /// // In your engine loop:
    /// while (running) {
    ///     SDL_Event e;
    ///     while (SDL_PollEvent(&e)) {
    ///         ui.processEvent(e);
    ///     }
    ///     ui.beginFrame(dt);
    ///     // ... your 3D rendering ...
    ///     ui.endFrame();       // renders UI on top, does NOT call SwapWindow
    ///     SDL_GL_SwapWindow(win);
    /// }
    /// @endcode
    FluentApp(SDL_Window* externalWindow, SDL_GLContext externalGLContext,
              bool darkMode = true, bool enableDPI = true);

    ~FluentApp();

    // Non-copyable, non-movable
    FluentApp(const FluentApp&) = delete;
    FluentApp& operator=(const FluentApp&) = delete;

    // Set the root UI builder function — called every frame
    void root(std::function<void(UIBuilder&)> buildFn);

    // Run the application (blocks until quit) — only for owned windows.
    void run();

    // Request exit (only meaningful when using run())
    void quit();

    // ─── External-window integration ─────────────────────────────────
    // Use these instead of run() when FluentApp does NOT own the window.

    /// Feed an SDL event to FluentUI (call for each event in your loop).
    void processEvent(const SDL_Event& e);

    /// Start a new UI frame. Call after polling events, before building UI.
    void beginFrame(float dt);

    /// Finish the UI frame: renders deferred elements + final draw.
    /// Does NOT call SDL_GL_SwapWindow — your engine does that.
    void endFrame();

    // Lifecycle hooks
    void onInit(std::function<void()> fn);
    void onShutdown(std::function<void()> fn);
    void onUpdate(std::function<void(float dt)> fn);

    /// Called when files are dropped onto the window.
    /// @param fn  Receives the list of dropped file paths.
    void onFileDrop(std::function<void(const std::vector<std::string>&)> fn);

    // Configuration
    void setTheme(const Style& style);
    void setTargetFPS(int fps);
    void enableDarkMode(bool dark);

    // Access
    UIContext* context() { return ctx_; }
    SDL_Window* window() { return window_; }
    bool isRunning() const { return running_; }

    // Keyboard shortcuts
    ShortcutRegistry shortcuts;

    // Undo/Redo (Phase 4)
    UndoStack undoStack;

    // Layout serialization (Phase 4)
    bool saveLayout(const std::string& filepath);
    bool loadLayout(const std::string& filepath);

    // Multi-Window (Phase 4)
    AppWindow* createWindow(const std::string& title, int width, int height);
    void closeWindow(AppWindow* win);

    // Phase E: Multi-viewport — detach a docked panel into its own OS-window.
    // The panel is removed from the main dock layout and a new AppWindow is
    // created at the given screen position with the panel's build callback.
    /// @return Pointer to the new AppWindow (lifetime owned by FluentApp).
    AppWindow* detachPanelToWindow(const std::string& panelId,
                                   std::function<void(UIBuilder&)> buildFn,
                                   int x = 100, int y = 100,
                                   int width = 480, int height = 360);

    /// Phase E: Drag-out detection — register a callback fired when the user
    /// drags any docked panel outside the main window bounds and releases.
    /// Callback receives the panel id and screen-space drop position.
    void setOnPanelDragOut(std::function<void(const std::string& panelId,
                                              int screenX, int screenY)> cb);

    // brief 09 Fase 2: register the build callback for a dockable panel by id, so
    // auto-detach can host it in a floating window without the user re-supplying it.
    // Pass the SAME builder you use with UIBuilder::dockPanel(panelId, ...).
    void registerPanelBuilder(const std::string& panelId,
                              std::function<void(UIBuilder&)> builder);

    // brief 09 Fase 2/3: turn on the full drag-out → floating window → re-dock flow.
    // Requires the panel builders to be registered via registerPanelBuilder(). When
    // a docked panel is dragged outside the main window it spawns a floating
    // AppWindow; dragging that window's titlebar back over a main-window dock zone
    // re-docks it and destroys the floating window.
    void enableAutoDetach(bool enable = true);

    /// Phase E5: enumerate currently-detached viewports (for serialization).
    std::vector<ViewportInfo> getViewports() const;

    /// Phase E5: pending viewports loaded from disk by loadLayout().
    /// Empty until loadLayout() reads a [Viewports] section. Call
    /// restoreViewports() to actually create the windows.
    const std::vector<ViewportInfo>& pendingViewports() const { return pendingViewports_; }

    /// Phase E5: recreate the previously-saved viewports using the supplied
    /// builder factory. Factory receives a panelId and returns the build fn.
    /// Clears pendingViewports() afterwards.
    void restoreViewports(std::function<std::function<void(UIBuilder&)>(const std::string&)> factory);

    // DPI Scaling (Phase 4)
    float getDPIScale() const;
    void setDPIScale(float scale); // Manual override

    // Dock space access (Phase 4)
    DockSpace& dockSpace();

    // Debug overlay toggle (Phase C)
    void showDebugOverlay(bool show);
    bool isDebugOverlayVisible() const;

private:
    float computeDelta();
    void updateDPIScale();

    // brief 09 Fase 2: spawn a floating window hosting `panelId` using the
    // registered builder; removes it from the main dock tree. Used by auto-detach.
    AppWindow* autoDetachPanel(const std::string& panelId, int screenX, int screenY);
    // brief 09 Fase 3: while a floating panel window's titlebar is dragged over a
    // main-window dock zone, preview it; on release re-dock and destroy the window.
    void updateFloatingRedock();

    // Get the SDL window ID for an event (works for mouse, key, window events)
    static SDL_WindowID getEventWindowID(const SDL_Event& e);

    SDL_Window* window_ = nullptr;
    SDL_GLContext mainGLContext_ = nullptr; // Cached main GL context for restore
    UIContext* ctx_ = nullptr;
    bool running_ = false;
    bool initialized_ = false;
    bool enableDPI_ = true;
    bool ownsWindow_ = true;  // false when constructed with external window

    // Callbacks
    std::function<void(UIBuilder&)> rootBuilder_;
    std::function<void()> initFn_;
    std::function<void()> shutdownFn_;
    std::function<void(float)> updateFn_;
    std::function<void(const std::vector<std::string>&)> fileDropFn_;

    // Timing
    uint64_t lastTime_ = 0;
    int targetFPS_ = 60;

    // Multi-Window (Phase 4)
    std::vector<std::unique_ptr<AppWindow>> secondaryWindows_;

    // Phase E: Drag-out detection callback
    std::function<void(const std::string&, int, int)> onPanelDragOut_;

    // brief 09 Fase 2: panel id → build callback registry (for auto-detach).
    std::unordered_map<std::string, std::function<void(UIBuilder&)>> panelBuilders_;
    bool autoDetach_ = false;

    // brief 09 Fase 3: live re-dock state (a floating panel window being dragged
    // by its titlebar over a main-window dock zone).
    AppWindow*  redockWin_ = nullptr;     // floating window currently hovering a zone
    DockPosition redockZone_{};           // hovered zone (Float = none)
    std::string redockTargetId_;          // panel the zone belongs to
    bool        redockActive_ = false;    // a zone is currently hovered

    // Phase E5: viewports loaded from layout file, awaiting restoreViewports().
    std::vector<ViewportInfo> pendingViewports_;
};

} // namespace FluentUI
