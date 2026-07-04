#pragma once
#include "core/UIEvent.h"
#include <cstdint>
#include <memory>
#include <string>

namespace FluentUI {

// Platform port (brief 20, Part A). One of the two orthogonal seams the core
// talks to the outside world through (the other is RenderBackend: draw lists ->
// GPU). PlatformBackend owns everything window/OS-related: the window, the event
// pump, DPI, present, clipboard, cursor, IME and time. SDL lives entirely behind
// an implementation of this interface (SDLPlatform) and never leaks into the core
// or public headers.
//
// Ownership is per-port and independent of the RenderBackend: "owned" drivers
// (the library creates the window + runs the loop) use SDLPlatform; "embedded"
// hosts (the engine owns the window/device and drives the frame) can supply a
// NullPlatform / feed UIEvents themselves. This replaces the old binary
// "standalone vs shared" switch with ownership-per-port.
//
// NOTE: window create/destroy are named *WindowHandle to avoid the Win32
// `CreateWindow`/`DestroyWindow` macros.

// ── Neutral window-creation flags (brief 25). The SDL platform ORs in the
//    backend-specific flag (SDL_WINDOW_OPENGL / _VULKAN) itself, so callers never
//    name an SDL constant. ──
enum : uint32_t {
  UIWindow_Resizable  = 1u << 0,
  UIWindow_HighDPI    = 1u << 1,
  UIWindow_Borderless = 1u << 2,  // brief 13: borderless chrome for a custom TitleBar
};

// Graphics API for CreateGraphicsSurface (brief 25, Part 2). Neutral mirror of
// RenderBackendType kept here so PlatformBackend has no dependency on Context.h.
enum class GraphicsApi { OpenGL, Vulkan };

// ── Neutral hit-test result for a custom (borderless) title bar (brief 25). The
//    SDL platform translates these to SDL_HITTEST_*; the callback stays SDL-free
//    so it can live in the SDL-free driver (FluentApp). ──
enum class UIHitTest {
  Normal,
  Draggable,
  ResizeTop,
  ResizeBottom,
  ResizeLeft,
  ResizeRight,
  ResizeTopLeft,
  ResizeTopRight,
  ResizeBottomLeft,
  ResizeBottomRight,
};
// Hit-test callback: given a point in window (logical) coordinates, classify it.
// `user` is the opaque pointer passed to SetWindowHitTest (the UIContext today).
using HitTestFn = UIHitTest (*)(int x, int y, void* user);

// Ready-made hit-test for a custom borderless title bar: reads the drag/resize
// regions the TitleBar() widget publishes into the UIContext (passed as `user`).
// SDL-free, so any host — FluentApp or a standalone SDL loop (examples/App) — can
// install it via SetWindowHitTest to get the same drag/resize behavior.
UIHitTest CustomTitleBarHitTest(int x, int y, void* user);

class PlatformBackend {
public:
  virtual ~PlatformBackend() = default;

  // ── Window (owned). In embedded mode these may be no-ops and the host
  //    supplies the handle. `flags` is a UIWindow_* bitmask. ──
  virtual WindowHandle CreateWindowHandle(const char* title, int w, int h, uint32_t flags) = 0;
  virtual void         DestroyWindowHandle(WindowHandle window) = 0;
  virtual void         GetFramebufferSize(WindowHandle window, int& w, int& h) = 0;
  virtual float        GetDpiScale(WindowHandle window) = 0;
  virtual void         SetWindowTitle(WindowHandle window, const char* title) = 0;
  // Present: swap/present. NOTE (brief 25): the SDL driver leaves the actual swap
  // to the RenderBackend (renderer.Present() == GL SwapWindow / Vulkan present in
  // EndFrame), so it never calls this in the owned loop; it exists so an alternate
  // driver can make the platform own the swap. SDLPlatform implements it as an GL
  // SwapWindow for that case; it is a no-op under an external/Vulkan frame.
  virtual void Present(WindowHandle window) = 0;

  // ── Events (owned). PollEvent returns false when the queue is drained and
  //    translates native events into neutral UIEvents (with `window` set to the
  //    target WindowHandle so the driver can route per-window). ──
  virtual bool PollEvent(UIEvent& out) = 0;
  virtual void WaitEvents(int timeoutMs) = 0; // idle/wake for the brief 10 motion loop

