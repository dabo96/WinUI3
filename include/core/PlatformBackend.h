#pragma once
#include "core/UIEvent.h"
#include <cstdint>
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
class PlatformBackend {
public:
  virtual ~PlatformBackend() = default;

  // ── Window (owned). In embedded mode these may be no-ops and the host
  //    supplies the handle. ──
  virtual WindowHandle CreateWindowHandle(const char* title, int w, int h, uint32_t flags) = 0;
  virtual void         DestroyWindowHandle(WindowHandle window) = 0;
  virtual void         GetFramebufferSize(WindowHandle window, int& w, int& h) = 0;
  virtual float        GetDpiScale(WindowHandle window) = 0;
  virtual void         SetWindowTitle(WindowHandle window, const char* title) = 0;
  virtual void         Present(WindowHandle window) = 0; // swap/present; no-op under an external frame

  // ── Events (owned). PollEvent returns false when the queue is drained and
  //    translates native events into neutral UIEvents. ──
  virtual bool PollEvent(UIEvent& out) = 0;
  virtual void WaitEvents(int timeoutMs) = 0; // idle/wake for the brief 10 motion loop

  // ── OS services. ──
  virtual void        SetClipboardText(const char* utf8) = 0;
  virtual std::string GetClipboardText() = 0;
  virtual void        SetCursor(int cursorId) = 0;
  virtual void        StartTextInput(WindowHandle window, int x, int y, int w, int h) = 0; // IME area
  virtual void        StopTextInput(WindowHandle window) = 0;
  virtual uint64_t    GetTicksMs() = 0;
  virtual void        OpenURL(const char* url) = 0;
};

} // namespace FluentUI
