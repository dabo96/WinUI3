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

namespace FluentUI {
// Static variable to store layout constraints for the next widget
static std::optional<LayoutConstraints> nextConstraints;

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
    parentAvailable = explicitSize.value();
  }

  Vec2 contentAvailable(std::max(0.0f, parentAvailable.x - padding.x * 2.0f),
                        std::max(0.0f, parentAvailable.y - padding.y * 2.0f));
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
  Vec2 desired(ctx->renderer.GetViewportSize().x - ctx->cursorPos.x * 2.0f,
               separator.thickness);

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

bool BeginPanel(const std::string &id, const Vec2 &desiredSize,
                bool reserveLayoutSpace, std::optional<bool> useAcrylic,
                std::optional<float> acrylicOpacity, const Vec2 &pos) {
  UIContext *ctx = GetContext();
  if (!ctx)
    return false;

  std::string key = "PANEL:" + id;
  uint32_t panelId = GenerateId(key.c_str());
  auto &state = ctx->panelStates[panelId];
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
      state.useAbsolutePos = false;
    }

    // Configurar acrylic: usar parámetros si se proporcionan, sino usar el
    // estilo global
    const PanelStyle &panelStyle = ctx->style.panel;
    state.useAcrylic =
        useAcrylic.has_value() ? useAcrylic.value() : panelStyle.useAcrylic;
    state.acrylicOpacity = acrylicOpacity.has_value()
                               ? acrylicOpacity.value()
                               : panelStyle.acrylicOpacity;

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

    // IMPORTANTE: Actualizar posición del panel en cada frame si NO usa posición absoluta
    // Esto permite que el panel se mueva con el scroll de contenedores padre (TabView, ScrollView)
    // La posición es: cursor del layout + offset del drag del usuario
    if (!state.useAbsolutePos && !state.dragging && !state.resizing) {
      state.position = ctx->cursorPos + state.dragPositionOffset;
    }
  }

  const PanelStyle &panelStyle = ctx->style.panel;
  float titleHeight =
      panelStyle.headerText.fontSize + panelStyle.padding.y * 2.0f;

  if (reserveLayoutSpace) {
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

  Vec2 minimizeButtonSize(titleHeight - panelStyle.padding.y * 1.5f,
                          titleHeight - panelStyle.padding.y * 1.5f);
  Vec2 minimizeButtonPos(state.position.x + state.size.x -
                             minimizeButtonSize.x - panelStyle.padding.x,
                         state.position.y +
                             (titleHeight - minimizeButtonSize.y) * 0.5f);

  if (leftPressed &&
      PointInRect(mousePos, minimizeButtonPos, minimizeButtonSize)) {
    state.minimized = !state.minimized;
    state.dragging = false;
    state.resizing = false;
  }

  // Manejar drag
  if (state.dragging) {
    if (!leftDown) {
      state.dragging = false;
      // Guardar el offset de la posición arrastrada relativo al cursor del layout
      if (!state.useAbsolutePos) {
        state.dragPositionOffset = state.position - ctx->cursorPos;
        state.hasBeenDragged = true;
      }
    } else {
      state.position = Vec2(mousePos.x - state.dragOffset.x,
                            mousePos.y - state.dragOffset.y);
    }
  }

  // Manejar resize
  if (state.resizing) {
    if (!leftDown) {
      state.resizing = false;
    } else {
      Vec2 delta(mousePos.x - state.resizeStartMouse.x,
                 mousePos.y - state.resizeStartMouse.y);
      Vec2 newSize = Vec2(state.resizeStartSize.x + delta.x,
                          state.resizeStartSize.y + delta.y);
      float minWidth = 120.0f;
      float minHeight = titleHeight + 40.0f;
      state.size.x = std::max(newSize.x, minWidth);
      state.size.y = std::max(newSize.y, minHeight);
    }
  }

  // Iniciar drag o resize
  if (!state.dragging && !state.resizing) {
    if (leftPressed && PointInRect(mousePos, state.position, titleSize)) {
      state.dragging = true;
      state.dragOffset =
          Vec2(mousePos.x - state.position.x, mousePos.y - state.position.y);
    } else if (!state.minimized) {
      Vec2 resizeHandleSize(12.0f, 12.0f);
      Vec2 resizeHandlePos(state.position.x + state.size.x - resizeHandleSize.x,
                           state.position.y + state.size.y -
                               resizeHandleSize.y);
      if (leftPressed &&
          PointInRect(mousePos, resizeHandlePos, resizeHandleSize)) {
        state.resizing = true;
        state.resizeStartMouse = mousePos;
        state.resizeStartSize = state.size;
      }
    }
  }

  Vec2 viewport = ctx->renderer.GetViewportSize();
  float maxPosX = std::max(0.0f, viewport.x - state.size.x);
  float maxPosY = std::max(
      0.0f, viewport.y - (state.minimized ? titleHeight : state.size.y));
  state.position.x = std::clamp(state.position.x, 0.0f, maxPosX);
  state.position.y = std::clamp(state.position.y, 0.0f, maxPosY);
  state.size.x = std::clamp(state.size.x, 100.0f, viewport.x);
  state.size.y = std::clamp(state.size.y, titleHeight, viewport.y);

  // Registrar área ocupada del panel ANTES de dibujarlo para que otros widgets
  // lo vean
  Vec2 panelSize =
      state.minimized ? Vec2(state.size.x, titleHeight) : state.size;
  RegisterOccupiedArea(ctx, state.position, panelSize);

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

  ctx->renderer.DrawRectFilled(state.position, titleSize, titleColor,
                               panelStyle.cornerRadius);
  // Sin borde visible - solo contraste de fondo como en Windows 11 Settings

  Vec2 titleTextPos(state.position.x + panelStyle.padding.x,
                    state.position.y +
                        (titleHeight - panelStyle.headerText.fontSize) * 0.5f);
  ctx->renderer.DrawText(titleTextPos, id, panelStyle.headerText.color,
                         panelStyle.headerText.fontSize);

  bool minimizeHover =
      PointInRect(mousePos, minimizeButtonPos, minimizeButtonSize);
  bool buttonPressed = minimizeHover && ctx->input.IsMouseDown(0);
  bool panelMinimized = state.minimized;
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
    Vec2 clipPos(state.position.x + panelStyle.padding.x,
                 state.position.y + titleHeight + panelStyle.padding.y);
    Vec2 clipSize(state.size.x - panelStyle.padding.x * 2.0f,
                  state.size.y - titleHeight - panelStyle.padding.y * 2.0f);
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
    float contentHeight = std::max(0.0f, state.size.y - titleHeight -
                                             panelStyle.padding.y * 2.0f);
    Vec2 contentOrigin(state.position.x + panelStyle.padding.x,
                       state.position.y + titleHeight + panelStyle.padding.y);

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

  // Sacar offset del área de contenido (similar a EndScrollView)
  if (!ctx->offsetStack.empty()) {
    ctx->offsetStack.pop_back();
  }

  // Cerrar el layout vertical si fue creado
  if (frameCtx.layoutPushed) {
    EndVertical(false); // No avanzar el cursor del padre aquí
  }

  if (frameCtx.clipPushed) {
    ctx->renderer.PopClipRect();
  }

  auto &state = it->second;
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
    
    // Set lastItemPos to panel position before advancing
    ctx->lastItemPos = state.position;
    AdvanceCursor(ctx, advanceSize);
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
  Color boxFill = panelStyle.background;
  if (hover) {
    boxFill = panelStyle.headerBackground;
  }
  // Hacer el fondo más distintivo según el tema para mejor visibilidad (sin borde)
  bool isDarkTheme = (ctx->style.backgroundColor.r < 0.5f);
  if (isDarkTheme) {
    boxFill = Color(boxFill.r * 1.15f, boxFill.g * 1.15f, boxFill.b * 1.15f, 1.0f);
  } else {
    boxFill = Color(boxFill.r * 0.92f, boxFill.g * 0.92f, boxFill.b * 0.92f, 1.0f);
  }
  
  // Relleno sin borde - solo contraste de fondo
  ctx->renderer.DrawRectFilled(boxPos, boxSize, boxFill,
                               panelStyle.cornerRadius * 0.5f);

  if (currentValue) {
    Vec2 innerPos(boxPos.x + 4.0f, boxPos.y + 4.0f);
    Vec2 innerSize(boxSize.x - 8.0f, boxSize.y - 8.0f);
    Color fillColor = hover ? toggleAccent.hover : toggleAccent.normal;
    ctx->renderer.DrawRectFilled(innerPos, innerSize, fillColor,
                                 panelStyle.cornerRadius * 0.3f);
  }

  Vec2 textPos(boxPos.x + boxSize.x + 8.0f,
               boxPos.y + (boxSize.y - textSize.y) * 0.5f);
  ctx->renderer.DrawText(textPos, label, textStyle.color, textStyle.fontSize);

  RegisterOccupiedArea(ctx, boxPos, layoutSize);

  ctx->lastItemPos = boxPos;
  AdvanceCursor(ctx, layoutSize);
  return toggled;
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
  Color circleFill = panelStyle.background;
  if (hover) {
    circleFill = panelStyle.headerBackground;
  }
  // Hacer el fondo más distintivo según el tema para mejor visibilidad (sin borde)
  bool isDarkTheme = (ctx->style.backgroundColor.r < 0.5f);
  if (isDarkTheme) {
    circleFill = Color(circleFill.r * 1.15f, circleFill.g * 1.15f, circleFill.b * 1.15f, 1.0f);
  } else {
    circleFill = Color(circleFill.r * 0.92f, circleFill.g * 0.92f, circleFill.b * 0.92f, 1.0f);
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
    desiredSize.y = 20.0f;

  fraction = std::clamp(fraction, 0.0f, 1.0f);

  LayoutConstraints constraints = ConsumeNextConstraints();
  Vec2 barSize = ApplyConstraints(ctx, constraints, desiredSize);
  barSize.x = std::max(barSize.x, 1.0f);
  barSize.y = std::max(barSize.y, 4.0f);

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

  ctx->renderer.DrawRectFilled(barPos, barSize, panelStyle.background,
                               panelStyle.cornerRadius);

  Vec2 fillSize(barSize.x * fraction, barSize.y);
  ctx->renderer.DrawRectFilled(barPos, fillSize, accentState.normal,
                               panelStyle.cornerRadius);

  if (!overlay.empty()) {
    Vec2 textSize = ctx->renderer.MeasureText(overlay, captionStyle.fontSize);
    Vec2 textPos(barPos.x + (barSize.x - textSize.x) * 0.5f,
                 barPos.y + (barSize.y - textSize.y) * 0.5f);
    ctx->renderer.DrawText(textPos, overlay, captionStyle.color,
                           captionStyle.fontSize);
  }

  RegisterOccupiedArea(ctx, barPos, barSize);

  ctx->lastItemPos = barPos;
  AdvanceCursor(ctx, barSize);
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
  Color bgColor = panelStyle.background;
  bool isDarkTheme = (ctx->style.backgroundColor.r < 0.5f);
  if (isDarkTheme) {
    bgColor = Color(bgColor.r * 1.12f, bgColor.g * 1.12f, bgColor.b * 1.12f, 1.0f);
  } else {
    bgColor = Color(bgColor.r * 0.94f, bgColor.g * 0.94f, bgColor.b * 0.94f, 1.0f);
  }
  
  // Solo mostrar borde cuando tiene focus
  if (hasFocus) {
    Color focusColor = FluentColors::Accent;
    focusColor.a = 0.4f;
    Vec2 focusOffset(1.5f, 1.5f);
    ctx->renderer.DrawRectFilled(fieldPos - focusOffset,
                                 fieldSize + focusOffset * 2.0f, focusColor,
                                 panelStyle.cornerRadius + 1.5f);
    bgColor = accentState.hover;
    bgColor.a = 0.15f;
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
    ctx->renderer.DrawRectFilled(caretPos, caretSize, accentState.normal, 0.0f);
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

  int selectedIndex = currentItem ? *currentItem : 0;
  selectedIndex =
      std::clamp(selectedIndex, 0, static_cast<int>(items.size() - 1));
  std::string selectedText = items[selectedIndex];

  float labelSpacing = 4.0f;
  const TextStyle &labelStyle =
      ctx->style.GetTextStyle(TypographyStyle::Subtitle);
  const TextStyle &itemStyle = ctx->style.GetTextStyle(TypographyStyle::Body);
  const PanelStyle &panelStyle = ctx->style.panel;
  const ColorState &accentState = ctx->style.button.background;

  Vec2 labelSize = ctx->renderer.MeasureText(label, labelStyle.fontSize);
  float fieldHeight = itemStyle.fontSize + panelStyle.padding.y * 2.0f;
  Vec2 fieldSize(width, fieldHeight);

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
  }

  float mouseX = ctx->input.MouseX();
  float mouseY = ctx->input.MouseY();
  bool hoverField =
      (mouseX >= fieldPos.x && mouseX <= fieldPos.x + fieldSize.x &&
       mouseY >= fieldPos.y && mouseY <= fieldPos.y + fieldSize.y);

  // Estado del dropdown
  auto boolEntry = ctx->boolStates.try_emplace(id, false);
  bool isOpen = boolEntry.first->second;

  bool clicked = hoverField && ctx->input.IsMousePressed(0);
  if (clicked) {
    isOpen = !isOpen;
  }

  // Cerrar si se hace click fuera
  if (isOpen && ctx->input.IsMousePressed(0) && !hoverField) {
    // Verificar si el click está fuera del dropdown también
    float dropdownHeight =
        std::min(static_cast<float>(items.size()) * fieldHeight, 200.0f);
    bool hoverDropdown =
        (mouseX >= fieldPos.x && mouseX <= fieldPos.x + fieldSize.x &&
         mouseY >= fieldPos.y + fieldSize.y &&
         mouseY <= fieldPos.y + fieldSize.y + dropdownHeight);
    if (!hoverDropdown) {
      isOpen = false;
    }
  }

  // También activar con Enter/Space cuando tiene focus
  if (hasFocus && (ctx->input.IsKeyPressed(SDL_SCANCODE_RETURN) ||
                   ctx->input.IsKeyPressed(SDL_SCANCODE_SPACE))) {
    isOpen = !isOpen;
  }

  // Actualizar estado
  boolEntry.first->second = isOpen;

  // Dibujar campo con fondo más distintivo - sin borde visible
  Color fieldBg = panelStyle.background;
  if (hoverField) {
    fieldBg = panelStyle.headerBackground;
  }
  // Hacer el fondo más distintivo según el tema (sin borde)
  bool isDarkTheme = (ctx->style.backgroundColor.r < 0.5f);
  if (isDarkTheme) {
    fieldBg = Color(fieldBg.r * 1.12f, fieldBg.g * 1.12f, fieldBg.b * 1.12f, 1.0f);
  } else {
    fieldBg = Color(fieldBg.r * 0.94f, fieldBg.g * 0.94f, fieldBg.b * 0.94f, 1.0f);
  }
  
  // Solo mostrar borde cuando tiene focus
  if (hasFocus) {
    Color focusColor = FluentColors::Accent;
    focusColor.a = 0.4f;
    Vec2 focusOffset(1.5f, 1.5f);
    ctx->renderer.DrawRectFilled(fieldPos - focusOffset,
                                 fieldSize + focusOffset * 2.0f, focusColor,
                                 panelStyle.cornerRadius + 1.5f);
    fieldBg = accentState.hover;
    fieldBg.a = 0.15f;
  }
  ctx->renderer.DrawRectFilled(fieldPos, fieldSize, fieldBg,
                               panelStyle.cornerRadius);

  // Texto seleccionado
  Vec2 textPadding(panelStyle.padding.x, panelStyle.padding.y);
  Vec2 textPos(fieldPos.x + textPadding.x, fieldPos.y + textPadding.y);
  ctx->renderer.DrawText(textPos, selectedText, itemStyle.color,
                         itemStyle.fontSize);

  // Flecha dropdown
  float arrowSize = 8.0f;
  Vec2 arrowPos(fieldPos.x + fieldSize.x - arrowSize - textPadding.x,
                fieldPos.y + (fieldSize.y - arrowSize) * 0.5f);
  // Dibujar triángulo simple (usando líneas)
  Vec2 arrowTop(arrowPos.x + arrowSize * 0.5f, arrowPos.y);
  Vec2 arrowBottom(arrowPos.x, arrowPos.y + arrowSize);
  Vec2 arrowBottom2(arrowPos.x + arrowSize, arrowPos.y + arrowSize);
  ctx->renderer.DrawLine(arrowTop, arrowBottom, itemStyle.color, 1.5f);
  ctx->renderer.DrawLine(arrowTop, arrowBottom2, itemStyle.color, 1.5f);
  ctx->renderer.DrawLine(arrowBottom, arrowBottom2, itemStyle.color, 1.5f);

  // Dibujar label
  ctx->renderer.DrawText(widgetPos, label, labelStyle.color,
                         labelStyle.fontSize);

  bool valueChanged = false;

  // Queue dropdown for deferred rendering (to ensure it appears on top)
  if (isOpen) {
    float dropdownHeight =
        std::min(static_cast<float>(items.size()) * fieldHeight, 200.0f);
    
    UIContext::DeferredComboDropdown dropdown;
    dropdown.fieldPos = fieldPos;
    dropdown.fieldSize = fieldSize;
    dropdown.dropdownPos = Vec2(fieldPos.x, fieldPos.y + fieldSize.y);
    dropdown.dropdownSize = Vec2(fieldSize.x, dropdownHeight);
    dropdown.items = items;
    dropdown.selectedIndex = selectedIndex;
    dropdown.comboId = id;
    dropdown.currentItemPtr = currentItem;
    dropdown.fieldHeight = fieldHeight;
    
    ctx->deferredComboDropdowns.push_back(dropdown);
  }

  // Update open state
  boolEntry.first->second = isOpen;

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
    state.viewSize = size;
  }

  // Si se proporciona scrollOffset externo, usarlo y actualizar el estado
  if (scrollOffset) {
    state.scrollOffset = *scrollOffset;
  }

  float scrollbarWidth = 12.0f;

  // Resolver posición
  Vec2 scrollViewPos;
  Vec2 fullSize(size.x, size.y);
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
  ctx->renderer.DrawRectFilled(scrollViewPos, size, scrollBg,
                               panelStyle.cornerRadius);

  // Calcular área de contenido (reservar espacio para scrollbar)
  Vec2 availableSize(size.x - scrollbarWidth, size.y - scrollbarWidth);
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
  frameCtx.size = size;
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

  // Cerrar el layout vertical y obtener el tamaño del contenido
  if (frameCtx.layoutPushed && !ctx->layoutStack.empty()) {
    LayoutStack &layout = ctx->layoutStack.back();
    // El contentSize es el tamaño total del contenido en el layout
    state.contentSize = layout.contentSize;
    EndVertical(false); // No avanzar el cursor del padre aquí
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
  bool needsVerticalScrollbar = state.contentSize.y > frameCtx.contentAreaSize.y;
  bool needsHorizontalScrollbar = state.contentSize.x > frameCtx.contentAreaSize.x;

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
  if (tabViewSize.x <= 0.0f) {
    tabViewSize.x = std::max(tabBarWidth + panelStyle.padding.x * 2.0f, 300.0f);
  }
  if (tabViewSize.y <= 0.0f) {
    tabViewSize.y = 300.0f;
  }

  // Resolver posición: usar pos si se proporciona, sino usar cursor
  Vec2 tabViewPos;
  if (pos.x != 0.0f || pos.y != 0.0f) {
    tabViewPos = pos;
    state.useAbsolutePos = true;
    state.absolutePos = pos;
  } else {
    tabViewPos = ctx->cursorPos;
    tabViewPos = ResolveAbsolutePosition(ctx, tabViewPos, tabViewSize);
    state.useAbsolutePos = false;
  }
  // Registrar área ocupada solo si NO se usa posición absoluta
  if (!state.useAbsolutePos) {
    RegisterOccupiedArea(ctx, tabViewPos, tabViewSize);
  }

  state.tabBarSize = Vec2(tabViewSize.x, tabHeight);

  // Configurar área de contenido PRIMERO (debajo de la barra de pestañas)
  // Usar un padding más pequeño dentro del TabView para evitar problemas de
  // clipping
  float innerPadding = 8.0f;
  Vec2 contentPos(tabViewPos.x + innerPadding,
                  tabViewPos.y + tabHeight + innerPadding);
  Vec2 contentSize(tabViewSize.x - innerPadding * 2.0f,
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

  // Dibujar pestañas
  float currentX = tabBarPos.x + panelStyle.padding.x;
  float mouseX = ctx->input.MouseX();
  float mouseY = ctx->input.MouseY();
  bool leftPressed = ctx->input.IsMousePressed(0);

  for (size_t i = 0; i < tabLabels.size(); ++i) {
    Vec2 labelSize =
        ctx->renderer.MeasureText(tabLabels[i], tabTextStyle.fontSize);
    Vec2 tabSize(labelSize.x + panelStyle.padding.x * 2.0f, tabHeight);
    Vec2 tabPos(currentX, tabBarPos.y);

    bool hover = (mouseX >= tabPos.x && mouseX <= tabPos.x + tabSize.x &&
                  mouseY >= tabPos.y && mouseY <= tabPos.y + tabSize.y);
    bool isActive = (static_cast<int>(i) == state.activeTab);

    // Dibujar fondo de la pestaña - sin borde, solo contraste
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
    ctx->renderer.DrawRectFilled(tabPos, tabSize, tabBg, 0.0f);

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

  // IMPORTANTE: Flush batch aquí para asegurar que el fondo del TabView se
  // renderice ANTES de aplicar el clipping y dibujar los widgets
  ctx->renderer.FlushBatch();


  // Configurar cursor para el contenido de la pestaña activa
  ctx->cursorPos = contentPos;
  ctx->lastItemPos = contentPos;
  ctx->lastItemSize = Vec2(0.0f, 0.0f);

  // Configurar scroll del contenido
  state.viewSize = contentSize;

  // Manejar interacción del mouse con scrollbar ANTES de dibujar contenido
  // (usando contentSize del frame anterior)
  bool leftDown = ctx->input.IsMouseDown(0);

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
  
  // Ajustar cursor con scroll actual (vertical)
  Vec2 scrolledContentPos =
      Vec2(contentPos.x, contentPos.y - tabScrollOffset.y);

  // Aplicar clipping al área de contenido ANTES de configurar el layout
  // Esto asegura que los widgets solo se dibujen dentro del área visible
  ctx->renderer.PushClipRect(contentPos, contentSize);

  // Iniciar layout vertical para el contenido con padding cero explícito
  // Esto debe hacerse DESPUÉS del clipping
  ctx->cursorPos = scrolledContentPos;
  ctx->lastItemPos = scrolledContentPos;
  ctx->lastItemSize = Vec2(0.0f, 0.0f);
  BeginVertical(ctx->style.spacing, std::make_optional(contentSize),
                std::make_optional(Vec2(0.0f, 0.0f)));

  // Guardar frame del tab para calcular tamaño de contenido al cerrar
  ctx->tabFrameStack.push_back(
      TabContentFrame{tabViewId, contentPos, contentSize, ctx->cursorPos});

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
  if (!ctx->layoutStack.empty()) {
    EndVertical(false);
  }

  // Tomar frame guardado
  if (!ctx->tabFrameStack.empty()) {
    TabContentFrame frame = ctx->tabFrameStack.back();
    ctx->tabFrameStack.pop_back();

    // Calcular tamaño de contenido real
    Vec2 contentSize;
    contentSize.x =
        frame.contentAreaSize.x; // por ahora igual al ancho del área
    contentSize.y = std::max(frame.contentAreaSize.y,
                             contentEndCursor.y - frame.contentStartCursor.y);

    // Actualizar estado del TabView
    UIContext *ctx2 = GetContext();
    if (ctx2) {
      auto it = ctx2->tabViewStates.find(frame.tabViewId);
      if (it != ctx2->tabViewStates.end()) {
        auto &st = it->second;
        st.contentSize = contentSize;

        // Manejar input para scroll vertical
        float mouseX = ctx2->input.MouseX();
        float mouseY = ctx2->input.MouseY();
        bool hoverContent =
            (mouseX >= frame.contentAreaPos.x &&
             mouseX <= frame.contentAreaPos.x + frame.contentAreaSize.x &&
             mouseY >= frame.contentAreaPos.y &&
             mouseY <= frame.contentAreaPos.y + frame.contentAreaSize.y);
        float wheelY = ctx2->input.MouseWheelY();
        if (hoverContent && std::abs(wheelY) > 0.001f &&
            contentSize.y > frame.contentAreaSize.y && !scrollConsumedThisFrame) {
          float scrollSpeed = 30.0f;
          // Usar scrollOffset del tab activo
          Vec2& tabScrollOffset = st.tabScrollOffsets[st.activeTab];
          tabScrollOffset.y = std::clamp(
              tabScrollOffset.y - wheelY * scrollSpeed, 0.0f,
              std::max(0.0f, contentSize.y - frame.contentAreaSize.y));
          scrollConsumedThisFrame = true; // Marcar como consumido
        }

        // Dibujar barra de scroll vertical si es necesaria
        if (contentSize.y > frame.contentAreaSize.y) {
          float scrollbarWidth = 10.0f;
          Vec2 barPos(frame.contentAreaPos.x + frame.contentAreaSize.x -
                          scrollbarWidth,
                      frame.contentAreaPos.y);
          Vec2 barSize(scrollbarWidth, frame.contentAreaSize.y);
          ctx2->renderer.DrawRectFilled(barPos, barSize,
                                        ctx2->style.panel.background, 0.0f);

          float ratio = frame.contentAreaSize.y / contentSize.y;
          float thumbHeight = std::max(20.0f, barSize.y * ratio);
          float maxThumbTravel = barSize.y - thumbHeight;
          // Usar scrollOffset del tab activo
          Vec2& tabScrollOffset = st.tabScrollOffsets[st.activeTab];
          float thumbY = (contentSize.y - frame.contentAreaSize.y) > 0.0f
                             ? (tabScrollOffset.y /
                                (contentSize.y - frame.contentAreaSize.y)) *
                                   maxThumbTravel
                             : 0.0f;
          Vec2 thumbPos(barPos.x, barPos.y + thumbY);
          Vec2 thumbSize(scrollbarWidth, thumbHeight);
          Color thumbColor = ctx2->style.button.background.normal;
          ctx2->renderer.DrawRectFilled(thumbPos, thumbSize, thumbColor, 4.0f);
        }
      }
    }
  }

  // Remover clipping al final
  ctx->renderer.PopClipRect();

  // Restaurar cursor después del TabView
  // Si se usa posición absoluta, NO restaurar/avanzar el cursor
  // EndVertical(false) ya no avanza el cursor del layout padre, así que está
  // bien Si necesitáramos verificar, podríamos usar el frame.tabViewId antes de
  // hacer pop
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

    // Dibujar tooltip con elevation para que destaque
    ctx->renderer.DrawRectWithElevation(tooltip.position, tooltipSize,
                                        panelStyle.background, 4.0f, 4.0f);
    ctx->renderer.DrawRect(tooltip.position, tooltipSize,
                           panelStyle.borderColor, 4.0f);

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
      itemBg = ctx->style.button.background.normal;
    } else if (hover) {
      itemBg = panelStyle.headerBackground;
    }
    ctx->renderer.DrawRectFilled(itemPos, itemSize, itemBg, 0.0f);

    // Dibujar indicador de selección
    if (isSelected) {
      Vec2 indicatorPos(itemPos.x, itemPos.y);
      Vec2 indicatorSize(3.0f, itemSize.y);
      ctx->renderer.DrawRectFilled(indicatorPos, indicatorSize,
                                   ctx->style.button.background.hover, 0.0f);
    }

    // Dibujar texto del item
    Vec2 textPos(itemPos.x + panelStyle.padding.x,
                 itemPos.y + (state.itemSize.y - textStyle.fontSize) * 0.5f);
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

// Variable estática para mantener el nivel de indentación actual del TreeView
static int g_treeViewDepth = 0;
static uint32_t g_currentTreeViewId = 0;

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
    // Más claro que el fondo para mejor contraste
    treeBg = Color(treeBg.r * 1.15f, treeBg.g * 1.15f, treeBg.b * 1.15f, 1.0f);
  } else {
    // Más oscuro que el fondo para mejor contraste
    treeBg = Color(treeBg.r * 0.92f, treeBg.g * 0.92f, treeBg.b * 0.92f, 1.0f);
  }
  ctx->renderer.DrawRectFilled(treeViewPos, treeViewSize, treeBg,
                               panelStyle.cornerRadius);

  // Aplicar clipping
  ctx->renderer.PushClipRect(treeViewPos, treeViewSize);

  // Configurar cursor para el contenido del TreeView
  ctx->cursorPos = treeViewPos + panelStyle.padding;
  ctx->lastItemPos = ctx->cursorPos;
  g_treeViewDepth = 0;

  // Iniciar layout vertical para apilar los nodos correctamente
  Vec2 contentSize(treeViewSize.x - panelStyle.padding.x * 2.0f,
                   treeViewSize.y - panelStyle.padding.y * 2.0f);
  BeginVertical(ctx->style.spacing, Vec2(contentSize.x, 0.0f), Vec2(0.0f, 0.0f));

  return true;
}

void EndTreeView() {
  UIContext *ctx = GetContext();
  if (!ctx)
    return;

  // Cerrar el layout vertical
  if (!ctx->layoutStack.empty()) {
    EndVertical(false); // No avanzar el cursor del padre aquí
  }

  // Remover clipping
  ctx->renderer.PopClipRect();

  // Obtener el tamaño final del contenido
  if (g_currentTreeViewId != 0) {
    auto it = ctx->treeViewStates.find(g_currentTreeViewId);
    if (it != ctx->treeViewStates.end()) {
      const auto &state = it->second;

      // Avanzar cursor solo si NO se usa posición absoluta
      if (!state.useAbsolutePos) {
        Vec2 treeViewPos = ctx->lastItemPos - ctx->style.panel.padding;
        ctx->cursorPos = treeViewPos + Vec2(0.0f, 200.0f); // Altura estimada
        ctx->lastItemPos = treeViewPos;
        AdvanceCursor(ctx, Vec2(state.itemSize.x, 200.0f));
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
  bool nodeIsSelected = isSelected ? *isSelected : false;

  Vec2 itemSize(state.itemSize.x, state.itemSize.y);
  Vec2 itemPos = ctx->cursorPos;

  // Calcular indentación
  float indentX = g_treeViewDepth * state.indentSize;
  itemPos.x += indentX;

  float mouseX = ctx->input.MouseX();
  float mouseY = ctx->input.MouseY();
  bool hover = (mouseX >= itemPos.x && mouseX <= itemPos.x + itemSize.x &&
                mouseY >= itemPos.y && mouseY <= itemPos.y + itemSize.y);
  bool clicked = hover && ctx->input.IsMousePressed(0);

  // Determinar si tiene hijos (simplificado: si isOpen != nullptr, asumimos que
  // puede tener hijos)
  bool hasChildren = (isOpen != nullptr);

  // Dibujar fondo del item
  Color itemBg = panelStyle.background;
  if (nodeIsSelected) {
    itemBg = ctx->style.button.background.normal;
  } else if (hover) {
    itemBg = panelStyle.headerBackground;
  }
  ctx->renderer.DrawRectFilled(itemPos, itemSize, itemBg, 0.0f);

  // Dibujar indicador de selección
  if (nodeIsSelected) {
    Vec2 indicatorPos(itemPos.x, itemPos.y);
    Vec2 indicatorSize(3.0f, itemSize.y);
    ctx->renderer.DrawRectFilled(indicatorPos, indicatorSize,
                                 ctx->style.button.background.hover, 0.0f);
  }

  // Dibujar botón de expand/collapse si tiene hijos
  float buttonX = itemPos.x;
  float buttonY = itemPos.y + (itemSize.y - state.expandButtonSize) * 0.5f;
  Vec2 buttonPos(buttonX, buttonY);
  Vec2 buttonSize(state.expandButtonSize, state.expandButtonSize);

  if (hasChildren) {
    bool hoverButton =
        (mouseX >= buttonPos.x && mouseX <= buttonPos.x + buttonSize.x &&
         mouseY >= buttonPos.y && mouseY <= buttonPos.y + buttonSize.y);

    // Dibujar botón de expand/collapse
    Color buttonBg =
        hoverButton ? panelStyle.headerBackground : panelStyle.background;
    ctx->renderer.DrawRectFilled(buttonPos, buttonSize, buttonBg, 2.0f);
    ctx->renderer.DrawRect(buttonPos, buttonSize, panelStyle.borderColor, 2.0f);

    // Dibujar símbolo + o -
    Vec2 center(buttonPos.x + buttonSize.x * 0.5f,
                buttonPos.y + buttonSize.y * 0.5f);
    float lineLength = buttonSize.x * 0.4f;

    // Línea horizontal
    Vec2 hStart(center.x - lineLength * 0.5f, center.y);
    Vec2 hEnd(center.x + lineLength * 0.5f, center.y);
    ctx->renderer.DrawLine(hStart, hEnd, textStyle.color, 2.0f);

    // Línea vertical solo si está cerrado
    if (!nodeIsOpen) {
      Vec2 vStart(center.x, center.y - lineLength * 0.5f);
      Vec2 vEnd(center.x, center.y + lineLength * 0.5f);
      ctx->renderer.DrawLine(vStart, vEnd, textStyle.color, 2.0f);
    }

    // Manejar click en el botón
    if (hoverButton && ctx->input.IsMousePressed(0)) {
      nodeIsOpen = !nodeIsOpen;
      ctx->treeNodeStates[nodeKey] = nodeIsOpen;
      if (isOpen) {
        *isOpen = nodeIsOpen;
      }
    }
  }

  // Dibujar texto del nodo
  float textX =
      itemPos.x + (hasChildren ? state.expandButtonSize + 4.0f : 0.0f) + 4.0f;
  Vec2 textPos(textX, itemPos.y + (itemSize.y - textStyle.fontSize) * 0.5f);
  ctx->renderer.DrawText(textPos, label, textStyle.color, textStyle.fontSize);

  // Manejar click en el item (selección)
  if (hover && clicked && !hasChildren) {
    // Si no tiene hijos, el click selecciona
    nodeIsSelected = true;
    if (isSelected) {
      *isSelected = true;
    }
  } else if (hover && clicked && hasChildren) {
    // Si tiene hijos y click fuera del botón, también selecciona
    if (mouseX > buttonPos.x + buttonSize.x + 4.0f) {
      nodeIsSelected = true;
      if (isSelected) {
        *isSelected = true;
      }
    }
  }

  RegisterOccupiedArea(ctx, itemPos, itemSize);
  AdvanceCursor(ctx, itemSize);

  // Actualizar el estado guardado
  if (isOpen) {
    ctx->treeNodeStates[nodeKey] = *isOpen;
  }

  // Devolver true si el nodo está abierto (útil para mostrar hijos)
  return nodeIsOpen && hasChildren;
}

void TreeNodePush() { g_treeViewDepth++; }

void TreeNodePop() {
  if (g_treeViewDepth > 0) {
    g_treeViewDepth--;
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

bool BeginModal(const std::string &id, const std::string &title, bool *open,
                const Vec2 &size) {
  UIContext *ctx = GetContext();
  if (!ctx)
    return false;

  std::string key = "MODAL:" + id;
  uint32_t modalId = GenerateId(key.c_str());
  auto &state = ctx->modalStates[modalId];

  // Si open es nullptr o está en false, no mostrar el modal
  if (!open || !*open) {
    state.open = false;
    return false;
  }

  if (!state.initialized) {
    Vec2 viewport = ctx->renderer.GetViewportSize();
    state.position =
        Vec2((viewport.x - size.x) * 0.5f, (viewport.y - size.y) * 0.5f);
    state.size = size;
    state.initialized = true;
  } else {
    state.size = size;
  }

  state.open = *open;

  Vec2 viewport = ctx->renderer.GetViewportSize();
  float titleHeight =
      ctx->style.panel.headerText.fontSize + ctx->style.panel.padding.y * 2.0f;
  Vec2 mousePos(ctx->input.MouseX(), ctx->input.MouseY());
  bool leftPressed = ctx->input.IsMousePressed(0);
  bool leftDown = ctx->input.IsMouseDown(0);

  // Dibujar overlay oscuro detrás del modal (backdrop)
  Color overlayColor(0.0f, 0.0f, 0.0f, 0.5f);
  ctx->renderer.DrawRectFilled(Vec2(0, 0), viewport, overlayColor, 0.0f);

  // Manejar drag del título
  Vec2 titleSize(state.size.x, titleHeight);
  if (leftPressed && PointInRect(mousePos, state.position, titleSize)) {
    state.dragging = true;
    state.dragOffset =
        Vec2(mousePos.x - state.position.x, mousePos.y - state.position.y);
  }

  if (state.dragging) {
    if (!leftDown) {
      state.dragging = false;
    } else {
      state.position = Vec2(mousePos.x - state.dragOffset.x,
                            mousePos.y - state.dragOffset.y);

      // Mantener el modal dentro de la ventana
      state.position.x =
          std::clamp(state.position.x, 0.0f, viewport.x - state.size.x);
      state.position.y =
          std::clamp(state.position.y, 0.0f, viewport.y - state.size.y);
    }
  }

  const PanelStyle &panelStyle = ctx->style.panel;

  // Dibujar modal con elevation elevada
  ctx->renderer.DrawRectWithElevation(state.position, state.size,
                                      panelStyle.background,
                                      panelStyle.cornerRadius, 16.0f);
  ctx->renderer.DrawRect(state.position, state.size, panelStyle.borderColor,
                         panelStyle.cornerRadius);

  // Dibujar título
  Vec2 titleBgPos = state.position;
  Vec2 titleBgSize(state.size.x, titleHeight);
  ctx->renderer.DrawRectFilled(titleBgPos, titleBgSize,
                               panelStyle.headerBackground,
                               panelStyle.cornerRadius);

  // Dibujar botón de cerrar (X)
  float closeButtonSize = titleHeight - panelStyle.padding.y * 1.5f;
  Vec2 closeButtonPos(
      state.position.x + state.size.x - closeButtonSize - panelStyle.padding.x,
      state.position.y + (titleHeight - closeButtonSize) * 0.5f);
  Vec2 closeButtonSizeVec(closeButtonSize, closeButtonSize);

  bool hoverClose = PointInRect(mousePos, closeButtonPos, closeButtonSizeVec);
  bool pressedClose = hoverClose && ctx->input.IsMouseDown(0);

  Color closeButtonColor = pressedClose ? panelStyle.titleButton.pressed
                           : hoverClose ? panelStyle.titleButton.hover
                                        : panelStyle.titleButton.normal;
  ctx->renderer.DrawRectFilled(closeButtonPos, closeButtonSizeVec,
                               closeButtonColor,
                               panelStyle.cornerRadius * 0.4f);

  // Dibujar X en el botón de cerrar
  float xSize = closeButtonSize * 0.3f;
  Vec2 xCenter(closeButtonPos.x + closeButtonSize * 0.5f,
               closeButtonPos.y + closeButtonSize * 0.5f);
  Vec2 xStart1(xCenter.x - xSize, xCenter.y - xSize);
  Vec2 xEnd1(xCenter.x + xSize, xCenter.y + xSize);
  Vec2 xStart2(xCenter.x - xSize, xCenter.y + xSize);
  Vec2 xEnd2(xCenter.x + xSize, xCenter.y - xSize);
  ctx->renderer.DrawLine(xStart1, xEnd1, Color(1.0f, 1.0f, 1.0f, 1.0f), 2.0f);
  ctx->renderer.DrawLine(xStart2, xEnd2, Color(1.0f, 1.0f, 1.0f, 1.0f), 2.0f);

  // Manejar click en el botón de cerrar
  if (hoverClose && ctx->input.IsMousePressed(0)) {
    *open = false;
    state.open = false;
  }

  // Dibujar texto del título
  Vec2 titleTextPos(state.position.x + panelStyle.padding.x,
                    state.position.y +
                        (titleHeight - panelStyle.headerText.fontSize) * 0.5f);
  ctx->renderer.DrawText(titleTextPos, title, panelStyle.headerText.color,
                         panelStyle.headerText.fontSize);

  // Configurar área de contenido
  Vec2 contentPos(state.position.x + panelStyle.padding.x,
                  state.position.y + titleHeight + panelStyle.padding.y);
  Vec2 contentSize(state.size.x - panelStyle.padding.x * 2.0f,
                   state.size.y - titleHeight - panelStyle.padding.y * 2.0f);

  // Aplicar clipping al área de contenido
  ctx->renderer.PushClipRect(contentPos, contentSize);

  // Configurar cursor para el contenido del modal
  ctx->cursorPos = contentPos;
  ctx->lastItemPos = contentPos;
  ctx->lastItemSize = Vec2(0.0f, 0.0f);

  // Iniciar layout vertical para el contenido
  BeginVertical(ctx->style.spacing, contentSize, Vec2(0.0f, 0.0f));

  // Manejar tecla Escape para cerrar
  if (ctx->input.IsKeyPressed(SDL_SCANCODE_ESCAPE)) {
    *open = false;
    state.open = false;
  }

  return true;
}

void EndModal() {
  UIContext *ctx = GetContext();
  if (!ctx)
    return;

  // Cerrar el layout vertical
  if (!ctx->layoutStack.empty()) {
    EndVertical(false);
  }


  // Remover clipping
  ctx->renderer.PopClipRect();
}

// Render all deferred ComboBox dropdowns
// This should be called AFTER all widgets but BEFORE Render()
void RenderDeferredDropdowns() {
  UIContext *ctx = GetContext();
  if (!ctx) return;

  const PanelStyle &panelStyle = ctx->style.panel;
  const TextStyle &itemStyle = ctx->style.GetTextStyle(TypographyStyle::Body);
  const ColorState &accentState = ctx->style.button.background;

  float mouseX = ctx->input.MouseX();
  float mouseY = ctx->input.MouseY();

  // Render each queued dropdown
  for (auto &dropdown : ctx->deferredComboDropdowns) {
    Vec2 dropdownPos = dropdown.dropdownPos;
    Vec2 dropdownSize = dropdown.dropdownSize;
    float fieldHeight = dropdown.fieldHeight;

    // Flush batch before drawing dropdown to ensure it's on top
    ctx->renderer.FlushBatch();

    // Draw dropdown background with elevation
    ctx->renderer.DrawRectWithElevation(dropdownPos, dropdownSize,
                                        panelStyle.background,
                                        panelStyle.cornerRadius, 16.0f);

    // Use acrylic effect for dropdown (Fluent Design)
    ctx->renderer.DrawRectAcrylic(dropdownPos, dropdownSize,
                                  panelStyle.background,
                                  panelStyle.cornerRadius, 0.95f);

    // Draw border
    Color dropdownBorder = FluentColors::BorderDark;
    dropdownBorder.a = 0.8f;
    ctx->renderer.DrawRect(dropdownPos, dropdownSize, dropdownBorder,
                           panelStyle.cornerRadius);

    // Draw dropdown items
    bool itemClicked = false;
    int clickedIndex = -1;

    for (size_t i = 0; i < dropdown.items.size(); ++i) {
      Vec2 itemPos(dropdownPos.x,
                   dropdownPos.y + static_cast<float>(i) * fieldHeight);
      Vec2 itemSize(dropdownSize.x, fieldHeight);

      bool hoverItem =
          (mouseX >= itemPos.x && mouseX <= itemPos.x + itemSize.x &&
           mouseY >= itemPos.y && mouseY <= itemPos.y + itemSize.y);

      // Highlight hovered item
      if (hoverItem) {
        ctx->renderer.DrawRectFilled(itemPos, itemSize, accentState.hover, 0.0f);
      }

      // Draw selection indicator for current item
      if (static_cast<int>(i) == dropdown.selectedIndex) {
        Vec2 indicatorPos(itemPos.x + 4.0f,
                          itemPos.y + (itemSize.y - 8.0f) * 0.5f);
        ctx->renderer.DrawCircle(
            Vec2(indicatorPos.x + 4.0f, indicatorPos.y + 4.0f), 4.0f,
            accentState.normal, true);
      }

      // Draw item text
      Vec2 textPadding(panelStyle.padding.x, panelStyle.padding.y);
      Vec2 itemTextPos(itemPos.x + textPadding.x + 20.0f,
                       itemPos.y + textPadding.y);
      ctx->renderer.DrawText(itemTextPos, dropdown.items[i], itemStyle.color,
                             itemStyle.fontSize);

      // Handle item click
      if (hoverItem && ctx->input.IsMousePressed(0)) {
        itemClicked = true;
        clickedIndex = static_cast<int>(i);
      }
    }

    // Update selection if item was clicked
    if (itemClicked && dropdown.currentItemPtr) {
      *dropdown.currentItemPtr = clickedIndex;
      // Close the dropdown by setting its state to false
      ctx->boolStates[dropdown.comboId] = false;
    }
  }

  // Clear the deferred ComboBox dropdowns queue
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

} // namespace FluentUI
