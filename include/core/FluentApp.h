#pragma once
#include "core/UIBuilder.h"
#include "core/ShortcutRegistry.h"
#include "core/UndoSystem.h"
#include "Theme/Style.h"
#include <SDL3/SDL.h>
#include <string>
#include <functional>
#include <cstdint>
#include <vector>
#include <memory>

namespace FluentUI {

// Forward declarations
struct UIContext;
class DockSpace;
class RenderBackend;

/// Application configuration passed to FluentApp constructor.
struct AppConfig {
    int width = 1280;        ///< Initial window width in pixels.
    int height = 720;        ///< Initial window height in pixels.
    bool resizable = true;   ///< Allow window resizing.
    bool darkMode = true;    ///< Start with dark Fluent theme.
    int targetFPS = 60;      ///< Frame rate cap (0 = vsync only).
    bool enableDPI = true;   ///< Auto-detect and apply DPI scaling.
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

private:
    friend class FluentApp; // Only FluentApp can construct

    AppWindow(const std::string& title, int width, int height,
              SDL_Window* parentWindow, SDL_GLContext parentGLContext);

    // Process one frame (called by FluentApp in the main loop)
    // Switches GL context, renders, and restores parent context.
    void processFrame(float dt, SDL_Window* parentWindow, SDL_GLContext parentGLContext);

    // Route an SDL event to this window's input system
    void routeEvent(const SDL_Event& e);

    // Update DPI scale from current display
    void updateDPIScale();

    SDL_Window* window_ = nullptr;
    SDL_GLContext glContext_ = nullptr;  // Own GL context
    RenderBackend* backend_ = nullptr;   // Own backend (for cleanup)
    UIContext* ctx_ = nullptr;
    bool open_ = true;
    std::function<void(UIBuilder&)> rootBuilder_;
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
};

} // namespace FluentUI
