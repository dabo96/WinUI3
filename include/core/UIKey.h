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

  // Modifiers
  LeftCtrl, RightCtrl, LeftShift, RightShift, LeftAlt, RightAlt,

  Count
};

} // namespace FluentUI
