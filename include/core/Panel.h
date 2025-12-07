#pragma once
#include "Element.h"
#include "Math/Color.h"
#include <vector>

namespace FluentUI {

class Panel : public Element {
public:
  Color backgroundColor;
  Color borderColor;
  float borderWidth = 0.0f;
  float cornerRadius = 6.0f;
  float shadowOpacity = 0.0f;
  float shadowOffsetY = 0.0f;

  // Contenedor de elementos hijos
  std::vector<Element *> children;

  Panel() {
    backgroundColor = Color{0.95f, 0.95f, 0.95f, 1.0f}; // Light gray
    borderColor = Color{0.8f, 0.8f, 0.8f, 1.0f};
  }

  void AddChild(Element *child);
  void RemoveChild(Element *child);
};

} // namespace FluentUI
