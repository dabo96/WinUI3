#pragma once
#include "Math/Color.h"
#include "Math/Vec2.h"
#include "Theme/Style.h"

#include <cstdint>

namespace FluentUI {

// Estados de un elemento UI
enum class ElementState { Normal, Hover, Pressed, Disabled, Focused };

// Estructura base para todos los elementos UI
struct Element {
  Vec2 position;
  Vec2 size;
  ElementState state = ElementState::Normal;
  bool visible = true;
  bool enabled = true;

  // ID único para identificación
  uint32_t id = 0;

  // Callbacks
  bool (*onClick)(Element *elem) = nullptr;
  bool (*onHover)(Element *elem) = nullptr;

  Element() = default;
  virtual ~Element() = default;

  // Verifica si un punto está dentro del elemento
  bool Contains(const Vec2 &point) const {
    return point.x >= position.x && point.x <= position.x + size.x &&
           point.y >= position.y && point.y <= position.y + size.y;
  }
};

} // namespace FluentUI
