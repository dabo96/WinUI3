#pragma once
#include "Math/Vec2.h"

namespace FluentUI {

// Tipos de constraints para layout
enum class SizeConstraint {
  Fixed, // Tamaño fijo
  Fill,  // Llenar espacio disponible
  Auto   // Tamaño automático basado en contenido
};

// Estructura para constraints de layout
struct LayoutConstraints {
  SizeConstraint width = SizeConstraint::Auto;
  SizeConstraint height = SizeConstraint::Auto;
  float fixedWidth = 0.0f;
  float fixedHeight = 0.0f;
  float minWidth = 0.0f;
  float minHeight = 0.0f;
  float maxWidth = 0.0f;
  float maxHeight = 0.0f;
};

// Helpers para crear constraints
inline LayoutConstraints FixedSize(float w, float h) {
  LayoutConstraints c;
  c.width = SizeConstraint::Fixed;
  c.height = SizeConstraint::Fixed;
  c.fixedWidth = w;
  c.fixedHeight = h;
  return c;
}

inline LayoutConstraints FillSize() {
  LayoutConstraints c;
  c.width = SizeConstraint::Fill;
  c.height = SizeConstraint::Fill;
  return c;
}

inline LayoutConstraints AutoSize() {
  LayoutConstraints c;
  c.width = SizeConstraint::Auto;
  c.height = SizeConstraint::Auto;
  return c;
}

} // namespace FluentUI
