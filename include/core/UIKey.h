#pragma once
#include <cstdint>

namespace FluentUI {

// Platform-neutral key identity (brief 20, Part B). Widgets query the keyboard
// through these instead of SDL scancodes, so the core carries no SDL types. Each
// PlatformBackend maps its native key codes to UIKey; today the mapping lives in
// InputState (SDL scancode -> UIKey) behind the neutral IsKey*/modifier API.
//
// IMPORTANT: the letter block A..Z and the digit block Num0..Num9 are CONTIGUOUS
// and ordered, so range/arithmetic code (e.g. hex/number entry) can iterate them
// as `UIKey((int)UIKey::A + i)`. Do not reorder those runs.
enum class UIKey : uint32_t {
  Unknown = 0,

  // Letters (contiguous A..Z)
  A, B, C, D, E, F, G, H, I, J, K, L, M,
  N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

  // Digits (contiguous Num0..Num9)
  Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,

  // Editing / whitespace
  Space, Enter, KeypadEnter, Escape, Tab, Backspace, Delete, Insert,

  // Navigation
  Left, Right, Up, Down, Home, End, PageUp, PageDown,

  // Function keys (contiguous F1..F12)
  F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,

  // Modifiers
  LeftCtrl, RightCtrl, LeftShift, RightShift, LeftAlt, RightAlt,

  Count
};

// Human-readable name for a key (SDL-free; used by ShortcutRegistry text). Kept
// inline and dependency-free so it works in any translation unit.
inline const char* UIKeyName(UIKey k) {
  static const char* kLetters[] = {"A","B","C","D","E","F","G","H","I","J","K","L","M",
                                    "N","O","P","Q","R","S","T","U","V","W","X","Y","Z"};
  static const char* kDigits[]  = {"0","1","2","3","4","5","6","7","8","9"};
  static const char* kFns[]     = {"F1","F2","F3","F4","F5","F6","F7","F8","F9","F10","F11","F12"};
  if (k >= UIKey::A && k <= UIKey::Z)       return kLetters[(int)k - (int)UIKey::A];
  if (k >= UIKey::Num0 && k <= UIKey::Num9) return kDigits[(int)k - (int)UIKey::Num0];
  if (k >= UIKey::F1 && k <= UIKey::F12)    return kFns[(int)k - (int)UIKey::F1];
  switch (k) {
    case UIKey::Space:       return "Space";
    case UIKey::Enter:       return "Enter";
    case UIKey::KeypadEnter: return "Enter";
    case UIKey::Escape:      return "Esc";
    case UIKey::Tab:         return "Tab";
    case UIKey::Backspace:   return "Backspace";
    case UIKey::Delete:      return "Delete";
    case UIKey::Insert:      return "Insert";
    case UIKey::Left:        return "Left";
    case UIKey::Right:       return "Right";
    case UIKey::Up:          return "Up";
    case UIKey::Down:        return "Down";
    case UIKey::Home:        return "Home";
    case UIKey::End:         return "End";
    case UIKey::PageUp:      return "PageUp";
    case UIKey::PageDown:    return "PageDown";
    default:                 return "???";
  }
}

} // namespace FluentUI
