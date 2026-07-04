#include "core/SDLPlatform.h"
#include "core/Context.h"     // GetPreferredBackend / RenderBackendType / Log
#include "core/InputState.h"  // ProcessSDLEvent target
#include <SDL3/SDL_vulkan.h>
#include <chrono>
#include <cstdlib>

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #define NOMINMAX
  #include <windows.h>
  #include <dwmapi.h>
  #pragma comment(lib, "dwmapi.lib")
  // Fallbacks for pre-Windows-11 SDK headers: the attribute/enum only exist in
  // the 10.0.22000+ SDK, but the DWM call is a harmless no-op on Windows 10.
  #ifndef DWMWA_WINDOW_CORNER_PREFERENCE
    #define DWMWA_WINDOW_CORNER_PREFERENCE 33
  #endif
  #ifndef DWMWCP_ROUND
    #define DWMWCP_ROUND 2
  #endif
  #ifndef DWMWA_BORDER_COLOR
    #define DWMWA_BORDER_COLOR 34
  #endif
#endif

namespace FluentUI {

// ─────────────────────────────────────────────────────────────────────────────
// brief 25: this translation unit is the SDL side of the PlatformBackend seam.
// Everything SDL that used to live inline in FluentApp.cpp is here, behind the
// SDLPlatform class, plus the SDL→UIEvent translation moved out of InputState.cpp.
// ─────────────────────────────────────────────────────────────────────────────

namespace {

// brief 20 (F-c): the window/GL-context handles are stored as opaque void* in the
// (SDL-free) headers; these cast them back to the SDL types for internal use.
inline SDL_Window*   asWin(WindowHandle h) { return static_cast<SDL_Window*>(h); }
inline SDL_GLContext asGL(void* h)         { return static_cast<SDL_GLContext>(h); }

// Extract the target window id from a raw SDL event so PollEvent can stamp the
// neutral UIEvent with its WindowHandle (routing key). Moved verbatim from
// FluentApp.cpp (brief 25). Returns 0 for global/window-less events.
SDL_WindowID getEventWindowID(const SDL_Event& e) {
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
        case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
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
        case SDL_EVENT_DROP_BEGIN:
        case SDL_EVENT_DROP_POSITION:
        case SDL_EVENT_DROP_FILE:
        case SDL_EVENT_DROP_TEXT:
        case SDL_EVENT_DROP_COMPLETE:
            return e.drop.windowID;
        default:
            return 0;
    }
}

} // namespace

// ─── SDLPlatform ─────────────────────────────────────────────────────────────

// Trampoline: SDL calls this with our HitTestReg*; forward to the neutral fn and
// translate the neutral result back to SDL_HITTEST_*. Private static member so it
// can name the private HitTestReg type.
SDL_HitTestResult SDLCALL SDLPlatform::HitTestTrampoline(SDL_Window* /*win*/,
                                                         const SDL_Point* area, void* data) {
    auto* reg = static_cast<HitTestReg*>(data);
    if (!reg || !reg->fn || !area) return SDL_HITTEST_NORMAL;
    switch (reg->fn(area->x, area->y, reg->user)) {
        case UIHitTest::Draggable:        return SDL_HITTEST_DRAGGABLE;
        case UIHitTest::ResizeTop:        return SDL_HITTEST_RESIZE_TOP;
        case UIHitTest::ResizeBottom:     return SDL_HITTEST_RESIZE_BOTTOM;
        case UIHitTest::ResizeLeft:       return SDL_HITTEST_RESIZE_LEFT;
        case UIHitTest::ResizeRight:      return SDL_HITTEST_RESIZE_RIGHT;
        case UIHitTest::ResizeTopLeft:    return SDL_HITTEST_RESIZE_TOPLEFT;
        case UIHitTest::ResizeTopRight:   return SDL_HITTEST_RESIZE_TOPRIGHT;
        case UIHitTest::ResizeBottomLeft: return SDL_HITTEST_RESIZE_BOTTOMLEFT;
        case UIHitTest::ResizeBottomRight:return SDL_HITTEST_RESIZE_BOTTOMRIGHT;
        case UIHitTest::Normal:
        default:                          return SDL_HITTEST_NORMAL;
    }
}