  // ── OS services. ──
  virtual void        SetClipboardText(const char* utf8) = 0;
  virtual std::string GetClipboardText() = 0;
  // Cursor id is a neutral cursor shape. The int values MUST match the order of
  // UIContext::CursorType {Arrow, IBeam, Hand, ResizeH, ResizeV, ResizeNESW,
  // ResizeNWSE}; the SDL platform lazily creates + caches the matching OS cursor.
  virtual void        SetCursor(int cursorId) = 0;
  virtual void        StartTextInput(WindowHandle window, int x, int y, int w, int h) = 0; // begin IME + set area
  virtual void        SetTextInputArea(WindowHandle window, int x, int y, int w, int h) = 0; // update IME/candidate rect
  virtual void        StopTextInput(WindowHandle window) = 0;
  virtual uint64_t    GetTicksMs() = 0;
  virtual void        Delay(uint32_t ms) = 0; // sleep the current thread (FPS cap)
  virtual void        OpenURL(const char* url) = 0;
  virtual std::string GetBasePath() = 0; // executable directory (asset lookup); "" if unknown

  // ── Window chrome / geometry (brief 25, Part 2). ──
  // Install a title-bar hit test (custom borderless chrome). fn/user are neutral.
  virtual void  SetWindowHitTest(WindowHandle window, HitTestFn fn, void* user) = 0;
  // Apply the native "borderless custom chrome" look to a window: Windows 11
  // rounded corners + a subtle 1px border, matching a normal window. No-op on
  // non-Windows / null platforms. Called for windows created UIWindow_Borderless,
  // and exposed so hosts that create their own borderless window can opt in too.
  virtual void  ApplyBorderlessChrome(WindowHandle window) = 0;
  virtual void  GetWindowPosition(WindowHandle window, int& x, int& y) = 0;
  virtual void  SetWindowPosition(WindowHandle window, int x, int y) = 0;
  // Global (desktop) mouse position in logical pixels (panel detach / re-dock).
  virtual void  GetGlobalMousePos(float& x, float& y) = 0;
  // Caption-button window ops (brief 26 de-SDL: NavigationWidgets title bar).
  virtual void  MinimizeWindow(WindowHandle window) = 0;
  virtual void  MaximizeWindow(WindowHandle window) = 0;
  virtual void  RestoreWindow(WindowHandle window) = 0;
  virtual bool  IsWindowMaximized(WindowHandle window) = 0;
  // Post a "close requested" event for `window` into the event queue so the driver
  // loop tears it down through the normal path (used by the title-bar close button).
  virtual void  RequestWindowClose(WindowHandle window) = 0;

  // ── Graphics bridge (brief 25, Part 2). The platform↔render seam. Today the
  //    RenderBackend still self-creates its GL context / Vulkan surface, so the
  //    SDL driver only queries/binds the current GL context; CreateGraphicsSurface
  //    is provided so a future backend can request the surface from the platform.
  //    For GL it returns the created GL context; for Vulkan a VkSurfaceKHR
  //    (as void*). `instance` is the VkInstance for Vulkan, ignored for GL. ──
  virtual void* CreateGraphicsSurface(WindowHandle window, GraphicsApi api, void* instance) = 0;
  virtual void* GetCurrentGLContext() = 0; // current GL context (null on Vulkan)
  virtual void  MakeContextCurrent(WindowHandle window, void* glContext) = 0; // GL make-current; no-op when glContext is null

  // Native OS window handle (HWND on Windows) for OS integration such as the UI
  // Automation accessibility peer. Returns null when unavailable.
  virtual void* GetNativeWindowHandle(WindowHandle window) = 0;
};

// ── Factories (brief 25). Defined in SDLPlatform.cpp so callers (FluentApp) can
//    stay SDL-free — they only include this header. ──
// Owned driver: creates an SDLPlatform and initializes SDL (SDL_Init). The
// returned platform owns SDL and calls SDL_Quit on destruction. Returns null if
// SDL_Init fails.
std::unique_ptr<PlatformBackend> CreateDefaultPlatform();
// Host driver: an SDLPlatform that does NOT own SDL init/quit (the engine host
// already called SDL_Init). Used by the external-window FluentApp path.
std::unique_ptr<PlatformBackend> CreateHostPlatform();

} // namespace FluentUI
