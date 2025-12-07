#pragma once

namespace FluentUI {
struct Color {
  float r, g, b, a;

  constexpr Color() noexcept : r(0.0f), g(0.0f), b(0.0f), a(1.0f) {}
  constexpr Color(float R, float G, float B, float A = 1.0f) noexcept : r(R), g(G), b(B), a(A) {}
  
  Color operator+(const Color& c) const { return Color(r + c.r, g + c.g, b + c.b, a + c.a); }
  Color operator-(const Color& c) const { return Color(r - c.r, g - c.g, b - c.b, a - c.a); }
  Color operator*(float s) const { return Color(r * s, g * s, b * s, a * s); }
  
  bool operator==(const Color& c) const { 
    return r == c.r && g == c.g && b == c.b && a == c.a; 
  }
  bool operator!=(const Color& c) const { return !(*this == c); }
};
} // namespace FluentUI