SDLPlatform::~SDLPlatform() {
    Shutdown();
}

bool SDLPlatform::Initialize(uint32_t sdlInitFlags) {
    if (!SDL_Init(sdlInitFlags)) {
        Log(LogLevel::Error, "SDLPlatform: SDL_Init failed: %s", SDL_GetError());
        return false;
    }
    ownsSDL_ = true;
    return true;
}

void SDLPlatform::Shutdown() {
    // Free cached system cursors before SDL_Quit (or while SDL is still alive for a
    // host-owned platform).
    for (SDL_Cursor*& c : cursors_) {
        if (c) { SDL_DestroyCursor(c); c = nullptr; }
    }
    currentCursor_ = -1;
    if (ownsSDL_) {
        SDL_Quit();
        ownsSDL_ = false;
    }
}

Uint64 SDLPlatform::BackendWindowFlag() {
    // brief 08/09: choose the OS-window creation flag for the active backend.
    //   - OpenGL → SDL_WINDOW_OPENGL.
    //   - Vulkan → SDL_WINDOW_VULKAN *only* when SDL was built with a Vulkan driver
    //     (so SDL_Vulkan_CreateSurface works). When SDL lacks Vulkan, the backend
    //     creates a native Win32 surface from the HWND, which needs no Vulkan flag;
    //     we keep SDL_WINDOW_OPENGL there to preserve the proven standalone path.
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

WindowHandle SDLPlatform::CreateWindowHandle(const char* title, int w, int h, uint32_t flags) {
    Uint64 sdlFlags = BackendWindowFlag();
    if (flags & UIWindow_Resizable)  sdlFlags |= SDL_WINDOW_RESIZABLE;
    if (flags & UIWindow_HighDPI)    sdlFlags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (flags & UIWindow_Borderless) sdlFlags |= SDL_WINDOW_BORDERLESS;

    SDL_Window* win = SDL_CreateWindow(title, w, h, sdlFlags);
    if (!win) {
        Log(LogLevel::Error, "SDLPlatform: SDL_CreateWindow failed: %s", SDL_GetError());
        return nullptr;
    }

    // Borderless custom-chrome windows get the native Win11 rounded corners +
    // border so they look like a normal window (no-op off Windows).
    if (flags & UIWindow_Borderless)
        ApplyBorderlessChrome(static_cast<WindowHandle>(win));

    return static_cast<WindowHandle>(win);
}

void SDLPlatform::DestroyWindowHandle(WindowHandle window) {
    if (!window) return;
    hitTests_.erase(asWin(window));
    SDL_DestroyWindow(asWin(window));
}

void SDLPlatform::GetFramebufferSize(WindowHandle window, int& w, int& h) {
    w = h = 0;
    if (window) SDL_GetWindowSize(asWin(window), &w, &h);
}

float SDLPlatform::GetDpiScale(WindowHandle window) {
    if (!window) return 1.0f;
    float scale = SDL_GetWindowDisplayScale(asWin(window));
    return scale > 0.0f ? scale : 1.0f;
}

void SDLPlatform::SetWindowTitle(WindowHandle window, const char* title) {
    if (window) SDL_SetWindowTitle(asWin(window), title ? title : "");
}

void SDLPlatform::Present(WindowHandle window) {
    // See PlatformBackend::Present note: the owned SDL loop leaves presenting to
    // the RenderBackend. This is here for a driver that wants the platform to own
    // the GL swap; it is a no-op when there is no GL context on the window.
    if (window && SDL_GL_GetCurrentContext()) SDL_GL_SwapWindow(asWin(window));
}

bool SDLPlatform::PollEvent(UIEvent& out) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (TranslateSDLEvent(e, out)) {
            // Stamp the routing key: the WindowHandle the event targets (null for
            // global/window-less events such as QUIT).
            out.window = static_cast<WindowHandle>(SDL_GetWindowFromID(getEventWindowID(e)));
            return true;
        }
        // Untranslated SDL event (gamepad-added, unhandled types): keep draining.
    }
    return false;
}

