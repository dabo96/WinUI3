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

  const ButtonStyle &buttonStyle = ctx->GetEffectiveButtonStyle();

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
  uint32_t buttonId = GenerateId("BTN:", label.c_str());

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

  // Perf 1.2: Register animation slots for GC tracking (only widgets that use animations)
  RegisterAnimSlots(buttonId);

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

  // Perf 2.2: Notify context of active animations
  if (bgAnim.IsAnimating()) ctx->NotifyColorAnimActive(AnimSlot(buttonId, 0));
  if (fgAnim.IsAnimating()) ctx->NotifyColorAnimActive(AnimSlot(buttonId, 1));
  if (borderAnim.IsAnimating()) ctx->NotifyColorAnimActive(AnimSlot(buttonId, 2));

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
            (btnSize.y - buttonStyle.padding.y * 2.0f - textSize.y) * 0.5f - 1.0f); // Pequeño ajuste visual
    
    // Asegurar que el texto no se dibuje fuera del botón por errores de redondeo
    textPos.x = std::max(textPos.x, btnPos.x + 2.0f);
    
    ctx->renderer.DrawText(textPos, label, fgColor, buttonStyle.text.fontSize);

    // Dibujar ripple effects
    auto &ripple = ctx->rippleEffects[buttonId];
    for (const auto &r : ripple.GetRipples()) {
      ctx->renderer.DrawRipple(r.center, r.radius, r.opacity);
    }
  }

  ctx->lastItemPos = btnPos;

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
  ctx->lastItemPos = pos;
  AdvanceCursor(ctx, totalSize);
}

// ============================================================
// Grid Layout
// ============================================================

void BeginGrid(const std::string& id, int columns, float rowHeight) {
  UIContext *ctx = GetContext();
  if (!ctx)
    return;

  if (columns < 1)
    columns = 1;

  uint32_t gridId = GenerateId("GRID:", id.c_str());

  // Calculate available width from current layout or viewport
  Vec2 available = GetCurrentAvailableSpace(ctx);
  float availableWidth = available.x > 0.0f
      ? available.x
      : ctx->renderer.GetViewportSize().x - ctx->cursorPos.x * 2.0f;

  float cellWidth = availableWidth / static_cast<float>(columns);

  UIContext::GridFrameContext grid;
  grid.id = gridId;
  grid.columns = columns;
  grid.currentCell = 0;
  grid.cellWidth = cellWidth;
  grid.rowHeight = rowHeight;
  grid.gridOrigin = ctx->cursorPos;
  grid.savedCursor = ctx->cursorPos;
  grid.savedLastItemPos = ctx->lastItemPos;
  grid.savedLastItemSize = ctx->lastItemSize;
  grid.maxRowHeight = 0.0f;
  grid.totalHeight = 0.0f;

  ctx->gridStack.push_back(grid);

  // Position cursor at first cell (row 0, col 0)
  ctx->cursorPos = grid.gridOrigin;
}

void GridNextCell() {
  UIContext *ctx = GetContext();
  if (!ctx || ctx->gridStack.empty())
    return;

  auto &grid = ctx->gridStack.back();

  // Track the height of the item just placed in the current cell
  float itemBottom = ctx->lastItemPos.y + ctx->lastItemSize.y;
  float cellTopY = grid.gridOrigin.y + grid.totalHeight;
  float itemHeight = itemBottom - cellTopY;
  if (itemHeight > grid.maxRowHeight) {
    grid.maxRowHeight = itemHeight;
  }

  grid.currentCell++;

  int col = grid.currentCell % grid.columns;
  int row = grid.currentCell / grid.columns;

  // If we just wrapped to a new row, commit the previous row's height
  if (col == 0) {
    float rowH = grid.rowHeight > 0.0f ? grid.rowHeight : grid.maxRowHeight;
    if (rowH <= 0.0f) rowH = 32.0f; // fallback default
    grid.totalHeight += rowH;
    grid.maxRowHeight = 0.0f;
  }

  // Position cursor at the new cell
  ctx->cursorPos = Vec2(
      grid.gridOrigin.x + static_cast<float>(col) * grid.cellWidth,
      grid.gridOrigin.y + grid.totalHeight
  );
}

