#pragma once
#include "Element.h"
#include "Math/Color.h"
#include "Math/Vec2.h"
#include <string>

namespace FluentUI {

class Button : public Element {
public:
  std::string text;
  Color backgroundColor;
  Color textColor;
  Color hoverColor;
  Color pressedColor;

  float cornerRadius = 6.0f;
  float padding = 12.0f;

  Button() {
    backgroundColor = Color{0.0f, 0.47f, 0.84f, 1.0f}; // Fluent Blue
    textColor = Color{1.0f, 1.0f, 1.0f, 1.0f};
    hoverColor = Color{0.0f, 0.55f, 0.92f, 1.0f};
    pressedColor = Color{0.0f, 0.38f, 0.70f, 1.0f};
  }

  // Factory method para crear botones
  static Button *Create(const std::string &label, const Vec2 &pos,
                        const Vec2 &size);
};

} // namespace FluentUI