void SDLPlatform::WaitEvents(int timeoutMs) {
    // brief 10 Part G: block on the queue when idle, then re-queue whatever woke us
    // so the driver's PollEvent loop processes it normally.
    SDL_Event we;
    if (SDL_WaitEventTimeout(&we, timeoutMs)) SDL_PushEvent(&we);
}

void SDLPlatform::SetClipboardText(const char* utf8) {
    SDL_SetClipboardText(utf8 ? utf8 : "");
}

std::string SDLPlatform::GetClipboardText() {
    char* clip = SDL_GetClipboardText();
    std::string result;
    if (clip) {
        result = clip;
        SDL_free(clip);
    }
    return result;
}

void SDLPlatform::SetCursor(int cursorId) {
    if (cursorId < 0 || cursorId >= kCursorCount) return;
    if (cursorId == currentCursor_) return;
    if (!cursors_[cursorId]) {
        // Order must match UIContext::CursorType.
        static const SDL_SystemCursor kMap[kCursorCount] = {
            SDL_SYSTEM_CURSOR_DEFAULT,    // Arrow
            SDL_SYSTEM_CURSOR_TEXT,       // IBeam
            SDL_SYSTEM_CURSOR_POINTER,    // Hand
            SDL_SYSTEM_CURSOR_EW_RESIZE,  // ResizeH
            SDL_SYSTEM_CURSOR_NS_RESIZE,  // ResizeV
            SDL_SYSTEM_CURSOR_NESW_RESIZE,// ResizeNESW
            SDL_SYSTEM_CURSOR_NWSE_RESIZE,// ResizeNWSE
        };
        cursors_[cursorId] = SDL_CreateSystemCursor(kMap[cursorId]);
    }
    if (cursors_[cursorId]) {
        SDL_SetCursor(cursors_[cursorId]);
        currentCursor_ = cursorId;
    }
}

void SDLPlatform::StartTextInput(WindowHandle window, int x, int y, int w, int h) {
    if (!window) return;
    SDL_Window* win = asWin(window);
    SDL_Rect rect{ x, y, w, h };
    SDL_SetTextInputArea(win, &rect, 0);
    SDL_StartTextInput(win);
}

void SDLPlatform::SetTextInputArea(WindowHandle window, int x, int y, int w, int h) {
    if (!window) return;
    SDL_Rect rect{ x, y, w, h };
    SDL_SetTextInputArea(asWin(window), &rect, 0);
}

void SDLPlatform::StopTextInput(WindowHandle window) {
    if (window) SDL_StopTextInput(asWin(window));
}

uint64_t SDLPlatform::GetTicksMs() {
    return SDL_GetTicks();
}

void SDLPlatform::Delay(uint32_t ms) {
    SDL_Delay(static_cast<Uint32>(ms));
}

void SDLPlatform::OpenURL(const char* url) {
    if (url) SDL_OpenURL(url);
}

std::string SDLPlatform::GetBasePath() {
    // SDL3 returns a const char* owned by SDL — do NOT free.
    const char* base = SDL_GetBasePath();
    return base ? std::string(base) : std::string();
}

void SDLPlatform::SetWindowHitTest(WindowHandle window, HitTestFn fn, void* user) {
    if (!window) return;
    SDL_Window* win = asWin(window);
    if (!fn) {
        hitTests_.erase(win);
        SDL_SetWindowHitTest(win, nullptr, nullptr);
        return;
    }
    auto& slot = hitTests_[win];
    if (!slot) slot = std::make_unique<HitTestReg>();
    slot->fn = fn;
    slot->user = user;
    if (!SDL_SetWindowHitTest(win, HitTestTrampoline, slot.get())) {
        Log(LogLevel::Warning, "SDLPlatform: SDL_SetWindowHitTest failed: %s", SDL_GetError());
    }
}

void SDLPlatform::GetWindowPosition(WindowHandle window, int& x, int& y) {
    x = y = 0;
    if (window) SDL_GetWindowPosition(asWin(window), &x, &y);
}

