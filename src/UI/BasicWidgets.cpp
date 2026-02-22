#include "UI/Widgets.h"
#include "UI/WidgetHelpers.h"
#include "Theme/FluentTheme.h"
#include "core/Animation.h"
#include "core/Context.h"
#include "core/Renderer.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <optional>

namespace FluentUI {

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

  if (!stack.isVertical) {
    // Wrong End function called - don't pop anything
    return;
  }

  ctx->layoutStack.pop_back();

  // Sacar offset local asociado AFTER type check
  if (!ctx->offsetStack.empty()) {
    ctx->offsetStack.pop_back();
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

  if (stack.isVertical) {
    // Wrong End function called - don't pop anything
    return;
  }

  ctx->layoutStack.pop_back();

  // Sacar offset local asociado AFTER type check
  if (!ctx->offsetStack.empty()) {
    ctx->offsetStack.pop_back();
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

bool Button(const std::string &label, const Vec2 &size, std::optional<Vec2> pos, bool enabled) {
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

  bool hasAbsolutePos = pos.has_value();
  Vec2 btnPos = hasAbsolutePos ? pos.value() : ctx->cursorPos;
  if (hasAbsolutePos) {
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
  bool pressed = hover && ctx->input.IsMouseDown(0);
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
    // Use button center for keyboard-triggered clicks, mouse position otherwise
    Vec2 clickPos = hasFocus && (ctx->input.IsKeyPressed(SDL_SCANCODE_RETURN) ||
                                  ctx->input.IsKeyPressed(SDL_SCANCODE_SPACE))
                        ? Vec2(btnPos.x + btnSize.x * 0.5f, btnPos.y + btnSize.y * 0.5f)
                        : Vec2(mouseX, mouseY);
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
  auto &bgAnim = ctx->colorAnimations[AnimSlot(buttonId, 0)];
  auto &fgAnim = ctx->colorAnimations[AnimSlot(buttonId, 1)];
  auto &borderAnim = ctx->colorAnimations[AnimSlot(buttonId, 2)];

  // Inicializar animaciones si es necesario (primera vez)
  if (!bgAnim.IsInitialized()) {
    bgAnim.SetImmediate(getTargetColor(buttonStyle.background));
  }
  if (!fgAnim.IsInitialized()) {
    fgAnim.SetImmediate(getTargetColor(buttonStyle.foreground));
  }
  if (!borderAnim.IsInitialized()) {
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

  // Viewport culling: skip drawing if button is completely off-screen
  if (IsRectInViewport(ctx, btnPos, btnSize)) {
    Vec2 shadowPos = btnPos + Vec2(0.0f, buttonStyle.shadowOffsetY);
    Color shadowColor(0.0f, 0.0f, 0.0f, buttonStyle.shadowOpacity);
    if (buttonStyle.shadowOpacity > 0.0f) {
      ctx->renderer.DrawRectFilled(shadowPos, btnSize, shadowColor,
                                   buttonStyle.cornerRadius);
    }

    if (hasFocus && enabled) {
      DrawFocusRing(ctx, btnPos, btnSize, buttonStyle.cornerRadius);
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

  if (IsRectInViewport(ctx, pos, finalSize)) {
    ctx->renderer.DrawText(pos, text, color, textStyle.fontSize);
  }
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
  // Use container available space if in a layout, otherwise viewport
  Vec2 available = GetCurrentAvailableSpace(ctx);
  float separatorWidth = available.x > 0.0f ? available.x
               : ctx->renderer.GetViewportSize().x - ctx->cursorPos.x * 2.0f;
  Vec2 desired(separatorWidth, separator.thickness);

  LayoutConstraints constraints = ConsumeNextConstraints();
  Vec2 finalSize = ApplyConstraints(ctx, constraints, desired);
  finalSize.y = std::max(finalSize.y, separator.thickness);

  // Resolver posición: usar cursor directamente (separator no tiene posición
  // absoluta)
  Vec2 pos = ctx->cursorPos;
  Vec2 drawPos = pos + Vec2(0.0f, separator.padding * 0.5f);
  if (IsRectInViewport(ctx, drawPos, finalSize)) {
    ctx->renderer.DrawRectFilled(drawPos, Vec2(finalSize.x, finalSize.y),
                                 separator.color, 0.0f);
  }

  Vec2 totalSize(finalSize.x, finalSize.y + separator.padding);
  RegisterOccupiedArea(ctx, pos, totalSize);

  ctx->lastItemPos = pos;
  AdvanceCursor(ctx, totalSize);
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

} // namespace FluentUI