void EndGrid() {
  UIContext *ctx = GetContext();
  if (!ctx || ctx->gridStack.empty())
    return;

  auto grid = ctx->gridStack.back();
  ctx->gridStack.pop_back();

  // Account for the last row (which wasn't committed by GridNextCell wrapping)
  // Check if any cells were emitted in the current (last) row
  int totalCells = grid.currentCell + 1; // +1 because currentCell is 0-based after last GridNextCell
  int lastRowCells = totalCells % grid.columns;
  // If lastRowCells == 0, the last GridNextCell already wrapped, but we still have
  // maxRowHeight from the final row if items were placed after the last GridNextCell.
  // Actually, the last item's height needs to be captured too.
  float itemBottom = ctx->lastItemPos.y + ctx->lastItemSize.y;
  float cellTopY = grid.gridOrigin.y + grid.totalHeight;
  float itemHeight = itemBottom - cellTopY;
  if (itemHeight > grid.maxRowHeight) {
    grid.maxRowHeight = itemHeight;
  }

  float lastRowH = grid.rowHeight > 0.0f ? grid.rowHeight : grid.maxRowHeight;
  if (lastRowH <= 0.0f) lastRowH = 32.0f;
  grid.totalHeight += lastRowH;

  // Restore saved state
  ctx->lastItemPos = grid.savedLastItemPos;
  ctx->lastItemSize = grid.savedLastItemSize;

  // The grid occupies the full width and totalHeight
  Vec2 gridSize(grid.cellWidth * static_cast<float>(grid.columns), grid.totalHeight);

  ctx->cursorPos = grid.gridOrigin;
  ctx->lastItemPos = grid.gridOrigin;

  // Advance cursor past the entire grid
  AdvanceCursor(ctx, gridSize);
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

// --- Image Widget ---

void Image(const std::string& id, void* textureHandle, const Vec2& size,
           const Vec2& uv0, const Vec2& uv1, std::optional<Vec2> pos) {
  UIContext* ctx = GetContext();
  if (!ctx || !textureHandle) return;

  Vec2 desiredSize = size;
  if (desiredSize.x <= 0.0f) desiredSize.x = 64.0f;
  if (desiredSize.y <= 0.0f) desiredSize.y = 64.0f;

  LayoutConstraints constraints = ConsumeNextConstraints();
  Vec2 imgSize = ApplyConstraints(ctx, constraints, desiredSize);
  imgSize.x = std::max(imgSize.x, 1.0f);
  imgSize.y = std::max(imgSize.y, 1.0f);

  bool hasAbsolutePos = pos.has_value();
  Vec2 imgPos;
  if (hasAbsolutePos) {
    imgPos = ResolveAbsolutePosition(ctx, pos.value(), imgSize);
  } else {
    imgPos = ctx->cursorPos;
  }

  if (IsRectInViewport(ctx, imgPos, imgSize)) {
    ctx->renderer.DrawImage(imgPos, imgSize, textureHandle, uv0, uv1);
  }

  if (!hasAbsolutePos) {
    AdvanceCursor(ctx, imgSize);
  }
}

// ---------------------------------------------------------------------------
// ColorPicker widget
// ---------------------------------------------------------------------------

bool ColorPicker(const std::string &label, Color *value,
                 std::optional<Vec2> pos) {
  UIContext *ctx = GetContext();
  if (!ctx || !value)
    return false;

  // ----- Constants -----
  constexpr float SV_SIZE       = 180.0f;
  constexpr float BAR_WIDTH     = 20.0f;
  constexpr float BAR_GAP       = 6.0f;
  constexpr float PREVIEW_H     = 26.0f;
  constexpr float ROW_GAP       = 4.0f;
  constexpr float SLIDER_HEIGHT = 20.0f;
  constexpr float LABEL_W       = 20.0f;
  constexpr float INNER_PAD     = 8.0f;
  constexpr float CORNER_R      = 4.0f;

  bool showAlpha = (value->a < 0.999f);
  float totalBarWidth = BAR_WIDTH + BAR_GAP; // hue bar
  if (showAlpha) totalBarWidth += BAR_WIDTH + BAR_GAP; // alpha bar

  float widgetWidth  = SV_SIZE + BAR_GAP + totalBarWidth;
  float labelTextH   = 18.0f;
  float bottomH      = PREVIEW_H + ROW_GAP + SLIDER_HEIGHT * 3.0f + ROW_GAP * 3.0f;
  float widgetHeight = labelTextH + ROW_GAP + SV_SIZE + ROW_GAP + bottomH;

  // Layout
  Vec2 desiredSize(widgetWidth + INNER_PAD * 2.0f, widgetHeight + INNER_PAD * 2.0f);
  LayoutConstraints constraints = ConsumeNextConstraints();
  Vec2 totalSize = ApplyConstraints(ctx, constraints, desiredSize);

  bool hasAbsPos = pos.has_value();
  Vec2 widgetPos = hasAbsPos ? pos.value() : ctx->cursorPos;
  if (hasAbsPos) {
    widgetPos = ResolveAbsolutePosition(ctx, widgetPos, totalSize);
  }

  uint32_t pickerId = GenerateId("CPICK:", label.c_str());

  // Get or create state
  auto &state = ctx->colorPickerStates[pickerId];
  if (!state.initialized) {
    value->ToHSV(state.hue, state.saturation, state.value);
    state.alpha = value->a;
    state.initialized = true;
  }

  ctx->lastSeenFrame[pickerId] = ctx->frame;

  bool changed = false;

  float mx = ctx->input.MouseX();
  float my = ctx->input.MouseY();
  bool mouseDown    = ctx->input.IsMouseDown(0);
  bool mousePressed = ctx->input.IsMousePressed(0);

  // Background panel
  Color bgColor = ctx->style.panel.background;
  ctx->renderer.DrawRectFilled(widgetPos, totalSize, bgColor, CORNER_R);
  ctx->renderer.DrawRect(widgetPos, totalSize, ctx->style.panel.borderColor, CORNER_R);

  Vec2 inner = widgetPos + Vec2(INNER_PAD, INNER_PAD);

  // Label
  ctx->renderer.DrawText(inner, label, ctx->style.GetTextStyle(TypographyStyle::Body).color, 14.0f);
  float yOff = labelTextH + ROW_GAP;

  // ===== SV Square =====
  Vec2 svPos(inner.x, inner.y + yOff);
  Vec2 svSize(SV_SIZE, SV_SIZE);

  // Two overlapping gradient quads:
  // 1) White (left) -> Hue color (right)
  Color hueColor = Color::FromHSV(state.hue, 1.0f, 1.0f);
  ctx->renderer.DrawRectGradient(svPos, svSize,
      Color(1, 1, 1, 1), hueColor,
      Color(1, 1, 1, 1), hueColor);
  // 2) Transparent (top) -> Black (bottom)
  ctx->renderer.DrawRectGradient(svPos, svSize,
      Color(0, 0, 0, 0), Color(0, 0, 0, 0),
      Color(0, 0, 0, 1), Color(0, 0, 0, 1));

  ctx->renderer.DrawRect(svPos, svSize, Color(0.3f, 0.3f, 0.3f, 0.8f));

  // SV interaction
  bool hoverSV = (mx >= svPos.x && mx < svPos.x + svSize.x &&
                  my >= svPos.y && my < svPos.y + svSize.y);
  if (hoverSV && mousePressed) {
    state.draggingSV = true;
    ctx->activeWidgetId = pickerId;
  }
  if (state.draggingSV) {
    if (mouseDown) {
      state.saturation = std::clamp((mx - svPos.x) / svSize.x, 0.0f, 1.0f);
      state.value = 1.0f - std::clamp((my - svPos.y) / svSize.y, 0.0f, 1.0f);
      changed = true;
    } else {
      state.draggingSV = false;
      if (ctx->activeWidgetId == pickerId) ctx->activeWidgetId = 0;
    }
  }

  // SV cursor
  {
    float cx = svPos.x + state.saturation * svSize.x;
    float cy = svPos.y + (1.0f - state.value) * svSize.y;
    ctx->renderer.DrawCircle(Vec2(cx, cy), 6.0f, Color(0, 0, 0, 0.7f), false);
    ctx->renderer.DrawCircle(Vec2(cx, cy), 5.0f, Color(1, 1, 1, 0.9f), false);
  }

  // ===== Hue Bar =====
  Vec2 hueBarPos(svPos.x + SV_SIZE + BAR_GAP, svPos.y);
  Vec2 hueBarSize(BAR_WIDTH, SV_SIZE);

  // Draw 6 rainbow segments
  {
    float segH = hueBarSize.y / 6.0f;
    Color hueColors[7] = {
      Color::FromHSV(0,   1, 1),
      Color::FromHSV(60,  1, 1),
      Color::FromHSV(120, 1, 1),
      Color::FromHSV(180, 1, 1),
      Color::FromHSV(240, 1, 1),
      Color::FromHSV(300, 1, 1),
      Color::FromHSV(360, 1, 1),
    };
    for (int i = 0; i < 6; ++i) {
      Vec2 segPos(hueBarPos.x, hueBarPos.y + segH * i);
      Vec2 segSize(hueBarSize.x, segH + 1.0f); // +1 to avoid gaps
      ctx->renderer.DrawRectGradient(segPos, segSize,
          hueColors[i], hueColors[i],
          hueColors[i + 1], hueColors[i + 1]);
    }
  }
  ctx->renderer.DrawRect(hueBarPos, hueBarSize, Color(0.3f, 0.3f, 0.3f, 0.8f));

  // Hue interaction
  bool hoverHue = (mx >= hueBarPos.x && mx < hueBarPos.x + hueBarSize.x &&
                   my >= hueBarPos.y && my < hueBarPos.y + hueBarSize.y);
  if (hoverHue && mousePressed) {
    state.draggingHue = true;
    ctx->activeWidgetId = pickerId;
  }
  if (state.draggingHue) {
    if (mouseDown) {
      state.hue = std::clamp((my - hueBarPos.y) / hueBarSize.y, 0.0f, 1.0f) * 360.0f;
      changed = true;
    } else {
      state.draggingHue = false;
      if (ctx->activeWidgetId == pickerId) ctx->activeWidgetId = 0;
    }
  }

  // Hue cursor
  {
    float cy = hueBarPos.y + (state.hue / 360.0f) * hueBarSize.y;
    ctx->renderer.DrawRectFilled(
        Vec2(hueBarPos.x - 1.0f, cy - 2.0f),
        Vec2(hueBarSize.x + 2.0f, 4.0f),
        Color(1, 1, 1, 0.9f));
    ctx->renderer.DrawRect(
        Vec2(hueBarPos.x - 1.0f, cy - 2.0f),
        Vec2(hueBarSize.x + 2.0f, 4.0f),
        Color(0, 0, 0, 0.6f));
  }

  // ===== Alpha Bar (optional) =====
  if (showAlpha) {
    Vec2 alphaBarPos(hueBarPos.x + BAR_WIDTH + BAR_GAP, svPos.y);
    Vec2 alphaBarSize(BAR_WIDTH, SV_SIZE);

    // Checkerboard background for alpha visibility
    float checkSize = 5.0f;
    Color c1(0.4f, 0.4f, 0.4f, 1.0f);
    Color c2(0.6f, 0.6f, 0.6f, 1.0f);
    int cols = static_cast<int>(alphaBarSize.x / checkSize);
    int rows = static_cast<int>(alphaBarSize.y / checkSize);
    for (int rr = 0; rr < rows; ++rr) {
      for (int cc = 0; cc < cols; ++cc) {
        Color ck = ((rr + cc) % 2 == 0) ? c1 : c2;
        ctx->renderer.DrawRectFilled(
            Vec2(alphaBarPos.x + cc * checkSize, alphaBarPos.y + rr * checkSize),
            Vec2(checkSize, checkSize), ck);
      }
    }

    // Alpha gradient overlay
    Color alphaTop = Color::FromHSV(state.hue, state.saturation, state.value, 1.0f);
    Color alphaBot = Color::FromHSV(state.hue, state.saturation, state.value, 0.0f);
    ctx->renderer.DrawRectGradient(alphaBarPos, alphaBarSize,
        alphaTop, alphaTop, alphaBot, alphaBot);

    ctx->renderer.DrawRect(alphaBarPos, alphaBarSize, Color(0.3f, 0.3f, 0.3f, 0.8f));

    // Alpha interaction
    bool hoverAlpha = (mx >= alphaBarPos.x && mx < alphaBarPos.x + alphaBarSize.x &&
                       my >= alphaBarPos.y && my < alphaBarPos.y + alphaBarSize.y);
    if (hoverAlpha && mousePressed) {
      state.draggingAlpha = true;
      ctx->activeWidgetId = pickerId;
    }
    if (state.draggingAlpha) {
      if (mouseDown) {
        state.alpha = 1.0f - std::clamp((my - alphaBarPos.y) / alphaBarSize.y, 0.0f, 1.0f);
        changed = true;
      } else {
        state.draggingAlpha = false;
        if (ctx->activeWidgetId == pickerId) ctx->activeWidgetId = 0;
      }
    }

    // Alpha cursor
    {
      float cy = alphaBarPos.y + (1.0f - state.alpha) * alphaBarSize.y;
      ctx->renderer.DrawRectFilled(
          Vec2(alphaBarPos.x - 1.0f, cy - 2.0f),
          Vec2(alphaBarSize.x + 2.0f, 4.0f),
          Color(1, 1, 1, 0.9f));
      ctx->renderer.DrawRect(
          Vec2(alphaBarPos.x - 1.0f, cy - 2.0f),
          Vec2(alphaBarSize.x + 2.0f, 4.0f),
          Color(0, 0, 0, 0.6f));
    }
  }

  // ===== Bottom section: Preview + Hex + RGB sliders =====
  float bottomY = svPos.y + SV_SIZE + ROW_GAP;
  float bottomW = totalSize.x - INNER_PAD * 2.0f;

  Color currentColor = Color::FromHSV(state.hue, state.saturation, state.value, state.alpha);

  // -- Color Preview --
  float previewW = 50.0f;
  Vec2 previewPos(inner.x, bottomY);
  Vec2 previewSize(previewW, PREVIEW_H);

  // Checkerboard under preview for alpha
  if (showAlpha) {
    float ck = 4.0f;
    int pc = static_cast<int>(previewSize.x / ck);
    int pr = static_cast<int>(previewSize.y / ck);
    for (int rr = 0; rr < pr; ++rr)
      for (int cc = 0; cc < pc; ++cc) {
        Color ckc = ((rr + cc) % 2 == 0) ? Color(0.4f, 0.4f, 0.4f) : Color(0.6f, 0.6f, 0.6f);
        ctx->renderer.DrawRectFilled(
            Vec2(previewPos.x + cc * ck, previewPos.y + rr * ck),
            Vec2(ck, ck), ckc);
      }
  }
  ctx->renderer.DrawRectFilled(previewPos, previewSize, currentColor, 3.0f);
  ctx->renderer.DrawRect(previewPos, previewSize, Color(0.3f, 0.3f, 0.3f, 0.8f), 3.0f);

  // -- Hex input --
  {
    char hexBuf[10];
    int ri = static_cast<int>(std::clamp(currentColor.r, 0.0f, 1.0f) * 255.0f + 0.5f);
    int gi = static_cast<int>(std::clamp(currentColor.g, 0.0f, 1.0f) * 255.0f + 0.5f);
    int bi = static_cast<int>(std::clamp(currentColor.b, 0.0f, 1.0f) * 255.0f + 0.5f);
    std::snprintf(hexBuf, sizeof(hexBuf), "#%02X%02X%02X", ri, gi, bi);

    if (!state.editingHex) {
      state.hexText = hexBuf;
    }

    float hexX = inner.x + previewW + BAR_GAP;
    float hexW = bottomW - previewW - BAR_GAP;
    Vec2 hexPos(hexX, bottomY);
    Vec2 hexSize(hexW, PREVIEW_H);

    Color hexBg = Color(0.1f, 0.1f, 0.1f, 0.6f);
    bool hoverHex = (mx >= hexPos.x && mx < hexPos.x + hexSize.x &&
                     my >= hexPos.y && my < hexPos.y + hexSize.y);

    if (hoverHex) {
      ctx->desiredCursor = UIContext::CursorType::IBeam;
      hexBg = Color(0.15f, 0.15f, 0.15f, 0.7f);
    }

    ctx->renderer.DrawRectFilled(hexPos, hexSize, hexBg, 3.0f);
    ctx->renderer.DrawRect(hexPos, hexSize, Color(0.3f, 0.3f, 0.3f, 0.5f), 3.0f);

    if (hoverHex && mousePressed) {
      state.editingHex = true;
      ctx->focusedWidgetId = pickerId;
    }

    // Confirm on click outside
    if (state.editingHex && mousePressed && !hoverHex) {
      Color parsed = Color::FromHex(state.hexText.c_str());
      parsed.a = state.alpha;
      parsed.ToHSV(state.hue, state.saturation, state.value);
      state.editingHex = false;
      changed = true;
    }

    // Keyboard handling when editing hex
    if (state.editingHex) {
      if (ctx->input.IsKeyPressed(SDL_SCANCODE_RETURN) ||
          ctx->input.IsKeyPressed(SDL_SCANCODE_KP_ENTER)) {
        Color parsed = Color::FromHex(state.hexText.c_str());
        parsed.a = state.alpha;
        parsed.ToHSV(state.hue, state.saturation, state.value);
        state.editingHex = false;
        changed = true;
      } else if (ctx->input.IsKeyPressed(SDL_SCANCODE_ESCAPE)) {
        state.editingHex = false;
      } else if (ctx->input.IsKeyPressed(SDL_SCANCODE_BACKSPACE)) {
        if (!state.hexText.empty())
          state.hexText.pop_back();
      } else {
        // Hex character input (0-9, a-f)
        for (int sc = SDL_SCANCODE_A; sc <= SDL_SCANCODE_F; ++sc) {
          if (ctx->input.IsKeyPressed(static_cast<SDL_Scancode>(sc)) &&
              state.hexText.size() < 7) {
            bool shift = ctx->input.IsKeyDown(SDL_SCANCODE_LSHIFT) ||
                         ctx->input.IsKeyDown(SDL_SCANCODE_RSHIFT);
            char ch = shift ? ('A' + (sc - SDL_SCANCODE_A)) : ('a' + (sc - SDL_SCANCODE_A));
            state.hexText += ch;
          }
        }
        for (int sc = SDL_SCANCODE_0; sc <= SDL_SCANCODE_9; ++sc) {
          if (ctx->input.IsKeyPressed(static_cast<SDL_Scancode>(sc)) &&
              state.hexText.size() < 7) {
            state.hexText += ('0' + (sc - SDL_SCANCODE_0));
          }
        }
      }

      // Cursor blink
      float blinkPhase = std::fmod(ctx->time * 2.0f, 2.0f);
      if (blinkPhase < 1.0f) {
        Vec2 txtSz = MeasureTextCached(ctx, state.hexText, 13.0f);
        float curX = hexPos.x + 4.0f + txtSz.x;
        ctx->renderer.DrawRectFilled(
            Vec2(curX, hexPos.y + 3.0f),
            Vec2(1.0f, hexSize.y - 6.0f),
            ctx->style.GetTextStyle(TypographyStyle::Body).color);
      }

      // Focused border
      ctx->renderer.DrawRect(hexPos, hexSize, Color(0.2f, 0.5f, 0.9f, 0.8f), 3.0f);
    }

    ctx->renderer.DrawText(
        Vec2(hexPos.x + 4.0f, hexPos.y + (hexSize.y - 13.0f) * 0.5f),
        state.hexText, ctx->style.GetTextStyle(TypographyStyle::Body).color, 13.0f);
  }

  // -- RGB Sliders --
  float sliderY = bottomY + PREVIEW_H + ROW_GAP;
  float sliderW = bottomW - LABEL_W - 4.0f;

  auto drawColorSlider = [&](const char* lbl, float &comp, float y,
                             const Color &leftCol, const Color &rightCol) -> bool {
    bool slChanged = false;
    Vec2 labelPos(inner.x, y + (SLIDER_HEIGHT - 12.0f) * 0.5f);
    ctx->renderer.DrawText(labelPos, lbl, ctx->style.GetTextStyle(TypographyStyle::Body).color, 12.0f);

    Vec2 slPos(inner.x + LABEL_W + 4.0f, y);
    Vec2 slSize(sliderW, SLIDER_HEIGHT);

    // Track gradient
    Vec2 trackPos(slPos.x, slPos.y + (SLIDER_HEIGHT - 8.0f) * 0.5f);
    Vec2 trackSize(slSize.x, 8.0f);
    ctx->renderer.DrawRectGradient(trackPos, trackSize,
        leftCol, rightCol, leftCol, rightCol);
    ctx->renderer.DrawRect(trackPos, trackSize, Color(0.3f, 0.3f, 0.3f, 0.5f));

    // Thumb
    float thumbX = slPos.x + comp * slSize.x;
    ctx->renderer.DrawCircle(Vec2(thumbX, slPos.y + SLIDER_HEIGHT * 0.5f),
                             6.0f, Color(1, 1, 1, 0.95f), true);
    ctx->renderer.DrawCircle(Vec2(thumbX, slPos.y + SLIDER_HEIGHT * 0.5f),
                             6.0f, Color(0, 0, 0, 0.5f), false);

    // Interaction with a per-channel unique ID
    uint32_t slId = GenerateId("CPSL:", lbl, label.c_str());
    bool hoverSl = (mx >= slPos.x && mx < slPos.x + slSize.x &&
                    my >= slPos.y && my < slPos.y + slSize.y);
    if (hoverSl && mousePressed) {
      ctx->activeWidgetId = slId;
    }
    if (ctx->activeWidgetId == slId) {
      if (mouseDown) {
        float newVal = std::clamp((mx - slPos.x) / slSize.x, 0.0f, 1.0f);
        if (newVal != comp) {
          comp = newVal;
          slChanged = true;
        }
      } else {
        ctx->activeWidgetId = 0;
      }
    }

    // Value text
    char valBuf[8];
    std::snprintf(valBuf, sizeof(valBuf), "%d", static_cast<int>(comp * 255.0f + 0.5f));
    Vec2 valSz = MeasureTextCached(ctx, valBuf, 11.0f);
    ctx->renderer.DrawText(
        Vec2(slPos.x + slSize.x - valSz.x, y + (SLIDER_HEIGHT - 11.0f) * 0.5f),
        valBuf, Color(0.7f, 0.7f, 0.7f), 11.0f);

    return slChanged;
  };

  float rComp = currentColor.r;
  float gComp = currentColor.g;
  float bComp = currentColor.b;

  bool rgbChanged = false;
  rgbChanged |= drawColorSlider("R", rComp, sliderY,
      Color(0, gComp, bComp), Color(1, gComp, bComp));
  sliderY += SLIDER_HEIGHT + ROW_GAP;
  rgbChanged |= drawColorSlider("G", gComp, sliderY,
      Color(rComp, 0, bComp), Color(rComp, 1, bComp));
  sliderY += SLIDER_HEIGHT + ROW_GAP;
  rgbChanged |= drawColorSlider("B", bComp, sliderY,
      Color(rComp, gComp, 0), Color(rComp, gComp, 1));

  if (rgbChanged) {
    Color newRgb(rComp, gComp, bComp);
    newRgb.ToHSV(state.hue, state.saturation, state.value);
    changed = true;
  }

  // Write back final color
  if (changed) {
    *value = Color::FromHSV(state.hue, state.saturation, state.value, state.alpha);
  }

  // Advance cursor in layout
  if (!hasAbsPos) {
    AdvanceCursor(ctx, totalSize);
  }

  return changed;
}

} // namespace FluentUI
