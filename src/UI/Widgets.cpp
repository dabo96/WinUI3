#include "UI/Widgets.h"
#include "Theme/FluentTheme.h"
#include "core/Animation.h"
#include "core/Context.h"
#include "core/Renderer.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <optional>
#include <unordered_set>

namespace FluentUI {
// Static variable to store layout constraints for the next widget
static std::optional<LayoutConstraints> nextConstraints;

// Global state variables
static uint32_t g_currentTreeViewId = 0;
static int g_treeViewDepth = 0;
static std::vector<uint32_t> g_collapsibleGroupStack;

// viewport
static bool IsRectInViewport(UIContext *ctx, const Vec2 &pos,
                             const Vec2 &size) {
  if (!ctx)
    return true; // Si no hay contexto, renderizar por seguridad

  Vec2 viewport = ctx->renderer.GetViewportSize();
  Vec2 clipPos(0.0f, 0.0f);
  Vec2 clipSize = viewport;

  // Verificar contra clip stack si existe
  if (!ctx->renderer.GetClipStack().empty()) {
    const auto &clip = ctx->renderer.GetClipStack().back();
    clipPos = Vec2(static_cast<float>(clip.x), static_cast<float>(clip.y));
    clipSize =
        Vec2(static_cast<float>(clip.width), static_cast<float>(clip.height));
  }

  // Verificar superposición con el área visible
  return !(pos.x + size.x <= clipPos.x || pos.x >= clipPos.x + clipSize.x ||
           pos.y + size.y <= clipPos.y || pos.y >= clipPos.y + clipSize.y);
}

static float ResolveSpacing(UIContext *ctx, float spacing) {
  if (spacing >= 0.0f) {
    return spacing;
  }
  return ctx ? ctx->style.spacing : 4.0f;
}

static Vec2 ResolvePadding(UIContext *ctx,
                           const std::optional<Vec2> &paddingOpt) {
  if (paddingOpt.has_value()) {
    return paddingOpt.value();
  }
  float p = ctx ? ctx->style.padding : 6.0f;
  return Vec2(p, p);
}

static Vec2 CurrentOffset(UIContext *ctx) {
  if (!ctx || ctx->offsetStack.empty()) {
    return Vec2(0.0f, 0.0f);
  }
  return ctx->offsetStack.back();
}

static Vec2 GetParentAvailableSpace(UIContext *ctx) {
  if (!ctx)
    return Vec2(0.0f, 0.0f);

  if (ctx->layoutStack.empty()) {
    Vec2 viewport = ctx->renderer.GetViewportSize();
    float availableX =
        std::max(0.0f, viewport.x - ctx->cursorPos.x - ctx->style.padding);
    float availableY =
        std::max(0.0f, viewport.y - ctx->cursorPos.y - ctx->style.padding);
    return Vec2(availableX, availableY);
  }
  return ctx->layoutStack.back().availableSpace;
}

static Vec2 ComputeAvailableSpace(UIContext *ctx,
                                  const std::optional<Vec2> &explicitSize,
                                  const Vec2 &padding) {
  Vec2 parentAvailable = GetParentAvailableSpace(ctx);
  if (explicitSize.has_value()) {
    // Si se especifica un tamaño explícito, usarlo
    // Si el ancho es 0, no usar el ancho del padre (dejar que el contenido determine el ancho)
    if (explicitSize.value().x > 0.0f) {
      parentAvailable.x = explicitSize.value().x;
    } else {
      // Ancho 0 significa "auto" - no usar el ancho del padre
      parentAvailable.x = 0.0f;
    }
    if (explicitSize.value().y > 0.0f) {
      parentAvailable.y = explicitSize.value().y;
    }
  }

  Vec2 contentAvailable(std::max(0.0f, parentAvailable.x - padding.x * 2.0f),
                        std::max(0.0f, parentAvailable.y - padding.y * 2.0f));
  // Si el ancho disponible es 0, mantenerlo en 0 para que el contenido determine el ancho
  if (parentAvailable.x == 0.0f) {
    contentAvailable.x = 0.0f;
  }
  return contentAvailable;
}

static LayoutConstraints ConsumeNextConstraints() {
  if (nextConstraints.has_value()) {
    LayoutConstraints c = nextConstraints.value();
    nextConstraints.reset();
    return c;
  }
  return LayoutConstraints{};
}

static Vec2 GetCurrentAvailableSpace(UIContext *ctx) {
  Vec2 space = GetParentAvailableSpace(ctx);
  if (ctx && !ctx->layoutStack.empty()) {
    space = ctx->layoutStack.back().availableSpace;
  }
  return Vec2(std::max(0.0f, space.x), std::max(0.0f, space.y));
}

static Vec2 ApplyConstraints(UIContext *ctx,
                             const LayoutConstraints &constraints,
                             const Vec2 &desiredSize) {
  Vec2 result = desiredSize;
  Vec2 available = GetCurrentAvailableSpace(ctx);

  switch (constraints.width) {
  case SizeConstraint::Fixed:
    result.x = constraints.fixedWidth;
    break;
  case SizeConstraint::Fill:
    result.x = available.x > 0.0f ? available.x : desiredSize.x;
    break;
  case SizeConstraint::Auto:
    if (constraints.fixedWidth > 0.0f) {
      result.x = constraints.fixedWidth;
    }
    break;
  }

  switch (constraints.height) {
  case SizeConstraint::Fixed:
    result.y = constraints.fixedHeight;
    break;
  case SizeConstraint::Fill:
    result.y = available.y > 0.0f ? available.y : desiredSize.y;
    break;
  case SizeConstraint::Auto:
    if (constraints.fixedHeight > 0.0f) {
      result.y = constraints.fixedHeight;
    }
    break;
  }

  if (constraints.minWidth > 0.0f)
    result.x = std::max(result.x, constraints.minWidth);
  if (constraints.maxWidth > 0.0f)
    result.x = std::min(result.x, constraints.maxWidth);
  if (constraints.minHeight > 0.0f)
    result.y = std::max(result.y, constraints.minHeight);
  if (constraints.maxHeight > 0.0f)
    result.y = std::min(result.y, constraints.maxHeight);

  result.x = std::max(result.x, 0.0f);
  result.y = std::max(result.y, 0.0f);
  return result;
}

static float CurrentLayoutSpacing(UIContext *ctx) {
  if (!ctx)
    return 0.0f;
  if (!ctx->layoutStack.empty())
    return ctx->layoutStack.back().spacing;
  return ctx->style.spacing;
}

static float GetCurrentSpacing(UIContext *ctx) {
  return CurrentLayoutSpacing(ctx);
}

static bool RectanglesOverlap(const Vec2 &pos1, const Vec2 &size1,
                              const Vec2 &pos2, const Vec2 &size2) {
  return !(pos1.x + size1.x <= pos2.x || pos2.x + size2.x <= pos1.x ||
           pos1.y + size1.y <= pos2.y || pos2.y + size2.y <= pos1.y);
}

static Vec2 ResolveAbsolutePosition(UIContext *ctx, const Vec2 &desiredPos,
                                    const Vec2 &widgetSize) {
  if (!ctx)
    return desiredPos;
  
  // Las posiciones absolutas son relativas al viewport (esquina superior izquierda = 0,0)
  // No deben ser afectadas por layouts o offsets acumulados
  Vec2 resolvedPos = desiredPos;
  
  // Asegurar que la posición no sea negativa (ajustar al borde del viewport si es necesario)
  // Esto previene que widgets se rendericen fuera de la vista cuando se usan coordenadas negativas
  Vec2 viewport = ctx->renderer.GetViewportSize();
  
  // Si el widget se sale por la izquierda o arriba, ajustarlo al borde
  if (resolvedPos.x < 0.0f) {
    resolvedPos.x = 0.0f;
  }
  if (resolvedPos.y < 0.0f) {
    resolvedPos.y = 0.0f;
  }
  
  // Opcionalmente, si el widget se sale completamente por la derecha o abajo, 
  // podríamos ajustarlo, pero es mejor dejar que el usuario controle esto
  // ya que algunos widgets pueden estar intencionalmente parcialmente fuera del viewport
  
  return resolvedPos;
}

static void RegisterOccupiedArea(UIContext *ctx, const Vec2 &pos,
                                 const Vec2 &size) {
  if (!ctx)
    return;

  OccupiedArea area;
  area.pos = pos;
  area.size = size;

  // Registrar en el layout actual si existe
  if (!ctx->layoutStack.empty()) {
    ctx->layoutStack.back().occupiedAreas.push_back(area);
  }

  // Nota: No registrar globalmente para evitar interferencias entre
  // contenedores
}

static void ClearOccupiedAreas(UIContext *ctx) {
  if (!ctx)
    return;
  ctx->globalOccupiedAreas.clear();
  for (auto &stack : ctx->layoutStack) {
    stack.occupiedAreas.clear();
  }
}

static void AdvanceCursor(UIContext *ctx, const Vec2 &size) {
  if (!ctx)
    return;

  ctx->lastItemSize = size;

  if (ctx->layoutStack.empty()) {
    ctx->cursorPos.y += size.y + ctx->style.spacing;
    return;
  }

  LayoutStack &stack = ctx->layoutStack.back();
  if (stack.isVertical) {
    stack.contentSize.x = std::max(stack.contentSize.x, size.x);
    stack.contentSize.y += size.y;
    stack.cursor.y += size.y;
    stack.availableSpace.y = std::max(0.0f, stack.availableSpace.y - size.y);
    stack.itemCount++;
    if (stack.spacing > 0.0f) {
      stack.cursor.y += stack.spacing;
      stack.availableSpace.y =
          std::max(0.0f, stack.availableSpace.y - stack.spacing);
    }
    ctx->cursorPos = Vec2(stack.cursor.x, stack.cursor.y);
  } else {
    stack.contentSize.x += size.x;
    stack.contentSize.y = std::max(stack.contentSize.y, size.y);
    stack.cursor.x += size.x;
    stack.availableSpace.x = std::max(0.0f, stack.availableSpace.x - size.x);
    stack.itemCount++;
    if (stack.spacing > 0.0f) {
      stack.cursor.x += stack.spacing;
      stack.availableSpace.x =
          std::max(0.0f, stack.availableSpace.x - stack.spacing);
    }
    ctx->cursorPos = Vec2(stack.cursor.x, stack.contentStart.y);
  }
}


// Optimización: Función auxiliar para medir texto con caché
Vec2 MeasureTextCached(UIContext *ctx, const std::string &text,
                       float fontSize) {
  if (!ctx || text.empty())
    return Vec2(0.0f, 0.0f);

  // Crear clave de caché
  std::string cacheKey = text + "|" + std::to_string(fontSize);

  // Buscar en caché
  auto it = ctx->textMeasurementCache.find(cacheKey);
  if (it != ctx->textMeasurementCache.end()) {
    return it->second;
  }

  // Si no está en caché, medir y guardar
  Vec2 size = ctx->renderer.MeasureText(text, fontSize);
  ctx->textMeasurementCache[cacheKey] = size;
  return size;
}

void SetNextConstraints(const LayoutConstraints &constraints) {
  nextConstraints = constraints;
}

// Helpers internos
static uint32_t GenerateId(const char *str) {
  // Hash simple para generar IDs únicos
  uint32_t hash = 5381;
  int c;
  while ((c = *str++)) {
    hash = ((hash << 5) + hash) + c;
  }
  return hash;
}

void BeginVertical(float spacing, std::optional<Vec2> size,
                   std::optional<Vec2> padding) {
  UIContext *ctx = GetContext();
  if (!ctx)
    return;

  // Limpiar áreas ocupadas al inicio del primer layout del frame
  if (ctx->layoutStack.empty()) {
    ClearOccupiedAreas(ctx);
  }

  float resolvedSpacing = ResolveSpacing(ctx, spacing);
  Vec2 resolvedPadding = ResolvePadding(ctx, padding);
  Vec2 available = ComputeAvailableSpace(ctx, size, resolvedPadding);

  LayoutStack stack;
  stack.origin = ctx->cursorPos;
  stack.spacing = resolvedSpacing;
  stack.isVertical = true;
  stack.padding = resolvedPadding;
  stack.contentSize = Vec2(0.0f, 0.0f);
  stack.availableSpace = available;
  stack.lineHeight = 0.0f;
  stack.itemCount = 0;
  stack.contentStart = stack.origin + stack.padding;
  stack.cursor = stack.contentStart;
  stack.occupiedAreas.clear();

  ctx->layoutStack.push_back(stack);
  ctx->cursorPos = stack.contentStart;

  // Empujar offset local del contenedor actual
  ctx->offsetStack.push_back(stack.contentStart);
}

void EndVertical(bool advanceParent) {
  UIContext *ctx = GetContext();
  if (!ctx || ctx->layoutStack.empty())
    return;

  LayoutStack stack = ctx->layoutStack.back();
  ctx->layoutStack.pop_back();

  // Sacar offset local asociado
  if (!ctx->offsetStack.empty()) {
    ctx->offsetStack.pop_back();
  }

  if (!stack.isVertical) {
    ctx->layoutStack.push_back(stack);
    return;
  }

  float spacingTotal =
      (stack.itemCount > 0 && stack.spacing > 0.0f)
          ? stack.spacing * static_cast<float>(stack.itemCount - 1)
          : 0.0f;
  Vec2 contentSize(stack.contentSize.x, stack.contentSize.y + spacingTotal);
  Vec2 finalSize(contentSize.x + stack.padding.x * 2.0f,
                 contentSize.y + stack.padding.y * 2.0f);

  ctx->cursorPos = stack.origin;
  ctx->lastItemPos = stack.origin;

  if (!ctx->layoutStack.empty()) {
    if (advanceParent) {
      AdvanceCursor(ctx, finalSize);
    } else {
      ctx->lastItemSize = finalSize;
    }
  } else {
    ctx->lastItemSize = finalSize;
    if (advanceParent) {
      ctx->cursorPos.x = stack.origin.x;
      ctx->cursorPos.y = stack.origin.y + finalSize.y + ctx->style.spacing;
    }
  }
}

void BeginHorizontal(float spacing, std::optional<Vec2> size,
                     std::optional<Vec2> padding) {
  UIContext *ctx = GetContext();
  if (!ctx)
    return;

  // Limpiar áreas ocupadas al inicio del primer layout del frame
  if (ctx->layoutStack.empty()) {
    ClearOccupiedAreas(ctx);
  }

  float resolvedSpacing = ResolveSpacing(ctx, spacing);
  Vec2 resolvedPadding = ResolvePadding(ctx, padding);
  Vec2 available = ComputeAvailableSpace(ctx, size, resolvedPadding);

  LayoutStack stack;
  stack.origin = ctx->cursorPos;
  stack.spacing = resolvedSpacing;
  stack.isVertical = false;
  stack.padding = resolvedPadding;
  stack.contentSize = Vec2(0.0f, 0.0f);
  stack.availableSpace = available;
  stack.lineHeight = 0.0f;
  stack.itemCount = 0;
  stack.contentStart = stack.origin + stack.padding;
  stack.cursor = stack.contentStart;
  stack.occupiedAreas.clear();

  ctx->layoutStack.push_back(stack);
  ctx->cursorPos = stack.contentStart;

  // Empujar offset local del contenedor actual
  ctx->offsetStack.push_back(stack.contentStart);
}

void EndHorizontal(bool advanceParent) {
  UIContext *ctx = GetContext();
  if (!ctx || ctx->layoutStack.empty())
    return;

  LayoutStack stack = ctx->layoutStack.back();
  ctx->layoutStack.pop_back();

  // Sacar offset local asociado
  if (!ctx->offsetStack.empty()) {
    ctx->offsetStack.pop_back();
  }

  if (stack.isVertical) {
    // Si por error llamamos EndHorizontal en un stack vertical, lo devolvemos
    ctx->layoutStack.push_back(stack);
    return;
  }

  float spacingTotal =
      (stack.itemCount > 0 && stack.spacing > 0.0f)
          ? stack.spacing * static_cast<float>(stack.itemCount - 1)
          : 0.0f;
  
  // En horizontal, el spacing se suma al ancho (x)
  Vec2 contentSize(stack.contentSize.x + spacingTotal, stack.contentSize.y);
  Vec2 finalSize(contentSize.x + stack.padding.x * 2.0f,
                 contentSize.y + stack.padding.y * 2.0f);

  ctx->cursorPos = stack.origin;
  ctx->lastItemPos = stack.origin;

  if (!ctx->layoutStack.empty()) {
    if (advanceParent) {
      AdvanceCursor(ctx, finalSize);
    } else {
      ctx->lastItemSize = finalSize;
    }
  } else {
    ctx->lastItemSize = finalSize;
    if (advanceParent) {
      // Avanzar cursor verticalmente después de un bloque horizontal completo
      ctx->cursorPos.x = stack.origin.x;
      ctx->cursorPos.y = stack.origin.y + finalSize.y + ctx->style.spacing;
    }
  }
}

bool Button(const std::string &label, const Vec2 &size, const Vec2 &pos, bool enabled) {
  UIContext *ctx = GetContext();
  if (!ctx)
    return false;

  const ButtonStyle &buttonStyle = ctx->style.button;

  Vec2 textSize = MeasureTextCached(ctx, label, buttonStyle.text.fontSize);
  if (textSize.x <= 0.0f || textSize.y <= 0.0f) {
    textSize = Vec2(label.length() * buttonStyle.text.fontSize * 0.6f,
                    buttonStyle.text.fontSize);
  }

  Vec2 desiredSize = size;
  if (desiredSize.x <= 0.0f) {
    desiredSize.x = textSize.x + buttonStyle.padding.x * 2.0f;
  }
  if (desiredSize.y <= 0.0f) {
    desiredSize.y = textSize.y + buttonStyle.padding.y * 2.0f;
  }

  LayoutConstraints constraints = ConsumeNextConstraints();
  Vec2 btnSize = ApplyConstraints(ctx, constraints, desiredSize);

  Vec2 btnPos = pos;
  bool hasAbsolutePos = (btnPos.x != 0.0f || btnPos.y != 0.0f);
  if (!hasAbsolutePos) {
    btnPos = ctx->cursorPos;
  } else {
    // Resolver posición absoluta considerando superposiciones
    btnPos = ResolveAbsolutePosition(ctx, btnPos, btnSize);
  }

  // Generar ID único para este botón
  // NO incluir posición para que el ID sea estable cuando el widget se mueve
  std::string buttonIdStr = "BTN:" + label;
  uint32_t buttonId = GenerateId(buttonIdStr.c_str());

  // Registrar como widget enfocable
  if (enabled) {
    ctx->focusableWidgets.push_back(buttonId);
    // Si es el primer widget enfocable y no hay focus, establecerlo
    if (ctx->focusIndex < 0 && ctx->focusableWidgets.size() == 1) {
      ctx->focusIndex = 0;
      ctx->focusedWidgetId = buttonId;
    }
  }

  float mouseX = ctx->input.MouseX();
  float mouseY = ctx->input.MouseY();
  bool hover =
      enabled && (mouseX >= btnPos.x && mouseX < (btnPos.x + btnSize.x) &&
                  mouseY >= btnPos.y && mouseY < (btnPos.y + btnSize.y));
  bool pressed = hover && ctx->input.IsMousePressed(0);
  bool clicked = hover && ctx->input.IsMousePressed(0);

  // También activar con Enter/Space cuando tiene focus
  bool hasFocus = (ctx->focusedWidgetId == buttonId);
  if (hasFocus && enabled) {
    if (ctx->input.IsKeyPressed(SDL_SCANCODE_RETURN) ||
        ctx->input.IsKeyPressed(SDL_SCANCODE_SPACE)) {
      clicked = true;
    }
  }

  // Agregar ripple effect cuando se hace click
  if (clicked && enabled) {
    Vec2 clickPos(mouseX, mouseY);
    auto &ripple = ctx->rippleEffects[buttonId];
    ripple.AddRipple(clickPos, std::max(btnSize.x, btnSize.y) * 1.5f, 0.4f);
  }

  // Determinar color objetivo según estado
  auto getTargetColor = [&](const ColorState &state) -> Color {
    if (!enabled)
      return state.disabled;
    if (pressed)
      return state.pressed;
    if (hover)
      return state.hover;
    return state.normal;
  };

  // Obtener o crear animaciones de color
  auto &bgAnim = ctx->colorAnimations[buttonId * 3 + 0];
  auto &fgAnim = ctx->colorAnimations[buttonId * 3 + 1];
  auto &borderAnim = ctx->colorAnimations[buttonId * 3 + 2];

  // Inicializar animaciones si es necesario (primera vez)
  Color initialBg = bgAnim.Get();
  if (initialBg.r == 0.0f && initialBg.g == 0.0f && initialBg.b == 0.0f &&
      initialBg.a == 0.0f) {
    bgAnim.SetImmediate(getTargetColor(buttonStyle.background));
  }
  Color initialFg = fgAnim.Get();
  if (initialFg.r == 0.0f && initialFg.g == 0.0f && initialFg.b == 0.0f &&
      initialFg.a == 0.0f) {
    fgAnim.SetImmediate(getTargetColor(buttonStyle.foreground));
  }
  Color initialBorder = borderAnim.Get();
  if (initialBorder.r == 0.0f && initialBorder.g == 0.0f &&
      initialBorder.b == 0.0f && initialBorder.a == 0.0f) {
    borderAnim.SetImmediate(getTargetColor(buttonStyle.border));
  }

  // Actualizar objetivos de animación
  bgAnim.SetTarget(getTargetColor(buttonStyle.background), 0.2f,
                   Easing::EaseOutCubic);
  fgAnim.SetTarget(getTargetColor(buttonStyle.foreground), 0.2f,
                   Easing::EaseOutCubic);
  borderAnim.SetTarget(getTargetColor(buttonStyle.border), 0.2f,
                       Easing::EaseOutCubic);

  // Obtener colores animados
  Color bgColor = bgAnim.Get();
  Color fgColor = fgAnim.Get();
  Color borderColor = borderAnim.Get();

  Vec2 shadowPos = btnPos + Vec2(0.0f, buttonStyle.shadowOffsetY);
  Color shadowColor(0.0f, 0.0f, 0.0f, buttonStyle.shadowOpacity);
  if (buttonStyle.shadowOpacity > 0.0f) {
    ctx->renderer.DrawRectFilled(shadowPos, btnSize, shadowColor,
                                 buttonStyle.cornerRadius);
  }

  // Dibujar borde de focus si tiene focus (dibujar antes del fondo)
  if (hasFocus && enabled) {
    Color focusColor = FluentColors::Accent;
    focusColor.a = 0.6f;
    // Dibujar borde de focus como un rectángulo ligeramente más grande
    Vec2 focusOffset(2.0f, 2.0f);
    ctx->renderer.DrawRectFilled(btnPos - focusOffset,
                                 btnSize + focusOffset * 2.0f, focusColor,
                                 buttonStyle.cornerRadius + 2.0f);
  }

  ctx->renderer.DrawRectFilled(btnPos, btnSize, bgColor,
                               buttonStyle.cornerRadius);
  if (buttonStyle.borderWidth > 0.0f) {
    ctx->renderer.DrawRect(btnPos, btnSize, borderColor,
                           buttonStyle.cornerRadius);
  }

  Vec2 contentPos(btnPos.x + buttonStyle.padding.x,
                  btnPos.y + buttonStyle.padding.y);

  Vec2 textPos(
      contentPos.x +
          (btnSize.x - buttonStyle.padding.x * 2.0f - textSize.x) * 0.5f,
      contentPos.y +
          (btnSize.y - buttonStyle.padding.y * 2.0f - textSize.y) * 0.5f);
  ctx->renderer.DrawText(textPos, label, fgColor, buttonStyle.text.fontSize);

  // Dibujar ripple effects
  auto &ripple = ctx->rippleEffects[buttonId];
  for (const auto &r : ripple.GetRipples()) {
    ctx->renderer.DrawRipple(r.center, r.radius, r.opacity);
  }

  ctx->lastItemPos = btnPos;

  // Registrar área ocupada
  RegisterOccupiedArea(ctx, btnPos, btnSize);

  // Solo avanzar cursor si no hay posición absoluta
  if (!hasAbsolutePos) {
    AdvanceCursor(ctx, btnSize);
  } else {
    ctx->lastItemSize = btnSize;
  }

  return enabled && clicked;
}

void Label(const std::string &text, std::optional<Vec2> position,
           TypographyStyle variant, bool disabled) {
  UIContext *ctx = GetContext();
  if (!ctx)
    return;

  const TextStyle &textStyle = ctx->style.GetTextStyle(variant);
  Color color = disabled ? ctx->style.label.disabledColor : textStyle.color;

  Vec2 measured = MeasureTextCached(ctx, text, textStyle.fontSize);
  if (measured.x <= 0.0f || measured.y <= 0.0f) {
    measured =
        Vec2(text.length() * textStyle.fontSize * 0.6f, textStyle.fontSize);
  }

  LayoutConstraints constraints = ConsumeNextConstraints();
  Vec2 finalSize = ApplyConstraints(ctx, constraints, measured);

  bool hasAbsolutePos = position.has_value();
  Vec2 pos;
  if (hasAbsolutePos) {
    // Resolver posición absoluta considerando superposiciones
    pos = ResolveAbsolutePosition(ctx, position.value(), finalSize);
  } else {
    pos = ctx->cursorPos;
  }

  ctx->renderer.DrawText(pos, text, color, textStyle.fontSize);
  ctx->lastItemPos = pos;

  // Registrar área ocupada
  RegisterOccupiedArea(ctx, pos, finalSize);

  if (!hasAbsolutePos) {
    AdvanceCursor(ctx, finalSize);
  } else {
    ctx->lastItemSize = finalSize;
  }
}

void Separator() {
  UIContext *ctx = GetContext();
  if (!ctx)
    return;

  const SeparatorStyle &separator = ctx->style.separator;
  
  // Calcular el ancho basándose en el contenido real del contenedor
  float width = 0.0f;
  
  // Si hay un layout activo, calcular el ancho basándose en el contenido
  if (!ctx->layoutStack.empty()) {
    const auto &layout = ctx->layoutStack.back();
    
    // Si el layout tiene un contentSize.x definido (ancho fijo), usarlo
    if (layout.contentSize.x > 0.0f) {
      width = layout.contentSize.x;
    } else {
      // Si es ancho automático, calcular basándose en el contenido renderizado
      // Usar el ancho máximo de los elementos renderizados hasta ahora
      float maxContentWidth = 0.0f;
      
      // Revisar todas las áreas ocupadas en el layout para encontrar el ancho máximo
      for (const auto &area : layout.occupiedAreas) {
        float areaRight = area.pos.x + area.size.x;
        float areaWidth = areaRight - layout.origin.x;
        if (areaWidth > maxContentWidth) {
          maxContentWidth = areaWidth;
        }
      }
      
      // También considerar el último elemento si existe
      if (ctx->lastItemPos.x > layout.origin.x) {
        float lastItemWidth = (ctx->lastItemPos.x + ctx->lastItemSize.x) - layout.origin.x;
        if (lastItemWidth > maxContentWidth) {
          maxContentWidth = lastItemWidth;
        }
      }
      
      // Si encontramos contenido, usar ese ancho
      if (maxContentWidth > 0.0f) {
        width = maxContentWidth;
      } else {
        // Si no hay contenido renderizado aún, usar un ancho razonable basado en el viewport
        Vec2 viewport = ctx->renderer.GetViewportSize();
        width = std::min(viewport.x - layout.origin.x - ctx->style.padding * 2.0f, 500.0f);
      }
    }
  } else {
    // Si no hay layout, usar el espacio disponible pero limitado razonablemente
    Vec2 available = GetCurrentAvailableSpace(ctx);
    Vec2 viewport = ctx->renderer.GetViewportSize();
    float maxWidth = std::min(viewport.x - ctx->cursorPos.x - ctx->style.padding * 2.0f, 500.0f);
    width = available.x > 0.0f ? std::min(available.x, maxWidth) : maxWidth;
  }
  
  // Asegurar un ancho mínimo razonable
  if (width <= 0.0f) {
    width = 100.0f; // Ancho mínimo por defecto
  }
  
  Vec2 desired(width, separator.thickness);

  LayoutConstraints constraints = ConsumeNextConstraints();
  Vec2 finalSize = ApplyConstraints(ctx, constraints, desired);
  finalSize.y = std::max(finalSize.y, separator.thickness);

  // Resolver posición: usar cursor directamente (separator no tiene posición
  // absoluta)
  Vec2 pos = ctx->cursorPos;
  Vec2 drawPos = pos + Vec2(0.0f, separator.padding * 0.5f);
  ctx->renderer.DrawRectFilled(drawPos, Vec2(finalSize.x, finalSize.y),
                               separator.color, 0.0f);

  Vec2 totalSize(finalSize.x, finalSize.y + separator.padding);
  RegisterOccupiedArea(ctx, pos, totalSize);

  ctx->lastItemPos = pos;
  AdvanceCursor(ctx, totalSize);
}

struct PanelFrameContext {
  uint32_t id;
  Vec2 layoutOrigin;
  float titleHeight;
  Vec2 clipPos;
  Vec2 clipSize;
  bool clipPushed;
  bool layoutPushed;
  bool reserveLayout = true;
  Vec2 reservedLayoutSize{0.0f, 0.0f};
  Vec2 savedCursor;
  Vec2 savedLastItemPos;
  Vec2 savedLastItemSize;
  Vec2 parentCursor;
  Vec2 parentContentSize;
  Vec2 parentAvailable;
  int parentItemCount = 0;
};

static std::vector<PanelFrameContext> panelStack;

// Context for ScrollView to pass data from BeginScrollView to EndScrollView
struct ScrollViewFrameContext {
  uint32_t id;
  Vec2 position;
  Vec2 size;
  Vec2 contentAreaPos;
  Vec2 contentAreaSize;
  Vec2 availableSize;
  float scrollbarWidth;
  bool layoutPushed;
  bool useAbsolutePos;
  Vec2 savedCursor;
  Vec2 savedLastItemPos;
  Vec2 savedLastItemSize;
};

static std::vector<ScrollViewFrameContext> scrollViewStack;
// Flag estático para rastrear si el scroll ya fue consumido en este frame
static bool scrollConsumedThisFrame = false;

// MenuBar constants
static constexpr float MENUBAR_HEIGHT = 32.0f;

static bool PointInRect(const Vec2 &p, const Vec2 &pos, const Vec2 &size) {
  return p.x >= pos.x && p.x <= pos.x + size.x && p.y >= pos.y &&
         p.y <= pos.y + size.y;
}

// Forward declarations for dock node functions used later
void RenderDockNode(UIContext* ctx, uint32_t nodeId);

// Helper functions for dock node management (moved here to be accessible from BeginPanel)
DockNode& GetDockNode(UIContext* ctx, uint32_t id) {
    if (ctx->dockNodes.find(id) == ctx->dockNodes.end()) {
        ctx->dockNodes[id] = DockNode();
        ctx->dockNodes[id].id = id;
    }
    return ctx->dockNodes[id];
}

// Helper function to recursively collect all docked panel IDs from dock nodes
static void CollectDockedPanelIds(UIContext* ctx, uint32_t nodeId, std::unordered_set<uint32_t>& dockedIds) {
    if (ctx->dockNodes.find(nodeId) == ctx->dockNodes.end()) {
        return;
    }
    
    DockNode& node = ctx->dockNodes[nodeId];
    
    if (node.isLeaf()) {
        // Leaf node: add all docked panel IDs
        for (const auto& panelName : node.dockedPanelIds) {
            std::string key = "PANEL:" + panelName;
            uint32_t panelId = GenerateId(key.c_str());
            dockedIds.insert(panelId);
        }
    } else {
        // Container node: recurse into children
        if (node.childrenIds[0] != 0) {
            CollectDockedPanelIds(ctx, node.childrenIds[0], dockedIds);
        }
        if (node.childrenIds[1] != 0) {
            CollectDockedPanelIds(ctx, node.childrenIds[1], dockedIds);
        }
    }
}

// Count available (undocked) panels that can be docked
// Excludes the panel currently being dragged (if any)
int CountAvailablePanels(UIContext* ctx, const std::string& excludePanelId = "") {
    if (!ctx) return 0;
    
    // Get ID of panel to exclude (the one being dragged)
    uint32_t excludePanelIdNum = 0;
    if (!excludePanelId.empty()) {
        std::string excludeKey = "PANEL:" + excludePanelId;
        excludePanelIdNum = GenerateId(excludeKey.c_str());
    }
    
    // Build a set of docked panel IDs by checking dock nodes directly
    std::unordered_set<uint32_t> dockedPanelIds;
    if (ctx->rootDockNodeId != 0) {
        CollectDockedPanelIds(ctx, ctx->rootDockNodeId, dockedPanelIds);
    }
    
    // Also check manually undocked panels - if a panel is manually undocked,
    // it should not be counted as docked even if it's in a dock node
    for (const auto& [panelId, state] : ctx->panelStates) {
        if (state.manuallyUndocked && dockedPanelIds.find(panelId) != dockedPanelIds.end()) {
            dockedPanelIds.erase(panelId);
        }
    }
    
    // Count all dockable panels that are not docked and not being dragged
    int count = 0;
    for (const auto& [panelId, state] : ctx->panelStates) {
        // Only count dockable panels
        if (!state.isDockable) continue;
        
        // Exclude the panel being dragged
        if (panelId == excludePanelIdNum) continue;
        
        // Count if not in the docked set
        if (dockedPanelIds.find(panelId) == dockedPanelIds.end()) {
            count++;
        }
    }
    
    return count;
}

bool IsLeafNodeEmpty(UIContext* ctx, uint32_t nodeId) {
    if (ctx->dockNodes.find(nodeId) == ctx->dockNodes.end()) {
        return true;
    }
    
    DockNode& node = ctx->dockNodes[nodeId];
    return node.isLeaf() && node.dockedPanelIds.empty();
}

void CollapseEmptyNodes(UIContext* ctx, uint32_t nodeId) {
    if (ctx->dockNodes.find(nodeId) == ctx->dockNodes.end()) {
        return;
    }
    
    DockNode& node = ctx->dockNodes[nodeId];
    
    // Only process leaf nodes that are empty
    if (!node.isLeaf() || !node.dockedPanelIds.empty()) {
        return;
    }
    
    // If this is the root node and it's empty, just leave it as is (empty root is valid)
    if (node.parentId == 0) {
        return;
    }
    
    // This is an empty leaf node with a parent, need to collapse it
    if (ctx->dockNodes.find(node.parentId) == ctx->dockNodes.end()) {
        return;
    }
    
    DockNode& parent = ctx->dockNodes[node.parentId];
    
    // Find which child we are (0 or 1)
    int childIndex = (parent.childrenIds[0] == nodeId) ? 0 : 1;
    int siblingIndex = 1 - childIndex;
    uint32_t siblingId = parent.childrenIds[siblingIndex];
    
    if (siblingId == 0 || ctx->dockNodes.find(siblingId) == ctx->dockNodes.end()) {
        return;
    }
    
    DockNode& sibling = ctx->dockNodes[siblingId];
    
    // If sibling is also an empty leaf, collapse parent to empty leaf
    if (IsLeafNodeEmpty(ctx, siblingId)) {
        // Both children are empty, convert parent to empty leaf
        parent.dockedPanelIds.clear();
        parent.activeTabIndex = 0;
        parent.splitDirection = DockSplitDirection::None;
        
        // Remove children from map
        ctx->dockNodes.erase(nodeId);
        ctx->dockNodes.erase(siblingId);
        parent.childrenIds[0] = 0;
        parent.childrenIds[1] = 0;
        
        // Recursively collapse parent if it now has a parent
        if (parent.parentId != 0) {
            CollapseEmptyNodes(ctx, node.parentId);
        }
    } else {
        // Sibling has content - DON'T collapse, just leave the empty node
        // This preserves the split structure so other panels maintain their positions
        // The empty node will simply not render any content, but the split remains
        // This way, when the user re-docks a panel, it can go back to the same position
        
        // Just remove the empty node from the map, but keep the parent structure intact
        ctx->dockNodes.erase(nodeId);
        
        // Mark the child slot as empty (0) but keep the split structure
        parent.childrenIds[childIndex] = 0;
        
        // The sibling will continue to occupy its space, maintaining the layout
        // No need to modify parent or sibling structure
    }
}

bool BeginPanel(const std::string &id, const Vec2 &desiredSize, bool reserveLayoutSpace, std::optional<bool> useAcrylic, std::optional<float> acrylicOpacity, const Vec2 &pos, bool isDockable) {
  UIContext *ctx = GetContext();
  if (!ctx)
    return false;

  bool isDocked = false;
  UIContext::DockedPanelInfo dockedInfo;
  
  std::string key = "PANEL:" + id;
  uint32_t panelId = GenerateId(key.c_str());
  auto &state = ctx->panelStates[panelId];
  
  // Only apply docking if panel wasn't manually undocked
  if (!state.manuallyUndocked && ctx->currentFrameDockedPanels.find(id) != ctx->currentFrameDockedPanels.end()) {
      dockedInfo = ctx->currentFrameDockedPanels[id];
      if (!dockedInfo.isVisible) {
          return false; // Inactive tab
      }
      isDocked = true;
  }


  if (!state.initialized) {
    state.size = desiredSize;
    if (state.size.x <= 0.0f)
      state.size.x = 300.0f;
    if (state.size.y <= 0.0f)
      state.size.y = 200.0f;

    // Resolver posición: usar pos si se proporciona, sino usar cursor
    // directamente NO usar ResolveAbsolutePosition aquí porque puede aplicar
    // offsets incorrectos cuando hay un layout activo. Usar ctx->cursorPos
    // directamente como los widgets básicos.
    if (pos.x != 0.0f || pos.y != 0.0f) {
      state.position = ResolveAbsolutePosition(ctx, pos, state.size);
      state.useAbsolutePos = true;
    } else {
      state.position = ctx->cursorPos;
      state.useAbsolutePos = false; // Allow layout positioning by default
    }

    // Configurar acrylic: usar parámetros si se proporcionan, sino usar el
    // estilo global
    const PanelStyle &panelStyle = ctx->style.panel;
    state.useAcrylic = useAcrylic.has_value() ? useAcrylic.value() : panelStyle.useAcrylic;
    state.acrylicOpacity = acrylicOpacity.has_value() ? acrylicOpacity.value() : panelStyle.acrylicOpacity;

    // Set dockable status (only root-level panels can be docked)
    state.isDockable = isDockable;

    state.initialized = true;
    state.reservedLayoutSize = state.size;
    state.expandedLayoutSize = state.size;
  } else {
    // Permitir cambiar configuración de acrylic en tiempo de ejecución
    if (useAcrylic.has_value()) {
      state.useAcrylic = useAcrylic.value();
    }
    if (acrylicOpacity.has_value()) {
      state.acrylicOpacity = acrylicOpacity.value();
    }
    
    // Always update dockable status
    state.isDockable = isDockable;

    if (isDocked) {
        // Before docking, save the current absolute position if the panel was floating
        // This allows us to restore it later when undocking
        if (state.useAbsolutePos && state.position.x != dockedInfo.pos.x && state.position.y != dockedInfo.pos.y) {
            state.absolutePos = state.position; // Save position before docking
        }
        
        state.position = dockedInfo.pos;
        state.size = dockedInfo.size;
        state.minimized = false; // Cannot be minimized if docked (structure handles it)
        state.useAbsolutePos = true; // Docking sets absolute pos
    } else {
      // When not docked, keep absolutePos updated if using absolute positioning
      // This ensures we always have the latest position to restore when docking
      if (state.useAbsolutePos && !state.dragging && !state.resizing) {
          state.absolutePos = state.position;
      }
      // IMPORTANTE: Si el panel no es dockeable pero fue arrastrado previamente,
      // resetear su estado para que vuelva a seguir el layout
      if (!state.isDockable && state.hasBeenDragged) {
        state.hasBeenDragged = false;
        state.useAbsolutePos = false;
        state.dragPositionOffset = Vec2(0.0f, 0.0f);
        state.position = ctx->cursorPos;
      }
      
      // IMPORTANTE: Actualizar posición del panel en cada frame si NO usa posición absoluta
      // Esto permite que el panel se mueva con el scroll de contenedores padre (TabView, ScrollView)
      // La posición es: cursor del layout + offset del drag del usuario
      if (!state.useAbsolutePos && !state.dragging && !state.resizing) {
        state.position = ctx->cursorPos + state.dragPositionOffset;
      }
        
      // Allow external size updates (e.g. from Splitter or Layout)
      // Only if not currently being resized by the user using the panel grip
      if (!state.resizing) {
        if (desiredSize.x > 0.0f) state.size.x = desiredSize.x;
        if (desiredSize.y > 0.0f) state.size.y = desiredSize.y;
      }
    }
  }

  const PanelStyle &panelStyle = ctx->style.panel;
  float titleHeight = panelStyle.headerText.fontSize + panelStyle.padding.y * 2.0f;

  if (isDocked) {
    titleHeight = 0.0f; // No title bar if docked (Tabs handle it)
  }

  if (reserveLayoutSpace && !isDocked) {
    if (desiredSize.x > 0.0f) {
      state.reservedLayoutSize.x = desiredSize.x;
      state.expandedLayoutSize.x = desiredSize.x;
    }
    if (desiredSize.y > 0.0f) {
      state.reservedLayoutSize.y =
          state.minimized ? titleHeight : desiredSize.y;
      state.expandedLayoutSize.y = desiredSize.y;
    }
  }

  Vec2 mousePos(ctx->input.MouseX(), ctx->input.MouseY());
  bool leftDown = ctx->input.IsMouseDown(0);
  bool leftPressed = ctx->input.IsMousePressed(0);

  Vec2 titleSize(state.size.x, titleHeight);

  // Declare variables here so they are available in the drawing block later
  Vec2 minimizeButtonSize(titleHeight - panelStyle.padding.y * 1.5f, titleHeight - panelStyle.padding.y * 1.5f);
  Vec2 minimizeButtonPos(state.position.x + state.size.x - minimizeButtonSize.x - panelStyle.padding.x, state.position.y + (titleHeight - minimizeButtonSize.y) * 0.5f);

  if (!isDocked) {
    if (leftPressed && PointInRect(mousePos, minimizeButtonPos, minimizeButtonSize)) {
      state.minimized = !state.minimized;
      state.dragging = false;
      state.resizing = false;
    }
  }
  
  if (state.dragging) {
    // Safety check: If panel is not dockable, stop dragging immediately
    if (!state.isDockable) {
      state.dragging = false;
      state.hasBeenDragged = false;
      state.useAbsolutePos = false;
      state.dragPositionOffset = Vec2(0.0f, 0.0f);
      state.position = ctx->cursorPos;
      if (ctx->draggedPanelId == id) {
        ctx->draggedPanelId = "";
      }
    }
    // Safety check: If another panel is already registered as the dragged panel this frame,
    // force this panel to stop dragging. This auto-corrects any multi-drag state bugs.
    else if (!ctx->draggedPanelId.empty() && ctx->draggedPanelId != id) {
      state.dragging = false;
    } else {
      ctx->draggedPanelId = id; // Set global dragged panel
      if (!leftDown) {
        state.dragging = false;
        ctx->draggedPanelId = ""; // Clear global dragged panel
        // Guardar el offset de la posición arrastrada relativo al cursor del layout
        if (!state.useAbsolutePos) {
          state.dragPositionOffset = state.position - ctx->cursorPos;
          state.hasBeenDragged = true;
        }
      } else {
        state.position = Vec2(mousePos.x - state.dragOffset.x, mousePos.y - state.dragOffset.y);
      }
    }
  }


  // Manejar resize
  if (state.resizing) {
    if (!leftDown) {
      state.resizing = false;
    } else {
      Vec2 delta(mousePos.x - state.resizeStartMouse.x, mousePos.y - state.resizeStartMouse.y);
      Vec2 newSize = Vec2(state.resizeStartSize.x + delta.x, state.resizeStartSize.y + delta.y);
      float minWidth = 120.0f;
      float minHeight = titleHeight + 40.0f;
      state.size.x = std::max(newSize.x, minWidth);
      state.size.y = std::max(newSize.y, minHeight);
    }
  }

  // Iniciar drag o resize
  // Only start drag if no other panel is currently being dragged (prevents moving multiple overlapping panels)
  // Z-Order Logic: Keep track of which panel is hovered.
  // Since we render back-to-front, the LAST panel to report "I am hovered" is the one on top.
  // We store this in `nextHoveredPanelId` for the NEXT frame to use.
  if (PointInRect(mousePos, state.position, state.size)) { // Check full panel size for hover
       ctx->nextHoveredPanelId = id;
  }

  // Iniciar drag o resize
  // Only start drag if no other panel is currently being dragged (prevents moving multiple overlapping panels)
  // AND if we were the top-most hovered panel last frame.
  // IMPORTANT: Only allow dragging if panel is dockable (panels inside containers should not be draggable)
  bool canDrag = ctx->draggedPanelId.empty() && !state.dragging && !state.resizing && !isDocked && state.isDockable;
  
  // Also check if we are the top-most panel (using last frame's data)
  // If hoveredPanelId is empty, maybe we are the only one? Or first frame?
  // If we are hovered, we should match hoveredPanelId.
  if (canDrag && !ctx->hoveredPanelId.empty() && ctx->hoveredPanelId != id) {
      canDrag = false;
  }
  
  // DEBUG LOGGING
  // Force re-read to be absolutely sure
  bool actualClick = ctx->input.IsMousePressed(0);
  if (actualClick) {
      bool hit = PointInRect(mousePos, state.position, titleSize);

      if (!canDrag) {
           if (!ctx->draggedPanelId.empty()) std::cout << "   -> Msg: Another panel dragging: " << ctx->draggedPanelId << std::endl;
           if (state.dragging) std::cout << "   -> Msg: Already dragging" << std::endl;
           if (isDocked) std::cout << "   -> Msg: Panel is docked" << std::endl;
           if (!state.isDockable) std::cout << "   -> Msg: Panel is not dockable" << std::endl;
           if (!ctx->hoveredPanelId.empty() && ctx->hoveredPanelId != id) std::cout << "   -> Msg: Not top panel (Top is " << ctx->hoveredPanelId << ")" << std::endl;
      }
  }

  if (canDrag) {
      if (leftPressed && PointInRect(mousePos, state.position, titleSize)) {
        // Allow dragging only for dockable panels
        state.dragging = true;
        state.hasBeenDragged = true; // Mark as manually moved
        state.useAbsolutePos = true; // Use absolute positioning while dragging
        state.dragOffset = Vec2(mousePos.x - state.position.x, mousePos.y - state.position.y);
        state.dragPositionOffset = state.position - ctx->cursorPos;
        
        // Register in docking system (we know isDockable is true here)
        ctx->draggedPanelId = id; // Claim the drag for docking system
      } else if (!state.minimized) {
        Vec2 resizeHandleSize(12.0f, 12.0f);
        Vec2 resizeHandlePos(state.position.x + state.size.x - resizeHandleSize.x, state.position.y + state.size.y - resizeHandleSize.y);
        if (leftPressed && PointInRect(mousePos, resizeHandlePos, resizeHandleSize)) {
          state.resizing = true;
          state.resizeStartMouse = mousePos;
          state.resizeStartSize = state.size;
        }
      }
    }
  Vec2 viewport = ctx->renderer.GetViewportSize();
  // Only clamp to viewport logic... (existing)
  float maxPosX = std::max(0.0f, viewport.x - state.size.x);
  float maxPosY = std::max(
      0.0f, viewport.y - (state.minimized ? titleHeight : state.size.y));
  
  // Docked panels assume valid position/size by dock node
  if ((state.useAbsolutePos || state.dragging || state.resizing) && !isDocked) {
    state.position.x = std::clamp(state.position.x, 0.0f, maxPosX);
    state.position.y = std::clamp(state.position.y, 0.0f, maxPosY);
  }
  
  if (!isDocked) { // Only clamp size if floating
      state.size.x = std::clamp(state.size.x, 100.0f, viewport.x);
      state.size.y = std::clamp(state.size.y, titleHeight, viewport.y);
  }

  // Registrar área ocupada del panel ANTES de dibujarlo para que otros widgets
  // lo vean - SOLO si no es flotante (absolute pos)
  Vec2 panelSize =
      state.minimized ? Vec2(state.size.x, titleHeight) : state.size;
  if (!state.useAbsolutePos) {
       RegisterOccupiedArea(ctx, state.position, panelSize);
  }

  // Logic to break out of parent clip if floating (dragged or moved)
  state.pushedOverrideClip = false;
  if ((state.dragging || state.hasBeenDragged) && !isDocked) {
       ctx->renderer.PushClipRect(Vec2(0,0), viewport, true);
       state.pushedOverrideClip = true;
  }

  Color panelColor = panelStyle.background;
  Color titleColor = panelStyle.headerBackground;

  if (!state.minimized) {
    if (panelStyle.shadowOpacity > 0.0f) {
      ctx->renderer.DrawRectFilled(
          state.position + Vec2(0.0f, panelStyle.shadowOffsetY), state.size,
          Color(0.0f, 0.0f, 0.0f, panelStyle.shadowOpacity),
          panelStyle.cornerRadius);
    }

    // Usar efecto acrylic según la configuración específica del panel
    if (state.useAcrylic) {
      ctx->renderer.DrawRectAcrylic(state.position, state.size, panelColor,
                                    panelStyle.cornerRadius,
                                    state.acrylicOpacity);
    } else {
      ctx->renderer.DrawRectFilled(state.position, state.size, panelColor,
                                   panelStyle.cornerRadius);
    }
  } else {
    if (state.useAcrylic) {
      ctx->renderer.DrawRectAcrylic(
          state.position, Vec2(state.size.x, titleHeight), panelColor,
          panelStyle.cornerRadius, state.acrylicOpacity);
    } else {
      ctx->renderer.DrawRectFilled(state.position,
                                   Vec2(state.size.x, titleHeight), panelColor,
                                   panelStyle.cornerRadius);
    }
  }

  bool minimizeHover = false;
  bool buttonPressed = false;
  bool panelMinimized = state.minimized;
  
  if (!isDocked) {
      // Header Background
       ctx->renderer.DrawRectFilled(
          state.position, Vec2(state.size.x, titleHeight),
          panelStyle.headerBackground, panelStyle.cornerRadius); 
          
      // Header Text
      Vec2 textSize =
          ctx->renderer.MeasureText(id, panelStyle.headerText.fontSize);
      Vec2 textPos(state.position.x + panelStyle.padding.x,
                   state.position.y + (titleHeight - textSize.y) * 0.5f);
      ctx->renderer.DrawText(textPos, id, panelStyle.headerText.color,
                             panelStyle.headerText.fontSize);
                             
      // Minimize Button Drawing
      minimizeHover = PointInRect(mousePos, minimizeButtonPos, minimizeButtonSize);
      buttonPressed = minimizeHover && ctx->input.IsMouseDown(0);
      
      auto resolveButtonColor = [&](const ColorState &stateColors) -> Color {
        if (panelMinimized) {
          return stateColors.disabled;
        }
        if (buttonPressed)
          return stateColors.pressed;
        if (minimizeHover)
          return stateColors.hover;
        return stateColors.normal;
      };
      
      ctx->renderer.DrawRectFilled(minimizeButtonPos, minimizeButtonSize,
                                   resolveButtonColor(panelStyle.titleButton),
                                   panelStyle.cornerRadius * 0.4f);
      std::string symbol = state.minimized ? "+" : "-";
      Vec2 symbolSize =
          ctx->renderer.MeasureText(symbol, panelStyle.headerText.fontSize * 0.8f);
      Vec2 symbolPos(
          minimizeButtonPos.x + (minimizeButtonSize.x - symbolSize.x) * 0.5f,
          minimizeButtonPos.y + (minimizeButtonSize.y - symbolSize.y) * 0.5f);
      ctx->renderer.DrawText(symbolPos, symbol, Color(1.0f, 1.0f, 1.0f, 1.0f),
                             panelStyle.headerText.fontSize * 0.8f);
  } else {
      // Docked panel: Draw undock button in top-right corner
      float undockButtonSize = 20.0f;
      Vec2 undockButtonPos(state.position.x + state.size.x - undockButtonSize - 4.0f,
                           state.position.y + 4.0f);
      Vec2 undockButtonSizeVec(undockButtonSize, undockButtonSize);
      
      bool undockHover = PointInRect(mousePos, undockButtonPos, undockButtonSizeVec);
      bool undockPressed = undockHover && ctx->input.IsMouseDown(0);
      
      Color undockButtonColor = undockPressed ? panelStyle.titleButton.pressed
                               : undockHover ? panelStyle.titleButton.hover
                               : panelStyle.titleButton.normal;
      
      ctx->renderer.DrawRectFilled(undockButtonPos, undockButtonSizeVec,
                                   undockButtonColor,
                                   panelStyle.cornerRadius * 0.4f);
      
      // Draw "pop-out" icon (a small square with an arrow)
      Vec2 iconCenter(undockButtonPos.x + undockButtonSize * 0.5f,
                      undockButtonPos.y + undockButtonSize * 0.5f);
      float iconSize = undockButtonSize * 0.4f;
      
      // Draw small square
      ctx->renderer.DrawRect(Vec2(iconCenter.x - iconSize * 0.5f, iconCenter.y - iconSize * 0.5f),
                            Vec2(iconSize, iconSize),
                            Color(1.0f, 1.0f, 1.0f, 1.0f),
                            2.0f);
      
      // Draw arrow pointing up-right
      Vec2 arrowStart(iconCenter.x - iconSize * 0.3f, iconCenter.y + iconSize * 0.3f);
      Vec2 arrowEnd(iconCenter.x + iconSize * 0.5f, iconCenter.y - iconSize * 0.5f);
      ctx->renderer.DrawLine(arrowStart, arrowEnd, Color(1.0f, 1.0f, 1.0f, 1.0f), 2.0f);
      
      // Handle undock button click
      if (undockHover && ctx->input.IsMousePressed(0)) {
          // Mark panel as manually undocked to prevent auto-redocking
          state.manuallyUndocked = true;
          
          // Remove panel from all dock nodes to prevent ghost tabs
          uint32_t nodeIdThatContainedPanel = 0;
          for (auto& [nodeId, node] : ctx->dockNodes) {
              auto it = std::find(node.dockedPanelIds.begin(), node.dockedPanelIds.end(), id);
              if (it != node.dockedPanelIds.end()) {
                  node.dockedPanelIds.erase(it);
                  nodeIdThatContainedPanel = nodeId;
                  // Adjust active tab if needed
                  if (node.activeTabIndex >= node.dockedPanelIds.size() && node.activeTabIndex > 0) {
                      node.activeTabIndex = node.dockedPanelIds.size() - 1;
                  }
              }
          }
          
          // Collapse empty nodes to free up the dock space
          if (nodeIdThatContainedPanel != 0) {
              CollapseEmptyNodes(ctx, nodeIdThatContainedPanel);
          }
          
          // Restore panel to its previous absolute position before docking
          Vec2 viewport = ctx->renderer.GetViewportSize();
          Vec2 restoredPos = state.absolutePos;
          
          // If we don't have a saved position (or it's at origin), use a default position near the mouse
          if ((restoredPos.x == 0.0f && restoredPos.y == 0.0f) || 
              restoredPos.x < 0.0f || restoredPos.y < 0.0f) {
              restoredPos = Vec2(mousePos.x - state.size.x * 0.5f, mousePos.y - 10.0f);
          }
          
          // Ensure panel is fully visible and not overlapping with viewport edges
          float padding = 20.0f;
          // Calculate safe bounds ensuring min <= max for clamp
          float minX = padding;
          float maxX = std::max(padding, viewport.x - state.size.x - padding);
          float minY = padding;
          float maxY = std::max(padding, viewport.y - state.size.y - padding);
          
          restoredPos.x = std::clamp(restoredPos.x, minX, maxX);
          restoredPos.y = std::clamp(restoredPos.y, minY, maxY);
          
          state.position = restoredPos;
          state.useAbsolutePos = true;
          state.hasBeenDragged = false; // Reset so panel can be moved freely immediately
          state.dragPositionOffset = Vec2(0.0f, 0.0f); // Reset drag offset
          isDocked = false;
      }
  }
  if (!state.minimized) {
    Vec2 resizeHandleSize(12.0f, 12.0f);
    Vec2 resizeHandlePos(state.position.x + state.size.x - resizeHandleSize.x,
                         state.position.y + state.size.y - resizeHandleSize.y);
    ctx->renderer.DrawRectFilled(resizeHandlePos, resizeHandleSize,
                                 panelStyle.titleButton.hover,
                                 panelStyle.cornerRadius * 0.3f);
  }

  Vec2 layoutOrigin = ctx->cursorPos;
  PanelFrameContext frameCtx{};
  frameCtx.id = panelId;
  frameCtx.layoutOrigin = layoutOrigin;
  frameCtx.titleHeight = titleHeight;
  frameCtx.clipPushed = false;
  frameCtx.layoutPushed = false;
  // Si se usa posición absoluta, no reservar espacio en el layout
  if (state.useAbsolutePos) {
    frameCtx.reserveLayout = false;
  } else {
    frameCtx.reserveLayout = reserveLayoutSpace;
  }
  if (!state.minimized) {
    state.expandedLayoutSize = state.size;
  }
  if (!state.minimized) {
    state.expandedLayoutSize = state.size;
    state.reservedLayoutSize = state.size;
  }
  Vec2 reservedSize = state.reservedLayoutSize;
  if (state.minimized) {
    reservedSize = Vec2(state.expandedLayoutSize.x, titleHeight);
  }
  frameCtx.reservedLayoutSize = reservedSize;
  frameCtx.savedCursor = ctx->cursorPos;
  frameCtx.savedLastItemPos = ctx->lastItemPos;
  frameCtx.savedLastItemSize = ctx->lastItemSize;
  if (!reserveLayoutSpace && !ctx->layoutStack.empty()) {
    const LayoutStack &parentStack = ctx->layoutStack.back();
    frameCtx.parentCursor = parentStack.cursor;
    frameCtx.parentContentSize = parentStack.contentSize;
    frameCtx.parentAvailable = parentStack.availableSpace;
    frameCtx.parentItemCount = parentStack.itemCount;
  }

  bool shouldRenderContent = !state.minimized;

  if (shouldRenderContent) {
    // For docked panels, position already points to content area
    float titleOffset = isDocked ? 0.0f : titleHeight;
    Vec2 clipPos(state.position.x + panelStyle.padding.x,
                 state.position.y + titleOffset + panelStyle.padding.y);
    Vec2 clipSize(state.size.x - panelStyle.padding.x * 2.0f,
                  state.size.y - titleOffset - panelStyle.padding.y * 2.0f);
    clipSize.x = std::max(0.0f, clipSize.x);
    clipSize.y = std::max(0.0f, clipSize.y);
    if (clipSize.x > 0.0f && clipSize.y > 0.0f) {
      ctx->renderer.PushClipRect(clipPos, clipSize);
      frameCtx.clipPos = clipPos;
      frameCtx.clipSize = clipSize;
      frameCtx.clipPushed = true;
    }

    float contentWidth =
        std::max(0.0f, state.size.x - panelStyle.padding.x * 2.0f);
    
    // For docked panels, state.position already points to content area (below tab bar)
    // So we should NOT add titleHeight again
    float contentHeight = std::max(0.0f, state.size.y - titleOffset -
                                             panelStyle.padding.y * 2.0f);
    Vec2 contentOrigin(state.position.x + panelStyle.padding.x,
                       state.position.y + titleOffset + panelStyle.padding.y);

    // Guardar el cursor original antes de configurar el layout del panel
    Vec2 savedCursorBeforeLayout = ctx->cursorPos;

    // Configurar el cursor dentro del panel
    ctx->cursorPos = contentOrigin;
    ctx->lastItemPos = contentOrigin;
    ctx->lastItemSize = Vec2(0.0f, 0.0f);

    // Empujar offset del área de contenido (similar a BeginScrollView)
    ctx->offsetStack.push_back(contentOrigin);

    // Iniciar layout vertical para organizar los widgets dentro del panel
    BeginVertical(ctx->style.spacing, Vec2(contentWidth, contentHeight), Vec2(0.0f, 0.0f));
    frameCtx.layoutPushed = true;
  }

  panelStack.push_back(frameCtx);

  // Manejar el cursor según el estado del panel y reserveLayoutSpace
  // IMPORTANTE: Si el panel está visible (shouldRenderContent = true),
  // NO tocar cursorPos aquí porque ya está configurado correctamente arriba
  // Si se usa posición absoluta, NO avanzar el cursor
  // NOTA: No avanzar el cursor aquí cuando el panel está minimizado
  // EndPanel se encargará de avanzar el cursor correctamente usando displayedSize
  if (!shouldRenderContent && !state.useAbsolutePos) {
    // Panel minimizado: solo resetear cursor a layoutOrigin
    // El avance se hará en EndPanel
    ctx->cursorPos = layoutOrigin;
  }
  // Si shouldRenderContent es true, el cursor ya está correctamente configurado
  // en contentOrigin arriba (línea 1008), no hacer nada más

  return shouldRenderContent;
}

void EndPanel() {
  UIContext *ctx = GetContext();
  if (!ctx)
    return;
  if (panelStack.empty())
    return;

  PanelFrameContext frameCtx = panelStack.back();
  panelStack.pop_back();
  auto it = ctx->panelStates.find(frameCtx.id);
  if (it == ctx->panelStates.end())
    return;

  auto &state = it->second;

  // Sacar offset del área de contenido (similar a EndScrollView)
  if (!ctx->offsetStack.empty()) {
    ctx->offsetStack.pop_back();
  }

  // Guardar el ancho máximo del contenido antes de cerrar el layout
  float maxContentWidth = 0.0f;
  if (frameCtx.layoutPushed && !ctx->layoutStack.empty()) {
    const auto &lastStack = ctx->layoutStack.back();
    maxContentWidth = lastStack.contentSize.x;
  }
  
  // Cerrar el layout vertical si fue creado
  if (frameCtx.layoutPushed) {
    EndVertical(false); // No avanzar el cursor del padre aquí
  }
  
  // Ajustar el ancho del Panel según el contenido si no se especificó un tamaño fijo
  if (!state.useAbsolutePos && frameCtx.layoutPushed) {
    const PanelStyle &panelStyle = ctx->style.panel;
    float contentAreaWidth = std::max(0.0f, state.size.x - panelStyle.padding.x * 2.0f);
    float newWidth = std::max(contentAreaWidth, maxContentWidth + panelStyle.padding.x * 2.0f);
    // Solo actualizar si el contenido es más ancho
    if (newWidth > state.size.x) {
      state.size.x = newWidth;
    }
  }

  if (frameCtx.clipPushed) {
    ctx->renderer.PopClipRect();
  }
  
  // Pop override clip if we pushed it
  if (state.pushedOverrideClip) {
       ctx->renderer.PopClipRect();
  }
  float titleHeight = frameCtx.titleHeight;
  Vec2 displayedSize(state.size.x,
                     state.minimized ? titleHeight : state.size.y);

  // Only restore cursor if NOT reserving layout space
  // When reserving layout space, we want the cursor to advance from current position
  if (!frameCtx.reserveLayout || state.useAbsolutePos) {
    ctx->cursorPos = frameCtx.savedCursor;
    ctx->lastItemPos = frameCtx.savedLastItemPos;
    ctx->lastItemSize = frameCtx.savedLastItemSize;
  }

  // Only restore parent layout state if NOT reserving layout space
  if (!ctx->layoutStack.empty() && (!frameCtx.reserveLayout || state.useAbsolutePos)) {
    LayoutStack &parentStack = ctx->layoutStack.back();
    parentStack.cursor = frameCtx.parentCursor;
    parentStack.contentSize = frameCtx.parentContentSize;
    parentStack.availableSpace = frameCtx.parentAvailable;
    parentStack.itemCount = frameCtx.parentItemCount;
  }

  // Avanzar cursor si se reserva espacio en layout
  if (frameCtx.reserveLayout && !state.useAbsolutePos) {
    // Usar displayedSize que ya tiene en cuenta si el panel está minimizado
    Vec2 advanceSize = displayedSize;
    
    // Registrar el panel como item para el layout del padre SOLO si no es flotante
  ctx->lastItemPos = state.position;
  if (!state.useAbsolutePos) {
       AdvanceCursor(ctx, displayedSize);
  }
  }
}


void Spacing(float pixels) {
  UIContext *ctx = GetContext();
  if (!ctx)
    return;

  if (ctx->layoutStack.empty()) {
    ctx->cursorPos.y += pixels;
    return;
  }

  LayoutStack &stack = ctx->layoutStack.back();
  if (stack.isVertical) {
    stack.cursor.y += pixels;
    stack.availableSpace.y = std::max(0.0f, stack.availableSpace.y - pixels);
    stack.contentSize.y += pixels;
    ctx->cursorPos = Vec2(stack.cursor.x, stack.cursor.y);
  } else {
    stack.cursor.x += pixels;
    stack.availableSpace.x = std::max(0.0f, stack.availableSpace.x - pixels);
    stack.contentSize.x += pixels;
    ctx->cursorPos = Vec2(stack.cursor.x, stack.contentStart.y);
  }
}

void SameLine(float offset) {
  UIContext *ctx = GetContext();
  if (!ctx)
    return;
  Vec2 newPos(ctx->lastItemPos.x + ctx->lastItemSize.x + offset,
              ctx->lastItemPos.y);
  ctx->cursorPos = newPos;

  if (!ctx->layoutStack.empty()) {
    LayoutStack &stack = ctx->layoutStack.back();
    stack.cursor = newPos;
    if (stack.isVertical) {
      float usedWidth = newPos.x - stack.contentStart.x;
      stack.contentSize.x = std::max(stack.contentSize.x, usedWidth);
    }
  }
}

bool Checkbox(const std::string &label, bool *value, const Vec2 &pos) {
  UIContext *ctx = GetContext();
  if (!ctx)
    return false;

  Vec2 boxSize(20.0f, 20.0f);
  const TextStyle &textStyle = ctx->style.GetTextStyle(TypographyStyle::Body);
  Vec2 textSize = ctx->renderer.MeasureText(label, textStyle.fontSize);
  Vec2 totalSize(boxSize.x + 8.0f + textSize.x,
                 std::max(boxSize.y, textSize.y));
  LayoutConstraints constraints = ConsumeNextConstraints();
  Vec2 layoutSize = ApplyConstraints(ctx, constraints, totalSize);

  // Resolver posición: usar pos si se proporciona, sino usar cursor
  // directamente
  Vec2 boxPos;
  if (pos.x != 0.0f || pos.y != 0.0f) {
    boxPos = ResolveAbsolutePosition(ctx, pos, layoutSize);
  } else {
    boxPos = ctx->cursorPos;
  }

  std::string idStr = "CHK:" + label;
  uint32_t id = GenerateId(idStr.c_str());

  auto boolEntry = ctx->boolStates.try_emplace(id, false);
  bool currentValue = value ? *value : boolEntry.first->second;

  float mouseX = ctx->input.MouseX();
  float mouseY = ctx->input.MouseY();
  bool hover = (mouseX >= boxPos.x && mouseX <= boxPos.x + boxSize.x &&
                mouseY >= boxPos.y && mouseY <= boxPos.y + boxSize.y);

  bool toggled = false;
  if (hover && ctx->input.IsMousePressed(0)) {
    currentValue = !currentValue;
    toggled = true;
  }

  if (value)
    *value = currentValue;
  else
    boolEntry.first->second = currentValue;

  const PanelStyle &panelStyle = ctx->style.panel;
  const ColorState &toggleAccent = ctx->style.button.background;
  
  // Fondo del checkbox - contraste sutil sin borde
  // Fondo del checkbox - contraste consistente con otros inputs (TextInput, ComboBox)
  Color boxFill;
  bool isDarkTheme = (ctx->style.backgroundColor.r < 0.5f);
  
  if (isDarkTheme) {
    // Fondo oscuro charcoal gray más profundo (#262626 ~ 0.15f)
    boxFill = Color(0.15f, 0.15f, 0.15f, 1.0f);
    if (hover) {
      // Ligeramente más claro al hover (#333333 ~ 0.2f)
      boxFill = Color(0.2f, 0.2f, 0.2f, 1.0f);
    }
  } else {
    // Tema claro
    boxFill = Color(0.96f, 0.96f, 0.96f, 1.0f);
    if (hover) {
      boxFill = Color(0.92f, 0.92f, 0.92f, 1.0f);
    }
  }
  
  // Relleno sin borde - solo contraste de fondo con esquinas redondeadas
  ctx->renderer.DrawRectFilled(boxPos, boxSize, boxFill,
                               panelStyle.cornerRadius * 0.6f);

  if (currentValue) {
    Vec2 innerPos(boxPos.x + 4.0f, boxPos.y + 4.0f);
    Vec2 innerSize(boxSize.x - 8.0f, boxSize.y - 8.0f);
    Color fillColor = hover ? toggleAccent.hover : toggleAccent.normal;
    ctx->renderer.DrawRectFilled(innerPos, innerSize, fillColor,
                                 panelStyle.cornerRadius * 0.4f);
  }

  Vec2 textPos(boxPos.x + boxSize.x + 8.0f,
               boxPos.y + (boxSize.y - textSize.y) * 0.5f);
  ctx->renderer.DrawText(textPos, label, textStyle.color, textStyle.fontSize);

  RegisterOccupiedArea(ctx, boxPos, layoutSize);

  ctx->lastItemPos = boxPos;
  AdvanceCursor(ctx, layoutSize);
  return toggled;
}

bool ToggleSwitch(const std::string &label, bool *value, const Vec2 &pos) {
  UIContext *ctx = GetContext();
  if (!ctx) return false;

  Vec2 switchSize(40.0f, 20.0f);
  const TextStyle &textStyle = ctx->style.GetTextStyle(TypographyStyle::Body);
  Vec2 textSize = ctx->renderer.MeasureText(label, textStyle.fontSize);
  Vec2 totalSize(switchSize.x + 8.0f + textSize.x,
                 std::max(switchSize.y, textSize.y));

  LayoutConstraints constraints = ConsumeNextConstraints();
  Vec2 layoutSize = ApplyConstraints(ctx, constraints, totalSize);

  Vec2 switchPos;
  if (pos.x != 0.0f || pos.y != 0.0f) {
    switchPos = ResolveAbsolutePosition(ctx, pos, layoutSize);
  } else {
    switchPos = ctx->cursorPos;
  }

  std::string idStr = "SW:" + label;
  uint32_t id = GenerateId(idStr.c_str());

  auto boolEntry = ctx->boolStates.try_emplace(id, false);
  bool currentValue = value ? *value : boolEntry.first->second;

  float mouseX = ctx->input.MouseX();
  float mouseY = ctx->input.MouseY();
  bool hover = (mouseX >= switchPos.x && mouseX <= switchPos.x + switchSize.x &&
                mouseY >= switchPos.y && mouseY <= switchPos.y + switchSize.y);

  bool toggled = false;
  if (hover && ctx->input.IsMousePressed(0)) {
    currentValue = !currentValue;
    toggled = true;
  }

  if (value) *value = currentValue;
  else boolEntry.first->second = currentValue;

  // Draw Switch Track
  Color trackColor;
  if (currentValue) {
    trackColor = ctx->style.button.background.normal; // Active color (using button accent)
  } else {
    trackColor = ctx->style.panel.borderColor; // Inactive border-like color
    if (hover) {
        // Slightly lighter on hover
        trackColor = Color(trackColor.r + 0.2f, trackColor.g + 0.2f, trackColor.b + 0.2f, 1.0f);
    }
  }
  
  // Fill track
  float radius = switchSize.y * 0.5f;
  ctx->renderer.DrawRectFilled(switchPos, switchSize, trackColor, radius);

  // Draw Thumb
  float thumbPadding = 3.0f;
  float thumbSize = switchSize.y - thumbPadding * 2.0f;
  float thumbX = currentValue 
      ? (switchPos.x + switchSize.x - thumbSize - thumbPadding) 
      : (switchPos.x + thumbPadding);
  
  Vec2 thumbPosVec(thumbX, switchPos.y + thumbPadding);
  
  // Thumb color: usually white for active, or secondary text for inactive
  Color thumbColor = (currentValue) ? Color(1.0f, 1.0f, 1.0f, 1.0f) : ctx->style.typography.body.color;

  ctx->renderer.DrawRectFilled(thumbPosVec, Vec2(thumbSize, thumbSize), thumbColor, thumbSize * 0.5f);

  // Label
  Vec2 textPos2(switchPos.x + switchSize.x + 8.0f,
               switchPos.y + (switchSize.y - textSize.y) * 0.5f);
  ctx->renderer.DrawText(textPos2, label, textStyle.color, textStyle.fontSize);

  RegisterOccupiedArea(ctx, switchPos, layoutSize);
  ctx->lastItemPos = switchPos;
  AdvanceCursor(ctx, layoutSize);

  return toggled;
}

bool SpinBox(const std::string &label, int *value, int min, int max, float width, const Vec2 &pos) {
  UIContext *ctx = GetContext();
  if (!ctx || !value) return false;

  float height = 30.0f;
  Vec2 boxSize(width, height);
  
  LayoutConstraints constraints = ConsumeNextConstraints();
  Vec2 layoutSize = ApplyConstraints(ctx, constraints, boxSize); // Label space not included in width param usually, but let's check standard. 
  // Let's assume 'width' is for the control itself.
  
  const TextStyle &textStyle = ctx->style.GetTextStyle(TypographyStyle::Body);
  Vec2 textSize = ctx->renderer.MeasureText(label, textStyle.fontSize);
  
  // Total size = control + padding + label
  Vec2 totalSize(boxSize.x + 8.0f + textSize.x, std::max(boxSize.y, textSize.y));
  
  // Recalculate layout size with full content
  layoutSize = ApplyConstraints(ctx, constraints, totalSize);

  Vec2 startPos;
  if (pos.x != 0.0f || pos.y != 0.0f) {
    startPos = ResolveAbsolutePosition(ctx, pos, layoutSize);
  } else {
    startPos = ctx->cursorPos;
  }

  // Draw Background (Container)
  // Reusing logic from TextInput for background color
  Color bgColor;
  bool isDarkTheme = (ctx->style.backgroundColor.r < 0.5f);
  if (isDarkTheme) {
    bgColor = Color(0.15f, 0.15f, 0.15f, 1.0f);
  } else {
    bgColor = Color(0.96f, 0.96f, 0.96f, 1.0f);
  }

  ctx->renderer.DrawRectFilled(startPos, boxSize, bgColor, 8.0f);
  // Border removed as per user request
  // ctx->renderer.DrawRect(startPos, boxSize, ctx->style.panel.borderColor, 4.0f);

  // Buttons Logic
  float btnWidth = 25.0f;
  Vec2 decBtnPos = startPos;
  Vec2 incBtnPos(startPos.x + boxSize.x - btnWidth, startPos.y);
  Vec2 btnSize(btnWidth, height);

  // Value Display Area
  float valueAreaWidth = boxSize.x - btnWidth * 2.0f;
  Vec2 valPos(startPos.x + btnWidth, startPos.y);
  
  bool changed = false;
  float mouseX = ctx->input.MouseX();
  float mouseY = ctx->input.MouseY();
  bool leftPressed = ctx->input.IsMousePressed(0);

  // Decrement Button
  bool hoverDec = PointInRect(Vec2(mouseX, mouseY), decBtnPos, btnSize);
  // Draw "-"
  if (hoverDec) {
     ctx->renderer.DrawRectFilled(decBtnPos, btnSize, ctx->style.button.background.hover, 4.0f);
     if (leftPressed) {
         *value = std::max(min, *value - 1);
         changed = true;
     }
  }
  ctx->renderer.DrawText(decBtnPos + Vec2(8, 4), "-", textStyle.color);

  // Increment Button
  bool hoverInc = PointInRect(Vec2(mouseX, mouseY), incBtnPos, btnSize);
  // Draw "+"
  if (hoverInc) {
      ctx->renderer.DrawRectFilled(incBtnPos, btnSize, ctx->style.button.background.hover, 4.0f);
      if (leftPressed) {
          *value = std::min(max, *value + 1);
          changed = true;
      }
  }
   ctx->renderer.DrawText(incBtnPos + Vec2(6, 4), "+", textStyle.color);

  // Text Value
  std::string valStr = std::to_string(*value);
  Vec2 valTextSize = ctx->renderer.MeasureText(valStr, textStyle.fontSize);
  // Center text
  Vec2 textDrawPos(valPos.x + (valueAreaWidth - valTextSize.x) * 0.5f, 
                   valPos.y + (height - valTextSize.y) * 0.5f);
  ctx->renderer.DrawText(textDrawPos, valStr, textStyle.color, textStyle.fontSize);
  
  // Label on right
  Vec2 labelPos(startPos.x + boxSize.x + 8.0f, startPos.y + (height - textSize.y) * 0.5f);
  ctx->renderer.DrawText(labelPos, label, textStyle.color, textStyle.fontSize);

  RegisterOccupiedArea(ctx, startPos, layoutSize);
  ctx->lastItemPos = startPos;
  AdvanceCursor(ctx, layoutSize);
  
  return changed;
}

bool RadioButton(const std::string &label, int *value, int optionValue,
                 const std::string &group, const Vec2 &pos) {
  UIContext *ctx = GetContext();
  if (!ctx)
    return false;

  Vec2 circleSize(20.0f, 20.0f);
  const TextStyle &textStyle = ctx->style.GetTextStyle(TypographyStyle::Body);
  Vec2 textSize = ctx->renderer.MeasureText(label, textStyle.fontSize);
  Vec2 totalSize(circleSize.x + 8.0f + textSize.x,
                 std::max(circleSize.y, textSize.y));
  LayoutConstraints constraints = ConsumeNextConstraints();
  Vec2 layoutSize = ApplyConstraints(ctx, constraints, totalSize);

  // Resolver posición: usar pos si se proporciona, sino usar cursor
  // directamente
  Vec2 circlePos;
  if (pos.x != 0.0f || pos.y != 0.0f) {
    circlePos = ResolveAbsolutePosition(ctx, pos, layoutSize);
  } else {
    circlePos = ctx->cursorPos;
  }
  Vec2 circleCenter(circlePos.x + circleSize.x * 0.5f,
                    circlePos.y + circleSize.y * 0.5f);
  float radius = circleSize.x * 0.5f;

  std::string groupKey = group.empty() ? "DEFAULT_RADIO_GROUP" : group;
  std::string idStr = "RADIO:" + groupKey + ":" + label;
  uint32_t id = GenerateId(idStr.c_str());

  // Obtener valor actual del grupo
  int currentValue = value ? *value : 0;
  bool isSelected = (currentValue == optionValue);

  float mouseX = ctx->input.MouseX();
  float mouseY = ctx->input.MouseY();
  float distFromCenter =
      std::sqrt((mouseX - circleCenter.x) * (mouseX - circleCenter.x) +
                (mouseY - circleCenter.y) * (mouseY - circleCenter.y));
  bool hover = distFromCenter <= radius;

  bool clicked = false;
  if (hover && ctx->input.IsMousePressed(0)) {
    if (value) {
      *value = optionValue;
    }
    clicked = true;
  }

  const PanelStyle &panelStyle = ctx->style.panel;
  const ColorState &accentState = ctx->style.button.background;

  // Fondo del radio button - contraste sutil sin borde
  // Fondo del radio button - contraste consistente con otros inputs
  Color circleFill;
  bool isDarkTheme = (ctx->style.backgroundColor.r < 0.5f);
  
  if (isDarkTheme) {
    // Fondo oscuro charcoal gray más profundo (#262626 ~ 0.15f)
    circleFill = Color(0.15f, 0.15f, 0.15f, 1.0f);
    if (hover) {
      // Ligeramente más claro al hover (#333333 ~ 0.2f)
      circleFill = Color(0.2f, 0.2f, 0.2f, 1.0f);
    }
  } else {
    // Tema claro
    circleFill = Color(0.96f, 0.96f, 0.96f, 1.0f);
    if (hover) {
      circleFill = Color(0.92f, 0.92f, 0.92f, 1.0f);
    }
  }
  
  // Círculo de fondo con contraste (sin borde)
  ctx->renderer.DrawCircle(circleCenter, radius, circleFill, true);

  // Dibujar círculo interior si está seleccionado
  if (isSelected) {
    float innerRadius = radius * 0.5f;
    Color fillColor = hover ? accentState.hover : accentState.normal;
    ctx->renderer.DrawCircle(circleCenter, innerRadius, fillColor, true);
  }

  Vec2 textPos(circlePos.x + circleSize.x + 8.0f,
               circlePos.y + (circleSize.y - textSize.y) * 0.5f);
  ctx->renderer.DrawText(textPos, label, textStyle.color, textStyle.fontSize);

  RegisterOccupiedArea(ctx, circlePos, layoutSize);

  ctx->lastItemPos = circlePos;
  AdvanceCursor(ctx, layoutSize);
  return clicked;
}

static float CalculateSliderFraction(float value, float minValue,
                                     float maxValue) {
  if (maxValue - minValue == 0.0f)
    return 0.0f;
  return std::clamp((value - minValue) / (maxValue - minValue), 0.0f, 1.0f);
}

bool SliderFloat(const std::string &label, float *value, float minValue,
                 float maxValue, float width, const char *format,
                 const Vec2 &pos) {
  UIContext *ctx = GetContext();
  if (!ctx)
    return false;

  float labelSpacing = 4.0f;
  float sliderHeight = 20.0f;
  const TextStyle &labelStyle = ctx->style.GetTextStyle(TypographyStyle::Body);
  const TextStyle &valueStyle =
      ctx->style.GetTextStyle(TypographyStyle::Caption);
  const PanelStyle &panelStyle = ctx->style.panel;
  const ColorState &accentState = ctx->style.button.background;

  Vec2 labelSize = ctx->renderer.MeasureText(label, labelStyle.fontSize);
  float trackWidth = width > 0.0f ? width : 200.0f;
  Vec2 trackSize(trackWidth, sliderHeight);

  // Calcular tamaño total primero
  Vec2 totalSize(trackSize.x + 100.0f,
                 labelSize.y + labelSpacing + trackSize.y);
  LayoutConstraints constraints = ConsumeNextConstraints();
  Vec2 finalSize = ApplyConstraints(ctx, constraints, totalSize);

  // Resolver posición: usar pos si se proporciona, sino usar cursor
  // directamente
  Vec2 widgetPos;
  if (pos.x != 0.0f || pos.y != 0.0f) {
    widgetPos = ResolveAbsolutePosition(ctx, pos, finalSize);
  } else {
    widgetPos = ctx->cursorPos;
  }
  Vec2 trackPos(widgetPos.x, widgetPos.y + labelSize.y + labelSpacing);

  std::string idStr = "SLDR_F:" + label;
  uint32_t id = GenerateId(idStr.c_str());

  auto floatEntry = ctx->floatStates.try_emplace(id, minValue);
  float currentValue = value ? *value : floatEntry.first->second;
  currentValue = std::clamp(currentValue, minValue, maxValue);

  float mouseX = ctx->input.MouseX();
  float mouseY = ctx->input.MouseY();
  bool hover = (mouseX >= trackPos.x && mouseX <= trackPos.x + trackSize.x &&
                mouseY >= trackPos.y && mouseY <= trackPos.y + trackSize.y);

  bool valueChanged = false;
  if (hover && ctx->input.IsMousePressed(0)) {
    ctx->activeWidgetId = id;
    ctx->activeWidgetType = ActiveWidgetType::Slider;
  }

  if (ctx->activeWidgetId == id &&
      ctx->activeWidgetType == ActiveWidgetType::Slider) {
    if (ctx->input.IsMouseDown(0)) {
      float relative =
          std::clamp((mouseX - trackPos.x) / trackSize.x, 0.0f, 1.0f);
      float newValue = minValue + relative * (maxValue - minValue);
      if (std::abs(newValue - currentValue) > 0.0001f) {
        currentValue = newValue;
        valueChanged = true;
      }
    } else {
      ctx->activeWidgetId = 0;
      ctx->activeWidgetType = ActiveWidgetType::None;
    }
  }

  if (value)
    *value = currentValue;
  else
    floatEntry.first->second = currentValue;

  float fraction = CalculateSliderFraction(currentValue, minValue, maxValue);

  if (!format) {
    format = "%.2f";
  }
  char valueBuffer[64];
  std::snprintf(valueBuffer, sizeof(valueBuffer), format, currentValue);
  std::string valueText(valueBuffer);
  Vec2 valueSize = ctx->renderer.MeasureText(valueText, valueStyle.fontSize);
  Vec2 valuePos(trackPos.x + trackSize.x + 8.0f,
                trackPos.y + (trackSize.y - valueSize.y) * 0.5f);
  if (finalSize.x > 0.0f) {
    float adjustedTrackWidth =
        std::max(10.0f, finalSize.x - valueSize.x - 8.0f);
    trackSize.x = adjustedTrackWidth;
    valuePos.x = trackPos.x + trackSize.x + 8.0f;
    totalSize.x = finalSize.x;
  }

  if (finalSize.y > totalSize.y) {
    totalSize.y = finalSize.y;
  }

  Color trackColor =
      hover ? panelStyle.headerBackground : panelStyle.background;
  ctx->renderer.DrawRectFilled(trackPos, trackSize, trackColor,
                               panelStyle.cornerRadius);

  Vec2 fillSize(trackSize.x * fraction, trackSize.y);
  ctx->renderer.DrawRectFilled(trackPos, fillSize, accentState.normal,
                               panelStyle.cornerRadius);

  Vec2 knobSize(6.0f, trackSize.y + 4.0f);
  Vec2 knobPos(trackPos.x + fillSize.x - knobSize.x * 0.5f, trackPos.y - 2.0f);
  Color knobColor = hover ? accentState.hover : accentState.normal;
  ctx->renderer.DrawRectFilled(knobPos, knobSize, knobColor,
                               panelStyle.cornerRadius);

  ctx->renderer.DrawText(widgetPos, label, labelStyle.color,
                         labelStyle.fontSize);

  ctx->renderer.DrawText(valuePos, valueText, valueStyle.color,
                         valueStyle.fontSize);

  RegisterOccupiedArea(ctx, widgetPos, finalSize);

  ctx->lastItemPos = widgetPos;
  AdvanceCursor(ctx, finalSize);
  return valueChanged;
}

bool SliderInt(const std::string &label, int *value, int minValue, int maxValue,
               float width, const Vec2 &pos) {
  UIContext *ctx = GetContext();
  if (!ctx)
    return false;

  if (width <= 0.0f) {
    width = 200.0f;
  }

  std::string idStr = "SLDR_I:" + label;
  uint32_t id = GenerateId(idStr.c_str());
  auto intEntry = ctx->intStates.try_emplace(id, minValue);
  int currentValue = value ? *value : intEntry.first->second;
  currentValue = std::clamp(currentValue, minValue, maxValue);

  float asFloat = static_cast<float>(currentValue);
  bool changed = SliderFloat(label, &asFloat, static_cast<float>(minValue),
                             static_cast<float>(maxValue), width, "%.0f", pos);
  int newInt = static_cast<int>(std::round(asFloat));
  newInt = std::clamp(newInt, minValue, maxValue);

  if (changed || newInt != currentValue) {
    if (value)
      *value = newInt;
    else
      intEntry.first->second = newInt;
    return true;
  }
  return false;
}

void ProgressBar(float fraction, const Vec2 &size, const std::string &overlay,
                 const Vec2 &pos) {
  UIContext *ctx = GetContext();
  if (!ctx)
    return;

  Vec2 desiredSize = size;
  if (desiredSize.x <= 0.0f)
    desiredSize.x = 200.0f;
  if (desiredSize.y <= 0.0f)
    desiredSize.y = 2.0f; // Altura por defecto: 2px como en la captura

  fraction = std::clamp(fraction, 0.0f, 1.0f);

  LayoutConstraints constraints = ConsumeNextConstraints();
  Vec2 barSize = ApplyConstraints(ctx, constraints, desiredSize);
  barSize.x = std::max(barSize.x, 1.0f);
  // Forzar altura a 2px para mantener el estilo delgado de la captura
  barSize.y = 2.0f;

  // Resolver posición: usar pos si se proporciona, sino usar cursor
  // directamente
  Vec2 barPos;
  if (pos.x != 0.0f || pos.y != 0.0f) {
    barPos = ResolveAbsolutePosition(ctx, pos, barSize);
  } else {
    barPos = ctx->cursorPos;
  }

  const PanelStyle &panelStyle = ctx->style.panel;
  const ColorState &accentState = ctx->style.button.background;
  const TextStyle &captionStyle =
      ctx->style.GetTextStyle(TypographyStyle::Caption);

  bool isDarkTheme = (ctx->style.backgroundColor.r < 0.5f);
  
  // Radio de esquinas: usar la mitad de la altura para forma de cápsula
  float cornerRadius = barSize.y * 0.5f;
  
  // Track (fondo) - gris claro con bordes redondeados
  Color trackColor = isDarkTheme ? Color(0.4f, 0.4f, 0.4f, 1.0f) : Color(0.85f, 0.85f, 0.85f, 1.0f);
  ctx->renderer.DrawRectFilled(barPos, barSize, trackColor, cornerRadius);

  // Segmento lleno - usar color de acento del tema con bordes redondeados en ambos extremos
  if (fraction > 0.0f) {
    Vec2 fillSize(barSize.x * fraction, barSize.y);
    // Usar el color de acento del tema (azul del tema Fluent)
    Color fillColor = accentState.normal;
    // El segmento lleno siempre tiene bordes redondeados (cápsula)
    // Con 2px de altura, el radio será 1px (mitad de la altura)
    ctx->renderer.DrawRectFilled(barPos, fillSize, fillColor, cornerRadius);
  }

  // Mostrar porcentaje al lado de la barra (no dentro)
  std::string percentageText;
  if (!overlay.empty()) {
    percentageText = overlay;
  } else {
    // Generar porcentaje automáticamente si no se proporciona overlay
    int percentage = static_cast<int>(fraction * 100.0f);
    percentageText = std::to_string(percentage) + "%";
  }
  
  if (!percentageText.empty()) {
    Vec2 textSize = MeasureTextCached(ctx, percentageText, captionStyle.fontSize);
    // Posicionar el texto al lado derecho de la barra con un pequeño espaciado
    float spacing = 8.0f;
    Vec2 textPos(barPos.x + barSize.x + spacing,
                 barPos.y + (barSize.y - textSize.y) * 0.5f);
    ctx->renderer.DrawText(textPos, percentageText, captionStyle.color,
                           captionStyle.fontSize);
    
    // Actualizar el área ocupada para incluir el texto
    Vec2 totalSize(barSize.x + spacing + textSize.x, std::max(barSize.y, textSize.y));
    RegisterOccupiedArea(ctx, barPos, totalSize);
    ctx->lastItemPos = barPos;
    AdvanceCursor(ctx, totalSize);
  } else {
    RegisterOccupiedArea(ctx, barPos, barSize);
    ctx->lastItemPos = barPos;
    AdvanceCursor(ctx, barSize);
  }
}

static size_t FindCaretPosition(const std::string &text, float targetX,
                                UIContext *ctx) {
  const TextStyle &textStyle = ctx->style.GetTextStyle(TypographyStyle::Body);
  float accumulated = 0.0f;
  for (size_t i = 0; i < text.size(); ++i) {
    std::string ch(1, text[i]);
    float charWidth = ctx->renderer.MeasureText(ch, textStyle.fontSize).x;
    if (targetX < accumulated + charWidth * 0.5f) {
      return i;
    }
    accumulated += charWidth;
  }
  return text.size();
}

bool TextInput(const std::string &label, std::string *value, float width,
               bool multiline, const Vec2 &pos) {
  UIContext *ctx = GetContext();
  if (!ctx)
    return false;
  if (width <= 0.0f)
    width = 200.0f;
  (void)multiline; // multiline not yet implemented

  float labelSpacing = 4.0f;

  const TextStyle &labelStyle =
      ctx->style.GetTextStyle(TypographyStyle::Subtitle);
  const TextStyle &inputTextStyle =
      ctx->style.GetTextStyle(TypographyStyle::Body);
  const PanelStyle &panelStyle = ctx->style.panel;
  const ColorState &accentState = ctx->style.button.background;

  float inputHeight = inputTextStyle.fontSize + panelStyle.padding.y * 2.0f;
  Vec2 labelSize = ctx->renderer.MeasureText(label, labelStyle.fontSize);
  Vec2 fieldSize(width, inputHeight);

  Vec2 totalSize(fieldSize.x, labelSize.y + labelSpacing + fieldSize.y);
  LayoutConstraints constraints = ConsumeNextConstraints();
  Vec2 finalSize = ApplyConstraints(ctx, constraints, totalSize);

  // Resolver posición: usar pos si se proporciona, sino usar cursor
  // directamente
  Vec2 widgetPos;
  if (pos.x != 0.0f || pos.y != 0.0f) {
    widgetPos = ResolveAbsolutePosition(ctx, pos, finalSize);
  } else {
    widgetPos = ctx->cursorPos;
  }
  Vec2 fieldPos(widgetPos.x, widgetPos.y + labelSize.y + labelSpacing);

  if (finalSize.x > 0.0f) {
    fieldSize.x = finalSize.x;
    totalSize.x = finalSize.x;
  }

  if (finalSize.y > totalSize.y) {
    float fieldHeight = finalSize.y - (labelSize.y + labelSpacing);
    fieldSize.y = std::max(fieldSize.y, fieldHeight);
    totalSize.y = finalSize.y;
  }

  std::string idStr = "TXT:" + label;
  uint32_t id = GenerateId(idStr.c_str());

  // Si value no es nullptr, usar su valor directamente
  // Si value es nullptr, usar el valor almacenado en stringStates
  std::string* textPtr = value;
  if (!textPtr) {
    auto stringEntry = ctx->stringStates.try_emplace(id, "");
    textPtr = &stringEntry.first->second;
  }
  
  // Asegurar que textPtr apunta al valor correcto
  std::string &textRef = *textPtr;
  
  auto caretIt = ctx->caretPositions.try_emplace(id, textRef.size());
  size_t &caret = caretIt.first->second;
  caret = std::min(caret, textRef.size());
  float &scroll = ctx->textScrollOffsets[id];

  Vec2 mousePos(ctx->input.MouseX(), ctx->input.MouseY());
  bool hover = PointInRect(mousePos, fieldPos, fieldSize);
  bool leftPressed = ctx->input.IsMousePressed(0);
  bool leftDown = ctx->input.IsMouseDown(0);

  if (leftPressed) {
    if (hover) {
      ctx->activeWidgetId = id;
      ctx->activeWidgetType = ActiveWidgetType::TextInput;
      float localX = mousePos.x - (fieldPos.x + panelStyle.padding.x) + scroll;
      caret = FindCaretPosition(textRef, std::max(localX, 0.0f), ctx);
    } else if (ctx->activeWidgetId == id &&
               ctx->activeWidgetType == ActiveWidgetType::TextInput) {
      ctx->activeWidgetId = 0;
      ctx->activeWidgetType = ActiveWidgetType::None;
    }
  }

  bool hasFocus = ctx->activeWidgetId == id &&
                  ctx->activeWidgetType == ActiveWidgetType::TextInput;
  bool valueChanged = false;

  if (hasFocus) {
    const std::string &inputText = ctx->input.TextInputBuffer();
    if (!inputText.empty()) {
      // Insertar el texto en la posición del cursor
      // textRef es una referencia, así que modifica directamente *value o stringStates
      textRef.insert(caret, inputText);
      caret += inputText.size();
      valueChanged = true;
    }

    if (ctx->input.IsKeyPressed(SDL_SCANCODE_BACKSPACE)) {
      if (caret > 0) {
        caret--;
        textRef.erase(caret, 1);
        valueChanged = true;
      }
    } else if (ctx->input.IsKeyPressed(SDL_SCANCODE_DELETE)) {
      if (caret < textRef.size()) {
        textRef.erase(caret, 1);
        valueChanged = true;
      }
    } else if (ctx->input.IsKeyPressed(SDL_SCANCODE_LEFT)) {
      if (caret > 0)
        caret--;
    } else if (ctx->input.IsKeyPressed(SDL_SCANCODE_RIGHT)) {
      if (caret < textRef.size())
        caret++;
    } else if (ctx->input.IsKeyPressed(SDL_SCANCODE_HOME)) {
      caret = 0;
    } else if (ctx->input.IsKeyPressed(SDL_SCANCODE_END)) {
      caret = textRef.size();
    } else if (!multiline && (ctx->input.IsKeyPressed(SDL_SCANCODE_RETURN) ||
                              ctx->input.IsKeyPressed(SDL_SCANCODE_KP_ENTER))) {
      ctx->activeWidgetId = 0;
      ctx->activeWidgetType = ActiveWidgetType::None;
    }
  }

  ctx->renderer.DrawText(widgetPos, label, labelStyle.color,
                         labelStyle.fontSize);

  // Fondo más distintivo para TextInput - sin borde visible (solo focus)
  Color bgColor;
  bool isDarkTheme = (ctx->style.backgroundColor.r < 0.5f);
  
  if (isDarkTheme) {
    // Fondo oscuro charcoal gray más profundo (#262626 ~ 0.15f)
    bgColor = Color(0.15f, 0.15f, 0.15f, 1.0f);
    if (hover) {
      // Ligeramente más claro al hover (#333333 ~ 0.2f)
      bgColor = Color(0.2f, 0.2f, 0.2f, 1.0f);
    }
  } else {
    // Para tema claro, usar colores más claros
    bgColor = Color(0.96f, 0.96f, 0.96f, 1.0f);
    if (hover) {
      bgColor = Color(0.92f, 0.92f, 0.92f, 1.0f);
    }
  }
  
  // Solo mostrar borde cuando tiene focus
  if (hasFocus) {
    Color focusColor = FluentColors::Accent;
    focusColor.a = 0.4f;
    Vec2 focusOffset(1.5f, 1.5f);
    ctx->renderer.DrawRectFilled(fieldPos - focusOffset,
                                 fieldSize + focusOffset * 2.0f, focusColor,
                                 panelStyle.cornerRadius + 1.5f);
    // Fondo completamente negro cuando tiene focus
    bgColor = Color(0.0f, 0.0f, 0.0f, 1.0f);
  }
  
  ctx->renderer.DrawRectFilled(fieldPos, fieldSize, bgColor,
                               panelStyle.cornerRadius);

  Vec2 textSize = ctx->renderer.MeasureText(textRef, inputTextStyle.fontSize);
  float textPadding = panelStyle.padding.x * 0.5f;
  float availableWidth = fieldSize.x - textPadding * 2.0f;
  float caretOffset =
      ctx->renderer
          .MeasureText(textRef.substr(0, caret), inputTextStyle.fontSize)
          .x;

  if (caretOffset - scroll > availableWidth)
    scroll = caretOffset - availableWidth;
  else if (caretOffset - scroll < 0.0f)
    scroll = caretOffset;
  scroll =
      std::clamp(scroll, 0.0f, std::max(0.0f, textSize.x - availableWidth));

  Vec2 textPos(fieldPos.x + textPadding - scroll,
               fieldPos.y + (fieldSize.y - inputTextStyle.fontSize) * 0.5f);
  ctx->renderer.DrawText(textPos, textRef, inputTextStyle.color,
                         inputTextStyle.fontSize);

  if (hasFocus && ((ctx->frame / 30) % 2 == 0)) {
    float caretX = textPos.x + caretOffset;
    Vec2 caretPos(caretX, fieldPos.y + textPadding * 0.5f);
    Vec2 caretSize(1.5f, fieldSize.y - textPadding);
    // Cursor blanco cuando tiene focus
    ctx->renderer.DrawRectFilled(caretPos, caretSize, Color(1.0f, 1.0f, 1.0f, 1.0f), 0.0f);
  }

  RegisterOccupiedArea(ctx, widgetPos, finalSize);

  ctx->lastItemPos = widgetPos;
  AdvanceCursor(ctx, finalSize);
  return valueChanged;
}

bool ComboBox(const std::string &label, int *currentItem,
              const std::vector<std::string> &items, float width,
              const Vec2 &pos) {
  UIContext *ctx = GetContext();
  if (!ctx || items.empty())
    return false;
  if (width <= 0.0f)
    width = 200.0f;

  std::string idStr = "COMBO:" + label;
  uint32_t id = GenerateId(idStr.c_str());

  // Registrar como widget enfocable
  ctx->focusableWidgets.push_back(id);
  bool hasFocus = (ctx->focusedWidgetId == id);

  // Retrieve State
  auto &state = ctx->comboBoxStates[id];
  if (!state.initialized) {
      state.initialized = true;
      state.isOpen = false;
      state.scrollOffset = 0.0f;
      state.hoveredIndex = currentItem ? *currentItem : -1;
  }

  int selectedIndex = currentItem ? *currentItem : 0;
  selectedIndex =
      std::clamp(selectedIndex, 0, static_cast<int>(items.size() - 1));
  std::string selectedText = items[selectedIndex];

  // If just opened via keyboard, ensure hovered index matches selected
  if (state.justOpened) {
      state.hoveredIndex = selectedIndex;
      state.justOpened = false;
  }

  float labelSpacing = 4.0f;
  const TextStyle &labelStyle =
      ctx->style.GetTextStyle(TypographyStyle::Subtitle);
  const TextStyle &itemStyle = ctx->style.GetTextStyle(TypographyStyle::Body);
  const PanelStyle &panelStyle = ctx->style.panel;
  
  Vec2 labelSize = ctx->renderer.MeasureText(label, labelStyle.fontSize);
  float fieldHeight = itemStyle.fontSize + panelStyle.padding.y * 2.0f;
  Vec2 fieldSize(width, fieldHeight);

  Vec2 totalSize(fieldSize.x, labelSize.y + labelSpacing + fieldSize.y);
  LayoutConstraints constraints = ConsumeNextConstraints();
  Vec2 finalSize = ApplyConstraints(ctx, constraints, totalSize);

  Vec2 widgetPos;
  if (pos.x != 0.0f || pos.y != 0.0f) {
    widgetPos = ResolveAbsolutePosition(ctx, pos, finalSize);
  } else {
    widgetPos = ctx->cursorPos;
  }
  Vec2 fieldPos(widgetPos.x, widgetPos.y + labelSize.y + labelSpacing);
  if (finalSize.x > 0.0f) {
    fieldSize.x = finalSize.x;
  }

  float mouseX = ctx->input.MouseX();
  float mouseY = ctx->input.MouseY();
  bool hoverField =
      (mouseX >= fieldPos.x && mouseX <= fieldPos.x + fieldSize.x &&
       mouseY >= fieldPos.y && mouseY <= fieldPos.y + fieldSize.y);

  bool valueChanged = false;

  // Interaction
  bool clicked = hoverField && ctx->input.IsMousePressed(0);
  if (clicked) {
    state.isOpen = !state.isOpen;
    if (state.isOpen) {
        state.justOpened = true; 
        ctx->focusIndex = -1; 
        ctx->focusedWidgetId = id;
    }
  }

  // Keyboard Navigation
  if (hasFocus) {
      if (ctx->input.IsKeyPressed(SDL_SCANCODE_RETURN) ||
          ctx->input.IsKeyPressed(SDL_SCANCODE_SPACE)) {
        state.isOpen = !state.isOpen;
        if (state.isOpen) {
             state.justOpened = true;
             state.hoveredIndex = selectedIndex;
        } else {
             // Confirm selection on close if it was open
             if (state.hoveredIndex >= 0 && state.hoveredIndex < (int)items.size()) {
                 if (*currentItem != state.hoveredIndex) {
                     *currentItem = state.hoveredIndex;
                     valueChanged = true;
                 }
             }
        }
      }
      
      if (state.isOpen) {
          if (ctx->input.IsKeyPressed(SDL_SCANCODE_DOWN)) {
              if (state.hoveredIndex < (int)items.size() - 1) {
                  state.hoveredIndex++;
              }
          }
           if (ctx->input.IsKeyPressed(SDL_SCANCODE_UP)) {
              if (state.hoveredIndex > 0) {
                  state.hoveredIndex--;
              }
          }
          if (ctx->input.IsKeyPressed(SDL_SCANCODE_ESCAPE)) {
              state.isOpen = false;
          }
      } else {
           // Direct change when closed
           if (ctx->input.IsKeyPressed(SDL_SCANCODE_DOWN)) {
               if (currentItem && *currentItem < (int)items.size() - 1) {
                   *currentItem = *currentItem + 1;
                   valueChanged = true;
               }
           }
           if (ctx->input.IsKeyPressed(SDL_SCANCODE_UP)) {
               if (currentItem && *currentItem > 0) {
                   *currentItem = *currentItem - 1;
                   valueChanged = true;
               }
           }
      }
  }

  // Dibujar campo con fondo distintivo
  bool isDarkTheme = (ctx->style.backgroundColor.r < 0.5f);
  Color fieldBg;
  if (isDarkTheme) {
    fieldBg = Color(0.15f, 0.15f, 0.15f, 1.0f);
    if (hoverField) {
      fieldBg = Color(0.2f, 0.2f, 0.2f, 1.0f);
    }
  } else {
    fieldBg = Color(0.96f, 0.96f, 0.96f, 1.0f);
    if (hoverField) {
      fieldBg = Color(0.92f, 0.92f, 0.92f, 1.0f);
    }
  }
  
  ctx->renderer.DrawRectFilled(fieldPos, fieldSize, fieldBg,
                               panelStyle.cornerRadius);

  // Label
  ctx->renderer.DrawText(widgetPos, label, labelStyle.color, labelStyle.fontSize);

  // Selected Text
  Vec2 textPadding(panelStyle.padding.x, panelStyle.padding.y);
  Vec2 textSize = MeasureTextCached(ctx, selectedText, itemStyle.fontSize);
  float textY = fieldPos.y + fieldSize.y * 0.5f - textSize.y * 0.5f;
  Vec2 textPos(fieldPos.x + textPadding.x, textY);
  Color textColor = isDarkTheme ? Color(0.83f, 0.83f, 0.83f, 1.0f) : itemStyle.color;
  ctx->renderer.DrawText(textPos, selectedText, textColor,
                         itemStyle.fontSize);

  // Chevron
  float arrowSize = 8.0f;
  float arrowY = fieldPos.y + (fieldSize.y - arrowSize) * 0.5f;
  Vec2 arrowPos(fieldPos.x + fieldSize.x - arrowSize - textPadding.x, arrowY);
  Vec2 arrowBottom(arrowPos.x + arrowSize * 0.5f, arrowPos.y + arrowSize); // Punta
  Vec2 arrowTopLeft(arrowPos.x, arrowPos.y); 
  Vec2 arrowTopRight(arrowPos.x + arrowSize, arrowPos.y); 
  
  ctx->renderer.DrawLine(arrowBottom, arrowTopLeft, textColor, 1.5f);
  ctx->renderer.DrawLine(arrowBottom, arrowTopRight, textColor, 1.5f);

  // Defer Dropdown Rendering
  if (state.isOpen) {
      UIContext::DeferredComboDropdown dropdown;
      dropdown.comboId = id;
      dropdown.fieldPos = fieldPos; 
      dropdown.fieldSize = fieldSize;
      
      // Smart Positioning
      float maxDropdownHeight = 200.0f;
      float contentHeight = items.size() * fieldHeight; 
      float actualHeight = std::min(contentHeight, maxDropdownHeight);
      
      Vec2 viewport = ctx->renderer.GetViewportSize();
      Vec2 dropdownPos(fieldPos.x, fieldPos.y + fieldSize.y);
      bool flipped = false;
      
      // Flip if not enough space below and enough space above
      if (dropdownPos.y + actualHeight > viewport.y && fieldPos.y - actualHeight > 0) {
          dropdownPos.y = fieldPos.y - actualHeight;
          flipped = true;
      }
      
      dropdown.dropdownPos = dropdownPos;
      dropdown.dropdownSize = Vec2(fieldSize.x, actualHeight);
      
      // Save for next frame interaction
      state.dropdownPos = dropdownPos;
      state.dropdownSize = dropdown.dropdownSize;
      
      dropdown.items = items;
      dropdown.selectedIndex = selectedIndex;
      dropdown.currentItemPtr = currentItem;
      dropdown.fieldHeight = fieldHeight;
      
      ctx->deferredComboDropdowns.push_back(dropdown);
  }

  // Keep boolStates updated for compatibility if checked elsewhere
  ctx->boolStates[id] = state.isOpen;

  RegisterOccupiedArea(ctx, widgetPos, finalSize);

  ctx->lastItemPos = widgetPos;
  AdvanceCursor(ctx, finalSize);
  return valueChanged;
}

bool BeginScrollView(const std::string &id, const Vec2 &size,
                     Vec2 *scrollOffset, const Vec2 &pos) {
  UIContext *ctx = GetContext();
  if (!ctx)
    return false;

  // Resetear el flag de scroll consumido al inicio del primer ScrollView del frame
  if (scrollViewStack.empty()) {
    scrollConsumedThisFrame = false;
  }

  std::string key = "SCROLLVIEW:" + id;
  uint32_t scrollViewId = GenerateId(key.c_str());
  auto &state = ctx->scrollViewStates[scrollViewId];

  if (!state.initialized) {
    state.scrollOffset = Vec2(0.0f, 0.0f);
    state.contentSize = Vec2(0.0f, 0.0f);
    state.viewSize = size;
    state.initialized = true;
  } else {
    // Si ya está inicializado, usar el tamaño calculado dinámicamente si está disponible
    // Solo usar el size pasado si es mayor (para permitir crecimiento) o si no hay tamaño calculado
    if (state.viewSize.x > 0.0f && state.viewSize.y > 0.0f && !state.useAbsolutePos) {
      // Usar el tamaño calculado, pero permitir que crezca si el size pasado es mayor
      if (size.x > state.viewSize.x) {
        state.viewSize.x = size.x;
      }
      if (size.y > state.viewSize.y) {
        state.viewSize.y = size.y;
      }
    } else {
      // Si no hay tamaño calculado o se usa posición absoluta, usar el size pasado
      state.viewSize = size;
    }
  }

  // Si se proporciona scrollOffset externo, usarlo y actualizar el estado
  if (scrollOffset) {
    state.scrollOffset = *scrollOffset;
  }

  float scrollbarWidth = 12.0f;

  // Usar el tamaño calculado dinámicamente si está disponible, sino usar el size pasado
  Vec2 actualSize = state.viewSize;
  if (actualSize.x <= 0.0f || actualSize.y <= 0.0f) {
    actualSize = size;
  }
  
  // Resolver posición
  Vec2 scrollViewPos;
  Vec2 fullSize(actualSize.x, actualSize.y);
  if (pos.x != 0.0f || pos.y != 0.0f) {
    scrollViewPos = ResolveAbsolutePosition(ctx, pos, fullSize);
    state.useAbsolutePos = true;
    state.absolutePos = scrollViewPos;
  } else {
    scrollViewPos = ctx->cursorPos;
    state.useAbsolutePos = false;
  }

  state.position = scrollViewPos;

  // Registrar área ocupada solo si NO se usa posición absoluta
  if (!state.useAbsolutePos) {
    RegisterOccupiedArea(ctx, scrollViewPos, fullSize);
  }

  const PanelStyle &panelStyle = ctx->style.panel;

  // Dibujar fondo del scroll view con contraste más pronunciado - sin borde
  Color scrollBg = panelStyle.background;
  bool isDarkTheme = (ctx->style.backgroundColor.r < 0.5f);
  if (isDarkTheme) {
    // Más claro que el fondo para mejor contraste
    scrollBg = Color(scrollBg.r * 1.15f, scrollBg.g * 1.15f, scrollBg.b * 1.15f, 1.0f);
  } else {
    // Más oscuro que el fondo para mejor contraste
    scrollBg = Color(scrollBg.r * 0.92f, scrollBg.g * 0.92f, scrollBg.b * 0.92f, 1.0f);
  }
  ctx->renderer.DrawRectFilled(scrollViewPos, actualSize, scrollBg,
                               panelStyle.cornerRadius);

  // Calcular área de contenido (reservar espacio para scrollbar)
  Vec2 availableSize(actualSize.x - scrollbarWidth, actualSize.y - scrollbarWidth);
  Vec2 contentAreaPos(scrollViewPos.x + panelStyle.padding.x,
                      scrollViewPos.y + panelStyle.padding.y);
  Vec2 contentAreaSize(availableSize.x - panelStyle.padding.x * 2.0f,
                       availableSize.y - panelStyle.padding.y * 2.0f);

  // Manejar scroll con rueda del mouse
  // Solo procesar scroll si el mouse está sobre el área de contenido de este ScrollView
  // y este ScrollView es el más anidado (está en el tope de la pila después de agregarlo)
  float mouseX = ctx->input.MouseX();
  float mouseY = ctx->input.MouseY();
  
  // Verificar hover sobre el área de contenido (no solo el ScrollView completo)
  bool hoverContentArea = 
      (mouseX >= contentAreaPos.x && mouseX <= contentAreaPos.x + contentAreaSize.x &&
       mouseY >= contentAreaPos.y && mouseY <= contentAreaPos.y + contentAreaSize.y);
  
  // Agregar este ScrollView a la pila primero para poder verificar si es el más anidado
  // (temporalmente, antes de agregarlo al final)
  
  // Solo procesar scroll si el hover está sobre el área de contenido Y 
  // el scroll no ha sido consumido aún (esto asegura que solo el primero en procesarse lo haga)
  // Como los ScrollViews más anidados se procesan primero, esto naturalmente hace que solo
  // el más anidado procese el scroll
  if (hoverContentArea && !scrollConsumedThisFrame) {
    float wheelY = ctx->input.MouseWheelY();
    if (std::abs(wheelY) > 0.001f) {
      float scrollSpeed = 30.0f;
      // El límite se calculará en EndScrollView cuando sepamos contentSize
      state.scrollOffset.y -= wheelY * scrollSpeed;
      state.scrollOffset.y = std::max(0.0f, state.scrollOffset.y);
      scrollConsumedThisFrame = true; // Marcar como consumido
    }
    float wheelX = ctx->input.MouseWheelX();
    if (std::abs(wheelX) > 0.001f) {
      float scrollSpeed = 30.0f;
      state.scrollOffset.x -= wheelX * scrollSpeed;
      state.scrollOffset.x = std::max(0.0f, state.scrollOffset.x);
      scrollConsumedThisFrame = true; // Marcar como consumido
    }
  }
  
  // También verificar hover sobre el ScrollView completo para otros propósitos
  bool hoverScrollView =
      (mouseX >= scrollViewPos.x && mouseX <= scrollViewPos.x + size.x &&
       mouseY >= scrollViewPos.y && mouseY <= scrollViewPos.y + size.y);

  // Manejar interacción del mouse con scrollbar (usando contentSize del frame anterior)
  // Esto permite que el scrollbar funcione ANTES de dibujar el contenido
  bool leftDown = ctx->input.IsMouseDown(0);
  bool leftPressed = ctx->input.IsMousePressed(0);

  // Calcular si necesitamos scrollbars (usando contentSize del frame anterior)
  bool needsVerticalScrollbar = state.contentSize.y > contentAreaSize.y;
  bool needsHorizontalScrollbar = state.contentSize.x > contentAreaSize.x;

  float maxScrollY = std::max(0.0f, state.contentSize.y - contentAreaSize.y);
  float maxScrollX = std::max(0.0f, state.contentSize.x - contentAreaSize.x);

  // Barra de scroll vertical - manejo del mouse
  if (needsVerticalScrollbar) {
    Vec2 vScrollbarPos(scrollViewPos.x + size.x - scrollbarWidth,
                       scrollViewPos.y);
    Vec2 vScrollbarSize(scrollbarWidth,
                        size.y - (needsHorizontalScrollbar ? scrollbarWidth : 0.0f));

    float scrollRatio = std::clamp(state.contentSize.y > 0.0f
                                       ? contentAreaSize.y / state.contentSize.y
                                       : 1.0f,
                                   0.0f, 1.0f);
    float thumbHeight = vScrollbarSize.y * scrollRatio;
    thumbHeight = std::max(thumbHeight, 20.0f);
    float maxThumbPos = vScrollbarSize.y - thumbHeight;
    float thumbPos =
        maxThumbPos * std::clamp(state.scrollOffset.y / maxScrollY, 0.0f, 1.0f);

    Vec2 thumbPosAbsolute(vScrollbarPos.x, vScrollbarPos.y + thumbPos);
    Vec2 thumbSize(scrollbarWidth, thumbHeight);

    bool hoverThumb = (mouseX >= thumbPosAbsolute.x &&
                       mouseX <= thumbPosAbsolute.x + thumbSize.x &&
                       mouseY >= thumbPosAbsolute.y &&
                       mouseY <= thumbPosAbsolute.y + thumbSize.y);
    bool hoverTrack = (mouseX >= vScrollbarPos.x &&
                       mouseX <= vScrollbarPos.x + vScrollbarSize.x &&
                       mouseY >= vScrollbarPos.y &&
                       mouseY <= vScrollbarPos.y + vScrollbarSize.y);

    if (state.draggingScrollbar && state.draggingScrollbarType == 1) {
      if (!leftDown) {
        state.draggingScrollbar = false;
        state.draggingScrollbarType = 0;
      } else {
        float mouseDelta = mouseY - state.dragStartMouse.y;
        float scrollDelta = (mouseDelta / maxThumbPos) * maxScrollY;
        state.scrollOffset.y =
            std::clamp(state.dragStartScroll + scrollDelta, 0.0f, maxScrollY);
      }
    } else if (leftPressed && hoverThumb) {
      state.draggingScrollbar = true;
      state.draggingScrollbarType = 1;
      state.dragStartMouse = Vec2(mouseX, mouseY);
      state.dragStartScroll = state.scrollOffset.y;
    } else if (leftPressed && hoverTrack) {
      float clickPos = mouseY - vScrollbarPos.y;
      float scrollRatioClick = clickPos / vScrollbarSize.y;
      state.scrollOffset.y = std::clamp(scrollRatioClick * maxScrollY, 0.0f, maxScrollY);
    }
  }

  // Barra de scroll horizontal - manejo del mouse
  if (needsHorizontalScrollbar) {
    Vec2 hScrollbarPos(scrollViewPos.x,
                       scrollViewPos.y + size.y - scrollbarWidth);
    Vec2 hScrollbarSize(size.x - (needsVerticalScrollbar ? scrollbarWidth : 0.0f),
                        scrollbarWidth);

    float scrollRatio = std::clamp(state.contentSize.x > 0.0f
                                       ? contentAreaSize.x / state.contentSize.x
                                       : 1.0f,
                                   0.0f, 1.0f);
    float thumbWidth = hScrollbarSize.x * scrollRatio;
    thumbWidth = std::max(thumbWidth, 20.0f);
    float maxThumbPos = hScrollbarSize.x - thumbWidth;
    float thumbPos =
        maxThumbPos * std::clamp(state.scrollOffset.x / maxScrollX, 0.0f, 1.0f);

    Vec2 thumbPosAbsolute(hScrollbarPos.x + thumbPos, hScrollbarPos.y);
    Vec2 thumbSize(thumbWidth, scrollbarWidth);

    bool hoverThumb = (mouseX >= thumbPosAbsolute.x &&
                       mouseX <= thumbPosAbsolute.x + thumbSize.x &&
                       mouseY >= thumbPosAbsolute.y &&
                       mouseY <= thumbPosAbsolute.y + thumbSize.y);
    bool hoverTrack = (mouseX >= hScrollbarPos.x &&
                       mouseX <= hScrollbarPos.x + hScrollbarSize.x &&
                       mouseY >= hScrollbarPos.y &&
                       mouseY <= hScrollbarPos.y + hScrollbarSize.y);

    if (state.draggingScrollbar && state.draggingScrollbarType == 2) {
      if (!leftDown) {
        state.draggingScrollbar = false;
        state.draggingScrollbarType = 0;
      } else {
        float mouseDelta = mouseX - state.dragStartMouse.x;
        float scrollDelta = (mouseDelta / maxThumbPos) * maxScrollX;
        state.scrollOffset.x =
            std::clamp(state.dragStartScroll + scrollDelta, 0.0f, maxScrollX);
      }
    } else if (leftPressed && hoverThumb) {
      state.draggingScrollbar = true;
      state.draggingScrollbarType = 2;
      state.dragStartMouse = Vec2(mouseX, mouseY);
      state.dragStartScroll = state.scrollOffset.x;
    } else if (leftPressed && hoverTrack) {
      float clickPos = mouseX - hScrollbarPos.x;
      float scrollRatioClick = clickPos / hScrollbarSize.x;
      state.scrollOffset.x = std::clamp(scrollRatioClick * maxScrollX, 0.0f, maxScrollX);
    }
  }

  // Aplicar clipping al área de contenido
  ctx->renderer.PushClipRect(contentAreaPos, contentAreaSize);

  // Guardar cursor y estado antes de modificarlo
  Vec2 savedCursor = ctx->cursorPos;
  Vec2 savedLastItemPos = ctx->lastItemPos;
  Vec2 savedLastItemSize = ctx->lastItemSize;

  // Configurar el cursor para el contenido (ajustado por scroll)
  ctx->cursorPos = contentAreaPos - state.scrollOffset;
  ctx->lastItemPos = ctx->cursorPos;
  ctx->lastItemSize = Vec2(0.0f, 0.0f);

  // Empujar offset del área de contenido
  ctx->offsetStack.push_back(contentAreaPos);

  // Iniciar layout vertical para organizar los widgets dentro del ScrollView
  BeginVertical(ctx->style.spacing, Vec2(contentAreaSize.x, 0.0f), Vec2(0.0f, 0.0f));

  // Guardar contexto para EndScrollView
  ScrollViewFrameContext frameCtx{};
  frameCtx.id = scrollViewId;
  frameCtx.position = scrollViewPos;
  frameCtx.size = actualSize; // Usar el tamaño real (calculado dinámicamente o pasado)
  frameCtx.contentAreaPos = contentAreaPos;
  frameCtx.contentAreaSize = contentAreaSize;
  frameCtx.availableSize = availableSize;
  frameCtx.scrollbarWidth = scrollbarWidth;
  frameCtx.layoutPushed = true;
  frameCtx.useAbsolutePos = state.useAbsolutePos;
  frameCtx.savedCursor = savedCursor;
  frameCtx.savedLastItemPos = savedLastItemPos;
  frameCtx.savedLastItemSize = savedLastItemSize;
  scrollViewStack.push_back(frameCtx);

  // Actualizar scrollOffset externo si se proporciona
  if (scrollOffset) {
    *scrollOffset = state.scrollOffset;
  }

  return true;
}

void EndScrollView() {
  UIContext *ctx = GetContext();
  if (!ctx)
    return;

  if (scrollViewStack.empty())
    return;

  ScrollViewFrameContext frameCtx = scrollViewStack.back();
  scrollViewStack.pop_back();

  auto it = ctx->scrollViewStates.find(frameCtx.id);
  if (it == ctx->scrollViewStates.end())
    return;

  auto &state = it->second;

  // Guardar el tamaño máximo del contenido antes de cerrar el layout
  float maxContentWidth = 0.0f;
  float maxContentHeight = 0.0f;
  if (frameCtx.layoutPushed && !ctx->layoutStack.empty()) {
    LayoutStack &layout = ctx->layoutStack.back();
    // El contentSize es el tamaño total del contenido en el layout
    state.contentSize = layout.contentSize;
    maxContentWidth = layout.contentSize.x;
    maxContentHeight = layout.contentSize.y;
    EndVertical(false); // No avanzar el cursor del padre aquí
  }
  
  // Ajustar el tamaño del ScrollView según el contenido si no se especificó un tamaño fijo
  if (!state.useAbsolutePos && frameCtx.layoutPushed) {
    const PanelStyle &panelStyle = ctx->style.panel;
    float scrollbarWidth = frameCtx.scrollbarWidth;
    
    // Ajustar ancho dinámicamente
    float contentAreaWidth = std::max(0.0f, state.viewSize.x - scrollbarWidth - panelStyle.padding.x * 2.0f);
    float newWidth = std::max(contentAreaWidth, maxContentWidth + scrollbarWidth + panelStyle.padding.x * 2.0f);
    // Solo actualizar si el contenido es más ancho
    if (newWidth > state.viewSize.x) {
      state.viewSize.x = newWidth;
    }
    
    // NO ajustar la altura del ScrollView dinámicamente - el ScrollView debe mantener
    // su tamaño y permitir scroll interno cuando el contenido es más grande.
    // El contenedor padre (TabView) es el que debe ajustar su altura dinámicamente.
    // El ScrollView solo debe ajustar su ancho si el contenido es más ancho.
  }

  // Sacar offset del área de contenido
  if (!ctx->offsetStack.empty()) {
    ctx->offsetStack.pop_back();
  }

  // Remover clipping
  ctx->renderer.PopClipRect();

  // Restaurar cursor original antes de avanzarlo
  // Esto evita que el scrollOffset afecte a widgets fuera del ScrollView
  ctx->cursorPos = frameCtx.savedCursor;
  ctx->lastItemPos = frameCtx.savedLastItemPos;
  ctx->lastItemSize = frameCtx.savedLastItemSize;

  const PanelStyle &panelStyle = ctx->style.panel;

  // Calcular si necesitamos barras de scroll
  // Solo mostrar scrollbar horizontal si el contenido realmente excede el área visible
  // y el ancho del contenido es significativamente mayor (más de 5px para evitar scrollbars por errores de redondeo)
  // Calcular si necesitamos barras de scroll
  // Solo mostrar scrollbar horizontal si el contenido realmente excede el área visible
  // y el ancho del contenido es significativamente mayor (más de 5px para evitar scrollbars por errores de redondeo)
  bool needsVerticalScrollbar = state.contentSize.y > frameCtx.contentAreaSize.y + 0.1f;
  bool needsHorizontalScrollbar = state.contentSize.x > frameCtx.contentAreaSize.x + 5.0f;
  
  if (frameCtx.id == 0 || frameCtx.id > 0) { // check specific ID if needed, or just spam for now
       // std::cout << "EndScroll: ID=" << frameCtx.id << " ContentY=" << state.contentSize.y << " AreaY=" << frameCtx.contentAreaSize.y << " NeedsV=" << needsVerticalScrollbar << std::endl;
  }

  // Clamp scroll offset a rangos válidos
  float maxScrollY = std::max(0.0f, state.contentSize.y - frameCtx.contentAreaSize.y);
  float maxScrollX = std::max(0.0f, state.contentSize.x - frameCtx.contentAreaSize.x);
  state.scrollOffset.y = std::clamp(state.scrollOffset.y, 0.0f, maxScrollY);
  state.scrollOffset.x = std::clamp(state.scrollOffset.x, 0.0f, maxScrollX);

  // Manejar input del mouse para las barras de scroll
  float mouseX = ctx->input.MouseX();
  float mouseY = ctx->input.MouseY();
  bool leftDown = ctx->input.IsMouseDown(0);
  bool leftPressed = ctx->input.IsMousePressed(0);

  // Barra de scroll vertical
  if (needsVerticalScrollbar) {
    Vec2 vScrollbarPos(frameCtx.position.x + frameCtx.size.x - frameCtx.scrollbarWidth,
                       frameCtx.position.y);
    Vec2 vScrollbarSize(frameCtx.scrollbarWidth,
                        frameCtx.size.y -
                            (needsHorizontalScrollbar ? frameCtx.scrollbarWidth : 0.0f));

    float scrollRatio = std::clamp(state.contentSize.y > 0.0f
                                       ? frameCtx.contentAreaSize.y / state.contentSize.y
                                       : 1.0f,
                                   0.0f, 1.0f);
    float thumbHeight = vScrollbarSize.y * scrollRatio;
    thumbHeight = std::max(thumbHeight, 20.0f); // Mínimo tamaño del thumb
    float maxThumbPos = vScrollbarSize.y - thumbHeight;
    float thumbPos =
        maxThumbPos * std::clamp(state.scrollOffset.y / maxScrollY, 0.0f, 1.0f);

    Vec2 thumbPosAbsolute(vScrollbarPos.x, vScrollbarPos.y + thumbPos);
    Vec2 thumbSize(frameCtx.scrollbarWidth, thumbHeight);

    // Detectar hover y drag en la barra vertical
    bool hoverThumb = (mouseX >= thumbPosAbsolute.x &&
                       mouseX <= thumbPosAbsolute.x + thumbSize.x &&
                       mouseY >= thumbPosAbsolute.y &&
                       mouseY <= thumbPosAbsolute.y + thumbSize.y);
    bool hoverTrack = (mouseX >= vScrollbarPos.x &&
                       mouseX <= vScrollbarPos.x + vScrollbarSize.x &&
                       mouseY >= vScrollbarPos.y &&
                       mouseY <= vScrollbarPos.y + vScrollbarSize.y);

    if (ctx->activeWidgetId == frameCtx.id && state.draggingScrollbar && state.draggingScrollbarType == 1) {
      if (!leftDown) {
        state.draggingScrollbar = false;
        state.draggingScrollbarType = 0;
        ctx->activeWidgetId = 0;
      } else {
        float mouseDelta = mouseY - state.dragStartMouse.y;
        float scrollDelta = (mouseDelta / maxThumbPos) * maxScrollY;
        state.scrollOffset.y =
            std::clamp(state.dragStartScroll + scrollDelta, 0.0f, maxScrollY);
      }
    } else if (leftPressed && hoverThumb && ctx->activeWidgetId == 0) {
      ctx->activeWidgetId = frameCtx.id;
      state.draggingScrollbar = true;
      state.draggingScrollbarType = 1;
      state.dragStartMouse = Vec2(mouseX, mouseY);
      state.dragStartScroll = state.scrollOffset.y;
    } else if (leftPressed && hoverTrack && ctx->activeWidgetId == 0) {
      // Click en el track: saltar a esa posición
      float clickPos = mouseY - vScrollbarPos.y;
      float scrollRatioClick = clickPos / vScrollbarSize.y;
      state.scrollOffset.y = std::clamp(scrollRatioClick * maxScrollY, 0.0f, maxScrollY);
    }

    // Dibujar barra de scroll vertical
    Color scrollbarBg =
        hoverTrack ? panelStyle.headerBackground : panelStyle.background;
    ctx->renderer.DrawRectFilled(vScrollbarPos, vScrollbarSize, scrollbarBg,
                                 0.0f);

    Color thumbColor = hoverThumb || state.draggingScrollbar
                           ? ctx->style.button.background.hover
                           : ctx->style.button.background.normal;
    ctx->renderer.DrawRectFilled(thumbPosAbsolute, thumbSize, thumbColor, 4.0f);
  }

  // Barra de scroll horizontal
  if (needsHorizontalScrollbar) {
    Vec2 hScrollbarPos(frameCtx.position.x,
                       frameCtx.position.y + frameCtx.size.y - frameCtx.scrollbarWidth);
    Vec2 hScrollbarSize(frameCtx.size.x -
                            (needsVerticalScrollbar ? frameCtx.scrollbarWidth : 0.0f),
                        frameCtx.scrollbarWidth);

    float scrollRatio = std::clamp(state.contentSize.x > 0.0f
                                       ? frameCtx.contentAreaSize.x / state.contentSize.x
                                       : 1.0f,
                                   0.0f, 1.0f);
    float thumbWidth = hScrollbarSize.x * scrollRatio;
    thumbWidth = std::max(thumbWidth, 20.0f); // Mínimo tamaño del thumb
    float maxThumbPos = hScrollbarSize.x - thumbWidth;
    float thumbPos =
        maxThumbPos * std::clamp(state.scrollOffset.x / maxScrollX, 0.0f, 1.0f);

    Vec2 thumbPosAbsolute(hScrollbarPos.x + thumbPos, hScrollbarPos.y);
    Vec2 thumbSize(thumbWidth, frameCtx.scrollbarWidth);

    // Detectar hover y drag en la barra horizontal
    bool hoverThumb = (mouseX >= thumbPosAbsolute.x &&
                       mouseX <= thumbPosAbsolute.x + thumbSize.x &&
                       mouseY >= thumbPosAbsolute.y &&
                       mouseY <= thumbPosAbsolute.y + thumbSize.y);
    bool hoverTrack = (mouseX >= hScrollbarPos.x &&
                       mouseX <= hScrollbarPos.x + hScrollbarSize.x &&
                       mouseY >= hScrollbarPos.y &&
                       mouseY <= hScrollbarPos.y + hScrollbarSize.y);

    if (ctx->activeWidgetId == frameCtx.id && state.draggingScrollbar && state.draggingScrollbarType == 2) {
      if (!leftDown) {
        state.draggingScrollbar = false;
        state.draggingScrollbarType = 0;
        ctx->activeWidgetId = 0;
      } else {
        float mouseDelta = mouseX - state.dragStartMouse.x;
        float scrollDelta = (mouseDelta / maxThumbPos) * maxScrollX;
        state.scrollOffset.x =
            std::clamp(state.dragStartScroll + scrollDelta, 0.0f, maxScrollX);
      }
    } else if (leftPressed && hoverThumb && ctx->activeWidgetId == 0) {
      ctx->activeWidgetId = frameCtx.id;
      state.draggingScrollbar = true;
      state.draggingScrollbarType = 2;
      state.dragStartMouse = Vec2(mouseX, mouseY);
      state.dragStartScroll = state.scrollOffset.x;
    } else if (leftPressed && hoverTrack && ctx->activeWidgetId == 0) {
      // Click en el track: saltar a esa posición
      float clickPos = mouseX - hScrollbarPos.x;
      float scrollRatioClick = clickPos / hScrollbarSize.x;
      state.scrollOffset.x = std::clamp(scrollRatioClick * maxScrollX, 0.0f, maxScrollX);
    }

    // Dibujar barra de scroll horizontal
    Color scrollbarBg =
        hoverTrack ? panelStyle.headerBackground : panelStyle.background;
    ctx->renderer.DrawRectFilled(hScrollbarPos, hScrollbarSize, scrollbarBg,
                                 0.0f);

    Color thumbColor = hoverThumb || state.draggingScrollbar
                           ? ctx->style.button.background.hover
                           : ctx->style.button.background.normal;
    ctx->renderer.DrawRectFilled(thumbPosAbsolute, thumbSize, thumbColor, 4.0f);
  }

  // Avanzar cursor si NO se usa posición absoluta
  // El cursor ya fue restaurado arriba, así que avanzar desde allí
  if (!frameCtx.useAbsolutePos) {
    // Usar la posición del ScrollView como lastItemPos para el avance correcto
    ctx->lastItemPos = frameCtx.position;
    // IMPORTANTE: Actualizar lastItemSize explícitamente para que el contenedor padre (TabView)
    // sepa cuánto espacio ocupa realmente este ScrollView
    ctx->lastItemSize = frameCtx.size;
    // El cursor ya está restaurado, así que AdvanceCursor avanzará desde él
    AdvanceCursor(ctx, frameCtx.size);
  } else {
    // Si usa posición absoluta, solo restaurar el cursor sin avanzarlo
    // El cursor ya fue restaurado arriba
  }
}

bool BeginTabView(const std::string &id, int *activeTab,
                  const std::vector<std::string> &tabLabels, const Vec2 &size,
                  const Vec2 &pos) {
  UIContext *ctx = GetContext();
  if (!ctx || tabLabels.empty())
    return false;

  std::string key = "TABVIEW:" + id;
  uint32_t tabViewId = GenerateId(key.c_str());
  auto &state = ctx->tabViewStates[tabViewId];

  if (!state.initialized) {
    state.activeTab = activeTab ? *activeTab : 0;
    state.initialized = true;
  } else {
    // Reset widthCalculated to allow dynamic updates based on content changes each frame
    state.widthCalculated = false; 
    
    if (activeTab) {
      state.activeTab = *activeTab;
    }
  }

  const PanelStyle &panelStyle = ctx->style.panel;
  const TextStyle &tabTextStyle =
      ctx->style.GetTextStyle(TypographyStyle::Body);
  float tabHeight = tabTextStyle.fontSize + panelStyle.padding.y * 2.0f;
  float tabSpacing = 4.0f;

  // Calcular tamaño de la barra de pestañas
  float tabBarWidth = 0.0f;
  for (const auto &label : tabLabels) {
    Vec2 labelSize = ctx->renderer.MeasureText(label, tabTextStyle.fontSize);
    tabBarWidth += labelSize.x + panelStyle.padding.x * 2.0f + tabSpacing;
  }
  tabBarWidth -= tabSpacing; // Remover el último spacing

  Vec2 tabViewSize = size;
  
  // ANCHO: Siempre calcular dinámicamente basándose en el contenido
  // Usar el tamaño calculado del contenido si está disponible
  // Modificación: Permitir usar content size incluso con absolute pos si el usuario no forzó un tamaño fijo
  bool userForcedWidth = (size.x > 0.0f && size.x <= 2000.0f);
  
  if (state.initialized && state.viewSize.x > 0.0f && (!state.useAbsolutePos || !userForcedWidth)) {
    // Usar el tamaño calculado previamente basado en el contenido
    tabViewSize.x = state.viewSize.x;
  } else {
    // Si no hay tamaño calculado, calcular tamaño inicial basado SOLO en las pestañas
    // Ignorar size.x si es 0 o muy grande (mayor que un threshold razonable)
    if (tabViewSize.x <= 0.0f || tabViewSize.x > 2000.0f) {
      tabViewSize.x = tabBarWidth + panelStyle.padding.x * 2.0f;
      // Asegurar un mínimo razonable
      if (tabViewSize.x < 200.0f) {
        tabViewSize.x = 200.0f;
      }
    }
  }
  
  // ALTURA: Si se especifica un valor fijo (size.y > 0 y <= threshold razonable), usarlo
  // Si no, calcular dinámicamente
  if (size.y > 0.0f && size.y <= 2000.0f) {
    // Altura fija especificada por el usuario
    state.fixedHeight = true;
    state.fixedHeightValue = size.y;
    tabViewSize.y = size.y;
  } else {
    // Altura automática - calcular dinámicamente
    state.fixedHeight = false;
    state.fixedHeightValue = 0.0f;
    
    // Usar la altura calculada dinámicamente si está disponible
    if (state.initialized && state.viewSize.y > 0.0f && (!state.useAbsolutePos || size.y <= 0.0f)) {
      // Usar la altura calculada previamente basada en el contenido
      tabViewSize.y = state.viewSize.y;
    } else if (tabViewSize.y <= 0.0f) {
      // Si no hay altura calculada y no se especificó una, usar un valor por defecto
      tabViewSize.y = 300.0f;
    }
  }
  
  // IMPORTANTE: Limitar el ancho al viewport Y al clipping del panel padre si existe
  // También considerar el botón de undock si el panel está dockeado
  // Si el ancho ya fue calculado del contenido (state.viewSize.x), no limitarlo aquí
  // porque ya fue limitado en EndTabView si era necesario
  // Solo limitar si es un ancho inicial o si realmente excede el viewport
  Vec2 viewport = ctx->renderer.GetViewportSize();
  float maxViewportWidth = viewport.x;
  // Si se usa posición absoluta, ajustar el máximo según la posición
  if (pos.x != 0.0f || pos.y != 0.0f) {
    maxViewportWidth = viewport.x - pos.x - 20.0f; // Dejar margen de 20px
  } else {
    // Si no usa posición absoluta, usar la posición del cursor
    maxViewportWidth = viewport.x - ctx->cursorPos.x - 20.0f; // Dejar margen de 20px
  }
  if (maxViewportWidth < 0.0f) maxViewportWidth = viewport.x - 20.0f; // Fallback
  
  // También considerar el clipping del panel padre si existe
  float maxClipWidth = maxViewportWidth;
  if (!ctx->renderer.GetClipStack().empty()) {
    const auto &clip = ctx->renderer.GetClipStack().back();
    float clipWidth = static_cast<float>(clip.width);
    float clipX = static_cast<float>(clip.x);
    // Calcular el ancho máximo disponible dentro del clipping
    float availableClipWidth = clipX + clipWidth - (pos.x != 0.0f ? pos.x : ctx->cursorPos.x);
    if (availableClipWidth > 0.0f && availableClipWidth < maxClipWidth) {
      maxClipWidth = availableClipWidth;
    }
  }
  
  // IMPORTANTE: El ajuste del botón de undock se hace después de calcular tabViewPos
  // para poder verificar la superposición real
  
  // Solo limitar si el ancho realmente excede el viewport/clipping Y no es un ancho calculado del contenido
  // Si state.viewSize.x ya tiene un valor calculado, confiar en él (ya fue limitado en EndTabView si era necesario)
  if (!state.initialized || !(state.viewSize.x > 0.0f)) {
    // Solo limitar anchos iniciales, no los calculados del contenido
    if (tabViewSize.x > maxClipWidth) {
      tabViewSize.x = maxClipWidth;
    }
  } else if (tabViewSize.x > maxClipWidth * 1.5f) {
    // Si el ancho calculado es mucho mayor que el viewport/clipping (más del 150%), limitarlo
    // Esto previene casos donde el cálculo del contenido fue incorrecto
    tabViewSize.x = maxClipWidth;
  }
  
  // NO actualizar state.viewSize aquí si ya tiene un valor calculado del contenido
  // Solo actualizar si es la primera vez. Si es absolute pos, NO sobrescribir si ya tenemos un tamaño válido.
  if (!state.initialized || (state.useAbsolutePos && state.viewSize.x <= 0.0f)) {
    state.viewSize = tabViewSize;
  }

  // Resolver posición: usar pos si se proporciona, sino usar cursor
  Vec2 tabViewPos;
  if (pos.x != 0.0f || pos.y != 0.0f) {
    tabViewPos = pos;
    state.useAbsolutePos = true;
    state.absolutePos = pos;
  } else {
    state.useAbsolutePos = false; // Reset to false specific for this frame call
    tabViewPos = ctx->cursorPos;
    tabViewPos = ResolveAbsolutePosition(ctx, tabViewPos, tabViewSize);
    state.useAbsolutePos = false;
  }
  
  // IMPORTANTE: Ajustar el tamaño del TabView para evitar superposición con el botón de undock
  // El botón de undock está en la esquina superior derecha del panel dockeado
  // Verificar si el TabView se superpone al área del botón de undock
  float undockButtonSize = 20.0f;
  float undockButtonPadding = 4.0f;
  float undockButtonTotalWidth = undockButtonSize + undockButtonPadding;
  
  // Verificar si estamos dentro de un panel dockeado y ajustar el ancho del TabView
  // para evitar que la barra de pestañas se superponga con el botón de undock
  if (!ctx->renderer.GetClipStack().empty()) {
    const auto &clip = ctx->renderer.GetClipStack().back();
    float clipRight = static_cast<float>(clip.x + clip.width);
    float viewportRight = viewport.x;
    float clipTop = static_cast<float>(clip.y);
    
    // Si el clip está cerca del borde derecho del viewport (dentro de 30px),
    // probablemente hay un botón de undock en la esquina superior derecha
    if (std::abs(clipRight - viewportRight) < 30.0f) {
      // Calcular la posición exacta del botón de undock basándonos en el clip
      // El botón está en: state.position.x + state.size.x - undockButtonSize - 4.0f, state.position.y + 4.0f
      // El clip está en: clipPos = state.position + Vec2(padding.x, titleOffset + padding.y)
      // Para paneles dockeados: titleOffset = 0.0f
      // Entonces: state.position.x = clipPos.x - padding.x
      //           state.position.y = clipPos.y - padding.y (para docked)
      //           state.size.x = clipSize.x + padding.x * 2.0f
      //           undockButtonPos.x = clipPos.x - padding.x + clipSize.x + padding.x * 2.0f - undockButtonSize - 4.0f
      //                             = clipPos.x + clipSize.x + padding.x - undockButtonSize - 4.0f
      //           undockButtonPos.y = clipPos.y - padding.y + 4.0f
      float estimatedPanelPadding = 8.0f; // padding.x del panel
      float undockButtonRight = clipRight + estimatedPanelPadding - 4.0f;
      float undockButtonLeft = undockButtonRight - undockButtonSize;
      float undockButtonTop = clipTop - estimatedPanelPadding + 4.0f;
      float undockButtonBottom = undockButtonTop + undockButtonSize;
      
      // Verificar si el TabView (especialmente la barra de pestañas) se superpone
      float tabViewRight = tabViewPos.x + tabViewSize.x;
      float tabBarTop = tabViewPos.y;
      float tabBarBottom = tabViewPos.y + tabHeight;
      
      // Si la barra de pestañas se superpone verticalmente con el botón de undock
      // Y el TabView se extiende horizontalmente dentro del área del botón
      bool verticalOverlap = (tabBarTop < undockButtonBottom && tabBarBottom > undockButtonTop);
      bool horizontalOverlap = (tabViewRight > undockButtonLeft);
      
      // IMPORTANTE: Siempre evitar la superposición si hay overlap vertical y horizontal
      // Reducir el ancho del TabView para dejar espacio al botón de undock
      if (verticalOverlap && horizontalOverlap) {
        float overlap = tabViewRight - undockButtonLeft;
        // Asegurar que siempre dejemos espacio para el botón de undock
        // Agregar un pequeño margen adicional para evitar que se toquen
        float margin = 4.0f;
        tabViewSize.x = std::max(0.0f, tabViewSize.x - overlap - margin);
        // Asegurar un ancho mínimo razonable
        if (tabViewSize.x < 100.0f) {
          tabViewSize.x = 100.0f;
        }
      }
    }
  }
  
  // Registrar área ocupada solo si NO se usa posición absoluta
  if (!state.useAbsolutePos) {
    RegisterOccupiedArea(ctx, tabViewPos, tabViewSize);
  }

  state.tabBarSize = Vec2(tabBarWidth + panelStyle.padding.x * 2.0f, tabHeight);

  // Calcular si necesitamos scrollbar horizontal para las pestañas ANTES de calcular contentSize
  // El scrollbar de las pestañas será más delgado y transparente (menos invasivo)
  float tabBarScrollbarWidth = 6.0f; // Scrollbar más delgado para las pestañas
  float availableTabBarWidth = tabViewSize.x - panelStyle.padding.x * 2.0f;
  bool needsTabBarScrollbar = tabBarWidth > availableTabBarWidth;
  
  // Ajustar el ancho disponible si hay scrollbar
  if (needsTabBarScrollbar) {
    availableTabBarWidth -= tabBarScrollbarWidth;
  }
  
  // Manejar scroll horizontal de las pestañas
  float maxTabBarScroll = std::max(0.0f, tabBarWidth - availableTabBarWidth);
  state.tabBarScrollOffset = std::clamp(state.tabBarScrollOffset, 0.0f, maxTabBarScroll);
  
  // IMPORTANTE: NO reducir la altura del contenido por el scrollbar de las pestañas
  // El scrollbar de las pestañas se dibujará sobre la barra de pestañas, no en la parte inferior

  // Configurar área de contenido PRIMERO (debajo de la barra de pestañas)
  // Usar un padding más pequeño dentro del TabView para evitar problemas de
  // clipping
  float innerPadding = 8.0f;
  Vec2 contentPos(tabViewPos.x + innerPadding,
                  tabViewPos.y + tabHeight + innerPadding);
  // Si el ancho del TabView fue calculado del contenido, usar ese ancho
  // Si no, calcular el área de contenido restando el padding
  float contentAreaWidth = tabViewSize.x - innerPadding * 2.0f;
  // Asegurar que el ancho del área de contenido no sea negativo
  if (contentAreaWidth < 0.0f) {
    contentAreaWidth = 0.0f;
  }
  // Si el ancho disponible es muy grande (mayor que un threshold razonable), 
  // no usarlo para forzar el cálculo del ancho del contenido
  if (contentAreaWidth > 2000.0f) {
    contentAreaWidth = 0.0f; // Forzar cálculo automático del ancho
  }
  // IMPORTANTE: El scrollbar horizontal de las pestañas se dibuja sobre la barra de pestañas
  // NO reducir la altura del contenido por el scrollbar de las pestañas
  // El scrollbar horizontal del CONTENIDO se manejará en EndTabView
  Vec2 contentSize(contentAreaWidth,
                   tabViewSize.y - tabHeight - innerPadding * 2.0f);

  // Asegurar que el contentSize sea válido
  if (contentSize.x < 0.0f)
    contentSize.x = 0.0f;
  if (contentSize.y < 0.0f)
    contentSize.y = 0.0f;

  // Dibujar fondo del TabView PRIMERO - esto debe renderizarse antes que los
  // widgets - con contraste sin borde
  Color tabViewBg = panelStyle.background;
  bool isDarkTheme = (ctx->style.backgroundColor.r < 0.5f);
  if (isDarkTheme) {
    // Más claro que el fondo para mejor contraste
    tabViewBg = Color(tabViewBg.r * 1.15f, tabViewBg.g * 1.15f, tabViewBg.b * 1.15f, 1.0f);
  } else {
    // Más oscuro que el fondo para mejor contraste
    tabViewBg = Color(tabViewBg.r * 0.92f, tabViewBg.g * 0.92f, tabViewBg.b * 0.92f, 1.0f);
  }
  ctx->renderer.DrawRectFilled(tabViewPos, tabViewSize, tabViewBg,
                               panelStyle.cornerRadius);

  // Dibujar barra de pestañas - con contraste sin borde
  Vec2 tabBarPos = tabViewPos;
  Vec2 tabBarBgPos = tabBarPos;
  Vec2 tabBarBgSize(tabViewSize.x, tabHeight);
  Color tabBarBg = panelStyle.headerBackground;
  if (isDarkTheme) {
    // Más claro que el fondo para mejor contraste
    tabBarBg = Color(tabBarBg.r * 1.15f, tabBarBg.g * 1.15f, tabBarBg.b * 1.15f, 1.0f);
  } else {
    // Más oscuro que el fondo para mejor contraste
    tabBarBg = Color(tabBarBg.r * 0.92f, tabBarBg.g * 0.92f, tabBarBg.b * 0.92f, 1.0f);
  }
  ctx->renderer.DrawRectFilled(tabBarBgPos, tabBarBgSize, tabBarBg, 0.0f);

  // Aplicar clipping a la barra de pestañas para que las pestañas se corten correctamente
  // (needsTabBarScrollbar y maxTabBarScroll ya fueron calculados arriba)
  Vec2 tabBarClipPos = tabBarPos + Vec2(panelStyle.padding.x, 0.0f);
  Vec2 tabBarClipSize(availableTabBarWidth, tabHeight);
  ctx->renderer.PushClipRect(tabBarClipPos, tabBarClipSize);

  // Dibujar pestañas con scroll horizontal aplicado
  float currentX = tabBarPos.x + panelStyle.padding.x - state.tabBarScrollOffset;
  float mouseX = ctx->input.MouseX();
  float mouseY = ctx->input.MouseY();
  bool leftPressed = ctx->input.IsMousePressed(0);
  bool leftDown = ctx->input.IsMouseDown(0);

  for (size_t i = 0; i < tabLabels.size(); ++i) {
    Vec2 labelSize =
        ctx->renderer.MeasureText(tabLabels[i], tabTextStyle.fontSize);
    Vec2 tabSize(labelSize.x + panelStyle.padding.x * 2.0f, tabHeight);
    Vec2 tabPos(currentX, tabBarPos.y);

    bool hover = (mouseX >= tabPos.x && mouseX <= tabPos.x + tabSize.x &&
                  mouseY >= tabPos.y && mouseY <= tabPos.y + tabSize.y);
    bool isActive = (static_cast<int>(i) == state.activeTab);

    // Dibujar fondo de la pestaña - sin borde, solo contraste, con bordes redondeados
    Color tabBg;
    if (isActive) {
      // Tab activa: usar el mismo color que el fondo del TabView para que no haya borde visible
      tabBg = tabViewBg;
    } else {
      // Tab inactiva: usar el mismo color que la barra de tabs
      tabBg = tabBarBg;
    }
    if (hover && !isActive) {
      tabBg = Color(tabBg.r * 1.1f, tabBg.g * 1.1f, tabBg.b * 1.1f, tabBg.a);
    }
    // Bordes redondeados en las pestañas (radio pequeño para esquinas suaves)
    // Solo redondear las esquinas superiores para que se vean mejor
    float tabCornerRadius = 6.0f;
    // Dibujar la pestaña con bordes redondeados
    ctx->renderer.DrawRectFilled(tabPos, tabSize, tabBg, tabCornerRadius);

    // Dibujar indicador de pestaña activa (línea inferior)
    if (isActive) {
      Vec2 indicatorStart(tabPos.x, tabPos.y + tabSize.y - 2.0f);
      Vec2 indicatorEnd(tabPos.x + tabSize.x, tabPos.y + tabSize.y - 2.0f);
      Color indicatorColor = ctx->style.button.background.normal;
      ctx->renderer.DrawLine(indicatorStart, indicatorEnd, indicatorColor,
                             3.0f);
    }

    // Dibujar texto de la pestaña
    Vec2 textPos(tabPos.x + panelStyle.padding.x,
                 tabPos.y + (tabHeight - labelSize.y) * 0.5f);
    Color textColor =
        isActive
            ? tabTextStyle.color
            : Color(tabTextStyle.color.r * 0.8f, tabTextStyle.color.g * 0.8f,
                    tabTextStyle.color.b * 0.8f, tabTextStyle.color.a);
    ctx->renderer.DrawText(textPos, tabLabels[i], textColor,
                           tabTextStyle.fontSize);

    // Manejar click en la pestaña
    if (hover && leftPressed) {
      state.activeTab = static_cast<int>(i);
      if (activeTab) {
        *activeTab = state.activeTab;
      }
    }

    currentX += tabSize.x + tabSpacing;
  }
  
  // Remover clipping de la barra de pestañas
  ctx->renderer.PopClipRect();
  
  // Dibujar scrollbar horizontal para las pestañas si es necesario
  // IMPORTANTE: El scrollbar de las pestañas debe ser delgado y transparente (menos invasivo)
  // Se dibuja sobre la barra de pestañas, en la parte inferior de la barra
  if (needsTabBarScrollbar && maxTabBarScroll > 0.0f) {
    // Posicionar el scrollbar en la parte inferior de la barra de pestañas
    Vec2 hScrollbarPos(tabBarPos.x, tabBarPos.y + tabHeight - tabBarScrollbarWidth);
    Vec2 hScrollbarSize(tabViewSize.x, tabBarScrollbarWidth);
    
    // Fondo del scrollbar - más transparente
    Color scrollbarBg = tabBarBg;
    scrollbarBg.a = 0.3f; // Más transparente
    ctx->renderer.DrawRectFilled(hScrollbarPos, hScrollbarSize, scrollbarBg, 0.0f);
    
    // Calcular thumb
    float ratio = availableTabBarWidth / tabBarWidth;
    float thumbWidth = std::max(20.0f, hScrollbarSize.x * ratio);
    float maxThumbTravel = hScrollbarSize.x - thumbWidth;
    float thumbX = maxTabBarScroll > 0.0f
                      ? (state.tabBarScrollOffset / maxTabBarScroll) * maxThumbTravel
                      : 0.0f;
    Vec2 thumbPos(hScrollbarPos.x + thumbX, hScrollbarPos.y);
    Vec2 thumbSize(thumbWidth, tabBarScrollbarWidth);
    
    // Detectar hover y drag
    bool hoverThumb = (mouseX >= thumbPos.x && mouseX <= thumbPos.x + thumbSize.x &&
                       mouseY >= thumbPos.y && mouseY <= thumbPos.y + thumbSize.y);
    bool hoverTrack = (mouseX >= hScrollbarPos.x && mouseX <= hScrollbarPos.x + hScrollbarSize.x &&
                       mouseY >= hScrollbarPos.y && mouseY <= hScrollbarPos.y + hScrollbarSize.y);
    
    // Generar ID estable para el scrollbar de las pestañas
    std::string tabScrollIdStr = "TabView_TabBar_Scroll_" + std::to_string(tabViewId);
    uint32_t tabScrollId = GenerateId(tabScrollIdStr.c_str());
    
    // Manejar drag del thumb
    if (ctx->activeWidgetId == tabScrollId && state.draggingTabBarScrollbar) {
      if (!leftDown) {
        state.draggingTabBarScrollbar = false;
        ctx->activeWidgetId = 0;
      } else {
        float mouseDeltaX = mouseX - state.dragStartMouse.x;
        float scrollDelta = (mouseDeltaX / maxThumbTravel) * maxTabBarScroll;
        state.tabBarScrollOffset = std::clamp(state.dragStartScroll + scrollDelta, 0.0f, maxTabBarScroll);
      }
    } else if (leftPressed && hoverThumb && ctx->activeWidgetId == 0) {
      ctx->activeWidgetId = tabScrollId;
      state.draggingTabBarScrollbar = true;
      state.dragStartMouse = Vec2(mouseX, mouseY);
      state.dragStartScroll = state.tabBarScrollOffset;
    } else if (leftPressed && hoverTrack && ctx->activeWidgetId == 0) {
      float clickPos = mouseX - hScrollbarPos.x;
      float scrollRatioClick = clickPos / hScrollbarSize.x;
      state.tabBarScrollOffset = std::clamp(scrollRatioClick * maxTabBarScroll, 0.0f, maxTabBarScroll);
    }
    
    // Dibujar thumb - más transparente y menos invasivo
    Color thumbColor = hoverThumb || state.draggingTabBarScrollbar
                           ? Color(1.0f, 1.0f, 1.0f, 0.6f) // Más visible al hover
                           : Color(1.0f, 1.0f, 1.0f, 0.4f); // Más transparente por defecto
    ctx->renderer.DrawRectFilled(thumbPos, thumbSize, thumbColor, 2.0f);
  } else {
    // Si no hay scrollbar, resetear el estado
    state.draggingTabBarScrollbar = false;
    state.tabBarScrollOffset = 0.0f;
  }

  // IMPORTANTE: NO hacer FlushBatch() aquí - PushClipRect() lo hará automáticamente
  // Hacer FlushBatch() explícito aquí causa flickering porque renderiza el contenido
  // antes de que esté completamente dibujado

  // NO sobrescribir state.viewSize aquí si ya tenemos un valor calculado del contenido
  // state.viewSize se actualiza en EndTabView basado en el ancho real del contenido
  // Solo establecerlo la primera vez o si no tenemos un valor calculado
  if (!state.initialized || state.viewSize.x <= 0.0f) {
    state.viewSize = contentSize;
  } else {
    // Mantener el ancho calculado, solo actualizar la altura si es necesario
    state.viewSize.y = contentSize.y;
  }

  // Manejar interacción del mouse con scrollbar ANTES de dibujar contenido
  // (usando contentSize del frame anterior)
  // leftDown ya está declarado arriba, no redeclarar

  // Calcular si necesitamos scrollbar vertical
  bool needsVerticalScrollbar = state.contentSize.y > contentSize.y;

  if (needsVerticalScrollbar) {
    float scrollbarWidth = 10.0f;
    Vec2 barPos(contentPos.x + contentSize.x - scrollbarWidth, contentPos.y);
    Vec2 barSize(scrollbarWidth, contentSize.y);

    float ratio = contentSize.y / state.contentSize.y;
    float thumbHeight = std::max(20.0f, barSize.y * ratio);
    float maxThumbTravel = barSize.y - thumbHeight;
    float maxScrollY = std::max(0.0f, state.contentSize.y - contentSize.y);
    // Usar scrollOffset del tab activo
    Vec2& tabScrollOffset = state.tabScrollOffsets[state.activeTab];
    float thumbY = maxScrollY > 0.0f
                       ? (tabScrollOffset.y / maxScrollY) * maxThumbTravel
                       : 0.0f;

    Vec2 thumbPos(barPos.x, barPos.y + thumbY);
    Vec2 thumbSize(scrollbarWidth, thumbHeight);

    // Detectar hover y drag
    bool hoverThumb = (mouseX >= thumbPos.x && mouseX <= thumbPos.x + thumbSize.x &&
                       mouseY >= thumbPos.y && mouseY <= thumbPos.y + thumbSize.y);
    bool hoverTrack = (mouseX >= barPos.x && mouseX <= barPos.x + barSize.x &&
                       mouseY >= barPos.y && mouseY <= barPos.y + barSize.y);

    if (state.draggingScrollbar) {
      if (!leftDown) {
        state.draggingScrollbar = false;
      } else {
        float mouseDelta = mouseY - state.dragStartMouse.y;
        float scrollDelta = (mouseDelta / maxThumbTravel) * maxScrollY;
        tabScrollOffset.y =
            std::clamp(state.dragStartScroll + scrollDelta, 0.0f, maxScrollY);
      }
    } else if (leftPressed && hoverThumb) {
      state.draggingScrollbar = true;
      state.dragStartMouse = Vec2(mouseX, mouseY);
      state.dragStartScroll = tabScrollOffset.y;
    } else if (leftPressed && hoverTrack) {
      // Click en el track: saltar a esa posición
      float clickPos = mouseY - barPos.y;
      float scrollRatioClick = clickPos / barSize.y;
      tabScrollOffset.y = std::clamp(scrollRatioClick * maxScrollY, 0.0f, maxScrollY);
    }
  }

  // Obtener scrollOffset para el tab activo (cada tab tiene su propio scrollOffset)
  Vec2& tabScrollOffset = state.tabScrollOffsets[state.activeTab];

  // Aplicar clipping al área de contenido ANTES de configurar el layout
  // Esto asegura que los widgets solo se dibujen dentro del área visible
  // IMPORTANTE: Usar el ancho real del TabView para el clipping, no contentSize.x que puede ser 0
  // El clipping debe permitir que el contenido se renderice correctamente
  Vec2 clipSize;
  // Siempre usar el ancho del TabView menos el padding para el clipping
  clipSize.x = tabViewSize.x - innerPadding * 2.0f;
  clipSize.y = contentSize.y; // Usar la altura del área de contenido
  
  // Asegurar que el clipSize sea válido (no negativo)
  if (clipSize.x < 0.0f) {
    clipSize.x = 0.0f;
  }
  if (clipSize.y < 0.0f) {
    clipSize.y = 0.0f;
  }
  
  // IMPORTANTE: NO hacer FlushBatch() explícito aquí - PushClipRect() lo hará automáticamente
  // Hacer FlushBatch() explícito aquí causa flickering porque renderiza el contenido
  // antes de que esté completamente dibujado. PushClipRect() hará el flush cuando sea necesario.
  
  // NO limitar el clipping al viewport aquí - el TabView ya se limitó en BeginTabView
  // Si limitamos el clipping también, cortaremos el contenido innecesariamente
  // IMPORTANTE: El clipping debe estar en contentPos (sin scroll) para que el área visible sea correcta
  ctx->renderer.PushClipRect(contentPos, clipSize);
  
  // IMPORTANTE: Aplicar el scroll usando offsetStack, similar a BeginScrollView
  // El offsetStack contiene la posición base (contentPos), y el cursor se ajusta restando el scrollOffset
  // Esto permite que el contenido se desplace correctamente tanto horizontal como verticalmente
  ctx->offsetStack.push_back(contentPos);
  
  // Configurar el cursor para el contenido (ajustado por scroll)
  // Similar a BeginScrollView: cursorPos = contentAreaPos - scrollOffset
  ctx->cursorPos = contentPos - tabScrollOffset;
  // IMPORTANTE: lastItemPos debe estar en coordenadas absolutas (sin scroll) para el cálculo del ancho
  // El scroll se aplica mediante offsetStack y cursorPos, pero para el cálculo del ancho necesitamos las coordenadas sin scroll
  ctx->lastItemPos = contentPos; // Usar contentPos (sin scroll) para que el cálculo del ancho sea correcto
  ctx->lastItemSize = Vec2(0.0f, 0.0f);
  
  BeginVertical(ctx->style.spacing, std::make_optional(Vec2(contentSize.x, contentSize.y)),
                std::make_optional(Vec2(0.0f, 0.0f)));

  // Calculate Parent Max Height constraint
  float parentMaxHeight = 10000.0f;
  if (!ctx->layoutStack.empty()) {
      parentMaxHeight = ctx->layoutStack.back().availableSpace.y;
  }

  // Guardar frame del tab para calcular tamaño de contenido al cerrar
  // IMPORTANTE: contentAreaSize debe ser el tamaño VISIBLE del área de contenido, no el tamaño deseado
  Vec2 contentAreaSize(contentAreaWidth, contentSize.y);
  ctx->tabFrameStack.push_back(
      TabContentFrame{tabViewId, contentPos, contentAreaSize, ctx->cursorPos, parentMaxHeight});

  // Actualizar activeTab externo
  if (activeTab) {
    *activeTab = state.activeTab;
  }

  return true;
}

void EndTabView() {
  UIContext *ctx = GetContext();
  if (!ctx)
    return;

  // Cerrar el layout vertical para finalizar y conocer el cursor final (tamaño
  // de contenido)
  Vec2 contentEndCursor = ctx->cursorPos;
  
  // Tomar frame guardado ANTES de cerrar el layout para tener acceso a contentStartCursor
  TabContentFrame frame;
  if (!ctx->tabFrameStack.empty()) {
    frame = ctx->tabFrameStack.back();
  }
  
  // Calcular el ancho REAL del contenido: encontrar el elemento más a la derecha
  // El ancho real es: (posición del elemento más a la derecha + su ancho) - posición inicial del contenido
  float maxContentWidth = 0.0f;
  
  // contentStartCursor se guarda después de BeginVertical, que usa scrolledContentPos
  // Pero scrolledContentPos tiene el scroll aplicado en Y, no en X
  // Así que contentStartCursor.x debería ser contentPos.x (sin scroll en X)
  float contentStartX = frame.contentAreaPos.x; // Usar contentAreaPos.x que es la posición real sin scroll
  
  // Calcular el ancho TOTAL del contenido de este tab
  // El ancho total es: desde el inicio del contenido hasta el elemento más a la derecha
  // IMPORTANTE: lastItemPos está en coordenadas con scroll aplicado mediante offsetStack
  // Como el scroll se aplica mediante offsetStack, lastItemPos ya está en coordenadas relativas
  // Necesitamos obtener la posición absoluta sumando el offset del scroll
  // NOTA: Esto se calculará después de obtener el estado del TabView (línea 3678)
  // Por ahora, calculamos sin el scroll offset y lo ajustaremos después si es necesario
  float lastItemRightEdgeAbsolute = ctx->lastItemPos.x + ctx->lastItemSize.x;
  float anchoTotalContenido = lastItemRightEdgeAbsolute - contentStartX;
  
  // También considerar el ancho del layout si es mayor
  if (!ctx->layoutStack.empty()) {
    const auto &lastStack = ctx->layoutStack.back();
    float anchoLayout = lastStack.contentSize.x;
    // Usar el mayor entre el ancho calculado de items y el ancho del layout
    if (anchoLayout > anchoTotalContenido) {
      anchoTotalContenido = anchoLayout;
    }
  }
  
  maxContentWidth = anchoTotalContenido;
  
  // IMPORTANTE: El clipping debe mantenerse activo hasta que TODO el contenido esté dibujado.
  // NO remover el clipping aquí - se removerá al final después de dibujar el scrollbar.
  // Esto previene el flickering causado por FlushBatch() cuando se remueve el clipping demasiado temprano.
  
  if (!ctx->layoutStack.empty()) {
    EndVertical(false);
  }
  
  // IMPORTANTE: En este punto, el contenido del TabView ya debería estar completamente en el batch.
  // El clipping todavía está activo, lo cual está bien porque previene que se dibuje fuera del área.

  // Remover el frame de la pila después de usarlo
  if (!ctx->tabFrameStack.empty()) {
    ctx->tabFrameStack.pop_back();
  }

  // Actualizar estado del TabView PRIMERO para tener acceso a state
  UIContext *ctx2 = GetContext();
  if (!ctx2) {
    // Remover el clip rect si existe
    if (!ctx->renderer.GetClipStack().empty()) {
      ctx->renderer.PopClipRect();
    }
    return;
  }
  
  auto it = ctx2->tabViewStates.find(frame.tabViewId);
  if (it == ctx2->tabViewStates.end()) {
    // Remover el clip rect si existe
    if (!ctx->renderer.GetClipStack().empty()) {
      ctx->renderer.PopClipRect();
    }
    return;
  }
  
  auto &st = it->second;
  
  // Ajustar el cálculo del ancho si es necesario (agregar scroll offset si lastItemPos está en coordenadas relativas)
  // Como usamos offsetStack, lastItemPos está en coordenadas relativas al offset
  // Necesitamos sumar el scroll offset para obtener la posición absoluta
  Vec2& tabScrollOffsetForWidth = st.tabScrollOffsets[st.activeTab];
  float lastItemRightEdgeAbsoluteAjustado = ctx->lastItemPos.x + ctx->lastItemSize.x + tabScrollOffsetForWidth.x;
  float anchoTotalContenidoAjustado = lastItemRightEdgeAbsoluteAjustado - contentStartX;
  // Usar el mayor entre el ancho calculado original y el ajustado
  if (anchoTotalContenidoAjustado > anchoTotalContenido) {
    anchoTotalContenido = anchoTotalContenidoAjustado;
    maxContentWidth = anchoTotalContenido; // Actualizar maxContentWidth con el valor ajustado
  }
  
  // Calcular tamaño de contenido real
  Vec2 contentSize;
  // Usar SOLO el ancho real del contenido renderizado calculado arriba
  contentSize.x = maxContentWidth > 0.0f ? maxContentWidth : 0.0f;
  
  // Calcular la altura REAL del contenido: desde el inicio hasta el último elemento
  // IMPORTANTE: lastItemPos puede estar en coordenadas con scroll aplicado o sin scroll
  // dependiendo de cómo se actualizó durante el frame. Necesitamos usar el cursor final
  // que está en coordenadas con scroll aplicado, y sumar el scroll offset para obtener
  // la posición absoluta real.
  float contentStartY = frame.contentAreaPos.y; // Posición inicial sin scroll
  Vec2& tabScrollOffset = st.tabScrollOffsets[st.activeTab];
  
  // contentEndCursor está en coordenadas con scroll aplicado (scrolledContentPos)
  // Necesitamos sumar el scroll offset para obtener la posición absoluta real
  // NOTA: contentEndCursor ya tiene el scroll aplicado mediante offsetStack, así que
  // necesitamos sumar el scroll offset para obtener la posición absoluta
  float contentEndYAbsolute = contentEndCursor.y + tabScrollOffset.y;
  float alturaTotalContenido = contentEndYAbsolute - contentStartY;
  
  // También considerar lastItemPos como respaldo si contentEndCursor no es confiable
  float lastItemBottomEdgeAbsolute = (ctx->lastItemPos.y + ctx->lastItemSize.y);
  // Si lastItemPos está en coordenadas con scroll, sumar el offset
  // Si no, ya está en coordenadas absolutas
  // Para estar seguros, asumimos que está con scroll y sumamos el offset
  float lastItemBottomWithScroll = lastItemBottomEdgeAbsolute + tabScrollOffset.y;
  float alturaDesdeLastItem = lastItemBottomWithScroll - contentStartY;
  
  // Usar el mayor entre contentEndCursor y lastItemPos para asegurar que capturamos todo el contenido
  alturaTotalContenido = std::max(alturaTotalContenido, alturaDesdeLastItem);
  
  // También considerar la altura del layout si es mayor
  float alturaLayout = 0.0f;
  if (!ctx->layoutStack.empty()) {
    const auto &lastStack = ctx->layoutStack.back();
    alturaLayout = lastStack.contentSize.y;
  }
  
  // Usar el mayor entre la altura calculada de items y la altura del layout
  // IMPORTANTE: Asegurarse de que la altura refleje el contenido real
  contentSize.y = std::max(alturaTotalContenido, alturaLayout);
  
  // Si el layout tiene contenido pero alturaTotalContenido es 0, usar solo alturaLayout
  if (alturaTotalContenido <= 0.0f && alturaLayout > 0.0f) {
    contentSize.y = alturaLayout;
  }
  
  // NO forzar un mínimo aquí - si el contenido es más pequeño que el área visible,
  // contentSize.y debe ser menor para que la scrollbar no aparezca innecesariamente
  // Solo asegurar que no sea negativo
  if (contentSize.y < 0.0f) {
    contentSize.y = 0.0f;
  }
  
  // IMPORTANTE: Usar el mayor entre el contentSize calculado y el guardado previamente
  // Esto previene que el scrollbar desaparezca cuando hay pequeñas fluctuaciones en el cálculo
  // Solo actualizar si el nuevo tamaño es significativamente mayor (más del 5% de diferencia)
  // o si el tamaño anterior era 0 (primera vez)
  if (st.contentSize.y > 0.0f) {
    float diferencia = contentSize.y - st.contentSize.y;
    float porcentajeDiferencia = std::abs(diferencia) / st.contentSize.y;
    // Si el nuevo tamaño es mayor, usarlo
    if (contentSize.y > st.contentSize.y) {
      st.contentSize = contentSize;
    }
    // Si el nuevo tamaño es menor pero la diferencia es pequeña (< 5%), mantener el anterior
    // para evitar que el scrollbar desaparezca por fluctuaciones
    else if (porcentajeDiferencia < 0.05f) {
      // Mantener el tamaño anterior para estabilidad
      contentSize.y = st.contentSize.y;
    } else {
      // La diferencia es significativa, usar el nuevo tamaño (el contenido realmente se redujo)
      st.contentSize = contentSize;
    }
  } else {
    // Primera vez o no hay tamaño guardado, usar el calculado
    st.contentSize = contentSize;
  }
  
  // Actualizar contentSize para usar en el resto de EndTabView
  contentSize = st.contentSize;
  
  // Guardar el ancho REAL del contenido de este tab específico
  // Este es el ancho total desde el inicio del contenido hasta el elemento más a la derecha
  float anchoFinal = maxContentWidth;
  st.tabContentWidths[st.activeTab] = anchoFinal > 0.0f ? anchoFinal : 0.0f;
  
  // Ajustar el ancho del TabView según el contenido de TODOS los tabs
  // Solo ajustar si no se usa posición absoluta (para evitar problemas de layout)
  if (!st.useAbsolutePos) {
    // Comparar el ancho de todos los tabs y encontrar el mayor
    float mayorAncho = 0.0f;
    bool hayContenidoMedido = false;
    
    // Limpiar índices fuera de rango (si el número de pestañas cambió)
    // Nota: No tenemos acceso directo a tabLabels.size() aquí, pero podemos asumir
    // que st.activeTab es válido. Una limpieza perfecta requeriría pasar tabCount a EndTabView
    // o almacenar tabCount en TabViewState. Por ahora, confiamos en el mapa.
    
    for (const auto &[tabIndex, tabWidth] : st.tabContentWidths) {
      if (tabWidth > 0.0f) {
        hayContenidoMedido = true;
        // Comparar y actualizar el mayor ancho
        if (tabWidth > mayorAncho) {
          mayorAncho = tabWidth;
        }
      }
    }
    
    // Solo ajustar si tenemos contenido medido y el ancho no ha sido calculado/bloqueado aún
    if (hayContenidoMedido && !st.widthCalculated) {
      // Comparar el ancho de las pestañas con el mayor ancho de contenido
      float tabBarWidth = st.tabBarSize.x;
      float innerPadding = 8.0f; // padding interno del TabView
      
      // El ancho del contenido almacenado ya incluye el ancho "real" de los widgets.
      // Necesitamos añadir el padding interno para obtener el ancho total del TabView
      float anchoContenidoConPadding = mayorAncho + innerPadding * 2.0f;
      
      // Comparar y usar el mayor entre el ancho de las pestañas y el contenido
      float nuevoAncho = tabBarWidth;
      if (anchoContenidoConPadding > tabBarWidth) {
        nuevoAncho = anchoContenidoConPadding;
      }
      
      // IMPORTANTE: Limitar el ancho al viewport para evitar que exceda la ventana
      Vec2 viewport = ctx2->renderer.GetViewportSize();
      // Calcular el ancho máximo basado en la posición del TabView
      float tabViewPosX = frame.contentAreaPos.x - innerPadding;
      float maxWidth = viewport.x - tabViewPosX - 20.0f; // Dejar margen de 20px
      if (maxWidth < 0.0f) maxWidth = viewport.x - 20.0f; // Fallback
      
      if (nuevoAncho > maxWidth) {
        nuevoAncho = maxWidth;
      }
      
      // Asegurar un ancho mínimo razonable
      nuevoAncho = std::max(nuevoAncho, tabBarWidth);
      
      // Actualizar el tamaño del TabView con el ancho calculado
      st.viewSize.x = nuevoAncho;
      st.widthCalculated = true; // Lock the width
    }
  }
  
  // IMPORTANTE: Ajustar la altura del TabView basándose en el contenido real
  // PERO solo si la altura NO es fija
  // IMPORTANTE: NO aumentar la altura si el contenido es más grande que el área visible
  // Esto previene que el TabView crezca infinitamente y hace que aparezca el scrollbar
  float tabHeight = st.tabBarSize.y;
  float innerPadding = 8.0f;
  
  if (!st.fixedHeight) {
    // Altura dinámica: calcular la altura mínima necesaria
    float minRequiredHeight = contentSize.y + tabHeight + innerPadding * 2.0f;
      // Logica mejorada de auto-height:
      // Permitir tanto expandir como contraer para ajustarse al contenido
      // Pero respetar los límites del viewport
      
      // Calcular altura máxima disponible (viewport - posición)
      // Calcular altura máxima disponible (viewport - posición)
      // Y también tener en cuenta el límite del padre (Dock/Panel) si existe
      Vec2 viewport = ctx2->renderer.GetViewportSize();
      float viewportLimit = viewport.y - frame.contentAreaPos.y - 20.0f;
      
      float maxAvailableHeight = viewportLimit;
      if (frame.maxAvailableHeight > 0.0f && frame.maxAvailableHeight < 10000.0f) {
          // Si tenemos un límite del padre explícito (ej. Dock Node), usarlo
          if (frame.maxAvailableHeight < maxAvailableHeight) {
              maxAvailableHeight = frame.maxAvailableHeight;
          }
      }

      if (maxAvailableHeight < 200.0f) maxAvailableHeight = 200.0f; // Mínimo de seguridad

      // Altura objetivo: lo que pide el contenido
      float targetHeight = minRequiredHeight;
      
      // Limitar al viewport
      if (targetHeight > maxAvailableHeight) {
          targetHeight = maxAvailableHeight;
      }
      
      // Aplicar cambio si es significativo (> 2px) o si estamos creciendo para evitar corte
      // Usar un pequeño umbral para evitar jitter por float precision
      if (std::abs(st.viewSize.y - targetHeight) > 2.0f && !st.useAbsolutePos) {
          st.viewSize.y = targetHeight;
      }
  } else {
    // Altura fija: usar el valor especificado
    st.viewSize.y = st.fixedHeightValue;
  }

  // IMPORTANTE: Calcular el tamaño actual del área visible del TabView
  // Usar frame.contentAreaSize como base (estable entre frames) pero actualizar
  // solo si el TabView realmente cambió de tamaño de manera significativa
  // (tabHeight e innerPadding ya están declarados arriba)
  Vec2 currentContentAreaSize;
  currentContentAreaSize.x = frame.contentAreaSize.x; // El ancho no cambia tan frecuentemente
  
  // Calcular la altura actual del área de contenido basándose en el tamaño actual del TabView
  float calculatedHeight = st.viewSize.y - tabHeight - innerPadding * 2.0f;
  if (calculatedHeight < 0.0f) {
    calculatedHeight = 0.0f;
  }
  
  // IMPORTANTE: Usar el frame.contentAreaSize.y como base para estabilidad
  // Solo actualizar si la diferencia es significativa (más del 5%) Y el TabView realmente cambió
  // Esto previene fluctuaciones que hacen desaparecer el scrollbar
  float diferenciaAltura = std::abs(calculatedHeight - frame.contentAreaSize.y);
  if (frame.contentAreaSize.y > 0.0f) {
    float porcentajeDiferencia = diferenciaAltura / frame.contentAreaSize.y;
    // Solo actualizar si la diferencia es significativa (más del 5%)
    // Y si el tamaño calculado es menor o igual (TabView se redujo o se mantuvo)
    // NO actualizar si el TabView creció mucho (previene que el scrollbar desaparezca)
    if (porcentajeDiferencia > 0.05f && calculatedHeight <= frame.contentAreaSize.y) {
      // TabView se redujo significativamente, usar el tamaño calculado
      currentContentAreaSize.y = calculatedHeight;
    } else {
      // Mantener el tamaño del frame para estabilidad
      currentContentAreaSize.y = frame.contentAreaSize.y;
    }
  } else {
    // Primera vez o frame no tiene tamaño, usar el calculado
    currentContentAreaSize.y = calculatedHeight;
  }
  
  // Manejar input para scroll vertical Y horizontal
  float mouseX = ctx2->input.MouseX();
  float mouseY = ctx2->input.MouseY();
  bool hoverContent =
      (mouseX >= frame.contentAreaPos.x &&
       mouseX <= frame.contentAreaPos.x + currentContentAreaSize.x &&
       mouseY >= frame.contentAreaPos.y &&
       mouseY <= frame.contentAreaPos.y + currentContentAreaSize.y);
  float wheelY = ctx2->input.MouseWheelY();
  float wheelX = ctx2->input.MouseWheelX();
  
  // NOTA: tabScrollOffset ya está declarado arriba (línea 3705), reutilizar esa referencia
  
  if (hoverContent && !scrollConsumedThisFrame) {
    float scrollSpeed = 30.0f;
    // Scroll vertical
    if (std::abs(wheelY) > 0.001f && contentSize.y > currentContentAreaSize.y) {
      tabScrollOffset.y = std::clamp(
          tabScrollOffset.y - wheelY * scrollSpeed, 0.0f,
          std::max(0.0f, contentSize.y - currentContentAreaSize.y));
      scrollConsumedThisFrame = true; // Marcar como consumido
    }
    // Scroll horizontal
    if (std::abs(wheelX) > 0.001f && contentSize.x > currentContentAreaSize.x) {
      tabScrollOffset.x = std::clamp(
          tabScrollOffset.x - wheelX * scrollSpeed, 0.0f,
          std::max(0.0f, contentSize.x - currentContentAreaSize.x));
      scrollConsumedThisFrame = true; // Marcar como consumido
    }
  }
  
  // Calcular si necesitamos scrollbars (vertical Y horizontal) para el CONTENIDO del tab
  // IMPORTANTE: Usar una condición estable basada en el estado guardado para evitar que el scrollbar
  // aparezca y desaparezca entre frames
  bool needsVerticalScrollbar = contentSize.y > currentContentAreaSize.y + 0.1f;
  bool needsHorizontalScrollbar = contentSize.x > currentContentAreaSize.x + 5.0f;
  
  // Si el scrollbar estaba visible en el frame anterior, mantenerlo visible aunque la condición
  // cambie ligeramente (histeresis) para evitar parpadeo
  if (st.contentSize.y > 0.0f && st.contentSize.y > currentContentAreaSize.y + 0.1f) {
    // El scrollbar estaba visible antes, mantenerlo visible si el contenido sigue siendo mayor
    // o si la diferencia es pequeña (menos del 10% de reducción)
    float diferencia = st.contentSize.y - contentSize.y;
    if (diferencia >= 0.0f && diferencia / st.contentSize.y < 0.1f) {
      // El contenido se redujo menos del 10%, mantener el scrollbar visible
      needsVerticalScrollbar = true;
    }
  }
  
  // Histeresis para scrollbar horizontal
  if (st.contentSize.x > 0.0f && st.contentSize.x > currentContentAreaSize.x + 5.0f) {
    float diferencia = st.contentSize.x - contentSize.x;
    if (diferencia >= 0.0f && diferencia / st.contentSize.x < 0.1f) {
      needsHorizontalScrollbar = true;
    }
  }
  
  // IMPORTANTE: NO remover el clipping aquí para evitar FlushBatch() prematuro que causa flickering.
  // El clipping se mantendrá activo y el Panel padre lo removerá al final.
  // El scrollbar se dibujará dentro del clipping del TabView, ajustando su posición para que sea visible.
  
  float scrollbarWidth = 10.0f;
  // NOTA: tabScrollOffset ya está declarado arriba (línea 3705), reutilizar esa referencia
  
  // Dibujar scrollbar VERTICAL si es necesario (para el contenido del tab)
  if (needsVerticalScrollbar) {
    // Ajustar la posición del scrollbar para que esté dentro del área clippeada pero visible
    // El scrollbar estará en el borde derecho del área de contenido
    // Si hay scrollbar horizontal, reducir la altura del scrollbar vertical
    Vec2 barPos(frame.contentAreaPos.x + currentContentAreaSize.x - scrollbarWidth, frame.contentAreaPos.y);
    Vec2 barSize(scrollbarWidth, currentContentAreaSize.y - (needsHorizontalScrollbar ? scrollbarWidth : 0.0f));
    ctx2->renderer.DrawRectFilled(barPos, barSize,
                                  ctx2->style.panel.background, 0.0f);

    float maxScrollY = contentSize.y - currentContentAreaSize.y;
    float ratio = currentContentAreaSize.y / contentSize.y;
    float thumbHeight = std::max(20.0f, barSize.y * ratio);
    float maxThumbTravel = barSize.y - thumbHeight;
    
    // Asegurar que el scroll offset esté dentro de los límites
    tabScrollOffset.y = std::clamp(tabScrollOffset.y, 0.0f, maxScrollY);
    
    float thumbY = maxScrollY > 0.0f
                       ? (tabScrollOffset.y / maxScrollY) * maxThumbTravel
                       : 0.0f;
    Vec2 thumbPos(barPos.x, barPos.y + thumbY);
    Vec2 thumbSize(scrollbarWidth, thumbHeight);
    
    // Detectar hover y drag en el scrollbar
    bool leftDown = ctx2->input.IsMouseDown(SDL_BUTTON_LEFT);
    bool leftPressed = ctx2->input.IsMousePressed(SDL_BUTTON_LEFT);
    
    bool hoverThumb = (mouseX >= thumbPos.x && mouseX <= thumbPos.x + thumbSize.x &&
                       mouseY >= thumbPos.y && mouseY <= thumbPos.y + thumbSize.y);
    bool hoverTrack = (mouseX >= barPos.x && mouseX <= barPos.x + barSize.x &&
                       mouseY >= barPos.y && mouseY <= barPos.y + barSize.y);
    
    // Manejar drag del thumb
    // Generar un ID estable para el scrollbar vertical del TabView
    std::string scrollIdStr = "TabView_VScroll_" + std::to_string(frame.tabViewId);
    uint32_t scrollId = GenerateId(scrollIdStr.c_str());

    if (ctx2->activeWidgetId == scrollId && st.draggingScrollbar) {
      if (!leftDown) {
        // Soltar el botón: detener el drag
        st.draggingScrollbar = false;
        ctx2->activeWidgetId = 0;
      } else {
        // Continuar arrastrando
        float mouseDeltaY = mouseY - st.dragStartMouse.y;
        float scrollDelta = (maxThumbTravel > 0.0f) ? (mouseDeltaY / maxThumbTravel) * maxScrollY : 0.0f;
        tabScrollOffset.y = std::clamp(st.dragStartScroll + scrollDelta, 0.0f, maxScrollY);
      }
    } else if (leftPressed && hoverThumb && ctx2->activeWidgetId == 0) {
      // Iniciar drag cuando se presiona el botón sobre el thumb
      ctx2->activeWidgetId = scrollId;
      st.draggingScrollbar = true;
      st.dragStartMouse = Vec2(mouseX, mouseY);
      st.dragStartScroll = tabScrollOffset.y;
    } else if (leftPressed && hoverTrack && ctx2->activeWidgetId == 0) {
      // Click en el track: saltar a esa posición
      float clickPos = mouseY - barPos.y;
      float scrollRatioClick = clickPos / barSize.y;
      tabScrollOffset.y = std::clamp(scrollRatioClick * maxScrollY, 0.0f, maxScrollY);
    }
    
    Color thumbColor = hoverThumb || st.draggingScrollbar
                           ? ctx2->style.button.background.hover
                           : ctx2->style.button.background.normal;
    ctx2->renderer.DrawRectFilled(thumbPos, thumbSize, thumbColor, 4.0f);
  }
  
  // Dibujar scrollbar HORIZONTAL si es necesario (para el contenido del tab)
  // IMPORTANTE: El scrollbar horizontal debe estar en la parte INFERIOR del área de contenido
  if (needsHorizontalScrollbar) {
    Vec2 hBarPos(frame.contentAreaPos.x, 
                 frame.contentAreaPos.y + currentContentAreaSize.y - scrollbarWidth);
    Vec2 hBarSize(currentContentAreaSize.x - (needsVerticalScrollbar ? scrollbarWidth : 0.0f), 
                  scrollbarWidth);
    ctx2->renderer.DrawRectFilled(hBarPos, hBarSize,
                                  ctx2->style.panel.background, 0.0f);

    float maxScrollX = std::max(0.0f, contentSize.x - currentContentAreaSize.x);
    
    // Asegurar que el scroll offset esté dentro de los límites ANTES de calcular el thumb
    tabScrollOffset.x = std::clamp(tabScrollOffset.x, 0.0f, maxScrollX);
    
    // Calcular el tamaño y posición del thumb
    float ratio = (contentSize.x > 0.0f && maxScrollX > 0.0f) ? currentContentAreaSize.x / contentSize.x : 1.0f;
    float thumbWidth = std::max(20.0f, hBarSize.x * ratio);
    float maxThumbTravel = std::max(0.0f, hBarSize.x - thumbWidth);
    
    float thumbX = (maxScrollX > 0.0f && maxThumbTravel > 0.0f)
                       ? (tabScrollOffset.x / maxScrollX) * maxThumbTravel
                       : 0.0f;
    Vec2 hThumbPos(hBarPos.x + thumbX, hBarPos.y);
    Vec2 hThumbSize(thumbWidth, scrollbarWidth);
    
    // Detectar hover y drag en el scrollbar horizontal
    // IMPORTANTE: Usar las mismas variables de mouse que se usaron arriba para el scrollbar vertical
    // mouseX y mouseY ya están declarados arriba en el scope de EndTabView
    bool leftDown = ctx2->input.IsMouseDown(SDL_BUTTON_LEFT);
    bool leftPressed = ctx2->input.IsMousePressed(SDL_BUTTON_LEFT);
    
    bool hoverHThumb = (mouseX >= hThumbPos.x && mouseX <= hThumbPos.x + hThumbSize.x &&
                        mouseY >= hThumbPos.y && mouseY <= hThumbPos.y + hThumbSize.y);
    bool hoverHTrack = (mouseX >= hBarPos.x && mouseX <= hBarPos.x + hBarSize.x &&
                        mouseY >= hBarPos.y && mouseY <= hBarPos.y + hBarSize.y);
    
    // Manejar drag del thumb horizontal
    // Generar un ID estable para el scrollbar horizontal del TabView
    std::string hScrollIdStr = "TabView_HScroll_" + std::to_string(frame.tabViewId);
    uint32_t hScrollId = GenerateId(hScrollIdStr.c_str());

    // Si ya estamos arrastrando el scrollbar horizontal, continuar el drag
    // IMPORTANTE: Verificar el estado de drag PRIMERO, antes de verificar hover
    // Esto permite que el drag continúe incluso si el mouse se sale temporalmente del thumb
    if (st.draggingHScrollbar && leftDown) {
      // Continuar arrastrando mientras el botón esté presionado
      float mouseDeltaX = mouseX - st.dragStartMouse.x;
      // Calcular el scroll delta basado en el movimiento del mouse
      // Si maxThumbTravel es 0, no hay espacio para mover el thumb, así que no hay scroll
      float scrollDelta = (maxThumbTravel > 0.0f) ? (mouseDeltaX / maxThumbTravel) * maxScrollX : 0.0f;
      tabScrollOffset.x = std::clamp(st.dragStartScroll + scrollDelta, 0.0f, maxScrollX);
      // Asegurar que activeWidgetId esté configurado correctamente
      if (ctx2->activeWidgetId != hScrollId) {
        ctx2->activeWidgetId = hScrollId;
      }
    } else if (!leftDown && st.draggingHScrollbar) {
      // Soltar el botón: detener el drag
      st.draggingHScrollbar = false;
      if (ctx2->activeWidgetId == hScrollId) {
        ctx2->activeWidgetId = 0;
      }
    } else if (leftPressed && hoverHThumb && !st.draggingHScrollbar) {
      // Iniciar drag cuando se presiona el botón sobre el thumb
      // Solo iniciar si no estamos ya arrastrando
      ctx2->activeWidgetId = hScrollId;
      st.draggingHScrollbar = true;
      st.dragStartMouse = Vec2(mouseX, mouseY);
      st.dragStartScroll = tabScrollOffset.x;
    } else if (leftPressed && hoverHTrack && !st.draggingHScrollbar && ctx2->activeWidgetId == 0) {
      // Click en el track: saltar a esa posición
      float clickPos = mouseX - hBarPos.x;
      float scrollRatioClick = (hBarSize.x > 0.0f) ? clickPos / hBarSize.x : 0.0f;
      tabScrollOffset.x = std::clamp(scrollRatioClick * maxScrollX, 0.0f, maxScrollX);
    }
    
    Color hThumbColor = hoverHThumb || st.draggingHScrollbar
                           ? ctx2->style.button.background.hover
                           : ctx2->style.button.background.normal;
    ctx2->renderer.DrawRectFilled(hThumbPos, hThumbSize, hThumbColor, 4.0f);
  } else {
    // Si no hay scrollbar horizontal, resetear el estado
    st.draggingHScrollbar = false;
  }
  
  // Resetear estado de drag si no hay scrollbar vertical
  if (!needsVerticalScrollbar) {
    st.draggingScrollbar = false;
  }
  
  // IMPORTANTE: Restaurar el offset del scroll antes de remover el clipping
  // Esto debe hacerse antes de PopClipRect para mantener el orden correcto
  if (ctx2 && !ctx2->offsetStack.empty()) {
    ctx2->offsetStack.pop_back();
  }
  
  // IMPORTANTE: Restaurar el clipping original.
  // El comentario anterior sugería no hacerlo para evitar flush, pero esto rompe el stack de clipping.
  // Es obligatorio balancear el Push con un Pop.
  if (ctx2) {
      ctx2->renderer.PopClipRect();
  }
}
  
  


void Tooltip(const std::string &text, float delay) {
  UIContext *ctx = GetContext();
  if (!ctx || text.empty())
    return;

  float mouseX = ctx->input.MouseX();
  float mouseY = ctx->input.MouseY();

  // Verificar si el mouse está sobre el widget actual (cursorPos)
  // Por simplicidad, usaremos una heurística: si el mouse está cerca del último
  // item
  bool mouseOverWidget = false;
  if (ctx->lastItemSize.x > 0.0f || ctx->lastItemSize.y > 0.0f) {
    Vec2 itemPos = ctx->lastItemPos;
    Vec2 itemSize = ctx->lastItemSize;
    mouseOverWidget =
        (mouseX >= itemPos.x && mouseX <= itemPos.x + itemSize.x &&
         mouseY >= itemPos.y && mouseY <= itemPos.y + itemSize.y);
  }

  auto &tooltip = ctx->tooltipState;

  if (mouseOverWidget) {
    tooltip.hoverTime += ctx->deltaTime;
    if (tooltip.hoverTime >= tooltip.delay) {
      tooltip.visible = true;
      tooltip.text = text;
      tooltip.delay = delay;

      // Posicionar el tooltip cerca del mouse, pero con un offset
      Vec2 offset(10.0f, 10.0f);
      Vec2 viewport = ctx->renderer.GetViewportSize();

      // Medir el tamaño del texto del tooltip
      const TextStyle &textStyle =
          ctx->style.GetTextStyle(TypographyStyle::Caption);
      Vec2 textSize = ctx->renderer.MeasureText(text, textStyle.fontSize);
      Vec2 tooltipSize(textSize.x + textStyle.fontSize,
                       textSize.y + textStyle.fontSize);

      // Calcular posición (arriba y a la derecha del mouse, asegurándose de no
      // salir de la ventana)
      Vec2 tooltipPos(mouseX + offset.x, mouseY - tooltipSize.y - offset.y);
      if (tooltipPos.x + tooltipSize.x > viewport.x) {
        tooltipPos.x = mouseX - tooltipSize.x - offset.x;
      }
      if (tooltipPos.y < 0.0f) {
        tooltipPos.y = mouseY + offset.y;
      }
      tooltip.position = tooltipPos;
    }
  } else {
    tooltip.hoverTime = 0.0f;
    tooltip.visible = false;
  }

  // Dibujar tooltip si es visible
  if (tooltip.visible) {
    const PanelStyle &panelStyle = ctx->style.panel;
    const TextStyle &textStyle =
        ctx->style.GetTextStyle(TypographyStyle::Caption);
    Vec2 textSize = ctx->renderer.MeasureText(tooltip.text, textStyle.fontSize);
    Vec2 tooltipSize(textSize.x + textStyle.fontSize,
                     textSize.y + textStyle.fontSize);
    Vec2 padding(textStyle.fontSize * 0.5f, textStyle.fontSize * 0.5f);

    // Dibujar tooltip con elevation para que destaque (sin borde)
    ctx->renderer.DrawRectWithElevation(tooltip.position, tooltipSize,
                                        panelStyle.background, 4.0f, 4.0f);

    // Dibujar texto del tooltip
    Vec2 textPos(tooltip.position.x + padding.x,
                 tooltip.position.y + padding.y);
    ctx->renderer.DrawText(textPos, tooltip.text, textStyle.color,
                           textStyle.fontSize);
  }
}

bool BeginContextMenu(const std::string &id) {
  UIContext *ctx = GetContext();
  if (!ctx)
    return false;

  std::string key = "CTXMENU:" + id;
  uint32_t menuId = GenerateId(key.c_str());
  auto &state = ctx->contextMenuStates[menuId];

  // Verificar si se debe abrir el context menu (clic derecho)
  bool rightPressed = ctx->input.IsMousePressed(2); // Botón derecho del mouse
  if (rightPressed && !state.open) {
    Vec2 mousePos(ctx->input.MouseX(), ctx->input.MouseY());

    // Verificar si el click está sobre el widget que invocó el menú
    // Por simplicidad, abriremos si no hay otro menú abierto
    if (ctx->activeContextMenuId == 0) {
      state.position = mousePos;
      state.open = true;
      state.initialized = true;
      ctx->activeContextMenuId = menuId;
    }
  }

  // Si el menú no está abierto, no renderizar nada
  if (!state.open) {
    return false;
  }

  // Guardar el tamaño inicial (se calculará durante EndContextMenu)
  Vec2 menuPos = state.position;
  const PanelStyle &panelStyle = ctx->style.panel;

  // Asegurarse de que el menú no se salga de la ventana
  Vec2 viewport = ctx->renderer.GetViewportSize();
  if (menuPos.x + 200.0f > viewport.x) {
    menuPos.x = viewport.x - 200.0f;
  }
  if (menuPos.y + 100.0f > viewport.y) {
    menuPos.y = viewport.y - 100.0f;
  }
  state.position = menuPos;

  // Iniciar layout para calcular el tamaño
  ctx->cursorPos = menuPos;
  ctx->lastItemPos = menuPos;
  ctx->lastItemSize = Vec2(0.0f, 0.0f);
  BeginVertical(0.0f, std::nullopt, Vec2(0.0f, 0.0f));

  return true;
}

bool ContextMenuItem(const std::string &label, bool enabled) {
  UIContext *ctx = GetContext();
  if (!ctx)
    return false;

  // Solo renderizar si hay un context menu activo
  if (ctx->activeContextMenuId == 0)
    return false;
  auto it = ctx->contextMenuStates.find(ctx->activeContextMenuId);
  if (it == ctx->contextMenuStates.end() || !it->second.open)
    return false;

  const PanelStyle &panelStyle = ctx->style.panel;
  const TextStyle &textStyle = ctx->style.GetTextStyle(TypographyStyle::Body);
  float itemHeight = textStyle.fontSize + panelStyle.padding.y * 2.0f;
  float itemWidth = 200.0f; // Ancho estándar para items del menú

  Vec2 textSize = ctx->renderer.MeasureText(label, textStyle.fontSize);
  itemWidth = std::max(itemWidth, textSize.x + panelStyle.padding.x * 2.0f);

  Vec2 itemSize(itemWidth, itemHeight);
  Vec2 itemPos = ctx->cursorPos;

  float mouseX = ctx->input.MouseX();
  float mouseY = ctx->input.MouseY();
  bool hover =
      enabled && (mouseX >= itemPos.x && mouseX <= itemPos.x + itemSize.x &&
                  mouseY >= itemPos.y && mouseY <= itemPos.y + itemSize.y);
  bool clicked = hover && enabled && ctx->input.IsMousePressed(0);

  // Dibujar fondo del item
  Color itemBg = hover ? panelStyle.headerBackground : panelStyle.background;
  if (!enabled) {
    itemBg = Color(itemBg.r * 0.5f, itemBg.g * 0.5f, itemBg.b * 0.5f, itemBg.a);
  }
  ctx->renderer.DrawRectFilled(itemPos, itemSize, itemBg, 0.0f);

  // Dibujar texto del item
  Vec2 textPos(itemPos.x + panelStyle.padding.x,
               itemPos.y + (itemHeight - textSize.y) * 0.5f);
  Color textColor =
      enabled ? textStyle.color
              : Color(textStyle.color.r * 0.5f, textStyle.color.g * 0.5f,
                      textStyle.color.b * 0.5f, textStyle.color.a);
  ctx->renderer.DrawText(textPos, label, textColor, textStyle.fontSize);

  RegisterOccupiedArea(ctx, itemPos, itemSize);
  AdvanceCursor(ctx, itemSize);

  return clicked;
}

void ContextMenuSeparator() {
  UIContext *ctx = GetContext();
  if (!ctx)
    return;

  // Solo renderizar si hay un context menu activo
  if (ctx->activeContextMenuId == 0)
    return;
  auto it = ctx->contextMenuStates.find(ctx->activeContextMenuId);
  if (it == ctx->contextMenuStates.end() || !it->second.open)
    return;

  const PanelStyle &panelStyle = ctx->style.panel;
  float separatorHeight = 1.0f;
  float separatorPadding = 4.0f;
  float separatorWidth = 200.0f;

  Vec2 separatorSize(separatorWidth, separatorHeight + separatorPadding * 2.0f);
  Vec2 separatorPos = ctx->cursorPos;

  // Dibujar línea separadora
  Vec2 lineStart(separatorPos.x + separatorPadding,
                 separatorPos.y + separatorPadding);
  Vec2 lineEnd(separatorPos.x + separatorWidth - separatorPadding,
               separatorPos.y + separatorPadding);
  ctx->renderer.DrawLine(lineStart, lineEnd, panelStyle.borderColor,
                         separatorHeight);

  RegisterOccupiedArea(ctx, separatorPos, separatorSize);
  AdvanceCursor(ctx, separatorSize);
}

void EndContextMenu() {
  UIContext *ctx = GetContext();
  if (!ctx)
    return;

  // Solo procesar si hay un context menu activo
  if (ctx->activeContextMenuId == 0)
    return;
  auto it = ctx->contextMenuStates.find(ctx->activeContextMenuId);
  if (it == ctx->contextMenuStates.end() || !it->second.open) {
    if (!ctx->layoutStack.empty()) {
      EndVertical(false);
    }
    return;
  }

  auto &state = it->second;

  // Calcular el tamaño total del menú basado en los items
  Vec2 menuSize(200.0f, ctx->cursorPos.y - state.position.y);
  if (menuSize.y < 10.0f) {
    menuSize.y = 10.0f; // Tamaño mínimo
  }
  state.size = menuSize;

  // Cerrar el layout primero
  if (!ctx->layoutStack.empty()) {
    EndVertical(false);
  }

  // Dibujar el contenedor del menú con elevation (fondo)
  // Nota: Los items ya se dibujaron arriba, así que dibujamos el contenedor
  // como fondo Para evitar esto en el futuro, podríamos dibujar el contenedor
  // primero en BeginContextMenu pero por ahora, guardamos el estado de
  // renderizado y lo aplicamos aquí
  ctx->renderer.PushClipRect(state.position, menuSize);

  // Dibujar fondo del menú
  ctx->renderer.DrawRectWithElevation(state.position, menuSize,
                                      ctx->style.panel.background, 4.0f, 8.0f);
  // Borde más visible para el menú contextual - usar color Fluent que no rompa
  // el estilo
  Color borderColor = FluentColors::BorderDark;
  borderColor.a = 0.7f; // Opacidad moderada para mantener el estilo Fluent
  ctx->renderer.DrawRect(state.position, menuSize, borderColor, 4.0f);

  ctx->renderer.PopClipRect();

  // Restaurar cursor
  ctx->cursorPos = state.position + Vec2(0.0f, menuSize.y);
}

bool BeginListView(const std::string &id, const Vec2 &size, int *selectedItem,
                   const std::vector<std::string> &items, const Vec2 &pos) {
  UIContext *ctx = GetContext();
  if (!ctx || items.empty())
    return false;

  std::string key = "LISTVIEW:" + id;
  uint32_t listViewId = GenerateId(key.c_str());
  auto &state = ctx->listViewStates[listViewId];

  if (!state.initialized) {
    state.selectedItem = selectedItem ? *selectedItem : -1;
    state.itemSize = Vec2(size.x > 0.0f ? size.x : 200.0f, 32.0f);
    state.scrollOffset = 0.0f;  // Inicializar scroll offset
    state.initialized = true;
  } else {
    if (selectedItem) {
      state.selectedItem = *selectedItem;
    }
    if (size.x > 0.0f) {
      state.itemSize.x = size.x;
    }
  }

  Vec2 listViewSize = size;
  if (listViewSize.x <= 0.0f) {
    listViewSize.x = 200.0f;
  }
  if (listViewSize.y <= 0.0f) {
    listViewSize.y = items.size() * state.itemSize.y;
  }

  // Resolver posición: usar pos si se proporciona, sino usar cursor
  Vec2 listViewPos;
  if (pos.x != 0.0f || pos.y != 0.0f) {
    listViewPos = pos;
    state.useAbsolutePos = true;
    state.absolutePos = pos;
  } else {
    listViewPos = ctx->cursorPos;
    listViewPos = ResolveAbsolutePosition(ctx, listViewPos, listViewSize);
    state.useAbsolutePos = false;
  }
  // Registrar área ocupada solo si NO se usa posición absoluta
  if (!state.useAbsolutePos) {
    RegisterOccupiedArea(ctx, listViewPos, listViewSize);
  }

  const PanelStyle &panelStyle = ctx->style.panel;
  const TextStyle &textStyle = ctx->style.GetTextStyle(TypographyStyle::Body);

  // Dibujar fondo del ListView con contraste más pronunciado - sin borde
  Color listBg = panelStyle.background;
  bool isDarkTheme = (ctx->style.backgroundColor.r < 0.5f);
  if (isDarkTheme) {
    // Más claro que el fondo para mejor contraste
    listBg = Color(listBg.r * 1.15f, listBg.g * 1.15f, listBg.b * 1.15f, 1.0f);
  } else {
    // Más oscuro que el fondo para mejor contraste
    listBg = Color(listBg.r * 0.92f, listBg.g * 0.92f, listBg.b * 0.92f, 1.0f);
  }
  ctx->renderer.DrawRectFilled(listViewPos, listViewSize, listBg,
                               panelStyle.cornerRadius);

  // Calcular tamaños
  float scrollbarWidth = 10.0f;
  float visibleHeight = listViewSize.y;
  float totalHeight = items.size() * state.itemSize.y;
  bool needsScrollbar = totalHeight > visibleHeight;
  float contentWidth = needsScrollbar ? listViewSize.x - scrollbarWidth : listViewSize.x;

  // Manejar mouse wheel
  float mouseX = ctx->input.MouseX();
  float mouseY = ctx->input.MouseY();
  bool hoverListView = (mouseX >= listViewPos.x && mouseX <= listViewPos.x + listViewSize.x &&
                        mouseY >= listViewPos.y && mouseY <= listViewPos.y + listViewSize.y);
  
  if (hoverListView && needsScrollbar && !scrollConsumedThisFrame) {
    float wheelY = ctx->input.MouseWheelY();
    if (std::abs(wheelY) > 0.001f) {
      float scrollSpeed = 30.0f;
      state.scrollOffset -= wheelY * scrollSpeed;
      state.scrollOffset = std::clamp(state.scrollOffset, 0.0f, 
                                      std::max(0.0f, totalHeight - visibleHeight));
      scrollConsumedThisFrame = true; // Marcar como consumido
    }
  }

  // Manejar interacción del scrollbar
  bool leftDown = ctx->input.IsMouseDown(0);
  bool leftPressed = ctx->input.IsMousePressed(0);

  if (needsScrollbar) {
    Vec2 barPos(listViewPos.x + contentWidth, listViewPos.y);
    Vec2 barSize(scrollbarWidth, visibleHeight);

    float ratio = visibleHeight / totalHeight;
    float thumbHeight = std::max(20.0f, barSize.y * ratio);
    float maxThumbTravel = barSize.y - thumbHeight;
    float maxScrollY = std::max(0.0f, totalHeight - visibleHeight);
    float thumbY = maxScrollY > 0.0f ? (state.scrollOffset / maxScrollY) * maxThumbTravel : 0.0f;

    Vec2 thumbPos(barPos.x, barPos.y + thumbY);
    Vec2 thumbSize(scrollbarWidth, thumbHeight);

    bool hoverThumb = (mouseX >= thumbPos.x && mouseX <= thumbPos.x + thumbSize.x &&
                       mouseY >= thumbPos.y && mouseY <= thumbPos.y + thumbSize.y);
    bool hoverTrack = (mouseX >= barPos.x && mouseX <= barPos.x + barSize.x &&
                       mouseY >= barPos.y && mouseY <= barPos.y + barSize.y);

    if (state.draggingScrollbar) {
      if (!leftDown) {
        state.draggingScrollbar = false;
      } else {
        float mouseDelta = mouseY - state.dragStartMouse.y;
        float scrollDelta = (mouseDelta / maxThumbTravel) * maxScrollY;
        state.scrollOffset = std::clamp(state.dragStartScroll + scrollDelta, 0.0f, maxScrollY);
      }
    } else if (leftPressed && hoverThumb) {
      state.draggingScrollbar = true;
      state.dragStartMouse = Vec2(mouseX, mouseY);
      state.dragStartScroll = state.scrollOffset;
    } else if (leftPressed && hoverTrack) {
      float clickPos = mouseY - barPos.y;
      float scrollRatioClick = clickPos / barSize.y;
      state.scrollOffset = std::clamp(scrollRatioClick * maxScrollY, 0.0f, maxScrollY);
    }
  }

  // Aplicar clipping
  Vec2 clipSize(contentWidth, visibleHeight);
  ctx->renderer.PushClipRect(listViewPos, clipSize);

  // Dibujar items con scroll offset
  int startIndex = 0;
  int endIndex = static_cast<int>(items.size());

  for (int i = startIndex; i < endIndex; ++i) {
    Vec2 itemPos(listViewPos.x, listViewPos.y + i * state.itemSize.y - state.scrollOffset);
    Vec2 itemSize(contentWidth, state.itemSize.y);

    // Verificar si el item es visible
    if (itemPos.y + itemSize.y < listViewPos.y ||
        itemPos.y > listViewPos.y + visibleHeight) {
      continue;
    }

    bool hover = (mouseX >= itemPos.x && mouseX <= itemPos.x + itemSize.x &&
                  mouseY >= itemPos.y && mouseY <= itemPos.y + itemSize.y);
    bool isSelected = (i == state.selectedItem);

    // Dibujar fondo del item
    Color itemBg = panelStyle.background;
    if (isSelected) {
      // Fondo gris oscuro sutil para selección (igual que TreeNode)
      itemBg = Color(0.25f, 0.25f, 0.25f, 1.0f);
    } else if (hover) {
      itemBg = panelStyle.headerBackground;
    }
    // Dibujar fondo con esquinas redondeadas si hay hover o selección (igual que TreeNode)
    if (isSelected || hover) {
        ctx->renderer.DrawRectFilled(itemPos, itemSize, itemBg, 4.0f);
    }

    // Dibujar indicador de selección
    if (isSelected) {
      float indicatorHeight = itemSize.y * 0.6f;
      float indicatorWidth = 3.0f;
      Vec2 indicatorPos(itemPos.x + 2.0f, itemPos.y + (itemSize.y - indicatorHeight) * 0.5f);
      Vec2 indicatorSize(indicatorWidth, indicatorHeight);
      
      ctx->renderer.DrawRectFilled(indicatorPos, indicatorSize,
                                   FluentColors::AccentTeal, indicatorWidth * 0.5f);
    }

    // Dibujar texto del item
    // Alineación similar a TreeNode: padding izquierdo + 8px extra
    // Ajuste vertical similar
    float verticalOffset = 1.0f;
    Vec2 textPos(itemPos.x + panelStyle.padding.x + 8.0f,
                 itemPos.y + (state.itemSize.y - textStyle.fontSize) * 0.5f - verticalOffset);
    ctx->renderer.DrawText(textPos, items[i], textStyle.color,
                           textStyle.fontSize);

    // Manejar click en el item
    if (hover && leftPressed) {
      state.selectedItem = i;
      if (selectedItem) {
        *selectedItem = i;
      }
    }
  }

  // Remover clipping
  ctx->renderer.PopClipRect();

  // Dibujar scrollbar
  if (needsScrollbar) {
    Vec2 barPos(listViewPos.x + contentWidth, listViewPos.y);
    Vec2 barSize(scrollbarWidth, visibleHeight);

    float ratio = visibleHeight / totalHeight;
    float thumbHeight = std::max(20.0f, barSize.y * ratio);
    float maxThumbTravel = barSize.y - thumbHeight;
    float maxScrollY = std::max(0.0f, totalHeight - visibleHeight);
    float thumbY = maxScrollY > 0.0f ? (state.scrollOffset / maxScrollY) * maxThumbTravel : 0.0f;

    Vec2 thumbPos(barPos.x, barPos.y + thumbY);
    Vec2 thumbSize(scrollbarWidth, thumbHeight);

    bool hoverThumb = (mouseX >= thumbPos.x && mouseX <= thumbPos.x + thumbSize.x &&
                       mouseY >= thumbPos.y && mouseY <= thumbPos.y + thumbSize.y);
    bool hoverTrack = (mouseX >= barPos.x && mouseX <= barPos.x + barSize.x &&
                       mouseY >= barPos.y && mouseY <= barPos.y + barSize.y);

    // Dibujar track
    Color scrollbarBg = hoverTrack ? panelStyle.headerBackground : panelStyle.background;
    ctx->renderer.DrawRectFilled(barPos, barSize, scrollbarBg, 0.0f);

    // Dibujar thumb
    Color thumbColor = hoverThumb || state.draggingScrollbar
                           ? ctx->style.button.background.hover
                           : ctx->style.button.background.normal;
    ctx->renderer.DrawRectFilled(thumbPos, thumbSize, thumbColor, 4.0f);
  }

  // Actualizar selectedItem externo
  if (selectedItem) {
    *selectedItem = state.selectedItem;
  }

  // Avanzar cursor solo si NO se usa posición absoluta
  if (!state.useAbsolutePos) {
    ctx->cursorPos = listViewPos + Vec2(0.0f, listViewSize.y);
    ctx->lastItemPos = listViewPos;
    AdvanceCursor(ctx, listViewSize);
  } else {
    // Si se usa posición absoluta, no modificar el cursor
    ctx->lastItemPos = listViewPos;
  }

  return true;
}

void EndListView() {
  // EndListView no necesita hacer nada especial por ahora
  // El contenido ya fue renderizado en BeginListView
}


bool BeginTreeView(const std::string &id, const Vec2 &size, const Vec2 &pos) {
  UIContext *ctx = GetContext();
  if (!ctx)
    return false;

  std::string key = "TREEVIEW:" + id;
  uint32_t treeViewId = GenerateId(key.c_str());
  auto &state = ctx->treeViewStates[treeViewId];
  g_currentTreeViewId = treeViewId;

  if (!state.initialized) {
    state.itemSize = Vec2(size.x > 0.0f ? size.x : 200.0f, 24.0f);
    state.indentSize = 20.0f;
    state.expandButtonSize = 14.0f;
    state.scrollOffset = 0.0f; // Inicializar scroll offset
    state.initialized = true;
  } else {
    if (size.x > 0.0f) {
      state.itemSize.x = size.x;
    }
  }

  Vec2 treeViewSize = size;
  if (treeViewSize.x <= 0.0f) {
    treeViewSize.x = 200.0f;
  }
  if (treeViewSize.y <= 0.0f) {
    treeViewSize.y = 300.0f;
  }

  // Resolver posición: usar pos si se proporciona, sino usar cursor
  Vec2 treeViewPos;
  if (pos.x != 0.0f || pos.y != 0.0f) {
    treeViewPos = pos;
    state.useAbsolutePos = true;
    state.absolutePos = pos;
  } else {
    treeViewPos = ctx->cursorPos;
    treeViewPos = ResolveAbsolutePosition(ctx, treeViewPos, treeViewSize);
    state.useAbsolutePos = false;
  }
  // Registrar área ocupada solo si NO se usa posición absoluta
  if (!state.useAbsolutePos) {
    RegisterOccupiedArea(ctx, treeViewPos, treeViewSize);
  }

  const PanelStyle &panelStyle = ctx->style.panel;

  // Dibujar fondo del TreeView con contraste más pronunciado - sin borde
  Color treeBg = panelStyle.background;
  bool isDarkTheme = (ctx->style.backgroundColor.r < 0.5f);
  if (isDarkTheme) {
    // Fondo oscuro charcoal gray más profundo (#262626 ~ 0.15f) - igual que TextInput
    treeBg = Color(0.15f, 0.15f, 0.15f, 1.0f);
  } else {
    // Más oscuro que el fondo para mejor contraste
    treeBg = Color(treeBg.r * 0.92f, treeBg.g * 0.92f, treeBg.b * 0.92f, 1.0f);
  }
  ctx->renderer.DrawRectFilled(treeViewPos, treeViewSize, treeBg,
                               panelStyle.cornerRadius);

  // Guardar posición y tamaño del TreeView para calcular altura real después
  state.viewPos = treeViewPos;
  state.viewSize = treeViewSize;

  // Calcular si necesitamos scrollbar (lo calcularemos en EndTreeView, pero preparamos aquí)
  float scrollbarWidth = 10.0f;
  float visibleHeight = treeViewSize.y - panelStyle.padding.y * 2.0f;
  float contentWidth = treeViewSize.x - panelStyle.padding.x * 2.0f;

  // IMPORTANTE: Asegurar que el scroll offset sea válido antes de usarlo
  // Esto previene problemas cuando el contenido cambia de tamaño
  // El ajuste final se hará en EndTreeView cuando sepamos la altura real del contenido
  // Por ahora, solo asegurar que no sea negativo
  if (state.scrollOffset < 0.0f) {
    state.scrollOffset = 0.0f;
  }

  // Aplicar clipping (ajustar ancho si hay scrollbar, pero lo calcularemos después)
  ctx->renderer.PushClipRect(treeViewPos, treeViewSize);

  // Configurar cursor para el contenido del TreeView con scroll offset aplicado
  Vec2 contentStartPos = treeViewPos + panelStyle.padding;
  ctx->cursorPos = Vec2(contentStartPos.x, contentStartPos.y - state.scrollOffset);
  ctx->lastItemPos = ctx->cursorPos;
  g_treeViewDepth = 0;

  // Iniciar layout vertical para apilar los nodos correctamente
  Vec2 contentSize(contentWidth, 0.0f); // Altura 0 = auto
  BeginVertical(ctx->style.spacing, Vec2(contentSize.x, 0.0f), Vec2(0.0f, 0.0f));

  return true;
}

void EndTreeView() {
  UIContext *ctx = GetContext();
  if (!ctx)
    return;

  // Guardar la altura real del contenido ANTES de cerrar el layout
  float realContentHeight = 0.0f;
  float totalContentHeight = 0.0f;
  Vec2 treeViewPos(0.0f, 0.0f);
  Vec2 treeViewSize(0.0f, 0.0f);
  const PanelStyle &panelStyle = ctx->style.panel;
  float scrollbarWidth = 10.0f;
  
  if (g_currentTreeViewId != 0) {
    auto it = ctx->treeViewStates.find(g_currentTreeViewId);
    if (it != ctx->treeViewStates.end()) {
      auto &state = it->second;
      
      // Usar la posición y tamaño guardados en BeginTreeView
      treeViewPos = state.viewPos;
      treeViewSize = state.viewSize;
      
      // Calcular la altura real del contenido basándose en el layout
      // IMPORTANTE: Calcular ANTES de cerrar el layout para obtener el contentSize correcto
      float contentStartY = treeViewPos.y + panelStyle.padding.y;
      
      if (!ctx->layoutStack.empty()) {
        const auto &layout = ctx->layoutStack.back();
        // La altura total del contenido es la altura del layout (sin padding)
        // Incluir el spacing entre items si hay
        float spacingTotal = (layout.itemCount > 0 && layout.spacing > 0.0f)
                                ? layout.spacing * static_cast<float>(layout.itemCount - 1)
                                : 0.0f;
        totalContentHeight = layout.contentSize.y + spacingTotal;
        // La altura real incluye el padding
        realContentHeight = totalContentHeight + panelStyle.padding.y * 2.0f;
        
        // También calcular desde lastItemPos como verificación
        // lastItemPos tiene el scroll aplicado, así que necesitamos la posición absoluta
        float contentEndYFromLastItem = (ctx->lastItemPos.y + ctx->lastItemSize.y) + state.scrollOffset;
        float totalFromLastItem = contentEndYFromLastItem - contentStartY;
        
        // Usar el mayor entre el cálculo del layout y el cálculo desde lastItemPos
        totalContentHeight = std::max(totalContentHeight, totalFromLastItem);
      } else {
        // Si no hay layout, calcular desde lastItemPos
        // lastItemPos tiene el scroll aplicado, así que necesitamos la posición absoluta
        float contentEndY = (ctx->lastItemPos.y + ctx->lastItemSize.y) + state.scrollOffset;
        totalContentHeight = contentEndY - contentStartY;
        realContentHeight = totalContentHeight + panelStyle.padding.y;
      }
      
      // Asegurar que totalContentHeight sea al menos un valor razonable
      if (totalContentHeight <= 0.0f) {
        // Si no hay contenido calculado, usar una estimación basada en lastItemPos
        float contentEndY = ctx->lastItemPos.y + ctx->lastItemSize.y + state.scrollOffset;
        totalContentHeight = std::max(0.0f, contentEndY - contentStartY);
      }
      
      // Calcular altura visible (sin padding)
      float visibleHeight = treeViewSize.y - panelStyle.padding.y * 2.0f;
      bool needsScrollbar = totalContentHeight > visibleHeight + 0.1f; // Agregar pequeño margen para evitar problemas de precisión
      
      // IMPORTANTE: Ajustar el scroll offset si el contenido se redujo
      // Si ya no se necesita scrollbar, resetear el offset a 0
      if (!needsScrollbar) {
        state.scrollOffset = 0.0f;
      } else {
        // Si se necesita scrollbar, asegurar que el offset esté dentro de los límites válidos
        float maxScrollY = std::max(0.0f, totalContentHeight - visibleHeight);
        if (state.scrollOffset > maxScrollY) {
          // Si el offset es mayor que el máximo permitido (contenido se redujo), ajustarlo
          state.scrollOffset = maxScrollY;
        }
        // Asegurar que el offset no sea negativo
        state.scrollOffset = std::max(0.0f, state.scrollOffset);
      }
      
      // Manejar mouse wheel para scroll
      float mouseX = ctx->input.MouseX();
      float mouseY = ctx->input.MouseY();
      bool hoverTreeView = (mouseX >= treeViewPos.x && mouseX <= treeViewPos.x + treeViewSize.x &&
                            mouseY >= treeViewPos.y && mouseY <= treeViewPos.y + treeViewSize.y);
      
      if (hoverTreeView && needsScrollbar && !scrollConsumedThisFrame) {
        float wheelY = ctx->input.MouseWheelY();
        if (std::abs(wheelY) > 0.001f) {
          float scrollSpeed = 30.0f;
          float maxScrollY = std::max(0.0f, totalContentHeight - visibleHeight);
          state.scrollOffset -= wheelY * scrollSpeed;
          state.scrollOffset = std::clamp(state.scrollOffset, 0.0f, maxScrollY);
          scrollConsumedThisFrame = true;
        }
      }
      
      // Manejar interacción del scrollbar (solo para input, el dibujo se hace después de PopClipRect)
      bool leftDown = ctx->input.IsMouseDown(0);
      bool leftPressed = ctx->input.IsMousePressed(0);
      
      if (needsScrollbar) {
        float contentWidth = treeViewSize.x - panelStyle.padding.x * 2.0f - scrollbarWidth;
        Vec2 barPos(treeViewPos.x + contentWidth + panelStyle.padding.x, treeViewPos.y + panelStyle.padding.y);
        Vec2 barSize(scrollbarWidth, visibleHeight);
        
        float maxScrollY = std::max(0.0f, totalContentHeight - visibleHeight);
        float ratio = totalContentHeight > 0.0f ? visibleHeight / totalContentHeight : 1.0f;
        float thumbHeight = std::max(20.0f, barSize.y * ratio);
        float maxThumbTravel = barSize.y - thumbHeight;
        float thumbY = maxScrollY > 0.0f ? (state.scrollOffset / maxScrollY) * maxThumbTravel : 0.0f;
        
        Vec2 thumbPos(barPos.x, barPos.y + thumbY);
        Vec2 thumbSize(scrollbarWidth, thumbHeight);
        
        bool hoverThumb = (mouseX >= thumbPos.x && mouseX <= thumbPos.x + thumbSize.x &&
                           mouseY >= thumbPos.y && mouseY <= thumbPos.y + thumbSize.y);
        bool hoverTrack = (mouseX >= barPos.x && mouseX <= barPos.x + barSize.x &&
                           mouseY >= barPos.y && mouseY <= barPos.y + barSize.y);
        
        // Manejar drag del scrollbar
        if (state.draggingScrollbar) {
          if (!leftDown) {
            state.draggingScrollbar = false;
          } else {
            float mouseDelta = mouseY - state.dragStartMouse.y;
            float scrollDelta = (mouseDelta / maxThumbTravel) * maxScrollY;
            state.scrollOffset = std::clamp(state.dragStartScroll + scrollDelta, 0.0f, maxScrollY);
          }
        } else if (leftPressed && hoverThumb) {
          state.draggingScrollbar = true;
          state.dragStartMouse = Vec2(mouseX, mouseY);
          state.dragStartScroll = state.scrollOffset;
        } else if (leftPressed && hoverTrack) {
          float clickPos = mouseY - barPos.y;
          float scrollRatioClick = clickPos / barSize.y;
          state.scrollOffset = std::clamp(scrollRatioClick * maxScrollY, 0.0f, maxScrollY);
        }
      }
    }
  }

  // Cerrar el layout vertical
  if (!ctx->layoutStack.empty()) {
    EndVertical(false); // No avanzar el cursor del padre aquí
  }

  // Remover clipping
  ctx->renderer.PopClipRect();

  // Dibujar scrollbar si es necesario (después de remover clipping)
  if (g_currentTreeViewId != 0) {
    auto it = ctx->treeViewStates.find(g_currentTreeViewId);
    if (it != ctx->treeViewStates.end()) {
      auto &state = it->second;
      
      // Recalcular totalContentHeight después de cerrar el layout para asegurar que sea correcto
      float recalculatedTotalHeight = totalContentHeight;
      if (!ctx->layoutStack.empty()) {
        // El layout ya fue cerrado, así que necesitamos usar otra forma
        // Usar lastItemPos que debería tener la posición del último elemento
        float contentStartY = treeViewPos.y + panelStyle.padding.y;
        float contentEndY = ctx->lastItemPos.y + ctx->lastItemSize.y + state.scrollOffset;
        recalculatedTotalHeight = std::max(totalContentHeight, contentEndY - contentStartY);
      }
      
      float visibleHeight = treeViewSize.y - panelStyle.padding.y * 2.0f;
      bool needsScrollbar = recalculatedTotalHeight > visibleHeight + 0.1f;
      
      if (needsScrollbar) {
        float contentWidth = treeViewSize.x - panelStyle.padding.x * 2.0f - scrollbarWidth;
        Vec2 barPos(treeViewPos.x + contentWidth + panelStyle.padding.x, treeViewPos.y + panelStyle.padding.y);
        Vec2 barSize(scrollbarWidth, visibleHeight);
        
        float maxScrollY = std::max(0.0f, recalculatedTotalHeight - visibleHeight);
        float ratio = recalculatedTotalHeight > 0.0f ? visibleHeight / recalculatedTotalHeight : 1.0f;
        float thumbHeight = std::max(20.0f, barSize.y * ratio);
        float maxThumbTravel = barSize.y - thumbHeight;
        float thumbY = maxScrollY > 0.0f ? (state.scrollOffset / maxScrollY) * maxThumbTravel : 0.0f;
        
        Vec2 thumbPos(barPos.x, barPos.y + thumbY);
        Vec2 thumbSize(scrollbarWidth, thumbHeight);
        
        float mouseX = ctx->input.MouseX();
        float mouseY = ctx->input.MouseY();
        bool hoverThumb = (mouseX >= thumbPos.x && mouseX <= thumbPos.x + thumbSize.x &&
                           mouseY >= thumbPos.y && mouseY <= thumbPos.y + thumbSize.y);
        bool hoverTrack = (mouseX >= barPos.x && mouseX <= barPos.x + barSize.x &&
                           mouseY >= barPos.y && mouseY <= barPos.y + barSize.y);
        
        // Dibujar track
        Color scrollbarBg = hoverTrack ? panelStyle.headerBackground : panelStyle.background;
        ctx->renderer.DrawRectFilled(barPos, barSize, scrollbarBg, 0.0f);
        
        // Dibujar thumb
        Color thumbColor = hoverThumb || state.draggingScrollbar
                               ? ctx->style.button.background.hover
                               : ctx->style.button.background.normal;
        ctx->renderer.DrawRectFilled(thumbPos, thumbSize, thumbColor, 4.0f);
      }
    }
  }

  // Actualizar lastItemPos y lastItemSize con los valores reales
  if (g_currentTreeViewId != 0) {
    auto it = ctx->treeViewStates.find(g_currentTreeViewId);
    if (it != ctx->treeViewStates.end()) {
      auto &state = it->second;

      // Actualizar lastItemPos y lastItemSize con la posición y tamaño reales
      ctx->lastItemPos = treeViewPos;
      ctx->lastItemSize = treeViewSize;

      // Avanzar cursor solo si NO se usa posición absoluta
      if (!state.useAbsolutePos) {
        ctx->cursorPos = treeViewPos + Vec2(0.0f, treeViewSize.y);
        AdvanceCursor(ctx, treeViewSize);
      } else {
        // Si se usa posición absoluta, no modificar el cursor
        // El cursor ya está en la posición correcta antes del TreeView
      }
    }
  }

  g_currentTreeViewId = 0;
  g_treeViewDepth = 0;
}

bool TreeNode(const std::string &id, const std::string &label, bool *isOpen,
              bool *isSelected) {
  UIContext *ctx = GetContext();
  if (!ctx || g_currentTreeViewId == 0)
    return false;

  auto it = ctx->treeViewStates.find(g_currentTreeViewId);
  if (it == ctx->treeViewStates.end())
    return false;

  const auto &state = it->second;
  const PanelStyle &panelStyle = ctx->style.panel;
  const TextStyle &textStyle = ctx->style.GetTextStyle(TypographyStyle::Body);

  // Usar el estado guardado o el valor por defecto
  std::string nodeKey =
      "TREENODE:" + std::to_string(g_currentTreeViewId) + ":" + id;
  bool nodeIsOpen = isOpen ? *isOpen : ctx->treeNodeStates[nodeKey];
  
  // Lógica de selección única:
  // Si el item tiene puntero isSelected, respetarlo, pero también chequear el global state
  // del TreeView para garantizar exclusividad visual.
  bool nodeIsSelected = (state.selectedNodeKey == nodeKey);
  
  // Sincronizar estado externo si se provee
  if (isSelected && *isSelected) {
       // Si el usuario fuerza true externamente, actualizar nuestro estado interno
       // (esto podría causar conflicto si hay múltiples true, el último gana)
       // Idealmente la UI maneja la selección.
       if (state.selectedNodeKey != nodeKey) {
           // Si externamente está true pero internamente no, lo marcamos?
           // Mejor dejar que el click mande. Solo visualizamos si coincide.
       }
  }

  // Calcular posición (la indentación ahora se maneja vía layout en TreeNodePush)
  Vec2 itemPos = ctx->cursorPos;
  // Ya no sumamos indentación manual aquí porque el cursor ya viene indentado por el layout
  // float indentX = g_treeViewDepth * state.indentSize; 
  // itemPos.x += indentX;

  // El ancho se ajusta automáticamente al ancho disponible en el layout
  // (que ya es más angosto por la indentación del layout)
  float adjustedWidth = std::max(0.0f, state.itemSize.x);
  
  // Si estamos dentro de un layout vertical con width definido, usar ese width
  if (!ctx->layoutStack.empty()) {
      float stackWidth = ctx->layoutStack.back().contentSize.x;
      // Usar el ancho del stack solo si es razonable, sino fallback a state.itemSize.x
      if (stackWidth > 10.0f) {
           adjustedWidth = stackWidth;
      }
  }
  
  // Asegurar un ancho mínimo para que sea clickeable
  if (adjustedWidth < 50.0f) adjustedWidth = std::max(50.0f, state.itemSize.x);
  
  Vec2 itemSize(adjustedWidth, state.itemSize.y);

  float mouseX = ctx->input.MouseX();
  float mouseY = ctx->input.MouseY();
  bool hover = (mouseX >= itemPos.x && mouseX <= itemPos.x + itemSize.x &&
                mouseY >= itemPos.y && mouseY <= itemPos.y + itemSize.y);
  bool clicked = hover && ctx->input.IsMousePressed(0);

  // Determinar si tiene hijos (si isOpen != nullptr)
  bool hasChildren = (isOpen != nullptr);

  // --- Dibujar Fondo ---
  Color itemBg = Color(0.0f, 0.0f, 0.0f, 0.0f);
  if (nodeIsSelected) {
    // Fondo gris oscuro sutil para selección
    itemBg = Color(0.25f, 0.25f, 0.25f, 1.0f);
  } else if (hover) {
    // Fondo de hover estándar
    itemBg = panelStyle.headerBackground;
  }

  // Dibujar fondo con esquinas redondeadas si hay hover o selección
  if (nodeIsSelected || hover) {
    ctx->renderer.DrawRectFilled(itemPos, itemSize, itemBg, 4.0f);
  }

  // --- Dibujar Indicador de Selección (Pill) ---
  if (nodeIsSelected) {
    float indicatorHeight = itemSize.y * 0.6f;
    float indicatorWidth = 3.0f;
    // Posicionar a la izquierda del item (dentro del área indentada)
    Vec2 indicatorPos(itemPos.x + 2.0f, itemPos.y + (itemSize.y - indicatorHeight) * 0.5f);
    Vec2 indicatorSize(indicatorWidth, indicatorHeight);
    
    // Usar AccentTeal para coincidir con la captura
    ctx->renderer.DrawRectFilled(indicatorPos, indicatorSize,
                                 FluentColors::AccentTeal, indicatorWidth * 0.5f);
  }

  // --- Dibujar Botón Expand/Collapse (Chevron) ---
  // CHEVRON REMOVIDO: El diseño ahora usa click en toda la fila para expandir/colapsar
  // No se dibuja nada aquí
  
  // --- Dibujar Texto ---
  // --- Dibujar Texto ---
  // Usar posición del cursor directamente ya que la indentación ahora es manejada por el layout
  // Ajustar posición del texto para centrado vertical correcto
  float textX = itemPos.x; 
  // Calcular centrado vertical preciso:
  // Centro de la fila = itemPos.y + itemSize.y * 0.5f
  // Centro del texto = textPos.y + fontSize * 0.5f
  // Entonces: textPos.y = itemPos.y + (itemSize.y - fontSize) * 0.5f
  // Entonces: textPos.y = itemPos.y + (itemSize.y - fontSize) * 0.5f
  // Ajuste fino para centrado vertical: subir 1px (restar a Y) parece ser lo que pide el usuario
  float verticalOffset = 1.0f; 
  Vec2 textPos(textX + 8.0f, itemPos.y + (itemSize.y - textStyle.fontSize) * 0.5f - verticalOffset);
  
  ctx->renderer.DrawText(textPos, label, textStyle.color, textStyle.fontSize);

  // Manejar click en el item (selección y expansión)
  if (clicked) {
    // Actualizar selección global del TreeView
    ctx->treeViewStates[g_currentTreeViewId].selectedNodeKey = nodeKey;
    nodeIsSelected = true;
    
    // Actualizar puntero externo si existe
    if (isSelected) {
      *isSelected = true;
    }
    
    // Si tiene hijos, el click también alterna la expansión
    if (hasChildren) {
      nodeIsOpen = !nodeIsOpen;
      ctx->treeNodeStates[nodeKey] = nodeIsOpen;
      if (isOpen) *isOpen = nodeIsOpen;
    }
  }


  RegisterOccupiedArea(ctx, itemPos, itemSize);
  AdvanceCursor(ctx, itemSize);

  // Actualizar el estado guardado
  if (isOpen) {
    ctx->treeNodeStates[nodeKey] = *isOpen;
  }

  return nodeIsOpen && hasChildren;
}

void TreeNodePush() { 
    g_treeViewDepth++; 
    
    // Indentar el layout actual para que los hijos (labels, botones, etc.) aparezcan indentados
    UIContext *ctx = GetContext();
    if (ctx && !ctx->layoutStack.empty()) {
        LayoutStack &stack = ctx->layoutStack.back();
        float indentSize = 20.0f; // Mismo valor que state.indentSize por defecto
        
        // Mover el cursor actual a la derecha
        stack.cursor.x += indentSize;
        ctx->cursorPos.x += indentSize;
        
        // Reducir el ancho disponible en la cantidad indentada
        stack.contentSize.x -= indentSize;
        
        // Ajustar el inicio del contenido para este nivel
        stack.contentStart.x += indentSize;
    }
}

void TreeNodePop() {
  if (g_treeViewDepth > 0) {
    g_treeViewDepth--;
    
    // Restaurar indentación del layout
    UIContext *ctx = GetContext();
    if (ctx && !ctx->layoutStack.empty()) {
        LayoutStack &stack = ctx->layoutStack.back();
        float indentSize = 20.0f;
        
        // Mover el cursor a la izquierda
        stack.cursor.x -= indentSize;
        ctx->cursorPos.x -= indentSize;
        
        // Restaurar ancho disponible
        stack.contentSize.x += indentSize;
        
        // Restaurar inicio del contenido
        stack.contentStart.x -= indentSize;
    }
  }
}

bool BeginMenuBar() {
  UIContext *ctx = GetContext();
  if (!ctx)
    return false;

  Vec2 viewport = ctx->renderer.GetViewportSize();

  auto &menuBar = ctx->menuBarState;
  menuBar.position = Vec2(0.0f, 0.0f);
  menuBar.size = Vec2(viewport.x, MENUBAR_HEIGHT);
  menuBar.initialized = true;

  const PanelStyle &panelStyle = ctx->style.panel;

  // Dibujar fondo del MenuBar - con contraste sin borde
  Color menuBarBg = panelStyle.headerBackground;
  bool isDarkTheme = (ctx->style.backgroundColor.r < 0.5f);
  if (isDarkTheme) {
    // Más claro que el fondo para mejor contraste
    menuBarBg = Color(menuBarBg.r * 1.15f, menuBarBg.g * 1.15f, menuBarBg.b * 1.15f, 1.0f);
  } else {
    // Más oscuro que el fondo para mejor contraste
    menuBarBg = Color(menuBarBg.r * 0.92f, menuBarBg.g * 0.92f, menuBarBg.b * 0.92f, 1.0f);
  }
  ctx->renderer.DrawRectFilled(menuBar.position, menuBar.size,
                               menuBarBg, 0.0f);

  // Configurar cursor para el MenuBar (sin padding)
  ctx->cursorPos = menuBar.position;
  ctx->lastItemPos = ctx->cursorPos;

  // Iniciar layout horizontal para los menús (sin padding)
  BeginHorizontal(0.0f, menuBar.size, Vec2(0.0f, 0.0f));

  return true;
}

void EndMenuBar() {
  UIContext *ctx = GetContext();
  if (!ctx)
    return;

  // Cerrar layout horizontal
  EndHorizontal(false);

  auto &menuBar = ctx->menuBarState;

  // Avanzar cursor después del MenuBar
  ctx->cursorPos = Vec2(0.0f, menuBar.size.y);
  ctx->lastItemPos = ctx->cursorPos;
  ctx->lastItemSize = menuBar.size;
}

bool BeginMenu(const std::string &label, bool enabled) {
  UIContext *ctx = GetContext();
  if (!ctx)
    return false;

  std::string key = "MENU:" + label;
  uint32_t menuId = GenerateId(key.c_str());
  auto &state = ctx->menuStates[menuId];

  const PanelStyle &panelStyle = ctx->style.panel;
  const TextStyle &textStyle = ctx->style.GetTextStyle(TypographyStyle::Body);
  Vec2 textSize = MeasureTextCached(ctx, label, textStyle.fontSize);

  // Use the same constant height as MenuBar
  float menuHeight = MENUBAR_HEIGHT;
  float menuPadding = 12.0f;
  Vec2 menuSize(textSize.x + menuPadding * 2.0f, menuHeight);
  Vec2 menuPos = ctx->cursorPos;

  float mouseX = ctx->input.MouseX();
  float mouseY = ctx->input.MouseY();
  bool hover =
      enabled && (mouseX >= menuPos.x && mouseX <= menuPos.x + menuSize.x &&
                  mouseY >= menuPos.y && mouseY <= menuPos.y + menuSize.y);
  bool clicked = hover && enabled && ctx->input.IsMousePressed(0);

  // Toggle del menú si se hace click
  if (clicked) {
    // Cerrar otros menús abiertos
    if (state.open) {
      state.open = false;
      ctx->activeMenuId = 0;
    } else {
      // Cerrar todos los otros menús
      for (auto &[id, otherState] : ctx->menuStates) {
        if (id != menuId) {
          otherState.open = false;
        }
      }
      state.open = true;
      ctx->activeMenuId = menuId;
    }
  } else if (state.open) {
    // If menu is already open, ensure activeMenuId is set
    ctx->activeMenuId = menuId;
  }

  // Dibujar fondo del menú
  Color menuBg = panelStyle.headerBackground;
  if (hover && enabled) {
    menuBg = panelStyle.background;
  }
  if (!enabled) {
    menuBg = Color(menuBg.r * 0.5f, menuBg.g * 0.5f, menuBg.b * 0.5f, menuBg.a);
  }
  
  // Force exact height before drawing to prevent any size modifications
  menuSize.y = MENUBAR_HEIGHT;
  ctx->renderer.DrawRectFilled(menuPos, menuSize, menuBg, 0.0f);

  // Dibujar texto del menú
  Vec2 textPos(menuPos.x + menuPadding,
               menuPos.y + (menuHeight - textSize.y) * 0.5f);
  Color textColor =
      enabled ? textStyle.color
              : Color(textStyle.color.r * 0.5f, textStyle.color.g * 0.5f,
                      textStyle.color.b * 0.5f, textStyle.color.a);
  ctx->renderer.DrawText(textPos, label, textColor, textStyle.fontSize);

  RegisterOccupiedArea(ctx, menuPos, menuSize);
  AdvanceCursor(ctx, menuSize);

  // Guardar estado
  state.id = label;
  state.position = menuPos;
  state.size = menuSize;
  state.hover = hover;
  state.initialized = true;

  // Si el menú está abierto, preparar el layout vertical para los items
  if (state.open) {
    // Push menu ID to stack so EndMenu knows which menu to close
    ctx->menuIdStack.push_back(menuId);
    ctx->menuItemStartIndexStack.push_back(ctx->currentMenuItems.size());
    
    Vec2 dropdownPos(state.position.x, state.position.y + state.size.y);
    float dropdownWidth = 200.0f;
    
    // Asegurar que el dropdown no se salga de la ventana
    Vec2 viewport = ctx->renderer.GetViewportSize();
    if (dropdownPos.x + dropdownWidth > viewport.x) {
      dropdownPos.x = viewport.x - dropdownWidth;
    }

    // Configurar cursor para los items del menú (dentro del dropdown)
    ctx->cursorPos = dropdownPos;

    // Iniciar layout vertical para los items del menú
    BeginVertical(0.0f, Vec2(dropdownWidth, 0.0f), Vec2(0.0f, 0.0f));
  }

  return state.open;
}

void EndMenu() {
  UIContext *ctx = GetContext();
  if (!ctx)
    return;

  // Pop the menu ID from the stack to know which menu we're closing
  if (ctx->menuIdStack.empty())
    return;
    
  uint32_t currentMenuId = ctx->menuIdStack.back();
  ctx->menuIdStack.pop_back();

  // Find the menu state for this specific menu
  auto it = ctx->menuStates.find(currentMenuId);
  if (it != ctx->menuStates.end() && it->second.open) {
    auto &state = it->second;

    // Cerrar el layout vertical primero para obtener el tamaño real del contenido
    if (!ctx->layoutStack.empty()) {
      EndVertical(false);
      
      // Explicitly restore cursor to parent horizontal layout position
      // This ensures we don't lose track of where the next menu item should be
      if (!ctx->layoutStack.empty()) {
         LayoutStack &parentStack = ctx->layoutStack.back();
         if (!parentStack.isVertical) {
             ctx->cursorPos = Vec2(parentStack.cursor.x, parentStack.contentStart.y);
         }
      }
    }

    // Calcular posición y tamaño del dropdown basado en el contenido real
    Vec2 dropdownStartPos(state.position.x, state.position.y + state.size.y);
    Vec2 currentCursor = ctx->cursorPos;

    float dropdownWidth = 200.0f;
    float dropdownHeight = currentCursor.y - dropdownStartPos.y;
    if (dropdownHeight < 10.0f) {
      dropdownHeight = 10.0f;
    }

    Vec2 dropdownPos = dropdownStartPos;

    // Asegurar que el dropdown no se salga de la ventana
    Vec2 viewport = ctx->renderer.GetViewportSize();
    if (dropdownPos.x + dropdownWidth > viewport.x) {
      dropdownPos.x = viewport.x - dropdownWidth;
    }
    if (dropdownPos.y + dropdownHeight > viewport.y) {
      dropdownPos.y = state.position.y - dropdownHeight;
      if (dropdownPos.y < 0.0f) {
        dropdownPos.y = 0.0f;
        dropdownHeight = viewport.y - dropdownPos.y;
      }
    }

    // Encolar el dropdown para renderizado diferido
    UIContext::DeferredMenuDropdown dropdown;
    dropdown.dropdownPos = dropdownPos;
    dropdown.dropdownSize = Vec2(dropdownWidth, dropdownHeight);
    dropdown.menuId = currentMenuId;
    
    // Move items from currentMenuItems to dropdown
    if (!ctx->menuItemStartIndexStack.empty()) {
        size_t startIndex = ctx->menuItemStartIndexStack.back();
        ctx->menuItemStartIndexStack.pop_back();
        
        if (startIndex < ctx->currentMenuItems.size()) {
            // Calculate max width from items
            float maxItemWidth = dropdownWidth;
            for (size_t i = startIndex; i < ctx->currentMenuItems.size(); ++i) {
                maxItemWidth = std::max(maxItemWidth, ctx->currentMenuItems[i].size.x);
            }
            
            // Update dropdown width
            dropdown.dropdownSize.x = maxItemWidth;
            
            // Update all items to match max width
            for (size_t i = startIndex; i < ctx->currentMenuItems.size(); ++i) {
                ctx->currentMenuItems[i].size.x = maxItemWidth;
            }

            // Copy items to dropdown and adjust their positions to be relative to dropdownPos
            // El primer item debe comenzar exactamente en dropdownPos.y, sin espacio extra
            float currentY = dropdownPos.y;
            for (size_t i = startIndex; i < ctx->currentMenuItems.size(); ++i) {
                UIContext::DeferredMenuItem item = ctx->currentMenuItems[i];
                // Recalcular posición para que el primer item comience exactamente en dropdownPos
                item.pos.y = currentY;
                item.pos.x = dropdownPos.x;
                dropdown.items.push_back(item);
                // Avanzar Y para el siguiente item usando la altura del item actual
                currentY += item.size.y;
            }
            
            // Remove items from currentMenuItems
            ctx->currentMenuItems.resize(startIndex);
        }
    }
    
    ctx->deferredMenuDropdowns.push_back(dropdown);
  }
}

bool MenuItem(const std::string &label, bool enabled) {
  UIContext *ctx = GetContext();
  if (!ctx)
    return false;

  // Solo renderizar si hay un menú activo en el stack
  if (ctx->menuIdStack.empty())
    return false;
    
  uint32_t currentMenuId = ctx->menuIdStack.back();
  auto it = ctx->menuStates.find(currentMenuId);
  if (it == ctx->menuStates.end() || !it->second.open)
    return false;

  const PanelStyle &panelStyle = ctx->style.panel;
  const TextStyle &textStyle = ctx->style.GetTextStyle(TypographyStyle::Body);
  float itemHeight = textStyle.fontSize + panelStyle.padding.y * 2.0f;
  float itemWidth = 200.0f;

  Vec2 textSize = MeasureTextCached(ctx, label, textStyle.fontSize);
  itemWidth = std::max(itemWidth, textSize.x + panelStyle.padding.x * 2.0f);

  Vec2 itemSize(itemWidth, itemHeight);
  Vec2 itemPos = ctx->cursorPos;

  float mouseX = ctx->input.MouseX();
  float mouseY = ctx->input.MouseY();
  bool hover =
      enabled && (mouseX >= itemPos.x && mouseX <= itemPos.x + itemSize.x &&
                  mouseY >= itemPos.y && mouseY <= itemPos.y + itemSize.y);
  bool clicked = hover && enabled && ctx->input.IsMousePressed(0);

  // En lugar de dibujar inmediatamente, encolar para renderizado diferido
  UIContext::DeferredMenuItem item;
  item.label = label;
  item.enabled = enabled;
  item.pos = itemPos;
  item.size = itemSize;
  
  // Calcular colores
  Color itemBg = hover ? panelStyle.headerBackground : panelStyle.background;
  if (!enabled) {
    itemBg = Color(itemBg.r * 0.5f, itemBg.g * 0.5f, itemBg.b * 0.5f, itemBg.a);
  }
  item.bgColor = itemBg;
  
  Color textColor =
      enabled ? textStyle.color
              : Color(textStyle.color.r * 0.5f, textStyle.color.g * 0.5f,
                      textStyle.color.b * 0.5f, textStyle.color.a);
  item.textColor = textColor;
  
  ctx->currentMenuItems.push_back(item);

  // No dibujamos nada aquí, se dibujará en RenderDeferredDropdowns

  RegisterOccupiedArea(ctx, itemPos, itemSize);
  AdvanceCursor(ctx, itemSize);

  // Si se hace click, cerrar el menú
  if (clicked) {
    it->second.open = false;
    ctx->activeMenuId = 0;
  }

  return clicked;
}

void MenuSeparator() {
  UIContext *ctx = GetContext();
  if (!ctx)
    return;

  // Solo renderizar si hay un menú activo en el stack
  if (ctx->menuIdStack.empty())
    return;
    
  uint32_t currentMenuId = ctx->menuIdStack.back();
  auto it = ctx->menuStates.find(currentMenuId);
  if (it == ctx->menuStates.end() || !it->second.open)
    return;

  const PanelStyle &panelStyle = ctx->style.panel;
  float separatorHeight = 1.0f;
  float separatorPadding = 1.0f; // Reduced padding
  float separatorWidth = 200.0f;

  Vec2 separatorSize(separatorWidth, separatorHeight + separatorPadding * 2.0f);
  Vec2 separatorPos = ctx->cursorPos;

  // Encolar separador para renderizado diferido
  UIContext::DeferredMenuItem item;
  item.type = UIContext::DeferredMenuItem::Type::Separator;
  item.pos = separatorPos;
  item.size = separatorSize;
  item.bgColor = panelStyle.borderColor; // Usar color de borde para la línea
  
  ctx->currentMenuItems.push_back(item);

  RegisterOccupiedArea(ctx, separatorPos, separatorSize);
  AdvanceCursor(ctx, separatorSize);
}

// Render all deferred ComboBox dropdowns
// This should be called AFTER all widgets but BEFORE Render()
void RenderDeferredDropdowns() {
  UIContext *ctx = GetContext();
  if (!ctx) return;

  const PanelStyle &panelStyle = ctx->style.panel;
  const TextStyle &itemStyle = ctx->style.GetTextStyle(TypographyStyle::Body);

  float mouseX = ctx->input.MouseX();
  float mouseY = ctx->input.MouseY();
  float wheel = ctx->input.MouseWheelY();

  // Render each queued dropdown
  for (auto &dropdown : ctx->deferredComboDropdowns) {
    Vec2 dropdownPos = dropdown.dropdownPos;
    Vec2 dropdownSize = dropdown.dropdownSize;
    float fieldHeight = dropdown.fieldHeight;
    uint32_t id = dropdown.comboId;
    auto &state = ctx->comboBoxStates[id];

    // IMPORTANTE: NO hacer FlushBatch() explícito aquí - PushClipRect() lo hará automáticamente
    // si hay contenido en el batch. Hacer flush explícito aquí causa flickering porque renderiza
    // el contenido antes de que esté completamente dibujado.

    // Check click outside to close (robust check including dropdown area)
    if (ctx->input.IsMousePressed(0)) {
        bool inDropdown = PointInRect(Vec2(mouseX, mouseY), dropdownPos, dropdownSize);
        bool inField = PointInRect(Vec2(mouseX, mouseY), dropdown.fieldPos, dropdown.fieldSize);
        if (!inDropdown && !inField) {
            state.isOpen = false;
            continue; // Skip rendering this frame (it's closed now)
        }
    }

    // Scroll Logic
    float contentHeight = dropdown.items.size() * fieldHeight;
    float maxScroll = std::max(0.0f, contentHeight - dropdownSize.y);
    
    if (wheel != 0.0f) {
        bool hoverDropdown = PointInRect(Vec2(mouseX, mouseY), dropdownPos, dropdownSize);
        if (hoverDropdown) {
             state.scrollOffset -= wheel * 30.0f;
        }
    }
    
    // Auto-scroll to hovered item if just opened or navigating
    // (This logic could be refined to only trigger on key press)
    // For now, clamp scroll
    state.scrollOffset = std::clamp(state.scrollOffset, 0.0f, maxScroll);
    
    // Draw dropdown background
    bool isDarkTheme = (ctx->style.backgroundColor.r < 0.5f);
    Color dropdownBg = isDarkTheme ? Color(0.149f, 0.149f, 0.149f, 1.0f) : Color(0.95f, 0.95f, 0.95f, 1.0f);
    
    ctx->renderer.DrawRectWithElevation(dropdownPos, dropdownSize, dropdownBg, panelStyle.cornerRadius, 16.0f);

    // CLIP CONTENT
    ctx->renderer.PushClipRect(dropdownPos, dropdownSize);

    bool itemClicked = false;
    int clickedIndex = -1;
    
    Color itemHoverBg = isDarkTheme ? Color(0.22f, 0.22f, 0.22f, 1.0f) : Color(0.9f, 0.9f, 0.9f, 1.0f);
    Color itemTextColor = isDarkTheme ? Color(0.94f, 0.94f, 0.94f, 1.0f) : itemStyle.color;
    Color activeBarColor = Color(0.25f, 0.63f, 1.0f, 1.0f); 

    for (size_t i = 0; i < dropdown.items.size(); ++i) {
      float itemY = dropdownPos.y + static_cast<float>(i) * fieldHeight - state.scrollOffset;
      
      // Optimization: Skip items outside view
      if (itemY + fieldHeight < dropdownPos.y || itemY > dropdownPos.y + dropdownSize.y) continue;

      Vec2 itemPos(dropdownPos.x, itemY);
      Vec2 itemSize(dropdownSize.x, fieldHeight);

      bool hoverItem = PointInRect(Vec2(mouseX, mouseY), itemPos, itemSize);
      bool isSelected = (static_cast<int>(i) == dropdown.selectedIndex);
      bool isHoveredKey = (static_cast<int>(i) == state.hoveredIndex);
      
      if (hoverItem) state.hoveredIndex = static_cast<int>(i);

      if (hoverItem || isSelected || isHoveredKey) {
        ctx->renderer.DrawRectFilled(itemPos, itemSize, itemHoverBg, panelStyle.cornerRadius * 0.5f);
      }

      if (isSelected || isHoveredKey) {
        float barWidth = 3.0f;
        ctx->renderer.DrawRectFilled(itemPos, Vec2(barWidth, itemSize.y), activeBarColor, 0.0f);
      }

      Vec2 itemTextSize = MeasureTextCached(ctx, dropdown.items[i], itemStyle.fontSize);
      float itemTextY = itemPos.y + itemSize.y * 0.5f - itemTextSize.y * 0.5f;
      Vec2 itemTextPos(itemPos.x + panelStyle.padding.x, itemTextY);
      ctx->renderer.DrawText(itemTextPos, dropdown.items[i], itemTextColor, itemStyle.fontSize);

      if (hoverItem && ctx->input.IsMousePressed(0)) {
        itemClicked = true;
        clickedIndex = static_cast<int>(i);
      }
    }

    ctx->renderer.PopClipRect();

    // Scrollbar
    if (contentHeight > dropdownSize.y) {
        float scrollRatio = dropdownSize.y / contentHeight;
        float barHeight = dropdownSize.y * scrollRatio;
        float barY = dropdownPos.y + (state.scrollOffset / contentHeight) * dropdownSize.y;
        
        float barWidth = 4.0f;
        Vec2 barPos(dropdownPos.x + dropdownSize.x - barWidth - 2.0f, barY);
        ctx->renderer.DrawRectFilled(barPos, Vec2(barWidth, barHeight), Color(0.5f, 0.5f, 0.5f, 0.5f), 2.0f);
    }

    if (itemClicked && dropdown.currentItemPtr) {
      *dropdown.currentItemPtr = clickedIndex;
      state.isOpen = false;
    }
  }

  ctx->deferredComboDropdowns.clear();

  // Render deferred menu dropdowns
  for (auto &dropdown : ctx->deferredMenuDropdowns) {
    // Draw dropdown background with elevation and contrast (sin borde)
    Color dropdownBg = ctx->style.panel.background;
    bool isDarkTheme = (ctx->style.backgroundColor.r < 0.5f);
    if (isDarkTheme) {
      // Más claro que el fondo para mejor contraste
      dropdownBg = Color(dropdownBg.r * 1.15f, dropdownBg.g * 1.15f, dropdownBg.b * 1.15f, 1.0f);
    } else {
      // Más oscuro que el fondo para mejor contraste
      dropdownBg = Color(dropdownBg.r * 0.92f, dropdownBg.g * 0.92f, dropdownBg.b * 0.92f, 1.0f);
    }
    
    ctx->renderer.DrawRectWithElevation(
        dropdown.dropdownPos, dropdown.dropdownSize,
        dropdownBg, 4.0f, 8.0f);

    // Draw menu items
    const TextStyle &textStyle = ctx->style.GetTextStyle(TypographyStyle::Body);
    const PanelStyle &panelStyle = ctx->style.panel;
    
    for (const auto &item : dropdown.items) {
        if (item.type == UIContext::DeferredMenuItem::Type::Separator) {
            // Draw separator line
            float separatorPadding = 1.0f; // Reduced padding
            Vec2 lineStart(item.pos.x + separatorPadding,
                           item.pos.y + separatorPadding);
            Vec2 lineEnd(item.pos.x + item.size.x - separatorPadding,
                         item.pos.y + separatorPadding);
            ctx->renderer.DrawLine(lineStart, lineEnd, item.bgColor, 1.0f);
        } else {
            // Draw item background
            ctx->renderer.DrawRectFilled(item.pos, item.size, item.bgColor, 0.0f);
            
            // Draw item text
            Vec2 textSize = MeasureTextCached(ctx, item.label, textStyle.fontSize);
            Vec2 textPos(item.pos.x + panelStyle.padding.x,
                         item.pos.y + (item.size.y - textSize.y) * 0.5f);
            
            ctx->renderer.DrawText(textPos, item.label, item.textColor, textStyle.fontSize);
        }
    }
  }

  // Clear the deferred menu dropdowns queue
  ctx->deferredMenuDropdowns.clear();
}

// -----------------------------------------------------------------------------
// Splitter Implementation
// -----------------------------------------------------------------------------
bool Splitter(const std::string &id, bool splitVertically, float thickness, float *size1, float *size2, float minSize1, float minSize2) {
    UIContext *ctx = GetContext();
    if (!ctx) return false;
    
    Vec2 cursor = ctx->cursorPos;
    
    // Determine the bounding box of the sensitive area (interaction)
    // We make the sensitive area larger than the visual area for easier grabbing
    float interactionThickness = std::max(thickness, 12.0f); // 12px hit area
    
    Vec2 splitterInteractionSize;
    Vec2 visualSize;
    
    if (splitVertically) {
        float height = 0.0f;
        if (!ctx->layoutStack.empty()) {
             height = ctx->layoutStack.back().contentSize.y; 
             if (height <= 0.0f) height = 100.0f;
        } else {
             height = 200.0f;
        }
        splitterInteractionSize = Vec2(interactionThickness, height);
        visualSize = Vec2(thickness, height);
    } else {
        float width = 0.0f;
        if (!ctx->layoutStack.empty()) {
             width = ctx->layoutStack.back().contentSize.x;
             if (width <= 0.0f) width = 200.0f;
        } else {
             width = 200.0f;
        }
        splitterInteractionSize = Vec2(width, interactionThickness);
        visualSize = Vec2(width, thickness);
    }

    Vec2 splitterPos = ctx->cursorPos;
    
    // Center the visual part within the interaction part (offset logic)
    // Actually, AdvanceCursor advances by the *layout* size.
    // If we want it "tight", the layout size should probably be the visual size, 
    // but the hit test should simply extend outwards.
    // OR, we declare the widget size as 'thickness', but check touches in 'thickness + padding'.
    
    std::string fullId = "SPLITTER:" + id;
    uint32_t widgetId = GenerateId(fullId.c_str());
    
    // Hit test with larger area centered on the splitter position
    Vec2 hitPos = splitterPos;
    Vec2 hitSize = splitterInteractionSize;
    
    // If vertical splitter, center hit box horizontally on splitterPos
    if (splitVertically) {
        hitPos.x -= (interactionThickness - thickness) * 0.5f;
    } else {
        hitPos.y -= (interactionThickness - thickness) * 0.5f;
    }
    
    bool hover = PointInRect(Vec2(ctx->input.MouseX(), ctx->input.MouseY()), hitPos, hitSize);
    
    if (hover) {
        // Set cursor? (Need SetCursor API, skipping for now)
        if (ctx->input.IsMousePressed(0)) {
            ctx->activeWidgetId = widgetId;
            ctx->activeWidgetType = ActiveWidgetType::Splitter; // Reuse Slider type for dragging
            if (ctx->window) SDL_SetWindowRelativeMouseMode(ctx->window, true); // Hide cursor and lock it
        }
    }
    
    bool changed = false;
    if (ctx->activeWidgetId == widgetId) {
        if (ctx->input.IsMouseDown(0)) {
             float delta = 0.0f;
             if (splitVertically) {
                 delta = ctx->input.MouseDeltaX();
             } else {
                 delta = ctx->input.MouseDeltaY();
             }
             
             if (delta != 0.0f) {
                 if (*size1 + delta < minSize1) delta = minSize1 - *size1;
                 if (*size2 - delta < minSize2) delta = *size2 - minSize2;
                 
                 *size1 += delta;
                 *size2 -= delta;
                 
                 // Explicitly clamp to ensure no drift below limits
                 if (*size1 < minSize1) {
                     float correction = minSize1 - *size1;
                     *size1 += correction;
                     *size2 -= correction;
                 }
                 if (*size2 < minSize2) {
                     float correction = minSize2 - *size2;
                     *size2 += correction;
                     *size1 -= correction;
                 }

                 changed = true;
             }
        } else {
             ctx->activeWidgetId = 0;
             if (ctx->window) SDL_SetWindowRelativeMouseMode(ctx->window, false);
        }
    }
    
    Color color = ctx->style.panel.borderColor;
    if (ctx->activeWidgetId == widgetId || hover) {
        color = ctx->style.button.background.hover;
    }
    
    ctx->renderer.DrawRectFilled(splitterPos, visualSize, color, 0.0f);
    
    // We only advance by the visible/physical thickness to keep layout tight
    RegisterOccupiedArea(ctx, splitterPos, visualSize);
    AdvanceCursor(ctx, visualSize);
    
    return changed;
}

// -----------------------------------------------------------------------------
// Color Helpers
// -----------------------------------------------------------------------------
static void RGBtoHSV(float r, float g, float b, float& h, float& s, float& v) {
    float K = 0.f;
    if (g < b) {
        std::swap(g, b);
        K = -1.f;
    }
    if (r < g) {
        std::swap(r, g);
        K = -2.f / 6.f - K;
    }
    float chroma = r - std::min(g, b);
    h = std::abs(K + (g - b) / (6.f * chroma + 1e-20f));
    s = chroma / (r + 1e-20f);
    v = r;
}

static void HSVtoRGB(float h, float s, float v, float& r, float& g, float& b) {
    if (s == 0.0f) {
        r = g = b = v;
        return;
    }
    h = std::fmod(h, 1.0f);
    if (h < 0.0f) h += 1.0f;
    int i = static_cast<int>(h * 6.0f);
    float f = (h * 6.0f) - i;
    float p = v * (1.0f - s);
    float q = v * (1.0f - s * f);
    float t = v * (1.0f - s * (1.0f - f));
    switch (i % 6) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        case 5: r = v; g = p; b = q; break;
    }
}

// -----------------------------------------------------------------------------
// ColorPicker Implementation
// -----------------------------------------------------------------------------
bool ColorPicker(const std::string &label, Color *color) {
    UIContext *ctx = GetContext();
    if (!ctx) return false;
    
    const float pickerWidth = 200.0f;
    const float pickerHeight = 150.0f;
    const float barHeight = 20.0f;
    const float margin = 0.0f; // Unified look: no gap between SV box and Hue bar
    const float padding = 4.0f; // Padding inside the widget background
    
    Vec2 pos = ctx->cursorPos;
    
    // Draw widget background
    // Include 20px extra at bottom for the label "MyColor"
    float rightSectionWidth = 80.0f; // Space for preview
    
    // Calculate vertical layout
    float headerHeight = 25.0f; // Space for Label at top
    float footerHeight = 25.0f; // Space for RGB text at bottom
    float totalH = headerHeight + pickerHeight + margin + barHeight + padding * 2.0f + footerHeight;

    Vec2 bgSize(pickerWidth + padding * 3.0f + rightSectionWidth, totalH);
    ctx->renderer.DrawRectFilled(pos, bgSize, ctx->style.panel.background, 8.0f); // Main background with 8px radius
    // Remove Border DrawRect call
    // ctx->renderer.DrawRect(pos, bgSize, ctx->style.panel.borderColor, 4.0f);
    
    // Adjust content position (shift down by header)
    Vec2 contentPos = pos + Vec2(padding, padding + headerHeight);
    
    // Register total size including background
    // Note: totalH already accounts for all components
    Vec2 totalOccupiedSize = bgSize;
    RegisterOccupiedArea(ctx, pos, totalOccupiedSize);
    AdvanceCursor(ctx, totalOccupiedSize);
    
    std::string idStr = "CP:" + label;
    uint32_t svId = GenerateId((idStr + "_SV").c_str());
    uint32_t hId = GenerateId((idStr + "_H").c_str());
    
    float h, s, v;
    RGBtoHSV(color->r, color->g, color->b, h, s, v);
    
    Vec2 svPos = contentPos;
    Vec2 svSize(pickerWidth, pickerHeight);
    
    float r, g, b;
    HSVtoRGB(h, 1.0f, 1.0f, r, g, b);
    Color hueColor(r, g, b, 1.0f);
    
    // Gradient Simulation (SV Box)
    // Draw grid of small rects (color interpolated) 
    // Grid resolution: 50x50 steps for smoothness
    int stepsX = 50;
    int stepsY = 50;
    float stepW = svSize.x / stepsX;
    float stepH = svSize.y / stepsY;

    // Local helper for interpolation
    auto LerpColor = [](const Color& a, const Color& b, float t) {
        return Color(
            a.r + (b.r - a.r) * t,
            a.g + (b.g - a.g) * t,
            a.b + (b.b - a.b) * t,
            a.a + (b.a - a.a) * t
        );
    };

    // Pre-calculate hue Color (S=1, V=1)
    // White is (S=0, V=1)
    // Black is (V=0)
    Color white(1.0f, 1.0f, 1.0f, 1.0f);
    Color black(0.0f, 0.0f, 0.0f, 1.0f);

    for (int y = 0; y < stepsY; ++y) {
        float vVal = 1.0f - (float)y / (stepsY - 1); // Top is V=1, Bottom is V=0
        for (int x = 0; x < stepsX; ++x) {
            float sVal = (float)x / (stepsX - 1); // Left is S=0, Right is S=1
            
            // Interpolate S (White -> Hue)
            Color satColor = LerpColor(white, hueColor, sVal);
            
            // Interpolate V (SatColor -> Black)
            // Note: Value affects brightness towards black.
            // V=1 -> satColor. V=0 -> Black.
            // So pixel = Lerp(Black, satColor, vVal)
            Color pixelColor = LerpColor(black, satColor, vVal);
            
            ctx->renderer.DrawRectFilled(
                svPos + Vec2(x * stepW, y * stepH), 
                Vec2(stepW + 1.0f, stepH + 1.0f), // +1 overlap to avoid gaps
                pixelColor, 
                0.0f
            );
        }
    }
    
    // Draw cursor
    // Ensure cursor is constrained purely for visual safety
    float safeS = std::clamp(s, 0.0f, 1.0f);
    float safeV = std::clamp(v, 0.0f, 1.0f);
    float cursorX = safeS * svSize.x;
    float cursorY = (1.0f - safeV) * svSize.y;
    ctx->renderer.DrawRectFilled(svPos + Vec2(cursorX - 3, cursorY - 3), Vec2(6, 6), Color(1,1,1,1), 3.0f);
    ctx->renderer.DrawRect(svPos + Vec2(cursorX - 3, cursorY - 3), Vec2(6, 6), Color(0,0,0,1), 3.0f);
    
    bool valueChanged = false;
    bool hoverSV = PointInRect(Vec2(ctx->input.MouseX(), ctx->input.MouseY()), svPos, svSize);
    
    if (hoverSV && ctx->input.IsMousePressed(0)) {
        ctx->activeWidgetId = svId;
    }
    
    if (ctx->activeWidgetId == svId) {
        if (ctx->input.IsMouseDown(0)) {
            float mx = std::clamp(ctx->input.MouseX() - svPos.x, 0.0f, svSize.x);
            float my = std::clamp(ctx->input.MouseY() - svPos.y, 0.0f, svSize.y);
            s = mx / svSize.x;
            v = 1.0f - (my / svSize.y);
            valueChanged = true;
        } else {
            ctx->activeWidgetId = 0;
        }
    }

    Vec2 hPos = Vec2(contentPos.x, contentPos.y + pickerHeight + margin);
    Vec2 hSize(pickerWidth, barHeight);
    
    for (int i = 0; i < 6; ++i) {
        float p0 = i / 6.0f;
        float segmentW = hSize.x / 6.0f;
        HSVtoRGB(p0, 1.0f, 1.0f, r, g, b);
        Color c0(r, g, b, 1.0f);
        ctx->renderer.DrawRectFilled(Vec2(hPos.x + i * segmentW, hPos.y), Vec2(segmentW, hSize.y), c0, 0.0f);
    }
    
    float hCursorX = h * hSize.x;
    ctx->renderer.DrawRectFilled(hPos + Vec2(hCursorX - 2, 0), Vec2(4, hSize.y), Color(1,1,1,1), 0.0f);
    
    bool hoverH = PointInRect(Vec2(ctx->input.MouseX(), ctx->input.MouseY()), hPos, hSize);
    if (hoverH && ctx->input.IsMousePressed(0)) {
        ctx->activeWidgetId = hId;
    }
    
    if (ctx->activeWidgetId == hId) {
        if (ctx->input.IsMouseDown(0)) {
            float mx = std::clamp(ctx->input.MouseX() - hPos.x, 0.0f, hSize.x);
            h = mx / hSize.x;
            valueChanged = true;
        } else {
            ctx->activeWidgetId = 0;
        }
    }
    
    if (valueChanged) {
        float newR, newG, newB;
        HSVtoRGB(h, s, v, newR, newG, newB);
        color->r = newR;
        color->g = newG;
        color->b = newB;
    }

    // RegisterOccupiedArea already done at the top to secure layout space
    // Vec2 totalSize(pickerWidth + padding * 2.0f, pickerHeight + margin + barHeight + padding * 2.0f + 30.0f);
    // RegisterOccupiedArea(ctx, pos, totalSize);
    // AdvanceCursor(ctx, totalSize);
    
    // Draw label at the TOP (Header)
    ctx->renderer.DrawText(Vec2(pos.x + padding, pos.y + padding), label, ctx->style.GetTextStyle(TypographyStyle::Body).color);

    // Draw Preview (Right Side, aligned with SV top)
    float rightSectionX = pos.x + padding + pickerWidth + padding * 3.0f; 
    float previewY = svPos.y; 
    
    // Preview Rect (40x40) inside background
    Vec2 previewPos(rightSectionX, previewY);
    ctx->renderer.DrawRectFilled(previewPos, Vec2(40, 40), *color, 8.0f);
    
    // RGB Text at the BOTTOM (Footer, Horizontal)
    auto fmt = [](float v) { 
        std::string s = std::to_string(v); 
        return s.length() > 4 ? s.substr(0, 4) : s; 
    };
    
    float footerY = pos.y + totalH - padding - 16.0f; // Approx baseline
    float colWidth = 60.0f;
    
    ctx->renderer.DrawText(Vec2(pos.x + padding, footerY), "R " + fmt(color->r), ctx->style.GetTextStyle(TypographyStyle::Body).color);
    ctx->renderer.DrawText(Vec2(pos.x + padding + colWidth, footerY), "G " + fmt(color->g), ctx->style.GetTextStyle(TypographyStyle::Body).color);
    ctx->renderer.DrawText(Vec2(pos.x + padding + colWidth * 2.0f, footerY), "B " + fmt(color->b), ctx->style.GetTextStyle(TypographyStyle::Body).color);

    return valueChanged;
}


void Image(uint32_t textureId, const Vec2 &size, const Vec2 &uv0, const Vec2 &uv1, const Color &tintColor) {
  UIContext *ctx = GetContext();
  if (!ctx) return;

  LayoutConstraints constraints = ConsumeNextConstraints();
  Vec2 layoutSize = ApplyConstraints(ctx, constraints, size);
  
  Vec2 pos = ctx->cursorPos;
  // If positioned absolutely (not supported by this signature yet, usually standard Image flows)
  
  ctx->renderer.DrawImage(textureId, pos, layoutSize, uv0, uv1, tintColor);
  
  RegisterOccupiedArea(ctx, pos, layoutSize);
  ctx->lastItemPos = pos;
  AdvanceCursor(ctx, layoutSize);
}

bool DragFloat(const std::string &label, float *value, float speed, float min, float max, float width, const Vec2 &pos) {
  UIContext *ctx = GetContext();
  if (!ctx || !value) return false;

  float height = 30.0f;
  Vec2 boxSize(width, height);
  
  const TextStyle &textStyle = ctx->style.GetTextStyle(TypographyStyle::Body);
  
  std::string displayLabel = label;
  size_t hashPos = label.find("##");
  if (hashPos != std::string::npos) {
      displayLabel = label.substr(0, hashPos);
  }
  
  Vec2 textSize = ctx->renderer.MeasureText(displayLabel, textStyle.fontSize);
  Vec2 totalSize(boxSize.x + 8.0f + textSize.x, std::max(boxSize.y, textSize.y));
  
  LayoutConstraints constraints = ConsumeNextConstraints();
  Vec2 layoutSize = ApplyConstraints(ctx, constraints, totalSize);

  Vec2 startPos;
  if (pos.x != 0.0f || pos.y != 0.0f) startPos = ResolveAbsolutePosition(ctx, pos, layoutSize);
  else startPos = ctx->cursorPos;

  // Interaction Area (Box)
  // Calculate positions: Label -> Spacing -> Box
  Vec2 labelPos(startPos.x, startPos.y + (height - textSize.y) * 0.5f);
  Vec2 boxPos(startPos.x + textSize.x + 8.0f, startPos.y);

  // Background (Box)
  Color bgColor = (ctx->style.backgroundColor.r < 0.5f) ? Color(0.15f, 0.15f, 0.15f, 1.0f) : Color(0.96f, 0.96f, 0.96f, 1.0f);
  ctx->renderer.DrawRectFilled(boxPos, boxSize, bgColor, 4.0f);

  std::string idStr = "DRAG:" + label;
  uint32_t id = GenerateId(idStr.c_str());

  bool changed = false;
  float mouseX = ctx->input.MouseX();
  float mouseY = ctx->input.MouseY();
  // Hover uses boxPos
  bool hover = PointInRect(Vec2(mouseX, mouseY), boxPos, boxSize);

  if (hover && ctx->input.IsMousePressed(0)) {
      ctx->activeWidgetId = id;
      ctx->activeWidgetType = ActiveWidgetType::Slider; 
      if (ctx->window) SDL_SetWindowRelativeMouseMode(ctx->window, true);
  }

  if (ctx->activeWidgetId == id) {
      if (ctx->input.IsMouseDown(0)) {
          float delta = ctx->input.MouseDeltaX();
          if (delta != 0.0f) {
              *value += delta * speed * 0.1f;
              if (min != max) *value = std::clamp(*value, min, max);
              changed = true;
          }
      } else {
          ctx->activeWidgetId = 0;
          if (ctx->window) SDL_SetWindowRelativeMouseMode(ctx->window, false);
      }
  }

  // Value Text (Centered in Box)
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%.3f", *value);
  std::string valStr = buf;
  Vec2 valTextSize = ctx->renderer.MeasureText(valStr, textStyle.fontSize);
  Vec2 valTextPos(boxPos.x + (boxSize.x - valTextSize.x) * 0.5f, boxPos.y + (boxSize.y - valTextSize.y) * 0.5f);
  ctx->renderer.DrawText(valTextPos, valStr, textStyle.color, textStyle.fontSize);

  // Label (Already calculated pos, Draw it)
  ctx->renderer.DrawText(labelPos, displayLabel, textStyle.color, textStyle.fontSize);

  RegisterOccupiedArea(ctx, startPos, layoutSize);
  ctx->lastItemPos = startPos;
  AdvanceCursor(ctx, layoutSize);
  return changed;
}

bool DragVector3(const std::string &label, float *values, float speed, float min, float max, const Vec2 &pos) {
    // 3 DragFloats side by side
    BeginHorizontal(5.0f, std::nullopt, Vec2(0.0f, 0.0f));
    bool c = false;
    
    // Label on left with fixed width for alignment
    // Center vertically relative to DragFloat (height 30.0f)
    UIContext* ctx = GetContext();
    float dragHeight = 30.0f;
    float fontSize = ctx->style.typography.body.fontSize;
    float yOffset = (dragHeight - fontSize) * 0.5f;
    
    // Apply temporary Y offset for centering
    ctx->cursorPos.y += yOffset;
    
    SetNextConstraints(FixedSize(80.0f, 0.0f));
    Label(label);

    // Apply negative offset to restore? No, AdvanceCursor resets it.
    // However, if we don't restore it, AdvanceCursor might calculate "row height" incorrectly 
    // if it uses current cursor Y? No, it uses contentStart.y + largest height.
    // But let's check: Labels usually take ~20px. DragFloat 30px.
    // If we shift label down, its "box" moves down. 
    // AdvanceCursor (horizontal) uses stack.contentSize.y (max height).
    // It doesn't care about Y pos much for height calc, but for next item...
    // But wait, AdvanceCursor resets cursor Y to contentStart.y. 
    // So subsequent DragFloats will start at top. Correct.
    
    // However, Label implementation has:
    // ctx->lastItemPos = pos;
    // RegisterOccupiedArea(ctx, pos, finalSize);
    // So occupied area will be shifted down. That is effectively correct for the label.
    
    // Use label as suffix to ensure uniqueness: "##X" + label -> "X##" + label
    // DragFloat uses label for ID generation.
    // We want Display: "X", ID: "X" + UniqueSuffix.
    c |= DragFloat("X##" + label + "_X", &values[0], speed, min, max, 60.0f);
    c |= DragFloat("Y##" + label + "_Y", &values[1], speed, min, max, 60.0f);
    c |= DragFloat("Z##" + label + "_Z", &values[2], speed, min, max, 60.0f);
    
    EndHorizontal();
    return c;
}

bool CollapsibleHeader(const std::string &label, bool defaultOpen, const Vec2 &pos) {
  UIContext *ctx = GetContext();
  if (!ctx) return false;

  std::string idStr = "HDR:" + label;
  uint32_t id = GenerateId(idStr.c_str());
  auto boolEntry = ctx->boolStates.try_emplace(id, defaultOpen);
  bool isOpen = boolEntry.first->second;

  const PanelStyle &panelStyle = ctx->style.panel;
  float width = ctx->layoutStack.back().availableSpace.x;
  float height = 24.0f;
  Vec2 headerSize(width, height);
  
  Vec2 headerPos = (pos.x == 0 && pos.y == 0) ? ctx->cursorPos : pos;
  
  bool hover = PointInRect(Vec2(ctx->input.MouseX(), ctx->input.MouseY()), headerPos, headerSize);
  if (hover && ctx->input.IsMousePressed(0)) {
      isOpen = !isOpen;
      boolEntry.first->second = isOpen;
  }
  
  // Determine background color
  Color bg = ctx->style.panel.background; // Default transparent/background
  if (isOpen) {
      // Use a dark grey for open state (mimicking ListView selection in dark mode)
      // avoiding 'style.button.background.normal' if it is blue.
      // We can synthesize a darker shade of the panel background or use a specific neutral color.
      if (ctx->style.backgroundColor.r < 0.5f) { // Dark theme
          bg = Color(1.0f, 1.0f, 1.0f, 0.05f); // Subtle white overlay -> Dark Grey
      } else {
           bg = Color(0.0f, 0.0f, 0.0f, 0.05f); // Subtle black overlay -> Light Grey
      }
  } else if (hover) {
      bg = ctx->style.button.background.hover; // Hover background
  }
  
  // Draw background
  ctx->renderer.DrawRectFilled(headerPos, headerSize, bg, 4.0f);
  
  // Draw Selection Pill (if open)
  if (isOpen) {
      float pillWidth = 3.0f;
      float pillHeight = headerSize.y - 8.0f;
      Vec2 pillPos(headerPos.x, headerPos.y + 4.0f);
      // Use accent color for the pill
      ctx->renderer.DrawRectFilled(pillPos, Vec2(pillWidth, pillHeight), FluentColors::Accent, 2.0f);
  }

  // Text (shifted slightly to account for pill)
  float textOffsetX = 12.0f; 
  Vec2 textSize = ctx->renderer.MeasureText(label, ctx->style.typography.bodyStrong.fontSize);
  float textY = headerPos.y + (headerSize.y - textSize.y) * 0.5f;
  ctx->renderer.DrawText(Vec2(headerPos.x + textOffsetX, textY), label, ctx->style.typography.bodyStrong.color);

  RegisterOccupiedArea(ctx, headerPos, headerSize);
  ctx->lastItemPos = headerPos;
  AdvanceCursor(ctx, headerSize);

  return isOpen;
}


bool BeginCollapsibleGroup(const std::string &label, bool defaultOpen, const Vec2 &pos) {
  UIContext *ctx = GetContext();
  if (!ctx) return false;

  std::string idStr = "GRP:" + label;
  uint32_t id = GenerateId(idStr.c_str());
  auto boolEntry = ctx->boolStates.try_emplace(id, defaultOpen);
  bool isOpen = boolEntry.first->second;

  // Retrieve/Initialize Group State
  auto &state = ctx->groupStates[id];
  if (!state.initialized) {
      state.initialized = true;
      state.widthCalculated = false;
      state.contentWidth = 0.0f;
  }

  const PanelStyle &panelStyle = ctx->style.panel;
  
  // Use available space initially, or calculated content width if locked
  float availableWidth = ctx->layoutStack.back().availableSpace.x;
  float width = availableWidth;
  
  if (state.widthCalculated && state.contentWidth > 0.0f) {
      // Use calculated width, bounded by available space? 
      // User asked to "maintain" width. Let's trust the calculation or min(calc, available).
      // Assuming inspector panels might resize, we might want max(calc, available)? 
      // No, user likely wants shrink-to-fit or stable width.
      // Let's use the stored width but ensure it doesn't exceed viewport/parent too wildly if resized.
      // Actually, standard behavior is usually filling the parent. 
      // BUT user asked for "dynamic like TabView", which shrinks to content.
      width = state.contentWidth;
  }

  float headerHeight = 24.0f;
  
  Vec2 headerPos = (pos.x == 0 && pos.y == 0) ? ctx->cursorPos : pos;
  Vec2 headerSize(width, headerHeight);
  
  // Interaction
  bool hover = PointInRect(Vec2(ctx->input.MouseX(), ctx->input.MouseY()), headerPos, headerSize);
  if (hover && ctx->input.IsMousePressed(0)) {
      isOpen = !isOpen;
      boolEntry.first->second = isOpen;
  }
  
  // Determine Background Color
  Color bg = ctx->style.panel.background; 
  
  if (isOpen) {
      // Background for Open State
      float contentHeight = 0.0f;
      if (ctx->floatStates.count(id)) {
          contentHeight = ctx->floatStates[id];
      }
      
      float totalHeight = headerHeight + contentHeight;
      
      if (ctx->style.backgroundColor.r < 0.5f) { 
          bg = Color(1.0f, 1.0f, 1.0f, 0.05f); 
      } else {
           bg = Color(0.0f, 0.0f, 0.0f, 0.05f);
      }
      
      ctx->renderer.DrawRectFilled(headerPos, Vec2(width, totalHeight), bg, 4.0f);
      
      g_collapsibleGroupStack.push_back(id);
      
      // Start Content Layout
      RegisterOccupiedArea(ctx, headerPos, headerSize);
      ctx->lastItemPos = headerPos;
      AdvanceCursor(ctx, headerSize);
      
      // If NOT calculated, we might want to capture width. 
      // We pass 'width' to BeginVertical. If it is the first run, it is availableWidth.
      BeginVertical(2.0f, std::make_optional(Vec2(width, 0.0f)), Vec2(10.0f, 10.0f)); 
      
      // Save start X for width calculation
      // We can use the layout stack logic in EndCollapsibleGroup
      
  } else {
      // Closed background
      if (hover) {
          bg = ctx->style.button.background.hover; 
          ctx->renderer.DrawRectFilled(headerPos, headerSize, bg, 4.0f);
      }
      
      RegisterOccupiedArea(ctx, headerPos, headerSize);
      ctx->lastItemPos = headerPos;
      AdvanceCursor(ctx, headerSize);
  }

  // Draw Selection Pill
  if (isOpen) {
      float pillWidth = 3.0f;
      float pillHeight = headerHeight - 8.0f;
      Vec2 pillPos(headerPos.x, headerPos.y + 4.0f);
      ctx->renderer.DrawRectFilled(pillPos, Vec2(pillWidth, pillHeight), FluentColors::Accent, 2.0f);
  }

  // Header Text
  float textOffsetX = 12.0f; 
  Vec2 textSize = ctx->renderer.MeasureText(label, ctx->style.typography.bodyStrong.fontSize);
  float textY = headerPos.y + (headerHeight - textSize.y) * 0.5f;
  ctx->renderer.DrawText(Vec2(headerPos.x + textOffsetX, textY), label, ctx->style.typography.bodyStrong.color);
  
  // If we are calculating width, we might want to ensure the header text is covered
  if (!state.widthCalculated) {
     float minHeaderWidth = textSize.x + textOffsetX + 20.0f; // + padding
     state.headerWidth = minHeaderWidth;
  }

  return isOpen;
}

void EndCollapsibleGroup() {
    UIContext *ctx = GetContext();
    if (!ctx || g_collapsibleGroupStack.empty()) return;

    // Get current layout state BEFORE ending it to measure content width
    float maxContentWidth = 0.0f;
    if (!ctx->layoutStack.empty()) {
        const auto& stack = ctx->layoutStack.back();
        maxContentWidth = stack.contentSize.x; 
        
        // Also check last item right edge to be safe?
        // stack.contentSize.x tracks the max width encountered in AdvanceCursor
    }

    // End Content Layout
    EndVertical();
    
    // Retrieve ID
    uint32_t id = g_collapsibleGroupStack.back();
    g_collapsibleGroupStack.pop_back();
    
    auto &state = ctx->groupStates[id];
    
    // Store content height
    float contentHeight = ctx->lastItemSize.y + 4.0f; 
    ctx->floatStates[id] = contentHeight;
    
    // Update Width Calculation
    if (!state.widthCalculated) {
        float finalWidth = std::max(state.headerWidth, maxContentWidth);
        // Add padding
        finalWidth += 16.0f; // Some padding
        
        if (finalWidth > 0.0f) {
            state.contentWidth = finalWidth;
            state.widthCalculated = true;
        }
    }
}
// Toast Notifications Implementation
void ShowNotification(const std::string &message, ToastType type) {
    UIContext *ctx = GetContext();
    if (!ctx) return;
    
    Toast toast; /* Toast is now in FluentUI namespace, visible here */
    toast.message = message;
    toast.type = type;
    toast.duration = 4.0f; // Default duration
    toast.elapsed = 0.0f;
    toast.opacity = 0.0f;
    
    ctx->toastQueue.push_back(toast);
}

void RenderNotifications() {
    UIContext *ctx = GetContext();
    if (!ctx || ctx->toastQueue.empty()) return;
    
    Vec2 viewport = ctx->renderer.GetViewportSize();
    float padding = 20.0f;
    float startY = viewport.y - padding;
    float startX = viewport.x - padding; // Align right
    
    // Iterate backwards to stack from bottom
    float currentY = startY;
    
    for (auto it = ctx->toastQueue.begin(); it != ctx->toastQueue.end();) {
        auto &toast = *it;
        
        // Update Logic
        toast.elapsed += ctx->deltaTime;
        
        // Fade In
        if (toast.elapsed < 0.3f) {
            toast.opacity = toast.elapsed / 0.3f;
        } 
        // Fade Out
        else if (toast.elapsed > toast.duration - 0.5f) {
            toast.opacity = (toast.duration - toast.elapsed) / 0.5f;
             if (toast.opacity < 0.0f) toast.opacity = 0.0f;
        } else {
            toast.opacity = 1.0f;
        }
        
        // Remove if expired
        if (toast.elapsed >= toast.duration) {
            it = ctx->toastQueue.erase(it);
            continue;
        }
        
        // Render
        const TextStyle &textStyle = ctx->style.GetTextStyle(TypographyStyle::Body);
        Vec2 textSize = ctx->renderer.MeasureText(toast.message, textStyle.fontSize);
        
        float panelPadding = 12.0f;
        Vec2 panelSize(textSize.x + panelPadding * 2.0f + 20.0f, textSize.y + panelPadding * 2.0f); // +20 for icon/bar
        
        // Calculate Position (Bottom Right)
        Vec2 panelPos(startX - panelSize.x, currentY - panelSize.y);
        
        // Colors
        Color bg, accent;
        switch (toast.type) {
            case ToastType::Success: 
                bg = Color(0.2f, 0.4f, 0.2f, 0.95f * toast.opacity); 
                accent = Color(0.4f, 0.8f, 0.4f, 1.0f * toast.opacity);
                break;
            case ToastType::Warning: 
                bg = Color(0.4f, 0.3f, 0.1f, 0.95f * toast.opacity); 
                accent = Color(1.0f, 0.8f, 0.2f, 1.0f * toast.opacity);
                break;
            case ToastType::Error:   
                bg = Color(0.4f, 0.2f, 0.2f, 0.95f * toast.opacity); 
                accent = Color(1.0f, 0.4f, 0.4f, 1.0f * toast.opacity);
                break;
            case ToastType::Info:    
            default:                 
                bg = Color(0.2f, 0.2f, 0.2f, 0.95f * toast.opacity); 
                accent = Color(0.2f, 0.6f, 1.0f, 1.0f * toast.opacity);
                break;
        }
        
        // Draw Shadow/Glow (Simulated by generic generic rect? or just Elevation)
        ctx->renderer.DrawRectWithElevation(panelPos, panelSize, bg, 4.0f, 4.0f);
        
        // Draw Accent Bar
        ctx->renderer.DrawRectFilled(panelPos, Vec2(4.0f, panelSize.y), accent, 0.0f); // Variable rounding not supported well yet?
        
        // Draw Text
        ctx->renderer.DrawText(Vec2(panelPos.x + 12.0f, panelPos.y + panelPadding), toast.message, Color(1,1,1, toast.opacity), textStyle.fontSize);
        
        currentY -= (panelSize.y + 10.0f); // Move up for next toast
        ++it;
    }

}
// Forward declarations
void RenderDockNode(UIContext* ctx, uint32_t nodeId);

// Helper function to count leaf nodes in the dock tree
int CountLeafNodes(UIContext* ctx, uint32_t nodeId) {
    if (ctx->dockNodes.find(nodeId) == ctx->dockNodes.end()) {
        return 0;
    }
    
    DockNode& node = ctx->dockNodes[nodeId];
    
    if (node.isLeaf()) {
        return 1;
    }
    
    int count = 0;
    if (node.childrenIds[0] != 0) {
        count += CountLeafNodes(ctx, node.childrenIds[0]);
    }
    if (node.childrenIds[1] != 0) {
        count += CountLeafNodes(ctx, node.childrenIds[1]);
    }
    
    return count;
}

void BeginDockSpace(const std::string &id) {
    UIContext *ctx = GetContext();
    if (!ctx) return;
    
    // Clear previous frame's docked info
    ctx->currentFrameDockedPanels.clear();
    
    // Reset dock preview state at the start of each frame
    // This ensures dock zones are recalculated fresh each frame
    ctx->dockPreviewType = 0;
    ctx->hoveredDockNodeId = 0;

    // Process Pending Dock Actions (Deferred from previous frame to avoid map invalidation)
    if (ctx->pendingDockAction.type != UIContext::PendingDockAction::Type::None) {
        auto& action = ctx->pendingDockAction;
        
        if (ctx->dockNodes.find(action.targetNodeId) != ctx->dockNodes.end()) {
            DockNode& targetNode = ctx->dockNodes[action.targetNodeId];
            
            if (action.type == UIContext::PendingDockAction::Type::Tab) {
                targetNode.dockedPanelIds.push_back(action.panelId);
                targetNode.activeTabIndex = static_cast<int>(targetNode.dockedPanelIds.size() - 1);
            } 
            else if (action.type == UIContext::PendingDockAction::Type::Split) {
                // Check if target node is already a container (has children)
                // If so, and one child is empty, we just need to create that child
                if (!targetNode.isLeaf() && 
                    (targetNode.childrenIds[0] == 0 || targetNode.childrenIds[1] == 0)) {
                    // Node is already a container with an empty child
                    // Create the missing child node
                    uint32_t emptyChildIndex = (targetNode.childrenIds[0] == 0) ? 0 : 1;
                    uint32_t newChildId = ctx->nextDockNodeId++;
                    targetNode.childrenIds[emptyChildIndex] = newChildId;
                    
                    DockNode& newChild = GetDockNode(ctx, newChildId);
                    newChild.parentId = action.targetNodeId;
                    newChild.dockedPanelIds.push_back(action.panelId);
                    newChild.activeTabIndex = 0;
                } else if (!targetNode.isLeaf()) {
                    // Target node is already a container with both children
                    // We need to wrap the existing children in a new container node
                    // Save all IDs and structure BEFORE any modifications
                    uint32_t existingChild1Id = targetNode.childrenIds[0];
                    uint32_t existingChild2Id = targetNode.childrenIds[1];
                    DockSplitDirection existingSplitDirection = targetNode.splitDirection;
                    float existingSplitRatio = targetNode.splitRatio;
                    
                    // Create wrapper node to hold existing structure
                    uint32_t wrapperChildId = ctx->nextDockNodeId++;
                    DockNode& wrapperChild = ctx->dockNodes[wrapperChildId];
                    wrapperChild.id = wrapperChildId;
                    wrapperChild.parentId = action.targetNodeId;
                    wrapperChild.splitDirection = existingSplitDirection;
                    wrapperChild.splitRatio = existingSplitRatio;
                    wrapperChild.childrenIds[0] = existingChild1Id;
                    wrapperChild.childrenIds[1] = existingChild2Id;
                    
                    // Update parent references for existing children (use fresh lookups)
                    if (existingChild1Id != 0) {
                        DockNode& existingChild1 = ctx->dockNodes[existingChild1Id];
                        existingChild1.parentId = wrapperChildId;
                    }
                    if (existingChild2Id != 0) {
                        DockNode& existingChild2 = ctx->dockNodes[existingChild2Id];
                        existingChild2.parentId = wrapperChildId;
                    }
                    
                    // Create new child for the panel being docked
                    uint32_t newChildId = ctx->nextDockNodeId++;
                    DockNode& newChild = ctx->dockNodes[newChildId];
                    newChild.id = newChildId;
                    newChild.parentId = action.targetNodeId;
                    newChild.dockedPanelIds.push_back(action.panelId);
                    newChild.activeTabIndex = 0;
                    
                    // Update target node structure (use fresh lookup to avoid reference invalidation)
                    DockNode& freshTargetNode = ctx->dockNodes[action.targetNodeId];
                    freshTargetNode.splitDirection = action.splitDirection;
                    freshTargetNode.splitRatio = 0.5f;
                    if (action.insertBefore) {
                        freshTargetNode.childrenIds[0] = newChildId;
                        freshTargetNode.childrenIds[1] = wrapperChildId;
                    } else {
                        freshTargetNode.childrenIds[0] = wrapperChildId;
                        freshTargetNode.childrenIds[1] = newChildId;
                    }
                } else {
                    // Perform Split (convert leaf to container)
                    DockNode parentCopy = targetNode; // Capture current state (content)
                    
                    // Reset target node to become a container
                    targetNode.dockedPanelIds.clear();
                    targetNode.activeTabIndex = 0;
                    targetNode.splitDirection = action.splitDirection;
                    targetNode.splitRatio = 0.5f; // Default split
                    
                    // Create children
                    ctx->nextDockNodeId++;
                    uint32_t child1Id = ctx->nextDockNodeId;
                    ctx->nextDockNodeId++;
                    uint32_t child2Id = ctx->nextDockNodeId;
                    
                    targetNode.childrenIds[0] = child1Id;
                    targetNode.childrenIds[1] = child2Id;
                    
                    // Create child nodes
                    DockNode& c1 = GetDockNode(ctx, child1Id);
                    c1.parentId = action.targetNodeId;
                    
                    DockNode& c2 = GetDockNode(ctx, child2Id);
                    c2.parentId = action.targetNodeId;
                    
                    // Assign content based on insertBefore
                    if (action.insertBefore) {
                        // New Panel -> Child 1 (Left/Top)
                        c1.dockedPanelIds.push_back(action.panelId);
                        c1.activeTabIndex = 0;
                        
                        // Old Content -> Child 2 (Right/Bottom)
                        c2.dockedPanelIds = parentCopy.dockedPanelIds;
                        c2.activeTabIndex = parentCopy.activeTabIndex;
                    } else {
                        // Old Content -> Child 1 (Left/Top)
                        c1.dockedPanelIds = parentCopy.dockedPanelIds;
                        c1.activeTabIndex = parentCopy.activeTabIndex;
                        
                        // New Panel -> Child 2 (Right/Bottom)
                        c2.dockedPanelIds.push_back(action.panelId);
                        c2.activeTabIndex = 0;
                    }
                }
            }
        }
        
        // Reset action
        ctx->pendingDockAction.type = UIContext::PendingDockAction::Type::None;
        
        // Restore manual undock flag if needed (handled in previous logic, but here we just process)
        // The dragging logic cleared manualUndocked.
        std::string pKey = "PANEL:" + action.panelId;
        uint32_t pId = GenerateId(pKey.c_str());
        if (ctx->panelStates.find(pId) != ctx->panelStates.end()) {
             ctx->panelStates[pId].manuallyUndocked = false;
        }
    }

    // Ensure root node exists
    if (ctx->rootDockNodeId == 0) {
        ctx->rootDockNodeId = ctx->nextDockNodeId++;
        auto& root = GetDockNode(ctx, ctx->rootDockNodeId);
        root.size = ctx->renderer.GetViewportSize(); 
        root.position = Vec2(0,0); // Assume full screen for now
        
        // Setup default split for demo purposes if empty
        // For now, start empty
    }
    
    // Update Root Node Size to match viewport (or available space)
    auto& root = GetDockNode(ctx, ctx->rootDockNodeId);
    Vec2 viewportSize = ctx->renderer.GetViewportSize();
    root.size = viewportSize;
    root.position = Vec2(0,0);
    
    // Render the tree
    RenderDockNode(ctx, ctx->rootDockNodeId);
    
    // After rendering all nodes, if we're dragging a panel but no dock zone was detected,
    // allow docking to the root node (entire viewport) if mouse is within viewport bounds
    if (!ctx->draggedPanelId.empty() && ctx->dockPreviewType == 0) {
        bool isPanelDockable = false;
        std::string pKey = "PANEL:" + ctx->draggedPanelId;
        uint32_t pId = GenerateId(pKey.c_str());
        if (ctx->panelStates.find(pId) != ctx->panelStates.end()) {
            isPanelDockable = ctx->panelStates[pId].isDockable;
        }
        
        if (isPanelDockable) {
            Vec2 mouse(ctx->input.MouseX(), ctx->input.MouseY());
            // Check if mouse is within the viewport bounds
            if (mouse.x >= 0 && mouse.x <= viewportSize.x &&
                mouse.y >= 0 && mouse.y <= viewportSize.y) {
                
                // Check if root node is empty (can dock as tab) or has content (need to create split)
                auto& rootNode = GetDockNode(ctx, ctx->rootDockNodeId);
                // Root is empty if it's a leaf node with no docked panels
                bool rootIsEmpty = rootNode.isLeaf() && rootNode.dockedPanelIds.empty();
                // Root has content if it's a leaf node with docked panels OR it's a container (has children)
                bool rootHasContent = (rootNode.isLeaf() && !rootNode.dockedPanelIds.empty()) || !rootNode.isLeaf();
                
                if (rootIsEmpty) {
                    // Root is empty, dock as tab
                    ctx->hoveredDockNodeId = ctx->rootDockNodeId;
                    ctx->dockPreviewType = 5; // Center (Tab) - dock to root as a tab
                    ctx->dockPreviewRectPos = Vec2(0, 0);
                    ctx->dockPreviewRectSize = viewportSize;
                    
                    // Draw preview
                    Color previewColor(0.2f, 0.6f, 1.0f, 0.3f);
                    ctx->renderer.DrawRectFilled(ctx->dockPreviewRectPos, ctx->dockPreviewRectSize, previewColor);
                    
                    // Handle drop
                    if (!ctx->input.IsMouseDown(0)) {
                        std::string panelId = ctx->draggedPanelId;
                        ctx->pendingDockAction.type = UIContext::PendingDockAction::Type::Tab;
                        ctx->pendingDockAction.targetNodeId = ctx->rootDockNodeId;
                        ctx->pendingDockAction.panelId = panelId;
                        
                        // Reset manuallyUndocked flag
                        if (ctx->panelStates.find(pId) != ctx->panelStates.end()) {
                            ctx->panelStates[pId].manuallyUndocked = false;
                        }
                        
                        ctx->draggedPanelId = "";
                    }
                } else if (rootHasContent) {
                    // Root has content, calculate available panels and divide space accordingly
                    // Exclude the panel currently being dragged from the count
                    int availablePanels = CountAvailablePanels(ctx, ctx->draggedPanelId);
                    
                    if (availablePanels <= 0) {
                        // No available panels, don't show dock preview
                        // This shouldn't happen if we're dragging, but handle it anyway
                    } else {
                        // Divide viewport into N zones based on available panels
                        // Use vertical split if viewport is wider, horizontal if taller
                        bool useVerticalSplit = viewportSize.x >= viewportSize.y;
                        
                        // Calculate which zone the mouse is in
                        float mouseXPercent = mouse.x / viewportSize.x;
                        float mouseYPercent = mouse.y / viewportSize.y;
                        
                        int zoneIndex = 0;
                        bool insertBefore = false;
                        DockSplitDirection splitDirection;
                        Vec2 previewPos, previewSize;
                        
                        if (useVerticalSplit) {
                            // Divide horizontally into N vertical zones
                            float zoneWidth = 1.0f / availablePanels;
                            zoneIndex = static_cast<int>(std::min(mouseXPercent / zoneWidth, static_cast<float>(availablePanels - 1)));
                            
                            // Determine split: if mouse is in left half of zone, split before; otherwise after
                            float zoneStart = zoneIndex * zoneWidth;
                            float zoneCenter = zoneStart + zoneWidth * 0.5f;
                            insertBefore = (mouseXPercent < zoneCenter);
                            
                            splitDirection = DockSplitDirection::Vertical;
                            
                            // Calculate preview rect for this zone
                            float previewWidth = viewportSize.x / availablePanels;
                            previewPos = Vec2(zoneIndex * previewWidth, 0);
                            previewSize = Vec2(previewWidth, viewportSize.y);
                        } else {
                            // Divide vertically into N horizontal zones
                            float zoneHeight = 1.0f / availablePanels;
                            zoneIndex = static_cast<int>(std::min(mouseYPercent / zoneHeight, static_cast<float>(availablePanels - 1)));
                            
                            // Determine split: if mouse is in top half of zone, split before; otherwise after
                            float zoneStart = zoneIndex * zoneHeight;
                            float zoneCenter = zoneStart + zoneHeight * 0.5f;
                            insertBefore = (mouseYPercent < zoneCenter);
                            
                            splitDirection = DockSplitDirection::Horizontal;
                            
                            // Calculate preview rect for this zone
                            float previewHeight = viewportSize.y / availablePanels;
                            previewPos = Vec2(0, zoneIndex * previewHeight);
                            previewSize = Vec2(viewportSize.x, previewHeight);
                        }
                        
                        ctx->hoveredDockNodeId = ctx->rootDockNodeId;
                        ctx->dockPreviewType = splitDirection == DockSplitDirection::Vertical ? 
                            (insertBefore ? 1 : 2) : (insertBefore ? 3 : 4);
                        ctx->dockPreviewRectPos = previewPos;
                        ctx->dockPreviewRectSize = previewSize;
                        
                        // Draw preview
                        Color previewColor(0.2f, 0.6f, 1.0f, 0.3f);
                        ctx->renderer.DrawRectFilled(ctx->dockPreviewRectPos, ctx->dockPreviewRectSize, previewColor);
                        
                        // Handle drop
                        if (!ctx->input.IsMouseDown(0)) {
                            std::string panelId = ctx->draggedPanelId;
                            ctx->pendingDockAction.type = UIContext::PendingDockAction::Type::Split;
                            ctx->pendingDockAction.targetNodeId = ctx->rootDockNodeId;
                            ctx->pendingDockAction.panelId = panelId;
                            ctx->pendingDockAction.splitDirection = splitDirection;
                            ctx->pendingDockAction.insertBefore = insertBefore;
                            
                            // Reset manuallyUndocked flag
                            if (ctx->panelStates.find(pId) != ctx->panelStates.end()) {
                                ctx->panelStates[pId].manuallyUndocked = false;
                            }
                            
                            ctx->draggedPanelId = "";
                        }
                    }
                }
            }
        }
    }
}

void EndDockSpace() {
    // Nothing to do for now
}

// Recursive Rendering of Dock Nodes
void RenderDockNode(UIContext* ctx, uint32_t nodeId) {
    // Safety check needed because map access is unsafe if id is invalid?
    // GetDockNode creates it if missing, but we want to avoid creating garbage.
    // However, for recursion we assume valid IDs.
    
    // Validar si existe primero? No, GetDockNode es seguro.
    // IMPORTANTE: NO mantener referencias a DockNode a través de llamadas que modifiquen el mapa.
    
    // Obtenemos una COPIA de los datos estructurales necesarios para decidir,
    // o re-buscamos el nodo después de llamadas recursivas.
    // Dado que GetDockNode retorna referencia, úsala con cuidado.
    // Mejor patrón: Usar ID y re-obtener referencia solo cuando se necesite.
    
    // Paso 1: Leer estado del nodo actual
    // Nota: dockNodes es map, punteros/referencias pueden invalidarse si agregamos elementos.
    // Si la estructura ya es estable (no agrega nodos), es seguro.
    // Pero el Drag & Drop Agrega nodos (Split).
    
    // Para renderizar:
    DockNode& node = GetDockNode(ctx, nodeId); 
    // ^ Esta referencia es válida AHORA.
    // Pero si llamamos a RenderDockNode(hijo) y eso inserta nodos, `node` puede morir.
    
    if (!node.isLeaf()) {
        // Render Children
        // Guardamos IDs localmente
        uint32_t child1Id = node.childrenIds[0];
        uint32_t child2Id = node.childrenIds[1];
        DockSplitDirection dir = node.splitDirection;
        float splitRatio = node.splitRatio;
        Vec2 pos = node.position;
        Vec2 size = node.size;
        
        // Check if one of the children is empty (0)
        bool child1Empty = (child1Id == 0);
        bool child2Empty = (child2Id == 0);
        bool showSplitter = !child1Empty && !child2Empty; // Only show splitter if both children exist
        
        // Calculate Sizes based on Split
        Vec2 c1Pos, c1Size, c2Pos, c2Size;
        
        if (dir == DockSplitDirection::Vertical) {
            float splitPos = pos.x + size.x * splitRatio;
            c1Pos = pos;
            c1Size = Vec2(size.x * splitRatio, size.y);
            c2Pos = Vec2(splitPos, pos.y);
            c2Size = Vec2(size.x * (1.0f - splitRatio), size.y);
        } else {
            float splitPos = pos.y + size.y * splitRatio;
            c1Pos = pos;
            c1Size = Vec2(size.x, size.y * splitRatio);
            c2Pos = Vec2(pos.x, splitPos);
            c2Size = Vec2(size.x, size.y * (1.0f - splitRatio));
        }
        
        // If one child is empty, the other child keeps its original position and size
        // Don't expand it to fill the parent - this maintains the layout of other panels
        // The empty space will simply not render anything, but the split structure remains
        
        // Update children layout BEFORE recurrence
        // We need to get referencing safely.
        // GetDockNode might invalidate 'node', but we are done reading 'node' for now.
        if (!child1Empty) {
            auto& c1 = GetDockNode(ctx, child1Id);
            c1.position = c1Pos;
            c1.size = c1Size;
        }
        if (!child2Empty) {
            auto& c2 = GetDockNode(ctx, child2Id);
            c2.position = c2Pos;
            c2.size = c2Size;
        }
        
        // Interactive Splitter - only show if both children exist
        // Only handle splitter interaction if both children exist
        if (showSplitter) {
            float splitterThickness = 6.0f;
            Vec2 splitterPos, splitterSize;
            bool isVerticalSplit = (dir == DockSplitDirection::Vertical);
            
            if (isVerticalSplit) {
                float splitX = pos.x + size.x * splitRatio;
                splitterPos = Vec2(splitX - splitterThickness * 0.5f, pos.y);
                splitterSize = Vec2(splitterThickness, size.y);
            } else {
                float splitY = pos.y + size.y * splitRatio;
                splitterPos = Vec2(pos.x, splitY - splitterThickness * 0.5f);
                splitterSize = Vec2(size.x, splitterThickness);
            }
            
            Vec2 mousePos(ctx->input.MouseX(), ctx->input.MouseY());
            bool splitterHover = PointInRect(mousePos, splitterPos, splitterSize);
            
            // Track splitter drag state
            static uint32_t activeSplitterId = 0;
            static bool splitterDragging = false;
            
            if (splitterHover && ctx->input.IsMousePressed(0) && !splitterDragging) {
                splitterDragging = true;
                activeSplitterId = nodeId;
            }
            
            if (splitterDragging && activeSplitterId == nodeId) {
                if (!ctx->input.IsMouseDown(0)) {
                    splitterDragging = false;
                    activeSplitterId = 0;
                } else {
                    // Update split ratio based on mouse position
                    auto& currentNode = GetDockNode(ctx, nodeId);
                    if (isVerticalSplit) {
                        float newRatio = (mousePos.x - pos.x) / size.x;
                        currentNode.splitRatio = std::clamp(newRatio, 0.1f, 0.9f);
                    } else {
                        float newRatio = (mousePos.y - pos.y) / size.y;
                        currentNode.splitRatio = std::clamp(newRatio, 0.1f, 0.9f);
                    }
                }
            }
            
            // Draw splitter
            Color splitterColor = (splitterHover || (splitterDragging && activeSplitterId == nodeId))
                                 ? Color(0.4f, 0.4f, 0.4f, 1.0f)
                                 : Color(0.2f, 0.2f, 0.2f, 1.0f);
            ctx->renderer.DrawRectFilled(splitterPos, splitterSize, splitterColor);
        }
        
        // Handle docking detection for empty child areas
        // If a child is empty, allow docking directly in that area
        if (!ctx->draggedPanelId.empty()) {
            bool isPanelDockable = false;
            std::string pKey = "PANEL:" + ctx->draggedPanelId;
            uint32_t pId = GenerateId(pKey.c_str());
            if (ctx->panelStates.find(pId) != ctx->panelStates.end()) {
                isPanelDockable = ctx->panelStates[pId].isDockable;
            }
            
            if (isPanelDockable && ctx->dockPreviewType == 0) {
                Vec2 mouse(ctx->input.MouseX(), ctx->input.MouseY());
                
                // Check if mouse is in empty child 1 area
                if (child1Empty && mouse.x >= c1Pos.x && mouse.x <= c1Pos.x + c1Size.x &&
                    mouse.y >= c1Pos.y && mouse.y <= c1Pos.y + c1Size.y) {
                    ctx->hoveredDockNodeId = nodeId;
                    ctx->dockPreviewType = 6; // Special flag for empty child 1
                    ctx->dockPreviewRectPos = c1Pos;
                    ctx->dockPreviewRectSize = c1Size;
                    
                    Color previewColor(0.2f, 0.6f, 1.0f, 0.3f);
                    ctx->renderer.DrawRectFilled(ctx->dockPreviewRectPos, ctx->dockPreviewRectSize, previewColor);
                    
                    // Handle drop - use deferred action to avoid modifying during render
                    if (!ctx->input.IsMouseDown(0)) {
                        std::string panelId = ctx->draggedPanelId;
                        
                        // Store info for deferred processing
                        // Mark that we want to create child 1 (left/top)
                        ctx->pendingDockAction.type = UIContext::PendingDockAction::Type::Split;
                        ctx->pendingDockAction.targetNodeId = nodeId;
                        ctx->pendingDockAction.panelId = panelId;
                        ctx->pendingDockAction.splitDirection = dir; // Use existing split direction
                        ctx->pendingDockAction.insertBefore = true; // Child 1 (left/top)
                        
                        // Reset manuallyUndocked flag
                        if (ctx->panelStates.find(pId) != ctx->panelStates.end()) {
                            ctx->panelStates[pId].manuallyUndocked = false;
                        }
                        
                        ctx->draggedPanelId = "";
                    }
                }
                // Check if mouse is in empty child 2 area
                else if (child2Empty && mouse.x >= c2Pos.x && mouse.x <= c2Pos.x + c2Size.x &&
                         mouse.y >= c2Pos.y && mouse.y <= c2Pos.y + c2Size.y) {
                    ctx->hoveredDockNodeId = nodeId;
                    ctx->dockPreviewType = 7; // Special flag for empty child 2
                    ctx->dockPreviewRectPos = c2Pos;
                    ctx->dockPreviewRectSize = c2Size;
                    
                    Color previewColor(0.2f, 0.6f, 1.0f, 0.3f);
                    ctx->renderer.DrawRectFilled(ctx->dockPreviewRectPos, ctx->dockPreviewRectSize, previewColor);
                    
                    // Handle drop - use deferred action to avoid modifying during render
                    if (!ctx->input.IsMouseDown(0)) {
                        std::string panelId = ctx->draggedPanelId;
                        
                        // Store info for deferred processing
                        // Mark that we want to create child 2 (right/bottom)
                        ctx->pendingDockAction.type = UIContext::PendingDockAction::Type::Split;
                        ctx->pendingDockAction.targetNodeId = nodeId;
                        ctx->pendingDockAction.panelId = panelId;
                        ctx->pendingDockAction.splitDirection = dir; // Use existing split direction
                        ctx->pendingDockAction.insertBefore = false; // Child 2 (right/bottom)
                        
                        // Reset manuallyUndocked flag
                        if (ctx->panelStates.find(pId) != ctx->panelStates.end()) {
                            ctx->panelStates[pId].manuallyUndocked = false;
                        }
                        
                        ctx->draggedPanelId = "";
                    }
                }
            }
        }
        
        // Recursive render - only render non-empty children
        if (!child1Empty) {
            RenderDockNode(ctx, child1Id);
        }
        if (!child2Empty) {
            RenderDockNode(ctx, child2Id);
        }
    } else {
        // Leaf Node - Render Content
        // Re-get node because logic above was 'if', this is 'else', 
        // but let's be safe and assume 'node' reference is valid here as we entered 'else' immediately
        // and didn't call GetDockNode yet.
        
        
        // Only draw background for EMPTY leaf nodes (no docked panels)
        // If there are panels, they will draw their own backgrounds
        if (node.dockedPanelIds.empty()) {
            ctx->renderer.DrawRectFilled(node.position, node.size, Color(0.15f, 0.15f, 0.15f, 1.0f));
            ctx->renderer.DrawRect(node.position, node.size, Color(0.3f, 0.3f, 0.3f, 1.0f), 1.0f);
        }
        
        // --- Docking Drag & Drop Logic ---
        bool isPanelDockable = false;
        if (!ctx->draggedPanelId.empty()) {
             std::string pKey = "PANEL:" + ctx->draggedPanelId;
             uint32_t pId = GenerateId(pKey.c_str());
             if (ctx->panelStates.find(pId) != ctx->panelStates.end()) {
                 isPanelDockable = ctx->panelStates[pId].isDockable;
             }
        }

        if (!ctx->draggedPanelId.empty() && isPanelDockable) {
             // Check for hover
             Vec2 mouse(ctx->input.MouseX(), ctx->input.MouseY());
             if (mouse.x >= node.position.x && mouse.x <= node.position.x + node.size.x &&
                 mouse.y >= node.position.y && mouse.y <= node.position.y + node.size.y) {
                 
                 // Check if root node has content - if so, prioritize free space logic over tab docking
                 auto& rootNode = GetDockNode(ctx, ctx->rootDockNodeId);
                 bool rootHasContent = (rootNode.isLeaf() && !rootNode.dockedPanelIds.empty()) || !rootNode.isLeaf();
                 
                 ctx->hoveredDockNodeId = node.id;
                 
                 float w = node.size.x;
                 float h = node.size.y;
                 float x = mouse.x - node.position.x;
                 float y = mouse.y - node.position.y;
                 
                 if (x < w * 0.2f) {
                     ctx->dockPreviewType = 1; // Left
                     ctx->dockPreviewRectPos = node.position;
                     ctx->dockPreviewRectSize = Vec2(w * 0.5f, h);
                 } else if (x > w * 0.8f) {
                     ctx->dockPreviewType = 2; // Right
                     ctx->dockPreviewRectPos = Vec2(node.position.x + w * 0.5f, node.position.y);
                     ctx->dockPreviewRectSize = Vec2(w * 0.5f, h);
                 } else if (y < h * 0.2f) {
                     ctx->dockPreviewType = 3; // Top
                     ctx->dockPreviewRectPos = node.position;
                     ctx->dockPreviewRectSize = Vec2(w, h * 0.5f);
                 } else if (y > h * 0.8f) {
                     ctx->dockPreviewType = 4; // Bottom
                     ctx->dockPreviewRectPos = Vec2(node.position.x, node.position.y + h * 0.5f);
                     ctx->dockPreviewRectSize = Vec2(w, h * 0.5f);
                 } else {
                     // Center area - check if we should allow tab docking or let free space logic handle it
                     bool isRootNode = (node.id == ctx->rootDockNodeId);
                     
                     // If root node has content, always use free space logic (don't set dockPreviewType here)
                     // This ensures that after undocking, the system recalculates properly using free space logic
                     // If root node is empty, allow tab docking
                     // If this is a non-root node, only allow tab docking if root is empty
                     // (when root has content, we want free space logic to handle everything)
                     bool shouldUseTab = !rootHasContent && (isRootNode ? node.dockedPanelIds.empty() : true);
                     
                     if (shouldUseTab) {
                         ctx->dockPreviewType = 5; // Center (Tab)
                         ctx->dockPreviewRectPos = node.position;
                         ctx->dockPreviewRectSize = node.size;
                     }
                     // If root has content, don't set dockPreviewType here for center area
                     // Let the free space logic handle it after all nodes are rendered
                 }
                 
                 // Only show preview and handle drop if we set a dockPreviewType
                 if (ctx->dockPreviewType != 0) {
                     Color previewColor(0.2f, 0.6f, 1.0f, 0.3f);
                     ctx->renderer.DrawRectFilled(ctx->dockPreviewRectPos, ctx->dockPreviewRectSize, previewColor);
                     
                      // Handle Drop (DEFERRED)
                      if (!ctx->input.IsMouseDown(0)) {
                          // Mouse Released -> Drop!
                           std::string panelId = ctx->draggedPanelId;
                           
                           // Check if panel is dockable checks
                           bool canDock = true;
                           std::string panelKey = "PANEL:" + panelId; 
                           uint32_t pId = GenerateId(panelKey.c_str());
                           if (ctx->panelStates.find(pId) != ctx->panelStates.end()) {
                               canDock = ctx->panelStates[pId].isDockable;
                           }
                           
                           if (canDock) {
                               // Reset manuallyUndocked flag immediately when user attempts to dock
                               // This allows the panel to be docked in the next frame
                               if (ctx->panelStates.find(pId) != ctx->panelStates.end()) {
                                   ctx->panelStates[pId].manuallyUndocked = false;
                               }
                               
                               if (ctx->dockPreviewType == 5) {
                                   // Tab
                                   ctx->pendingDockAction.type = UIContext::PendingDockAction::Type::Tab;
                                   ctx->pendingDockAction.targetNodeId = node.id;
                                   ctx->pendingDockAction.panelId = panelId;
                               } else {
                                   // Split
                                   ctx->pendingDockAction.type = UIContext::PendingDockAction::Type::Split;
                                   ctx->pendingDockAction.targetNodeId = node.id;
                                   ctx->pendingDockAction.panelId = panelId;
                                   
                                   if (ctx->dockPreviewType == 1) { // Left
                                       ctx->pendingDockAction.splitDirection = DockSplitDirection::Vertical;
                                       ctx->pendingDockAction.insertBefore = true;
                                   } else if (ctx->dockPreviewType == 2) { // Right
                                       ctx->pendingDockAction.splitDirection = DockSplitDirection::Vertical;
                                       ctx->pendingDockAction.insertBefore = false;
                                   } else if (ctx->dockPreviewType == 3) { // Top
                                       ctx->pendingDockAction.splitDirection = DockSplitDirection::Horizontal;
                                       ctx->pendingDockAction.insertBefore = true;
                                   } else if (ctx->dockPreviewType == 4) { // Bottom
                                       ctx->pendingDockAction.splitDirection = DockSplitDirection::Horizontal;
                                       ctx->pendingDockAction.insertBefore = false; // Original Top (Child 0), Panel Bottom (Child 1)
                                   }
                               }
                               
                               // Clear drag state visually immediately
                               ctx->draggedPanelId = "";
                           }
                      }
                 }
             }
        }
        
        if (!node.dockedPanelIds.empty()) {
            // Render Tab Bar
            float tabBarHeight = 30.0f;
            Vec2 tabPos = node.position;
            
            // Background for tabs
            ctx->renderer.DrawRectFilled(tabPos, Vec2(node.size.x, tabBarHeight), Color(0.1f, 0.1f, 0.1f, 1.0f));
            
            float currentX = tabPos.x;
            for (int i = 0; i < node.dockedPanelIds.size(); ++i) {
                const std::string& panelId = node.dockedPanelIds[i];
                bool isActive = (i == node.activeTabIndex);
                
                 // Tab Width
                Vec2 textSize = ctx->renderer.MeasureText(panelId, 14.0f);
                float tabWidth = textSize.x + 20.0f; // 10px padding on each side
                
                Color tabColor = isActive ? Color(0.25f, 0.25f, 0.25f, 1.0f) : Color(0.15f, 0.15f, 0.15f, 1.0f);
                ctx->renderer.DrawRectFilled(Vec2(currentX, tabPos.y), Vec2(tabWidth, tabBarHeight), tabColor);
                
                // Text
                ctx->renderer.DrawText(Vec2(currentX + 10, tabPos.y + 5), panelId, Color(1,1,1,1), 14.0f);
                
                // Click to select
                Vec2 mouse(ctx->input.MouseX(), ctx->input.MouseY());
                if (ctx->input.IsMouseDown(0) && 
                    mouse.x >= currentX && mouse.x <= currentX + tabWidth &&
                    mouse.y >= tabPos.y && mouse.y <= tabPos.y + tabBarHeight) {
                    node.activeTabIndex = i;
                }
                
                currentX += tabWidth + 2.0f; // Spacing
            }
            
            // Content Area Setup
            // IMPORTANT: When `BeginPanel` is called for the ACTIVE panel, it needs to render HERE.
            // We need a way to tell `BeginPanel` "Hey, if you are Panel X, render at Position ... with Size ..."
            // We can store this in `Context::panelStates`.
            
            if (node.activeTabIndex >= 0 && node.activeTabIndex < node.dockedPanelIds.size()) {
                std::string activePanel = node.dockedPanelIds[node.activeTabIndex];
                
                Vec2 contentPos = Vec2(node.position.x, node.position.y + tabBarHeight);
                Vec2 contentSize = Vec2(node.size.x, node.size.y - tabBarHeight);
                
                // Store info for BeginPanel to pick up
                UIContext::DockedPanelInfo info;
                info.pos = contentPos;
                info.size = contentSize;
                info.isVisible = true;
                ctx->currentFrameDockedPanels[activePanel] = info;
            }
            
            // Mark inactive tabs as invisible so BeginPanel skips them
             for (int i = 0; i < node.dockedPanelIds.size(); ++i) {
                if (i == node.activeTabIndex) continue;
                std::string inactivePanel = node.dockedPanelIds[i];
                UIContext::DockedPanelInfo info;
                info.isVisible = false;
                ctx->currentFrameDockedPanels[inactivePanel] = info;
            }
        } else {
             // Empty Node - no text displayed (removed as per user request)
             // ctx->renderer.DrawText(node.position + Vec2(10, 10), "Empty Dock", Color(0.5f, 0.5f, 0.5f, 1.0f), 14.0f);
        }
    }
}
}
