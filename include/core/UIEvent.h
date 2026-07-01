#pragma once
#include "core/UIKey.h"
#include <cstdint>
#include <string>

namespace FluentUI {

// Opaque native window handle. The core never dereferences it; only the
// PlatformBackend implementation knows its real type (SDL_Window*, HWND, …).
using WindowHandle = void*;

// Platform-neutral event stream (brief 20, Part A/B). A PlatformBackend polls
// native events and translates them into these; the core (InputState) consumes
// only UIEvent and never sees SDL. Fields are a superset — only those relevant to
// `type` are meaningful.
enum class UIEventType {
  None,
  MouseMove,
  MouseButton,
  MouseWheel,
  KeyDown,
  KeyUp,
  TextInput,    // committed UTF-8 text
  TextEditing,  // IME composition (preedit)
  Resize,
  DpiChange,
  // Drag-drop phases (brief 18.7). text carries the payload (file path / text),
  // x/y the drop position in window coords.
  DropBegin,
  DropPosition,
  DropFile,
  DropText,
  DropComplete,
  Focus,
  Quit
};

// Modifier bitmask carried on key/mouse events.
enum : uint32_t {
  UIMod_None  = 0,
  UIMod_Ctrl  = 1u << 0,
  UIMod_Shift = 1u << 1,
  UIMod_Alt   = 1u << 2,
  UIMod_Gui   = 1u << 3,
};

struct UIEvent {
  UIEventType type = UIEventType::None;
  WindowHandle window = nullptr;

  // Pointer (MouseMove / MouseButton / MouseWheel), logical pixels.
  float x = 0.0f, y = 0.0f;
  int button = 0;          // 0=left, 1=middle, 2=right, …
  bool pressed = false;    // MouseButton: true=down, false=up
  float wheelX = 0.0f, wheelY = 0.0f;

  // Keyboard (KeyDown / KeyUp).
  UIKey key = UIKey::Unknown;
  uint32_t mods = UIMod_None;
  bool repeat = false;

  // Text (TextInput committed / TextEditing preedit / Drop text payload).
  std::string text;
  int editStart = 0;       // TextEditing: caret within composition
  int editLength = 0;      // TextEditing: selection length within composition

  // Resize / DpiChange.
  int width = 0, height = 0;
  float dpiScale = 1.0f;

  // Focus.
  bool focused = false;
};

} // namespace FluentUI