void SDLPlatform::SetWindowPosition(WindowHandle window, int x, int y) {
    if (window) SDL_SetWindowPosition(asWin(window), x, y);
}

void SDLPlatform::GetGlobalMousePos(float& x, float& y) {
    x = y = 0.0f;
    SDL_GetGlobalMouseState(&x, &y);
}

void SDLPlatform::MinimizeWindow(WindowHandle window) {
    if (window) SDL_MinimizeWindow(asWin(window));
}

void SDLPlatform::MaximizeWindow(WindowHandle window) {
    if (window) SDL_MaximizeWindow(asWin(window));
}

void SDLPlatform::RestoreWindow(WindowHandle window) {
    if (window) SDL_RestoreWindow(asWin(window));
}

bool SDLPlatform::IsWindowMaximized(WindowHandle window) {
    return window && (SDL_GetWindowFlags(asWin(window)) & SDL_WINDOW_MAXIMIZED) != 0;
}

void SDLPlatform::RequestWindowClose(WindowHandle window) {
    if (!window) return;
    SDL_Event ev;
    SDL_zero(ev);
    ev.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
    ev.window.windowID = SDL_GetWindowID(asWin(window));
    SDL_PushEvent(&ev);
}

void* SDLPlatform::CreateGraphicsSurface(WindowHandle window, GraphicsApi api, void* instance) {
    if (!window) return nullptr;
    SDL_Window* win = asWin(window);
    if (api == GraphicsApi::OpenGL) {
        return static_cast<void*>(SDL_GL_CreateContext(win));
    }
    // Vulkan: create a VkSurfaceKHR from the SDL window over the given VkInstance.
    // (Value-init to the null handle without relying on VK_NULL_HANDLE, which SDL's
    // minimal Vulkan typedefs do not define.)
    VkSurfaceKHR surface{};
    if (!SDL_Vulkan_CreateSurface(win, static_cast<VkInstance>(instance), nullptr, &surface)) {
        Log(LogLevel::Error, "SDLPlatform: SDL_Vulkan_CreateSurface failed: %s", SDL_GetError());
        return nullptr;
    }
    return reinterpret_cast<void*>(surface);
}

void* SDLPlatform::GetCurrentGLContext() {
    return static_cast<void*>(SDL_GL_GetCurrentContext());
}

void SDLPlatform::MakeContextCurrent(WindowHandle window, void* glContext) {
    if (window && glContext) SDL_GL_MakeCurrent(asWin(window), asGL(glContext));
}

void* SDLPlatform::GetNativeWindowHandle(WindowHandle window) {
#ifdef _WIN32
    if (!window) return nullptr;
    return SDL_GetPointerProperty(
        SDL_GetWindowProperties(asWin(window)), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
#else
    (void)window;
    return nullptr;
#endif
}

void SDLPlatform::ApplyBorderlessChrome(WindowHandle window) {
#ifdef _WIN32
    // Windows 11: round the corners and draw a subtle border so a borderless
    // window matches a normal one. DWM masks the corners at composition time (no
    // renderer change needed) and auto-squares them when the window is maximized.
    HWND hwnd = static_cast<HWND>(GetNativeWindowHandle(window));
    if (!hwnd) return;
    DWORD pref = DWMWCP_ROUND;
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &pref, sizeof(pref));
    // COLORREF is 0x00BBGGRR; a neutral dark gray reads well against the dark
    // chrome without standing out. (No-op on Windows 10 / older SDK headers.)
    COLORREF border = RGB(0x48, 0x48, 0x48);
    DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &border, sizeof(border));
#else
    (void)window;
#endif
}

// ─── Factories (declared in PlatformBackend.h so FluentApp stays SDL-free) ────

