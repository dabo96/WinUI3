#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// NullPlatform (brief 25, Part 4). A header-only PlatformBackend that owns no OS
// window and pumps no native events: the embedded host drives the core entirely
// through injected UIEvents. Every window/OS call is a no-op or a trivial stub, so
// the UI is fully drivable without SDL — this is the platform the embedded harness
// (brief 27) plugs in. Also handy for unit tests that need a real PlatformBackend
// without a display.
// ─────────────────────────────────────────────────────────────────────────────
#include "core/PlatformBackend.h"
#include <chrono>
#include <cstdint>
#include <deque>
#include <string>

namespace FluentUI {

class NullPlatform final : public PlatformBackend {
public:
  // Host-facing injection point: enqueue a synthetic UIEvent that PollEvent will
  // hand back (FIFO). This is how an embedded host / test feeds input.
  void PushEvent(const UIEvent& e) { queue_.push_back(e); }

  // Optional: set the size GetFramebufferSize reports (defaults to 800x600 so
  // layout has a sane viewport without a real window).
  void SetFramebufferSize(int w, int h) { fbWidth_ = w; fbHeight_ = h; }

  // ── PlatformBackend ──
  WindowHandle CreateWindowHandle(const char* /*title*/, int /*w*/, int /*h*/,
                                  uint32_t /*flags*/) override {
    // Return a unique, non-null opaque handle. The core never dereferences it.
    return reinterpret_cast<WindowHandle>(static_cast<uintptr_t>(++nextHandle_));
  }
  void DestroyWindowHandle(WindowHandle /*window*/) override {}
  void GetFramebufferSize(WindowHandle /*window*/, int& w, int& h) override {
    w = fbWidth_;
    h = fbHeight_;
  }
  float GetDpiScale(WindowHandle /*window*/) override { return 1.0f; }
  void  SetWindowTitle(WindowHandle /*window*/, const char* /*title*/) override {}
  void  Present(WindowHandle /*window*/) override {}

  bool PollEvent(UIEvent& out) override {
    if (queue_.empty()) return false;
    out = queue_.front();
    queue_.pop_front();
    return true;
  }
  void WaitEvents(int /*timeoutMs*/) override {}

  void SetClipboardText(const char* utf8) override { clipboard_ = utf8 ? utf8 : ""; }
  std::string GetClipboardText() override { return clipboard_; }
  void SetCursor(int /*cursorId*/) override {}
  void StartTextInput(WindowHandle /*window*/, int /*x*/, int /*y*/, int /*w*/, int /*h*/) override {}
  void SetTextInputArea(WindowHandle /*window*/, int /*x*/, int /*y*/, int /*w*/, int /*h*/) override {}
  void StopTextInput(WindowHandle /*window*/) override {}
  uint64_t GetTicksMs() override {
    using namespace std::chrono;
    return static_cast<uint64_t>(
        duration_cast<milliseconds>(steady_clock::now() - start_).count());
  }
  void Delay(uint32_t /*ms*/) override {}
  void OpenURL(const char* /*url*/) override {}
  std::string GetBasePath() override { return {}; }

  void SetWindowHitTest(WindowHandle /*window*/, HitTestFn /*fn*/, void* /*user*/) override {}
  void ApplyBorderlessChrome(WindowHandle /*window*/) override {}
  void GetWindowPosition(WindowHandle /*window*/, int& x, int& y) override { x = 0; y = 0; }
  void SetWindowPosition(WindowHandle /*window*/, int /*x*/, int /*y*/) override {}
  void GetGlobalMousePos(float& x, float& y) override { x = 0.0f; y = 0.0f; }
  void MinimizeWindow(WindowHandle /*window*/) override {}
  void MaximizeWindow(WindowHandle /*window*/) override {}
  void RestoreWindow(WindowHandle /*window*/) override {}
  bool IsWindowMaximized(WindowHandle /*window*/) override { return false; }
  void RequestWindowClose(WindowHandle /*window*/) override {}

  void* CreateGraphicsSurface(WindowHandle /*window*/, GraphicsApi /*api*/, void* /*instance*/) override {
    return nullptr;
  }
  void* GetCurrentGLContext() override { return nullptr; }
  void  MakeContextCurrent(WindowHandle /*window*/, void* /*glContext*/) override {}

  void* GetNativeWindowHandle(WindowHandle /*window*/) override { return nullptr; }

private:
  std::deque<UIEvent> queue_;
  std::string clipboard_;
  int fbWidth_ = 800, fbHeight_ = 600;
  uint64_t nextHandle_ = 0;
  std::chrono::steady_clock::time_point start_ = std::chrono::steady_clock::now();
};

} // namespace FluentUI
