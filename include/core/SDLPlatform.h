#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// INTERNAL platform header (brief 20/25). This is the ONE header allowed to expose
// SDL — it is the SDL side of the PlatformBackend seam. Public headers stay
// SDL-free; only the SDL implementation (SDLPlatform.cpp) and SDL-owning hosts
// (e.g. examples that run their own SDL loop) include this. FluentApp does NOT
// include this header any more — it talks to PlatformBackend through the factory
// in PlatformBackend.h.
// ─────────────────────────────────────────────────────────────────────────────
#include <SDL3/SDL.h>
#include "core/PlatformBackend.h"
#include "core/UIEvent.h"
#include "core/UIKey.h"
#include <unordered_map>
#include <memory>

namespace FluentUI {

class InputState;

// ─── SDL → neutral seam (kept as free functions) ─────────────────────────────
// These stay free (not private members) because external SDL hosts that own their
// own event loop (see examples/App.cpp and the FluentApp external-window docs)
// translate native SDL events into UIEvents themselves. SDLPlatform::PollEvent
// uses TranslateSDLEvent internally.

// Map an SDL scancode to the platform-neutral UIKey (inverse of the mapping used
// by widgets). Returns UIKey::Unknown for keys outside the neutral set.
UIKey UIKeyFromScancode(SDL_Scancode sc);

// Translate a single SDL_Event into a neutral UIEvent. Returns false when the
// event produces no UIEvent (e.g. gamepad-added, unhandled types). Does NOT set
// out.window — SDLPlatform::PollEvent fills that from the event's window id.
bool TranslateSDLEvent(const SDL_Event& e, UIEvent& out);

// Convenience: translate `e` and feed it to `input` (neutral ProcessEvent). Used
// by external SDL hosts that drive InputState directly (examples/App.cpp).
void ProcessSDLEvent(InputState& input, const SDL_Event& e);

// ─── The SDL implementation of PlatformBackend (brief 25) ────────────────────
// Encapsulates ALL SDL that used to live inline in FluentApp: window, GL context
// binding, Vulkan surface, title-bar hit test, event pump, DPI, IME, clipboard,
// global mouse, time and SDL_Init/Quit. Instantiate via CreateDefaultPlatform()
// (owned) or CreateHostPlatform() (host already inited SDL) — see PlatformBackend.h.
class SDLPlatform final : public PlatformBackend {
public:
  SDLPlatform() = default;
  ~SDLPlatform() override;

  // SDL_Init with the given flags (SDL_INIT_VIDEO | SDL_INIT_GAMEPAD by default).
  // Marks this platform as the owner of SDL, so Shutdown()/dtor calls SDL_Quit.
  // Returns false if SDL_Init fails.
  bool Initialize(uint32_t sdlInitFlags = SDL_INIT_VIDEO | SDL_INIT_GAMEPAD);
  // SDL_Quit if this platform owns SDL (idempotent).
  void Shutdown();

  // ── PlatformBackend ──
  WindowHandle CreateWindowHandle(const char* title, int w, int h, uint32_t flags) override;
  void         DestroyWindowHandle(WindowHandle window) override;
  void         GetFramebufferSize(WindowHandle window, int& w, int& h) override;
  float        GetDpiScale(WindowHandle window) override;
  void         SetWindowTitle(WindowHandle window, const char* title) override;
  void         Present(WindowHandle window) override;

  bool PollEvent(UIEvent& out) override;
  void WaitEvents(int timeoutMs) override;

  void        SetClipboardText(const char* utf8) override;
  std::string GetClipboardText() override;
  void        SetCursor(int cursorId) override;
  void        StartTextInput(WindowHandle window, int x, int y, int w, int h) override;
  void        SetTextInputArea(WindowHandle window, int x, int y, int w, int h) override;
  void        StopTextInput(WindowHandle window) override;
  uint64_t    GetTicksMs() override;
  void        Delay(uint32_t ms) override;
  void        OpenURL(const char* url) override;
  std::string GetBasePath() override;

  void  SetWindowHitTest(WindowHandle window, HitTestFn fn, void* user) override;
  void  GetWindowPosition(WindowHandle window, int& x, int& y) override;
  void  SetWindowPosition(WindowHandle window, int x, int y) override;
  void  GetGlobalMousePos(float& x, float& y) override;
  void  MinimizeWindow(WindowHandle window) override;
  void  MaximizeWindow(WindowHandle window) override;
  void  RestoreWindow(WindowHandle window) override;
  bool  IsWindowMaximized(WindowHandle window) override;
  void  RequestWindowClose(WindowHandle window) override;

  void* CreateGraphicsSurface(WindowHandle window, GraphicsApi api, void* instance) override;
  void* GetCurrentGLContext() override;
  void  MakeContextCurrent(WindowHandle window, void* glContext) override;

  void* GetNativeWindowHandle(WindowHandle window) override;

private:
  // Choose the OS-window creation flag for the active render backend (GL vs
  // Vulkan, gated on whether SDL was built with a Vulkan driver). Mirrors the old
  // FluentApp::BackendWindowFlag().
  static Uint64 BackendWindowFlag();

  // Per-window registration for the neutral hit test. Heap-boxed so the pointer we
  // hand SDL as callback data stays stable across map rehashes.
  struct HitTestReg { HitTestFn fn = nullptr; void* user = nullptr; };
  std::unordered_map<SDL_Window*, std::unique_ptr<HitTestReg>> hitTests_;
  // SDL callback trampoline: forwards to a HitTestReg's neutral fn and maps the
  // neutral result back to SDL_HITTEST_*. Static member so it can name HitTestReg.
  static SDL_HitTestResult SDLCALL HitTestTrampoline(SDL_Window* win,
                                                     const SDL_Point* area, void* data);

  bool ownsSDL_ = false; // true when this platform called SDL_Init (dtor -> SDL_Quit)

  // Lazily-created system cursor cache (brief 26 de-SDL: moved out of Context.cpp).
  // Indexed by the neutral cursor id (UIContext::CursorType order). SDL owns the
  // SDL_Cursor* lifetimes; freed in the dtor.
  static constexpr int kCursorCount = 7;
  SDL_Cursor* cursors_[kCursorCount] = {};
  int currentCursor_ = -1;
};

} // namespace FluentUI