std::unique_ptr<PlatformBackend> CreateDefaultPlatform() {
    auto p = std::make_unique<SDLPlatform>();
    if (!p->Initialize(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) return nullptr;
    return p;
}

std::unique_ptr<PlatformBackend> CreateHostPlatform() {
    // The host (engine) already called SDL_Init; this platform must NOT own it.
    return std::make_unique<SDLPlatform>();
}

// ─── brief 20/25: SDL → neutral-event seam (moved out of InputState.cpp so SDL
//     event translation lives in the SDL platform TU). ─────────────────────────

UIKey UIKeyFromScancode(SDL_Scancode sc)
{
    if (sc >= SDL_SCANCODE_A && sc <= SDL_SCANCODE_Z)
        return static_cast<UIKey>(static_cast<int>(UIKey::A) + (sc - SDL_SCANCODE_A));
    if (sc == SDL_SCANCODE_0) return UIKey::Num0;
    if (sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_9)
        return static_cast<UIKey>(static_cast<int>(UIKey::Num1) + (sc - SDL_SCANCODE_1));
    if (sc >= SDL_SCANCODE_F1 && sc <= SDL_SCANCODE_F12)
        return static_cast<UIKey>(static_cast<int>(UIKey::F1) + (sc - SDL_SCANCODE_F1));
    switch (sc) {
        case SDL_SCANCODE_SPACE:     return UIKey::Space;
        case SDL_SCANCODE_RETURN:    return UIKey::Enter;
        case SDL_SCANCODE_KP_ENTER:  return UIKey::KeypadEnter;
        case SDL_SCANCODE_ESCAPE:    return UIKey::Escape;
        case SDL_SCANCODE_TAB:       return UIKey::Tab;
        case SDL_SCANCODE_BACKSPACE: return UIKey::Backspace;
        case SDL_SCANCODE_DELETE:    return UIKey::Delete;
        case SDL_SCANCODE_INSERT:    return UIKey::Insert;
        case SDL_SCANCODE_LEFT:      return UIKey::Left;
        case SDL_SCANCODE_RIGHT:     return UIKey::Right;
        case SDL_SCANCODE_UP:        return UIKey::Up;
        case SDL_SCANCODE_DOWN:      return UIKey::Down;
        case SDL_SCANCODE_HOME:      return UIKey::Home;
        case SDL_SCANCODE_END:       return UIKey::End;
        case SDL_SCANCODE_PAGEUP:    return UIKey::PageUp;
        case SDL_SCANCODE_PAGEDOWN:  return UIKey::PageDown;
        case SDL_SCANCODE_LCTRL:     return UIKey::LeftCtrl;
        case SDL_SCANCODE_RCTRL:     return UIKey::RightCtrl;
        case SDL_SCANCODE_LSHIFT:    return UIKey::LeftShift;
        case SDL_SCANCODE_RSHIFT:    return UIKey::RightShift;
        case SDL_SCANCODE_LALT:      return UIKey::LeftAlt;
        case SDL_SCANCODE_RALT:      return UIKey::RightAlt;
        default:                     return UIKey::Unknown;
    }
}

static UIKey UIKeyFromGamepadButton(int button)
{
    switch (button) {
        case SDL_GAMEPAD_BUTTON_DPAD_UP:        return UIKey::Up;
        case SDL_GAMEPAD_BUTTON_DPAD_DOWN:      return UIKey::Down;
        case SDL_GAMEPAD_BUTTON_DPAD_LEFT:      return UIKey::Left;
        case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:     return UIKey::Right;
        case SDL_GAMEPAD_BUTTON_SOUTH:          return UIKey::Enter;   // A
        case SDL_GAMEPAD_BUTTON_EAST:           return UIKey::Escape;  // B
        case SDL_GAMEPAD_BUTTON_WEST:           return UIKey::Space;   // X
        case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:  return UIKey::Tab;
        case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return UIKey::Tab;
        default:                                return UIKey::Unknown;
    }
}

bool TranslateSDLEvent(const SDL_Event& e, UIEvent& out)
{
    out = UIEvent{};
    switch (e.type) {
    case SDL_EVENT_KEY_DOWN: out.type = UIEventType::KeyDown; out.key = UIKeyFromScancode(e.key.scancode); return true;
    case SDL_EVENT_KEY_UP:   out.type = UIEventType::KeyUp;   out.key = UIKeyFromScancode(e.key.scancode); return true;
    case SDL_EVENT_TEXT_INPUT:
        out.type = UIEventType::TextInput;
        out.text = e.text.text ? e.text.text : "";
        return true;
    case SDL_EVENT_TEXT_EDITING:
        out.type = UIEventType::TextEditing;
        out.text = e.edit.text ? e.edit.text : "";
        out.editStart = e.edit.start;
        out.editLength = e.edit.length;
        return true;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
        out.type = UIEventType::MouseButton;
        out.pressed = (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
        out.button = static_cast<int>(e.button.button) - 1; // 0 = left
        out.x = static_cast<float>(e.button.x);
        out.y = static_cast<float>(e.button.y);
        return true;
    case SDL_EVENT_MOUSE_MOTION:
        out.type = UIEventType::MouseMove;
        out.x = static_cast<float>(e.motion.x);
        out.y = static_cast<float>(e.motion.y);
        return true;
    case SDL_EVENT_MOUSE_WHEEL:
        out.type = UIEventType::MouseWheel;
        out.wheelX = static_cast<float>(e.wheel.x);
        out.wheelY = static_cast<float>(e.wheel.y);
        return true;
    case SDL_EVENT_DROP_BEGIN:    out.type = UIEventType::DropBegin;    out.x = e.drop.x; out.y = e.drop.y; return true;
    case SDL_EVENT_DROP_POSITION: out.type = UIEventType::DropPosition; out.x = e.drop.x; out.y = e.drop.y; return true;
    case SDL_EVENT_DROP_FILE:
        out.type = UIEventType::DropFile;
        out.text = e.drop.data ? e.drop.data : "";
        out.x = e.drop.x; out.y = e.drop.y;
        return true;
    case SDL_EVENT_DROP_TEXT:
        out.type = UIEventType::DropText;
        out.text = e.drop.data ? e.drop.data : "";
        out.x = e.drop.x; out.y = e.drop.y;
        return true;
    case SDL_EVENT_DROP_COMPLETE: out.type = UIEventType::DropComplete; return true;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP: {
        UIKey k = UIKeyFromGamepadButton(e.gbutton.button);
        if (k == UIKey::Unknown) return false;
        out.type = (e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) ? UIEventType::KeyDown : UIEventType::KeyUp;
        out.key = k;
        return true;
    }
    case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
        // Right stick → scroll wheel; threshold ~25% (8000 of 32767).
        const Sint16 deadzone = 8000;
        if (e.gaxis.axis == SDL_GAMEPAD_AXIS_RIGHTX && std::abs(e.gaxis.value) > deadzone) {
            out.type = UIEventType::MouseWheel;
            out.wheelX = static_cast<float>(e.gaxis.value) / 32767.0f * 0.25f;
            return true;
        }
        if (e.gaxis.axis == SDL_GAMEPAD_AXIS_RIGHTY && std::abs(e.gaxis.value) > deadzone) {
            out.type = UIEventType::MouseWheel;
            out.wheelY = -static_cast<float>(e.gaxis.value) / 32767.0f * 0.25f;
            return true;
        }
        return false;
    }
    case SDL_EVENT_GAMEPAD_ADDED:
        SDL_OpenGamepad(e.gdevice.which); // side effect only — produces no UIEvent
        return false;
    // ── Window events (routed by the driver; size is queried by the handler) ──
    case SDL_EVENT_WINDOW_RESIZED:
        out.type = UIEventType::Resize;
        out.width = e.window.data1; out.height = e.window.data2;
        return true;
    case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
    case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
    case SDL_EVENT_WINDOW_MOVED:
        out.type = UIEventType::DpiChange;
        return true;
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        out.type = UIEventType::WindowClose;
        return true;
    case SDL_EVENT_QUIT:
        out.type = UIEventType::Quit;
        return true;
    default:
        return false;
    }
}

void ProcessSDLEvent(InputState& input, const SDL_Event& e)
{
    UIEvent ui;
    if (TranslateSDLEvent(e, ui)) input.ProcessEvent(ui);
}

} // namespace FluentUI
