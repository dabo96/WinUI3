#pragma once
#include <cstdint>
#include <cmath>
#include <algorithm>

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

  // Pack to 32-bit RGBA (8 bits per channel)
  uint32_t ToUint32() const {
    auto clamp01 = [](float v) { return std::clamp(v, 0.0f, 1.0f); };
    uint8_t rb = static_cast<uint8_t>(clamp01(r) * 255.0f + 0.5f);
    uint8_t gb = static_cast<uint8_t>(clamp01(g) * 255.0f + 0.5f);
    uint8_t bb = static_cast<uint8_t>(clamp01(b) * 255.0f + 0.5f);
    uint8_t ab = static_cast<uint8_t>(clamp01(a) * 255.0f + 0.5f);
    return (static_cast<uint32_t>(ab) << 24) | (static_cast<uint32_t>(bb) << 16) |
           (static_cast<uint32_t>(gb) << 8) | static_cast<uint32_t>(rb);
  }

  // Unpack from 32-bit RGBA
  static Color FromUint32(uint32_t packed) {
    float rb = static_cast<float>(packed & 0xFF) / 255.0f;
    float gb = static_cast<float>((packed >> 8) & 0xFF) / 255.0f;
    float bb = static_cast<float>((packed >> 16) & 0xFF) / 255.0f;
    float ab = static_cast<float>((packed >> 24) & 0xFF) / 255.0f;
    return Color(rb, gb, bb, ab);
  }

  // Relative luminance (ITU-R BT.709)
  float Luminance() const {
    return 0.2126f * r + 0.7152f * g + 0.0722f * b;
  }

  static Color Lerp(const Color& a, const Color& b, float t) {
    return Color(a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t,
                 a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t);
  }

  // Convert from HSV (h: 0-360, s: 0-1, v: 0-1) to Color
  static Color FromHSV(float h, float s, float v, float alpha = 1.0f) {
    float c = v * s;
    float hp = std::fmod(h / 60.0f, 6.0f);
    float x = c * (1.0f - std::fabs(std::fmod(hp, 2.0f) - 1.0f));
    float m = v - c;

    float rf, gf, bf;
    if (hp < 1.0f)      { rf = c; gf = x; bf = 0; }
    else if (hp < 2.0f) { rf = x; gf = c; bf = 0; }
    else if (hp < 3.0f) { rf = 0; gf = c; bf = x; }
    else if (hp < 4.0f) { rf = 0; gf = x; bf = c; }
    else if (hp < 5.0f) { rf = x; gf = 0; bf = c; }
    else                 { rf = c; gf = 0; bf = x; }

    return Color(rf + m, gf + m, bf + m, alpha);
  }

  // Convert to HSV (h: 0-360, s: 0-1, v: 0-1)
  void ToHSV(float& h, float& s, float& v) const {
    float cmax = std::max({r, g, b});
    float cmin = std::min({r, g, b});
    float delta = cmax - cmin;

    v = cmax;
    s = (cmax > 0.0f) ? (delta / cmax) : 0.0f;

    if (delta < 1e-6f) {
      h = 0.0f;
    } else if (cmax == r) {
      h = 60.0f * std::fmod((g - b) / delta, 6.0f);
    } else if (cmax == g) {
      h = 60.0f * ((b - r) / delta + 2.0f);
    } else {
      h = 60.0f * ((r - g) / delta + 4.0f);
    }
    if (h < 0.0f) h += 360.0f;
  }

  // Create from hex string "#RRGGBB" or "#RRGGBBAA"
  static Color FromHex(const char* hex) {
    if (!hex || hex[0] != '#') return Color(0, 0, 0, 1);
    unsigned int val = 0;
    int len = 0;
    for (const char* p = hex + 1; *p; ++p, ++len) {
      val <<= 4;
      if (*p >= '0' && *p <= '9') val |= (*p - '0');
      else if (*p >= 'a' && *p <= 'f') val |= (*p - 'a' + 10);
      else if (*p >= 'A' && *p <= 'F') val |= (*p - 'A' + 10);
    }
    if (len == 6) {
      return Color(((val >> 16) & 0xFF) / 255.0f,
                   ((val >> 8) & 0xFF) / 255.0f,
                   (val & 0xFF) / 255.0f, 1.0f);
    } else if (len == 8) {
      return Color(((val >> 24) & 0xFF) / 255.0f,
                   ((val >> 16) & 0xFF) / 255.0f,
                   ((val >> 8) & 0xFF) / 255.0f,
                   (val & 0xFF) / 255.0f);
    }
    return Color(0, 0, 0, 1);
  }
};
} // namespace FluentUI
