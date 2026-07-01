#include <SDL3/SDL.h>
#include "UI/Widgets.h"
#include "UI/WidgetHelpers.h"
#include "UI/Icons.h"
#include "Theme/FluentTheme.h"
#include "core/Animation.h"
#include "core/Context.h"
#include "core/Renderer.h"
#include "core/Elevation.h"
#include "core/WidgetNode.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <optional>

namespace FluentUI {

// UTF-8 helpers for correct codepoint-aware editing
static size_t Utf8PrevCodepoint(const std::string& s, size_t pos) {
  if (pos == 0) return 0;
  pos--;
  while (pos > 0 && (static_cast<unsigned char>(s[pos]) & 0xC0) == 0x80) {
    pos--;
  }
  return pos;
}

static size_t Utf8NextCodepoint(const std::string& s, size_t pos) {
  if (pos >= s.size()) return s.size();
  unsigned char c = static_cast<unsigned char>(s[pos]);
  size_t len = 1;
  if ((c & 0x80) == 0) len = 1;
  else if ((c & 0xE0) == 0xC0) len = 2;
  else if ((c & 0xF0) == 0xE0) len = 3;
  else if ((c & 0xF8) == 0xF0) len = 4;
  size_t next = pos + len;
  return (next > s.size()) ? s.size() : next;
}

bool Checkbox(const std::string &label, uint32_t iconCodepoint, bool *value, std::optional<Vec2> pos);

bool Checkbox(const std::string &label, bool *value, std::optional<Vec2> pos) {
  return Checkbox(label, 0u, value, pos);
}

bool Checkbox(const std::string &label, uint32_t iconCodepoint, bool *value, std::optional<Vec2> pos) {
  UIContext *ctx = GetContext();
  if (!ctx)
    return false;

  // Fluent 2 spec: medium checkbox = 16×16px, borderRadiusSmall = 2px,
  // border 1px (thin), check stroke ~1.5px, fill = brand accent when checked.
  Vec2 boxSize(S(16.0f), S(16.0f));
  float boxRadius = S(2.0f);
  const TextStyle &textStyle = ctx->style.GetTextStyle(TypographyStyle::Body);
  Vec2 textSize = MeasureTextCached(ctx, label, textStyle.fontSize);
  float trailingIconSize = textStyle.fontSize;
  float trailingIconSlot = (iconCodepoint != 0u) ? (S(6.0f) + trailingIconSize) : 0.0f;
  Vec2 totalSize(boxSize.x + S(8.0f) + textSize.x + trailingIconSlot,
                 std::max(boxSize.y, std::max(textSize.y, trailingIconSize)));
  LayoutConstraints constraints = ConsumeNextConstraints(SizeConstraint::Fill);
  Vec2 layoutSize = ApplyConstraints(ctx, constraints, totalSize);

  Vec2 widgetPos = pos.has_value()
                       ? ResolveAbsolutePosition(ctx, pos.value(), layoutSize)
                       : ctx->cursorPos;
  // Center the 16px box vertically within the line height
  Vec2 boxPos(widgetPos.x,
              widgetPos.y + (layoutSize.y - boxSize.y) * 0.5f);

  uint32_t id = GenerateId("CHK:", label.c_str());

  // Phase F1: register as focusable
  ctx->focusableWidgets.push_back(id);

  bool& boolSlot = ctx->GetWidgetState(id).boolVal; // brief 22 (fase 3)
  bool currentValue = value ? *value : boolSlot;

  // Hit area = caja + gap + label (contenido real), NO el ancho rellenado por
  // Fill, para no dejar seleccionable la fila entera a la derecha.
  Vec2 hitSize(totalSize.x, layoutSize.y);
  bool hover = IsMouseOver(ctx, widgetPos, hitSize);

  bool toggled = false;
  if (hover && ctx->input.IsMousePressed(0)) {
    currentValue = !currentValue;
    toggled = true;
  }
  // Phase F1: keyboard activation (Space / Enter when focused)
  if (ctx->focusedWidgetId == id &&
      (ctx->input.IsKeyPressed(UIKey::Space) ||
       ctx->input.IsKeyPressed(UIKey::Enter))) {
    currentValue = !currentValue;
    toggled = true;
  }

  if (value)
    *value = currentValue;
  else
    boolSlot = currentValue;

  if (toggled) {
    std::string idStr = "CHK:" + label;
    auto cbIt = ctx->valueChangedCallbacks.find(idStr);
    if (cbIt != ctx->valueChangedCallbacks.end()) cbIt->second(idStr, value);
  }

  const PanelStyle &panelStyle = ctx->style.panel;
  Color accent = ctx->style.accentColor;
  Color accentHover(std::min(1.0f, accent.r * 1.15f),
                    std::min(1.0f, accent.g * 1.15f),
                    std::min(1.0f, accent.b * 1.15f), accent.a);

  if (ctx->focusedWidgetId == id) {
    DrawFocusRing(ctx, boxPos, boxSize, boxRadius);
  }

  if (currentValue) {
    // Checked: filled with accent (brand) color
    Color fillColor = hover ? accentHover : accent;
    ctx->renderer.DrawRectFilled(boxPos, boxSize, fillColor, boxRadius);
    // Draw checkmark (two strokes, 1.5px each)
    float cs = boxSize.x; // 16
    Vec2 p1(boxPos.x + cs * 0.22f, boxPos.y + cs * 0.52f);
    Vec2 p2(boxPos.x + cs * 0.42f, boxPos.y + cs * 0.72f);
    Vec2 p3(boxPos.x + cs * 0.78f, boxPos.y + cs * 0.30f);
    Color checkColor(1.0f, 1.0f, 1.0f, 1.0f);
    ctx->renderer.DrawLine(p1, p2, checkColor, S(1.5f));
    ctx->renderer.DrawLine(p2, p3, checkColor, S(1.5f));
  } else {
    // Unchecked: transparent fill + 1px border, hover lightens border
    Color borderCol = hover ? ctx->style.label.text.color
                            : Color(panelStyle.borderColor.r,
                                    panelStyle.borderColor.g,
                                    panelStyle.borderColor.b, 0.85f);
    ctx->renderer.DrawRect(boxPos, boxSize, borderCol, boxRadius);
  }

  Vec2 textPos(boxPos.x + boxSize.x + S(8.0f),
               widgetPos.y + (layoutSize.y - textSize.y) * 0.5f);
  ctx->renderer.DrawText(textPos, label, textStyle.color, textStyle.fontSize);

  if (iconCodepoint != 0u) {
    DrawWidgetIcon(ctx, widgetPos, layoutSize, iconCodepoint, textStyle.color,
                   trailingIconSize,
                   (textPos.x - widgetPos.x) + textSize.x + S(6.0f), 0.0f);
  }

  ctx->lastItemPos = widgetPos;
  AdvanceCursor(ctx, layoutSize);

  bool focused = (ctx->focusedWidgetId == id);
  // Phase B1: checkbox is "active" while the mouse is held over it (toggle on press).
  bool active = hover && ctx->input.IsMouseDown(0);
  SetLastItem(id, widgetPos, widgetPos + hitSize, hover, active, focused, toggled);
  return toggled;
}

bool RadioButton(const std::string &label, uint32_t iconCodepoint, int *value, int optionValue,
                 const std::string &group, std::optional<Vec2> pos);

bool RadioButton(const std::string &label, int *value, int optionValue,
                 const std::string &group, std::optional<Vec2> pos) {
  return RadioButton(label, 0u, value, optionValue, group, pos);
}

bool RadioButton(const std::string &label, uint32_t iconCodepoint, int *value, int optionValue,
                 const std::string &group, std::optional<Vec2> pos) {
  UIContext *ctx = GetContext();
  if (!ctx)
    return false;

  Vec2 circleSize(20.0f, 20.0f);
  const TextStyle &textStyle = ctx->style.GetTextStyle(TypographyStyle::Body);
  Vec2 textSize = MeasureTextCached(ctx,label, textStyle.fontSize);
  float trailingIconSize = textStyle.fontSize;
  float trailingIconSlot = (iconCodepoint != 0u) ? (6.0f + trailingIconSize) : 0.0f;
  Vec2 totalSize(circleSize.x + 8.0f + textSize.x + trailingIconSlot,
                 std::max(circleSize.y, std::max(textSize.y, trailingIconSize)));
  LayoutConstraints constraints = ConsumeNextConstraints(SizeConstraint::Fill);
  Vec2 layoutSize = ApplyConstraints(ctx, constraints, totalSize);

  // Resolver posición: usar pos si se proporciona, sino usar cursor
  // directamente
  Vec2 circlePos;
  if (pos.has_value()) {
    circlePos = ResolveAbsolutePosition(ctx, pos.value(), layoutSize);
  } else {
    circlePos = ctx->cursorPos;
  }
  Vec2 circleCenter(circlePos.x + circleSize.x * 0.5f,
                    circlePos.y + circleSize.y * 0.5f);
  float radius = circleSize.x * 0.5f;

  const char* groupKey = group.empty() ? "DEFAULT_RADIO_GROUP:" : group.c_str();
  uint32_t id = GenerateId("RADIO:", groupKey, label.c_str());

  // Phase F1: register as focusable
  ctx->focusableWidgets.push_back(id);

  // Obtener valor actual del grupo
  int currentValue = value ? *value : 0;
  bool isSelected = (currentValue == optionValue);

  // Hit area = círculo + gap + label (contenido real), NO el ancho rellenado por
  // Fill (layoutSize.x ocupa todo el contenedor, lo que dejaba seleccionable la
  // fila entera a la derecha aunque no hubiera nada que clicar).
  Vec2 hitSize(totalSize.x, layoutSize.y);
  bool hover = IsMouseOver(ctx, circlePos, hitSize);

  bool clicked = false;
  if (hover && ctx->input.IsMousePressed(0)) {
    if (value) {
      *value = optionValue;
    }
    clicked = true;
  }
  // Phase F1: keyboard activation (Space / Enter when focused)
  if (ctx->focusedWidgetId == id &&
      (ctx->input.IsKeyPressed(UIKey::Space) ||
       ctx->input.IsKeyPressed(UIKey::Enter))) {
    if (value) *value = optionValue;
    clicked = true;
  }

  const PanelStyle &panelStyle = ctx->style.panel;
  Color accent = ctx->style.accentColor;
  Color accentHover(std::min(1.0f, accent.r * 1.15f),
                    std::min(1.0f, accent.g * 1.15f),
                    std::min(1.0f, accent.b * 1.15f), accent.a);

  // Focus ring for keyboard navigation
  if (ctx->focusedWidgetId == id) {
    DrawFocusRing(ctx, circlePos, circleSize, radius);
  }

  // Estilo consistente con el Checkbox: el fondo del contenedor suele coincidir
  // con el del radio, así que un relleno sólo no contrasta — se dibuja un borde
  // (anillo) visible cuando no está seleccionado, y un disco de acento con punto
  // blanco al centro cuando lo está.
  if (isSelected) {
    Color fillColor = hover ? accentHover : accent;
    ctx->renderer.DrawCircle(circleCenter, radius, fillColor, true);
    ctx->renderer.DrawCircle(circleCenter, radius * 0.40f,
                             Color(1.0f, 1.0f, 1.0f, 1.0f), true);
  } else {
    // Hover: relleno sutil para dar feedback de afordancia.
    if (hover) {
      Color hoverFill = AdjustContainerBackground(panelStyle.headerBackground,
                                                  ctx->style.isDarkTheme);
      ctx->renderer.DrawCircle(circleCenter, radius, hoverFill, true);
    }
    // Borde (anillo) — mismo criterio de color que el borde del Checkbox.
    Color borderCol = hover ? ctx->style.label.text.color
                            : Color(panelStyle.borderColor.r,
                                    panelStyle.borderColor.g,
                                    panelStyle.borderColor.b, 0.85f);
    ctx->renderer.DrawCircle(circleCenter, radius, borderCol, false);
  }

  Vec2 textPos(circlePos.x + circleSize.x + 8.0f,
               circlePos.y + (circleSize.y - textSize.y) * 0.5f);
  ctx->renderer.DrawText(textPos, label, textStyle.color, textStyle.fontSize);

  if (iconCodepoint != 0u) {
    DrawWidgetIcon(ctx, circlePos, layoutSize, iconCodepoint, textStyle.color,
                   trailingIconSize,
                   (textPos.x - circlePos.x) + textSize.x + 6.0f, 0.0f);
  }

  ctx->lastItemPos = circlePos;
  AdvanceCursor(ctx, layoutSize);
  // Phase B1: publish radio state
  {
    bool active = hover && ctx->input.IsMouseDown(0);
    SetLastItem(id, circlePos, circlePos + hitSize,
                hover, active, ctx->focusedWidgetId == id, clicked);
  }
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
                 std::optional<Vec2> pos) {
  UIContext *ctx = GetContext();
  if (!ctx)
    return false;

  // Fluent 2 spec: track = 4px thick (rail), thumb = 20px diameter (medium).
  // Hit area is full 20px height so the slider is easy to grab.
  float labelSpacing = 6.0f;
  float hitHeight = 20.0f;
  float trackThickness = 4.0f;
  float thumbRadius = 10.0f;  // 20px diameter
  const TextStyle &labelStyle = ctx->style.GetTextStyle(TypographyStyle::Body);
  const TextStyle &valueStyle =
      ctx->style.GetTextStyle(TypographyStyle::Caption);
  const PanelStyle &panelStyle = ctx->style.panel;

  Vec2 labelSize = MeasureTextCached(ctx, label, labelStyle.fontSize);
  float trackWidth = width > 0.0f ? width : 200.0f;
  Vec2 trackSize(trackWidth, hitHeight);  // hit/layout size, visual track is thinner

  // Calcular tamaño total primero
  Vec2 totalSize(trackSize.x + 100.0f,
                 labelSize.y + labelSpacing + trackSize.y);
  LayoutConstraints constraints = ConsumeNextConstraints(SizeConstraint::Fill);
  Vec2 finalSize = ApplyConstraints(ctx, constraints, totalSize);

  // Resolver posición: usar pos si se proporciona, sino usar cursor
  // directamente
  Vec2 widgetPos;
  if (pos.has_value()) {
    widgetPos = ResolveAbsolutePosition(ctx, pos.value(), finalSize);
  } else {
    widgetPos = ctx->cursorPos;
  }
  Vec2 trackPos(widgetPos.x, widgetPos.y + labelSize.y + labelSpacing);

  uint32_t id = GenerateId("SLDR_F:", label.c_str());

  // Phase F1: register as focusable
  ctx->focusableWidgets.push_back(id);

  bool floatFresh = ctx->widgetStates.find(id) == ctx->widgetStates.end(); // brief 22 (fase 3)
  float& floatSlot = ctx->GetWidgetState(id).floatVal;
  if (floatFresh) floatSlot = minValue; // preserva default try_emplace(id, minValue)
  float currentValue = value ? *value : floatSlot;
  currentValue = std::clamp(currentValue, minValue, maxValue);

  // Phase F1: keyboard adjust (Left/Right arrows when focused)
  bool kbChanged = false;
  if (ctx->focusedWidgetId == id) {
    float step = (maxValue - minValue) / 100.0f;
    if (step <= 0.0f) step = 0.01f;
    bool shiftKb = ctx->input.ShiftDown();
    if (shiftKb) step *= 10.0f;
    if (ctx->input.IsKeyPressed(UIKey::Left)) {
      currentValue = std::clamp(currentValue - step, minValue, maxValue);
      kbChanged = true;
    } else if (ctx->input.IsKeyPressed(UIKey::Right)) {
      currentValue = std::clamp(currentValue + step, minValue, maxValue);
      kbChanged = true;
    } else if (ctx->input.IsKeyPressed(UIKey::Home)) {
      currentValue = minValue;
      kbChanged = true;
    } else if (ctx->input.IsKeyPressed(UIKey::End)) {
      currentValue = maxValue;
      kbChanged = true;
    }
  }

  // Pre-calculate the value text to determine final track width BEFORE interaction
  if (!format) {
    format = "%.2f";
  }
  char valueBuffer[64];
  std::snprintf(valueBuffer, sizeof(valueBuffer), format, currentValue);
  std::string valueText(valueBuffer);
  Vec2 valueSize = MeasureTextCached(ctx, valueText, valueStyle.fontSize);
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

  float mouseX = ctx->input.MouseX();
  bool hover = IsMouseOver(ctx, trackPos, trackSize);

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
    floatSlot = currentValue;

  // Invoke valueChanged callback if slider moved
  if (valueChanged) {
    std::string idStr = "SLDR_F:" + label;
    auto cbIt = ctx->valueChangedCallbacks.find(idStr);
    if (cbIt != ctx->valueChangedCallbacks.end()) cbIt->second(idStr, value);
  }

  // Recalculate value text after interaction may have changed it
  std::snprintf(valueBuffer, sizeof(valueBuffer), format, currentValue);
  valueText = valueBuffer;
  valueSize = MeasureTextCached(ctx, valueText, valueStyle.fontSize);
  valuePos = Vec2(trackPos.x + trackSize.x + 8.0f,
                  trackPos.y + (trackSize.y - valueSize.y) * 0.5f);

  float fraction = CalculateSliderFraction(currentValue, minValue, maxValue);

  // Focus ring for keyboard navigation
  if (ctx->focusedWidgetId == id) {
    DrawFocusRing(ctx, trackPos, trackSize, S(thumbRadius));
  }

  // Draw the rail (4px thick, centered vertically in hit area)
  float trackY = trackPos.y + (trackSize.y - S(trackThickness)) * 0.5f;
  Vec2 railPos(trackPos.x, trackY);
  Vec2 railSize(trackSize.x, S(trackThickness));
  Color railColor = panelStyle.headerBackground;
  ctx->renderer.DrawRectFilled(railPos, railSize, railColor,
                               S(trackThickness) * 0.5f);

  // Filled portion (brand accent) — sliderFillColor only used if alpha>0
  Color sliderFill = (ctx->style.sliderFillColor.a > 0.01f)
      ? ctx->style.sliderFillColor
      : ctx->style.accentColor;
  Vec2 fillSize(railSize.x * fraction, railSize.y);
  ctx->renderer.DrawRectFilled(railPos, fillSize, sliderFill,
                               S(trackThickness) * 0.5f);

  // Thumb: 20px circle at the fraction position, centered on the rail
  float thumbCx = trackPos.x + trackSize.x * fraction;
  float thumbCy = trackPos.y + trackSize.y * 0.5f;
  float r = S(thumbRadius);
  // Outer ring (border in accent color)
  ctx->renderer.DrawCircle(Vec2(thumbCx, thumbCy), r, sliderFill, true);
  // Inner dot — slightly smaller, white/neutral fill so the accent shows as a ring
  bool active = ctx->activeWidgetId == id;
  float innerScale = active ? 0.40f : (hover ? 0.45f : 0.55f);
  Color innerColor(1.0f, 1.0f, 1.0f, 1.0f);
  ctx->renderer.DrawCircle(Vec2(thumbCx, thumbCy), r * innerScale, innerColor, true);

  ctx->renderer.DrawText(widgetPos, label, labelStyle.color,
                         labelStyle.fontSize);

  ctx->renderer.DrawText(valuePos, valueText, valueStyle.color,
                         valueStyle.fontSize);

  // Phase F1: apply keyboard change to value if it happened
  if (kbChanged) {
    if (value) *value = currentValue;
    floatSlot = currentValue;
    valueChanged = true;
  }

  ctx->lastItemPos = widgetPos;
  AdvanceCursor(ctx, finalSize);
  // Phase B1: publish slider state
  {
    bool isActiveSlider = (ctx->activeWidgetId == id &&
                           ctx->activeWidgetType == ActiveWidgetType::Slider);
    SetLastItem(id, widgetPos, widgetPos + finalSize,
                hover, isActiveSlider, ctx->focusedWidgetId == id, valueChanged);
  }
  return valueChanged;
}

bool SliderInt(const std::string &label, int *value, int minValue, int maxValue,
               float width, std::optional<Vec2> pos) {
  UIContext *ctx = GetContext();
  if (!ctx)
    return false;

  if (width <= 0.0f) {
    width = 200.0f;
  }

  uint32_t id = GenerateId("SLDR_I:", label.c_str());
  bool intFresh = ctx->widgetStates.find(id) == ctx->widgetStates.end(); // brief 22 (fase 3)
  int& intSlot = ctx->GetWidgetState(id).intVal;
  if (intFresh) intSlot = minValue; // preserva default try_emplace(id, minValue)
  int originalValue = value ? *value : intSlot;
  int currentValue = std::clamp(originalValue, minValue, maxValue);

  // Write back clamped value immediately if out of range
  if (currentValue != originalValue) {
    if (value)
      *value = currentValue;
    else
      intSlot = currentValue;
  }

  float asFloat = static_cast<float>(currentValue);
  bool changed = SliderFloat(label, &asFloat, static_cast<float>(minValue),
                             static_cast<float>(maxValue), width, "%.0f", pos);
  int newInt = static_cast<int>(std::round(asFloat));
  newInt = std::clamp(newInt, minValue, maxValue);

  if (changed || newInt != currentValue) {
    if (value)
      *value = newInt;
    else
      intSlot = newInt; // brief 22 (fase 3)
    return true;
  }
  return currentValue != originalValue;
}

// ============================================================================
// Multiline TextInput helper functions
// ============================================================================

// Find which line and column the caret is at (line/col are 0-based)
static void FindLineCol(const std::string& text, size_t pos, int& line, int& col) {
    line = 0; col = 0;
    for (size_t i = 0; i < pos && i < text.size(); ++i) {
        if (text[i] == '\n') { line++; col = 0; }
        else col++;
    }
}

// Find byte offset of the start of a given line
static size_t LineStart(const std::string& text, int targetLine) {
    if (targetLine <= 0) return 0;
    int line = 0;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\n') {
            line++;
            if (line == targetLine) return i + 1;
        }
    }
    return (line < targetLine) ? text.size() : 0;
}

// Find byte offset of the end of a given line (before '\n' or at string end)
static size_t LineEnd(const std::string& text, int targetLine) {
    size_t start = LineStart(text, targetLine);
    size_t i = start;
    while (i < text.size() && text[i] != '\n') i++;
    return i;
}

// Count total lines in text
static int CountLines(const std::string& text) {
    int lines = 1;
    for (char c : text) if (c == '\n') lines++;
    return lines;
}

// Split text into lines
static std::vector<std::string> SplitLines(const std::string& text) {
    std::vector<std::string> lines;
    size_t start = 0;
    for (size_t i = 0; i <= text.size(); ++i) {
        if (i == text.size() || text[i] == '\n') {
            lines.push_back(text.substr(start, i - start));
            start = i + 1;
        }
    }
    return lines;
}

// Find caret position within a single line given an x offset
static size_t FindCaretInLine(const std::string& lineText, float targetX,
                               UIContext* ctx) {
    const TextStyle& textStyle = ctx->style.GetTextStyle(TypographyStyle::Body);
    float accumulated = 0.0f;
    const char* ptr = lineText.data();
    const char* end = ptr + lineText.size();
    size_t bytePos = 0;
    while (ptr < end) {
        uint32_t cp = DecodeUTF8(ptr, end);
        if (cp == 0) break;
        float charWidth = ctx->renderer.GetGlyphAdvance(cp, textStyle.fontSize);
        if (targetX < accumulated + charWidth * 0.5f) {
            return bytePos;
        }
        accumulated += charWidth;
        bytePos = static_cast<size_t>(ptr - lineText.data());
    }
    return lineText.size();
}

// Issue 8: FindCaretPosition using DecodeUTF8 + GetGlyphAdvance directly (no allocations)
static size_t FindCaretPosition(const std::string &text, float targetX,
                                UIContext *ctx) {
  const TextStyle &textStyle = ctx->style.GetTextStyle(TypographyStyle::Body);
  float accumulated = 0.0f;
  const char* ptr = text.data();
  const char* end = ptr + text.size();
  size_t bytePos = 0;
  while (ptr < end) {
    const char* charStart = ptr;
    uint32_t cp = DecodeUTF8(ptr, end);
    if (cp == 0) break;
    float charWidth = ctx->renderer.GetGlyphAdvance(cp, textStyle.fontSize);
    if (targetX < accumulated + charWidth * 0.5f) {
      return bytePos;
    }
    accumulated += charWidth;
    bytePos = static_cast<size_t>(ptr - text.data());
  }
  return text.size();
}

// brief 17: shared selection-boundary helpers, extracted from TextInput's
// double/triple-click logic so SelectableText (and any future selectable text
// widget) reuses the exact same word/line semantics.
//   WordBounds  — the [start,end) byte range of the word containing `pos`
//                 (alnum/underscore run); falls back to a single char when not on
//                 a word char, matching TextInput's behaviour.
//   LineBounds  — the [start,end) byte range of the logical line containing `pos`
//                 (between surrounding '\n's; the terminating '\n' is excluded).
static void WordBounds(const std::string &text, size_t pos, size_t &outStart,
                       size_t &outEnd) {
  auto isWordChar = [](unsigned char c) { return std::isalnum(c) || c == '_'; };
  size_t wStart = std::min(pos, text.size());
  size_t wEnd = wStart;
  while (wStart > 0 && isWordChar(static_cast<unsigned char>(text[wStart - 1])))
    --wStart;
  while (wEnd < text.size() && isWordChar(static_cast<unsigned char>(text[wEnd])))
    ++wEnd;
  if (wStart == wEnd && wEnd < text.size())
    wEnd = wStart + 1; // not on a word char: select the single char
  outStart = wStart;
  outEnd = wEnd;
}

static void LineBounds(const std::string &text, size_t pos, size_t &outStart,
                       size_t &outEnd) {
  size_t lStart = std::min(pos, text.size());
  size_t lEnd = lStart;
  while (lStart > 0 && text[lStart - 1] != '\n')
    --lStart;
  while (lEnd < text.size() && text[lEnd] != '\n')
    ++lEnd;
  outStart = lStart;
  outEnd = lEnd;
}

bool TextInput(const std::string &label, std::string *value, float width,
               bool multiline, std::optional<Vec2> pos, const char* placeholder, size_t maxLength) {
  UIContext *ctx = GetContext();
  if (!ctx)
    return false;
  if (width <= 0.0f)
    width = 200.0f;

  // Hidden-label convention (ImGui-style): an empty label, or one beginning with
  // "##", reserves no header row and draws no label text — only the id is derived
  // from it. Lets brief 17 widgets (AutoSuggestBox / TokenizingTextBox) embed a
  // TextInput with no stray gap or visible "##..." string.
  bool hideLabel = label.empty() ||
                   (label.size() >= 2 && label[0] == '#' && label[1] == '#');

  float labelSpacing = hideLabel ? 0.0f : 4.0f;

  const TextStyle &labelStyle =
      ctx->style.GetTextStyle(TypographyStyle::Subtitle);
  const TextStyle &inputTextStyle =
      ctx->style.GetTextStyle(TypographyStyle::Body);
  const PanelStyle &panelStyle = ctx->style.panel;
  const ColorState &accentState = ctx->style.button.background;

  // Compact input height (~30px for 14px font), independent of panel padding
  // so panel theming doesn't inflate field heights.
  float singleLineHeight = inputTextStyle.fontSize + 16.0f;
  float inputHeight = multiline ? std::max(100.0f, singleLineHeight * 4.0f) : singleLineHeight;
  Vec2 labelSize = hideLabel ? Vec2(0.0f, 0.0f)
                             : MeasureTextCached(ctx, label, labelStyle.fontSize);
  Vec2 fieldSize(width, inputHeight);

  Vec2 totalSize(fieldSize.x, labelSize.y + labelSpacing + fieldSize.y);
  LayoutConstraints constraints = ConsumeNextConstraints(SizeConstraint::Fill);
  Vec2 finalSize = ApplyConstraints(ctx, constraints, totalSize);

  // Resolver posición: usar pos si se proporciona, sino usar cursor
  // directamente
  Vec2 widgetPos;
  if (pos.has_value()) {
    widgetPos = ResolveAbsolutePosition(ctx, pos.value(), finalSize);
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

  uint32_t id = GenerateId("TXT:", label.c_str());

  // Phase F1: register as focusable
  ctx->focusableWidgets.push_back(id);

  // Si value no es nullptr, usar su valor directamente
  // Si value es nullptr, usar el valor almacenado en WidgetState.stringVal
  std::string* textPtr = value;
  if (!textPtr) {
    textPtr = &ctx->GetWidgetState(id).stringVal; // brief 22 (fase 3)
  }

  // Asegurar que textPtr apunta al valor correcto
  std::string &textRef = *textPtr;

  // brief 22 (fase 4): caret/scroll/anchor fundidos en TextEditState. El
  // try_emplace original inicializaba caret=textRef.size() SOLO en el primer
  // frame; se replica detectando si el sub-struct de texto aún no existe.
  bool firstTextFrame = ctx->GetWidgetState(id).text == nullptr;
  auto &ts = ctx->GetTextState(id);
  if (firstTextFrame) ts.caret = textRef.size();
  size_t &caret = ts.caret;
  caret = std::min(caret, textRef.size());
  float &scroll = ts.scrollOffset;

  // Vertical scroll offset for multiline (stored in WidgetState.floatVal at offset key)
  uint32_t mlScrollKey = id ^ 0xA5A5A5A5u; // Distinct key for multiline vertical scroll
  float &mlScroll = ctx->GetWidgetState(mlScrollKey).floatVal; // brief 22 (fase 3)

  // Selection anchor (SIZE_MAX = no selection). Default de TextEditState::anchor
  // ya es SIZE_MAX, idéntico al try_emplace(id, SIZE_MAX) original.
  size_t &selAnchor = ts.anchor;

  // Helper lambdas for selection
  auto HasSelection = [&]() { return selAnchor != SIZE_MAX && selAnchor != caret; };
  auto SelectionStart = [&]() -> size_t { return HasSelection() ? std::min(selAnchor, caret) : caret; };
  auto SelectionEnd = [&]() -> size_t { return HasSelection() ? std::max(selAnchor, caret) : caret; };
  auto ClearSelection = [&]() { selAnchor = SIZE_MAX; };
  auto DeleteSelection = [&]() {
    if (!HasSelection()) return;
    size_t start = SelectionStart();
    size_t end = SelectionEnd();
    textRef.erase(start, end - start);
    caret = start;
    ClearSelection();
  };

  Vec2 mousePos(ctx->input.MouseX(), ctx->input.MouseY());
  bool hover = PointInRect(mousePos, fieldPos, fieldSize) &&
               !IsMouseInputBlocked(ctx);
  bool leftPressed = ctx->input.IsMousePressed(0);
  bool leftDown = ctx->input.IsMouseDown(0);

  // Set I-beam cursor when hovering text input
  if (hover) {
    ctx->desiredCursor = UIContext::CursorType::IBeam;
  }

  if (leftPressed) {
    if (hover) {
      ctx->activeWidgetId = id;
      ctx->activeWidgetType = ActiveWidgetType::TextInput;

      // Check shift state to extend selection from existing anchor
      bool shiftClick = ctx->input.ShiftDown();

      // Compute caret position from click location
      size_t newCaret = caret;
      if (multiline) {
        float textPad = panelStyle.padding.x * 0.5f;
        float lineH = inputTextStyle.fontSize * 1.4f;
        float localY = mousePos.y - (fieldPos.y + panelStyle.padding.y * 0.5f) + mlScroll;
        int clickedLine = std::max(0, static_cast<int>(localY / lineH));
        int totalLines = CountLines(textRef);
        clickedLine = std::min(clickedLine, totalLines - 1);
        float localX = mousePos.x - (fieldPos.x + textPad);
        size_t lineStartOff = LineStart(textRef, clickedLine);
        size_t lineEndOff = LineEnd(textRef, clickedLine);
        std::string lineText = textRef.substr(lineStartOff, lineEndOff - lineStartOff);
        size_t posInLine = FindCaretInLine(lineText, std::max(localX, 0.0f), ctx);
        newCaret = lineStartOff + posInLine;
      } else {
        float localX = mousePos.x - (fieldPos.x + panelStyle.padding.x) + scroll;
        newCaret = FindCaretPosition(textRef, std::max(localX, 0.0f), ctx);
      }

      // Multi-click detection
      auto &clickInfo = ts.clickInfo; // brief 22 (fase 4)
      uint64_t now = SDL_GetTicks();
      const uint64_t MULTICLICK_MS = 350;
      const float MULTICLICK_DIST = 4.0f;
      float dx = mousePos.x - clickInfo.lastClickPos.x;
      float dy = mousePos.y - clickInfo.lastClickPos.y;
      bool isMulticlick = (now - clickInfo.lastClickTime) <= MULTICLICK_MS &&
                          std::abs(dx) <= MULTICLICK_DIST &&
                          std::abs(dy) <= MULTICLICK_DIST;
      if (isMulticlick) {
        clickInfo.clickCount = std::min(clickInfo.clickCount + 1, 3);
      } else {
        clickInfo.clickCount = 1;
      }
      clickInfo.lastClickTime = now;
      clickInfo.lastClickPos = mousePos;

      if (shiftClick && selAnchor != SIZE_MAX) {
        // Extend selection from existing anchor; do not reset
        caret = newCaret;
        clickInfo.clickCount = 1; // Shift+click resets multi-click tracking
      } else if (clickInfo.clickCount == 2) {
        // Double-click: select word containing newCaret (brief 17 shared helper)
        size_t wStart, wEnd;
        WordBounds(textRef, newCaret, wStart, wEnd);
        selAnchor = wStart;
        caret = wEnd;
      } else if (clickInfo.clickCount >= 3) {
        // Triple-click: select entire line (brief 17 shared helper)
        size_t lStart, lEnd;
        LineBounds(textRef, newCaret, lStart, lEnd);
        selAnchor = lStart;
        caret = lEnd;
      } else {
        // Single click: position caret, reset selection
        caret = newCaret;
        selAnchor = caret;
        ClearSelection();
      }
    } else if (ctx->activeWidgetId == id &&
               ctx->activeWidgetType == ActiveWidgetType::TextInput) {
      ctx->activeWidgetId = 0;
      ctx->activeWidgetType = ActiveWidgetType::None;
    }
  }

  // Click-drag selection: update caret while mouse is held down
  if (!leftPressed && leftDown && ctx->activeWidgetId == id &&
      ctx->activeWidgetType == ActiveWidgetType::TextInput) {
    size_t newCaret = caret;
    if (multiline) {
      float textPad = panelStyle.padding.x * 0.5f;
      float lineH = inputTextStyle.fontSize * 1.4f;
      float localY = mousePos.y - (fieldPos.y + panelStyle.padding.y * 0.5f) + mlScroll;
      int clickedLine = std::max(0, static_cast<int>(localY / lineH));
      int totalLines = CountLines(textRef);
      clickedLine = std::min(clickedLine, totalLines - 1);
      float localX = mousePos.x - (fieldPos.x + textPad);
      size_t lineStartOff = LineStart(textRef, clickedLine);
      size_t lineEndOff = LineEnd(textRef, clickedLine);
      std::string lineText = textRef.substr(lineStartOff, lineEndOff - lineStartOff);
      size_t posInLine = FindCaretInLine(lineText, std::max(localX, 0.0f), ctx);
      newCaret = lineStartOff + posInLine;
    } else {
      float localX = mousePos.x - (fieldPos.x + panelStyle.padding.x) + scroll;
      newCaret = FindCaretPosition(textRef, std::max(localX, 0.0f), ctx);
    }
    if (newCaret != caret) {
      caret = newCaret;
      // selAnchor was set on leftPressed; selection is anchor..caret
    }
  }

  bool hasFocus = ctx->activeWidgetId == id &&
                  ctx->activeWidgetType == ActiveWidgetType::TextInput;
  bool valueChanged = false;

  // brief 18.4: per-field IME ownership. The focused field claims IME so text
  // input + the candidate window follow focus instead of being globally on:
  //   - on focus (idempotent): SDL_StartTextInput + position the candidate window
  //     at the field rect (SDL_SetTextInputArea, refined to the caret while
  //     composing below). Works for single- and multi-line fields alike.
  //   - on blur of the field that owned IME: SDL_StopTextInput.
  if (ctx->window) {
    if (hasFocus) {
      if (ctx->imeOwnerId != id) {
        SDL_StartTextInput(static_cast<SDL_Window*>(ctx->window));
        ctx->imeOwnerId = id;
      }
      SDL_Rect imeArea;
      imeArea.x = static_cast<int>(fieldPos.x);
      imeArea.y = static_cast<int>(fieldPos.y);
      imeArea.w = static_cast<int>(fieldSize.x);
      imeArea.h = static_cast<int>(fieldSize.y);
      SDL_SetTextInputArea(static_cast<SDL_Window*>(ctx->window), &imeArea, 0);
    } else if (ctx->imeOwnerId == id) {
      SDL_StopTextInput(static_cast<SDL_Window*>(ctx->window));
      ctx->imeOwnerId = 0;
    }
  }

  // Capture pre-edit state for undo (snapshot before any modifications)
  std::string preEditText = textRef;
  size_t preEditCaret = caret;

  // Consume any pending TextInput callback set by the callback-overload wrapper.
  // Pointer is opaque in Context.h to avoid header dependency cycle.
  const TextInputCallback* cbPtr =
      static_cast<const TextInputCallback*>(ctx->pendingTextInputCallback);
  uint32_t cbMask = ctx->pendingTextInputCallbackMask;
  ctx->pendingTextInputCallback = nullptr;
  ctx->pendingTextInputCallbackMask = 0;

  auto invokeCallback = [&](TextInputCallbackType evtType, uint32_t key, uint32_t charInput) -> uint32_t {
    if (!cbPtr || !*cbPtr) return charInput;
    if ((cbMask & static_cast<uint32_t>(evtType)) == 0) return charInput;
    TextInputCallbackData data;
    data.type = evtType;
    data.buffer = &textRef;
    data.cursorPos = caret;
    data.selectionStart = HasSelection() ? SelectionStart() : SIZE_MAX;
    data.selectionEnd = HasSelection() ? SelectionEnd() : SIZE_MAX;
    data.key = key;
    data.charInput = charInput;
    (*cbPtr)(data);
    if (data.cursorPos <= textRef.size()) caret = data.cursorPos;
    return data.charInput;
  };

  // Query keyboard modifier state
  bool ctrlHeld = ctx->input.CtrlDown();
  bool shiftHeld = ctx->input.ShiftDown();

  if (hasFocus) {
    // Ctrl+A: Select all
    if (ctrlHeld && ctx->input.IsKeyPressed(UIKey::A)) {
      selAnchor = 0;
      caret = textRef.size();
    }
    // Ctrl+C: Copy
    else if (ctrlHeld && ctx->input.IsKeyPressed(UIKey::C)) {
      if (HasSelection()) {
        std::string selected = textRef.substr(SelectionStart(), SelectionEnd() - SelectionStart());
        ctx->input.SetClipboardText(selected);
      }
    }
    // Ctrl+X: Cut
    else if (ctrlHeld && ctx->input.IsKeyPressed(UIKey::X)) {
      if (HasSelection()) {
        std::string selected = textRef.substr(SelectionStart(), SelectionEnd() - SelectionStart());
        ctx->input.SetClipboardText(selected);
        DeleteSelection();
        valueChanged = true;
      }
    }
    // Ctrl+V: Paste
    else if (ctrlHeld && ctx->input.IsKeyPressed(UIKey::V)) {
      std::string clipStr = ctx->input.GetClipboardText();
      if (!clipStr.empty()) {
        if (HasSelection()) {
          DeleteSelection();
          valueChanged = true;
        }
        // In single-line mode, replace newlines with spaces
        if (!multiline) {
          for (char& c : clipStr) {
            if (c == '\n' || c == '\r') c = ' ';
          }
        } else {
          // Normalize \r\n to \n
          std::string normalized;
          normalized.reserve(clipStr.size());
          for (size_t ci = 0; ci < clipStr.size(); ++ci) {
            if (clipStr[ci] == '\r') {
              normalized += '\n';
              if (ci + 1 < clipStr.size() && clipStr[ci + 1] == '\n') ci++;
            } else {
              normalized += clipStr[ci];
            }
          }
          clipStr = std::move(normalized);
        }
        // Enforce maxLength limit on paste
        if (maxLength > 0 && textRef.size() + clipStr.size() > maxLength) {
          size_t canInsert = (textRef.size() < maxLength) ? maxLength - textRef.size() : 0;
          clipStr = clipStr.substr(0, canInsert);
        }
        if (!clipStr.empty()) {
          textRef.insert(caret, clipStr);
          caret += clipStr.size();
          valueChanged = true;
        }
      }
    }
    // Ctrl+Z: Undo
    else if (ctrlHeld && ctx->input.IsKeyPressed(UIKey::Z) && !shiftHeld) {
      auto& undoState = ts.undo; // brief 22 (fase 4)
      if (!undoState.undoStack.empty()) {
        // Save current state to redo stack
        undoState.redoStack.push_back({textRef, caret, ctx->frame});
        auto& entry = undoState.undoStack.back();
        textRef = entry.text;
        caret = std::min(entry.caret, textRef.size());
        undoState.undoStack.pop_back();
        ClearSelection();
        valueChanged = true;
      }
    }
    // Ctrl+Y or Ctrl+Shift+Z: Redo
    else if ((ctrlHeld && ctx->input.IsKeyPressed(UIKey::Y)) ||
             (ctrlHeld && shiftHeld && ctx->input.IsKeyPressed(UIKey::Z))) {
      auto& undoState = ts.undo; // brief 22 (fase 4)
      if (!undoState.redoStack.empty()) {
        undoState.undoStack.push_back({textRef, caret, ctx->frame});
        auto& entry = undoState.redoStack.back();
        textRef = entry.text;
        caret = std::min(entry.caret, textRef.size());
        undoState.redoStack.pop_back();
        ClearSelection();
        valueChanged = true;
      }
    }
    else {
      // Normal text input (only if no Ctrl held, to avoid inserting chars on Ctrl combos)
      if (!ctrlHeld) {
        const std::string &inputText = ctx->input.TextInputBuffer();
        if (!inputText.empty()) {
          // Apply CharFilter callback per byte (best-effort; full UTF-8 codepoint pre-decode would be ideal).
          std::string filtered;
          filtered.reserve(inputText.size());
          if (cbPtr && (cbMask & static_cast<uint32_t>(TextInputCallbackType::CharFilter))) {
            for (unsigned char c : inputText) {
              uint32_t cp = c;
              uint32_t kept = invokeCallback(TextInputCallbackType::CharFilter, 0, cp);
              if (kept != 0) filtered.push_back(static_cast<char>(kept));
            }
          } else {
            filtered = inputText;
          }
          if (!filtered.empty()) {
            if (HasSelection()) {
              DeleteSelection();
            }
            // Enforce maxLength limit
            std::string toInsert = filtered;
            if (maxLength > 0 && textRef.size() + toInsert.size() > maxLength) {
              size_t canInsert = (textRef.size() < maxLength) ? maxLength - textRef.size() : 0;
              toInsert = toInsert.substr(0, canInsert);
            }
            if (!toInsert.empty()) {
              textRef.insert(caret, toInsert);
              caret += toInsert.size();
              valueChanged = true;
            }
          }
        }
      }

      // Tab → Completion callback
      if (cbPtr && (cbMask & static_cast<uint32_t>(TextInputCallbackType::Completion)) &&
          ctx->input.IsKeyPressed(UIKey::Tab)) {
        std::string before = textRef;
        invokeCallback(TextInputCallbackType::Completion, static_cast<uint32_t>(UIKey::Tab), 0);
        if (textRef != before) valueChanged = true;
      }

      // Up/Down (single-line only) → History callback
      if (!multiline && cbPtr && (cbMask & static_cast<uint32_t>(TextInputCallbackType::History))) {
        if (ctx->input.IsKeyPressed(UIKey::Up)) {
          std::string before = textRef;
          invokeCallback(TextInputCallbackType::History, static_cast<uint32_t>(UIKey::Up), 0);
          if (textRef != before) valueChanged = true;
        } else if (ctx->input.IsKeyPressed(UIKey::Down)) {
          std::string before = textRef;
          invokeCallback(TextInputCallbackType::History, static_cast<uint32_t>(UIKey::Down), 0);
          if (textRef != before) valueChanged = true;
        }
      }

      if (ctx->input.IsKeyPressed(UIKey::Backspace)) {
        if (HasSelection()) {
          DeleteSelection();
          valueChanged = true;
        } else if (caret > 0) {
          size_t prev = Utf8PrevCodepoint(textRef, caret);
          textRef.erase(prev, caret - prev);
          caret = prev;
          valueChanged = true;
        }
      } else if (ctx->input.IsKeyPressed(UIKey::Delete)) {
        if (HasSelection()) {
          DeleteSelection();
          valueChanged = true;
        } else if (caret < textRef.size()) {
          size_t next = Utf8NextCodepoint(textRef, caret);
          textRef.erase(caret, next - caret);
          valueChanged = true;
        }
      } else if (ctx->input.IsKeyPressed(UIKey::Left)) {
        if (shiftHeld) {
          if (selAnchor == SIZE_MAX) selAnchor = caret;
        } else {
          if (HasSelection()) { caret = SelectionStart(); ClearSelection(); goto skip_move_left; }
          ClearSelection();
        }
        if (ctrlHeld) {
          // Word-by-word navigation: skip to previous word boundary
          while (caret > 0 && (caret - 1 < textRef.size()) && textRef[caret - 1] == ' ') caret--;
          while (caret > 0 && (caret - 1 < textRef.size()) && textRef[caret - 1] != ' ') caret--;
        } else {
          if (caret > 0) caret--;
        }
        skip_move_left:;
      } else if (ctx->input.IsKeyPressed(UIKey::Right)) {
        if (shiftHeld) {
          if (selAnchor == SIZE_MAX) selAnchor = caret;
        } else {
          if (HasSelection()) { caret = SelectionEnd(); ClearSelection(); goto skip_move_right; }
          ClearSelection();
        }
        if (ctrlHeld) {
          // Word-by-word navigation: skip to next word boundary
          while (caret < textRef.size() && textRef[caret] != ' ') caret++;
          while (caret < textRef.size() && textRef[caret] == ' ') caret++;
        } else {
          if (caret < textRef.size()) caret++;
        }
        skip_move_right:;
      } else if (ctx->input.IsKeyPressed(UIKey::Home)) {
        if (shiftHeld) {
          if (selAnchor == SIZE_MAX) selAnchor = caret;
        } else {
          ClearSelection();
        }
        if (multiline && !ctrlHeld) {
          // Home goes to start of current line
          int curLine, curCol;
          FindLineCol(textRef, caret, curLine, curCol);
          caret = LineStart(textRef, curLine);
        } else {
          // Ctrl+Home or single-line: go to start of text
          caret = 0;
        }
      } else if (ctx->input.IsKeyPressed(UIKey::End)) {
        if (shiftHeld) {
          if (selAnchor == SIZE_MAX) selAnchor = caret;
        } else {
          ClearSelection();
        }
        if (multiline && !ctrlHeld) {
          // End goes to end of current line
          int curLine, curCol;
          FindLineCol(textRef, caret, curLine, curCol);
          caret = LineEnd(textRef, curLine);
        } else {
          // Ctrl+End or single-line: go to end of text
          caret = textRef.size();
        }
      } else if (multiline && (ctx->input.IsKeyPressed(UIKey::Up) ||
                                ctx->input.IsKeyPressed(UIKey::Down))) {
        // Up/Down arrow navigation for multiline
        if (shiftHeld) {
          if (selAnchor == SIZE_MAX) selAnchor = caret;
        } else {
          ClearSelection();
        }
        int curLine, curCol;
        FindLineCol(textRef, caret, curLine, curCol);
        int totalLines = CountLines(textRef);
        if (ctx->input.IsKeyPressed(UIKey::Up)) {
          if (curLine > 0) {
            int targetLine = curLine - 1;
            size_t ls = LineStart(textRef, targetLine);
            size_t le = LineEnd(textRef, targetLine);
            int lineLen = static_cast<int>(le - ls);
            caret = ls + static_cast<size_t>(std::min(curCol, lineLen));
          }
        } else { // DOWN
          if (curLine < totalLines - 1) {
            int targetLine = curLine + 1;
            size_t ls = LineStart(textRef, targetLine);
            size_t le = LineEnd(textRef, targetLine);
            int lineLen = static_cast<int>(le - ls);
            caret = ls + static_cast<size_t>(std::min(curCol, lineLen));
          }
        }
      } else if (multiline && (ctx->input.IsKeyPressed(UIKey::Enter) ||
                                ctx->input.IsKeyPressed(UIKey::KeypadEnter))) {
        // In multiline mode, Enter inserts a newline
        if (HasSelection()) {
          DeleteSelection();
        }
        textRef.insert(caret, 1, '\n');
        caret++;
        valueChanged = true;
      } else if (!multiline && (ctx->input.IsKeyPressed(UIKey::Enter) ||
                                ctx->input.IsKeyPressed(UIKey::KeypadEnter))) {
        ctx->activeWidgetId = 0;
        ctx->activeWidgetType = ActiveWidgetType::None;
      }
    }
  }

  // Push pre-edit state to undo stack when text actually changed
  if (valueChanged && preEditText != textRef) {
    ts.undo.PushUndo(preEditText, preEditCaret, ctx->frame); // brief 22 (fase 4)
  }

  // Invoke valueChanged callback if text changed
  if (valueChanged) {
    std::string idStr = "TXT:" + label;
    auto cbIt = ctx->valueChangedCallbacks.find(idStr);
    if (cbIt != ctx->valueChangedCallbacks.end()) cbIt->second(idStr, textPtr);
  }

  if (!hideLabel) {
    ctx->renderer.DrawText(widgetPos, label, labelStyle.color,
                           labelStyle.fontSize);
  }

  // Fondo recesado (consistente con DragFloat/ComboBox) para que el campo se
  // distinga claramente de la superficie del panel.
  Color bgColor = InputFieldBackground(ctx, hover && !hasFocus);

  if (hasFocus) {
    DrawFocusRing(ctx, fieldPos, fieldSize, panelStyle.cornerRadius);
    bgColor = accentState.hover;
    bgColor.a = 0.15f;
  }

  ctx->renderer.DrawRectFilled(fieldPos, fieldSize, bgColor,
                               panelStyle.cornerRadius);
  // Brief 11: subtle inset shadow so the text field reads as recessed (sunken).
  // Drawn AFTER the fill, clipped to the rounded interior.
  ctx->renderer.DrawInsetShadow(fieldPos, fieldSize, panelStyle.cornerRadius, 2.0f,
                                Color(0.0f, 0.0f, 0.0f, 0.16f));
  // Borde 1px visible salvo cuando el focus ring ya lo marca.
  if (!hasFocus) {
    ctx->renderer.DrawRect(fieldPos, fieldSize, InputFieldBorder(ctx, hover),
                           panelStyle.cornerRadius);
  }

  float textPadding = panelStyle.padding.x * 0.5f;

  if (multiline) {
    // ================================================================
    // Multiline rendering
    // ================================================================
    float lineH = inputTextStyle.fontSize * 1.4f;
    float padY = panelStyle.padding.y * 0.5f;
    float availableHeight = fieldSize.y - padY * 2.0f;
    float availableWidth = fieldSize.x - textPadding * 2.0f - 8.0f; // 8px for scrollbar

    std::vector<std::string> lines = SplitLines(textRef);
    int numLines = static_cast<int>(lines.size());
    float totalContentHeight = numLines * lineH;

    // Determine caret line/col for scroll tracking
    int caretLine, caretCol;
    FindLineCol(textRef, caret, caretLine, caretCol);

    // Auto-scroll to keep caret visible
    float caretY = caretLine * lineH;
    if (caretY - mlScroll < 0.0f)
      mlScroll = caretY;
    else if (caretY + lineH - mlScroll > availableHeight)
      mlScroll = caretY + lineH - availableHeight;
    mlScroll = std::clamp(mlScroll, 0.0f, std::max(0.0f, totalContentHeight - availableHeight));

    // Handle mouse wheel scrolling when hovering
    if (hover) {
      float wheelY = ctx->input.MouseWheelY();
      if (wheelY != 0.0f) {
        mlScroll -= wheelY * lineH * 3.0f;
        mlScroll = std::clamp(mlScroll, 0.0f, std::max(0.0f, totalContentHeight - availableHeight));
      }
    }

    // Push clip rect for the text area
    ctx->renderer.PushClipRect(
        Vec2(fieldPos.x + textPadding, fieldPos.y + padY),
        Vec2(availableWidth, availableHeight));

    // Draw selection highlight (per-line)
    if (hasFocus && HasSelection()) {
      size_t selStart = SelectionStart();
      size_t selEnd = SelectionEnd();
      int selStartLine, selStartCol, selEndLine, selEndCol;
      FindLineCol(textRef, selStart, selStartLine, selStartCol);
      FindLineCol(textRef, selEnd, selEndLine, selEndCol);

      Color selColor = accentState.normal;
      selColor.a = 0.35f;

      for (int ln = selStartLine; ln <= selEndLine && ln < numLines; ++ln) {
        float lineTop = fieldPos.y + padY + ln * lineH - mlScroll;
        float lineLeft = fieldPos.x + textPadding;

        size_t lineStartOff = LineStart(textRef, ln);
        const std::string& lineText = lines[ln];

        // Determine selection range within this line
        float hlLeftX, hlRightX;
        if (ln == selStartLine) {
          size_t localStart = selStart - lineStartOff;
          hlLeftX = MeasureTextCached(ctx, lineText.substr(0, localStart), inputTextStyle.fontSize).x;
        } else {
          hlLeftX = 0.0f;
        }
        if (ln == selEndLine) {
          size_t localEnd = selEnd - lineStartOff;
          hlRightX = MeasureTextCached(ctx, lineText.substr(0, localEnd), inputTextStyle.fontSize).x;
        } else {
          // Select entire line including trailing space
          hlRightX = MeasureTextCached(ctx, lineText, inputTextStyle.fontSize).x + 4.0f;
        }

        float absLeft = lineLeft + hlLeftX;
        float absRight = lineLeft + hlRightX;
        float fieldLeft = fieldPos.x + textPadding;
        float fieldRight = fieldPos.x + fieldSize.x - textPadding - 8.0f;
        absLeft = std::clamp(absLeft, fieldLeft, fieldRight);
        absRight = std::clamp(absRight, fieldLeft, fieldRight);

        if (absRight > absLeft) {
          ctx->renderer.DrawRectFilled(
              Vec2(absLeft, lineTop + 1.0f),
              Vec2(absRight - absLeft, lineH - 2.0f),
              selColor, 2.0f);
        }
      }
    }

    // Draw text lines or placeholder
    if (textRef.empty() && !hasFocus && placeholder) {
      Color placeholderColor = inputTextStyle.color;
      placeholderColor.a *= 0.4f;
      Vec2 linePos(fieldPos.x + textPadding, fieldPos.y + padY);
      ctx->renderer.DrawText(linePos, placeholder, placeholderColor, inputTextStyle.fontSize);
    } else {
      for (int ln = 0; ln < numLines; ++ln) {
        float lineTop = fieldPos.y + padY + ln * lineH - mlScroll;
        if (lineTop + lineH < fieldPos.y + padY) continue;
        if (lineTop > fieldPos.y + padY + availableHeight) break;

        Vec2 linePos(fieldPos.x + textPadding, lineTop);
        ctx->renderer.DrawText(linePos, lines[ln], inputTextStyle.color,
                                inputTextStyle.fontSize);
      }
    }

    // Draw caret with smooth sine-based blink
    if (hasFocus && !HasSelection()) {
      float blinkAlpha = 0.5f + 0.5f * std::sin(ctx->frame * 0.1f);
      size_t lineStartOff = LineStart(textRef, caretLine);
      std::string caretLineText = (caretLine < numLines)
          ? lines[caretLine]
          : std::string();
      size_t localCaret = caret - lineStartOff;
      float caretOffsetX = MeasureTextCached(ctx, caretLineText.substr(0, localCaret), inputTextStyle.fontSize).x;
      float caretX = fieldPos.x + textPadding + caretOffsetX;
      float caretY = fieldPos.y + padY + caretLine * lineH - mlScroll;
      Color caretColor = accentState.normal;
      caretColor.a = blinkAlpha;
      ctx->renderer.DrawRectFilled(
          Vec2(caretX, caretY),
          Vec2(1.5f, lineH),
          caretColor, 0.0f);
    }

    ctx->renderer.PopClipRect();

    // Draw vertical scrollbar if content overflows
    if (totalContentHeight > availableHeight) {
      float scrollbarWidth = 6.0f;
      float scrollbarX = fieldPos.x + fieldSize.x - scrollbarWidth - 1.0f;
      float scrollbarAreaY = fieldPos.y + padY;
      float scrollbarAreaH = availableHeight;

      float thumbRatio = availableHeight / totalContentHeight;
      float thumbH = std::max(20.0f, scrollbarAreaH * thumbRatio);
      float scrollRange = totalContentHeight - availableHeight;
      float thumbOffset = (scrollRange > 0.0f) ? (mlScroll / scrollRange) * (scrollbarAreaH - thumbH) : 0.0f;

      // Scrollbar track (subtle)
      Color trackColor(0.5f, 0.5f, 0.5f, 0.1f);
      ctx->renderer.DrawRectFilled(
          Vec2(scrollbarX, scrollbarAreaY),
          Vec2(scrollbarWidth, scrollbarAreaH),
          trackColor, scrollbarWidth * 0.5f);

      // Scrollbar thumb
      Color thumbColor(0.5f, 0.5f, 0.5f, 0.4f);
      // Highlight if mouse is over the scrollbar area
      float mouseLocalX = mousePos.x - scrollbarX;
      float mouseLocalY = mousePos.y - (scrollbarAreaY + thumbOffset);
      if (mouseLocalX >= 0 && mouseLocalX <= scrollbarWidth &&
          mouseLocalY >= 0 && mouseLocalY <= thumbH) {
        thumbColor.a = 0.6f;
      }
      ctx->renderer.DrawRectFilled(
          Vec2(scrollbarX, scrollbarAreaY + thumbOffset),
          Vec2(scrollbarWidth, thumbH),
          thumbColor, scrollbarWidth * 0.5f);
    }

  } else {
    // ================================================================
    // Single-line rendering (original code)
    // ================================================================
    float availableWidth = fieldSize.x - textPadding * 2.0f;
    Vec2 textSize = MeasureTextCached(ctx, textRef, inputTextStyle.fontSize);
    float caretOffset =
        MeasureTextCached(ctx, textRef.substr(0, caret), inputTextStyle.fontSize)
            .x;

    if (caretOffset - scroll > availableWidth)
      scroll = caretOffset - availableWidth;
    else if (caretOffset - scroll < 0.0f)
      scroll = caretOffset;
    scroll =
        std::clamp(scroll, 0.0f, std::max(0.0f, textSize.x - availableWidth));

    Vec2 textPos(fieldPos.x + textPadding - scroll,
                 fieldPos.y + (fieldSize.y - inputTextStyle.fontSize) * 0.5f);

    // Draw selection highlight
    if (hasFocus && HasSelection()) {
      size_t selStart = SelectionStart();
      size_t selEnd = SelectionEnd();
      float selStartX = MeasureTextCached(ctx, textRef.substr(0, selStart), inputTextStyle.fontSize).x;
      float selEndX = MeasureTextCached(ctx, textRef.substr(0, selEnd), inputTextStyle.fontSize).x;
      float hlLeft = textPos.x + selStartX;
      float hlRight = textPos.x + selEndX;
      // Clamp to field bounds
      float fieldLeft = fieldPos.x + textPadding;
      float fieldRight = fieldPos.x + fieldSize.x - textPadding;
      hlLeft = std::clamp(hlLeft, fieldLeft, fieldRight);
      hlRight = std::clamp(hlRight, fieldLeft, fieldRight);
      if (hlRight > hlLeft) {
        Color selColor = accentState.normal;
        selColor.a = 0.35f;
        ctx->renderer.DrawRectFilled(Vec2(hlLeft, fieldPos.y + 2.0f),
                                     Vec2(hlRight - hlLeft, fieldSize.y - 4.0f),
                                     selColor, 2.0f);
      }
    }

    if (textRef.empty() && !hasFocus && placeholder) {
      Color placeholderColor = inputTextStyle.color;
      placeholderColor.a *= 0.4f;
      ctx->renderer.DrawText(textPos, placeholder, placeholderColor, inputTextStyle.fontSize);
    } else {
      ctx->renderer.DrawText(textPos, textRef, inputTextStyle.color,
                             inputTextStyle.fontSize);
    }

    if (hasFocus && !HasSelection()) {
      float caretX = textPos.x + caretOffset;

      // IME composition: draw inline at caret position with underline
      if (ctx->input.HasComposition()) {
        const std::string& compText = ctx->input.CompositionText();
        Vec2 compSize = MeasureTextCached(ctx, compText, inputTextStyle.fontSize);
        Vec2 compPos(caretX, textPos.y);
        // Draw composition text
        Color compColor = inputTextStyle.color;
        compColor.a = 0.7f;
        ctx->renderer.DrawText(compPos, compText, compColor, inputTextStyle.fontSize);
        // Draw underline
        float underlineY = compPos.y + inputTextStyle.fontSize + 1.0f;
        ctx->renderer.DrawRectFilled(Vec2(compPos.x, underlineY),
                                     Vec2(compSize.x, 1.0f), accentState.normal, 0.0f);

        // Set text input area for IME candidate window positioning
        SDL_Rect inputArea;
        inputArea.x = static_cast<int>(caretX);
        inputArea.y = static_cast<int>(fieldPos.y);
        inputArea.w = static_cast<int>(compSize.x);
        inputArea.h = static_cast<int>(fieldSize.y);
        SDL_SetTextInputArea(static_cast<SDL_Window*>(ctx->window), &inputArea, 0);
      } else {
        // Normal caret blink
        float blinkAlpha = 0.5f + 0.5f * std::sin(ctx->frame * 0.1f);
        Vec2 caretPos(caretX, fieldPos.y + textPadding * 0.5f);
        Vec2 caretSize(1.5f, fieldSize.y - textPadding);
        Color caretColor = accentState.normal;
        caretColor.a = blinkAlpha;
        ctx->renderer.DrawRectFilled(caretPos, caretSize, caretColor, 0.0f);
      }
    }
  }

  ctx->lastItemPos = widgetPos;
  AdvanceCursor(ctx, finalSize);

  // Phase B2: callbacks (Edit fires when buffer mutated this frame; Always fires every focus frame)
  if (cbPtr) {
    if (valueChanged && (cbMask & static_cast<uint32_t>(TextInputCallbackType::Edit))) {
      invokeCallback(TextInputCallbackType::Edit, 0, 0);
    }
    if (hasFocus && (cbMask & static_cast<uint32_t>(TextInputCallbackType::Always))) {
      invokeCallback(TextInputCallbackType::Always, 0, 0);
    }
  }

  // Phase B1: publish text-input state
  {
    bool isActiveTI = (ctx->activeWidgetId == id &&
                       ctx->activeWidgetType == ActiveWidgetType::TextInput);
    SetLastItem(id, widgetPos, widgetPos + finalSize,
                hover, isActiveTI, ctx->focusedWidgetId == id, valueChanged);
  }
  return valueChanged;
}

// TextInput overload with callback support (Phase B2).
bool TextInput(const std::string &label, std::string *value, float width,
               bool multiline, std::optional<Vec2> pos, const char* placeholder, size_t maxLength,
               const TextInputCallback& callback, uint32_t callbackMask) {
  UIContext *ctx = GetContext();
  if (!ctx) return false;
  ctx->pendingTextInputCallback = const_cast<void*>(static_cast<const void*>(&callback));
  ctx->pendingTextInputCallbackMask = callbackMask;
  return TextInput(label, value, width, multiline, pos, placeholder, maxLength);
}

// ============================================================================
// DragFloat / DragInt widgets
// ============================================================================

static constexpr uint64_t DOUBLE_CLICK_MS = 300;

bool DragFloat(const std::string& label, float* value, float speed,
               float min, float max, const char* format,
               std::optional<Vec2> pos) {
  UIContext* ctx = GetContext();
  if (!ctx) return false;

  const TextStyle& labelStyle = ctx->style.GetTextStyle(TypographyStyle::Body);
  const TextStyle& valueStyle = ctx->style.GetTextStyle(TypographyStyle::Body);
  const PanelStyle& panelStyle = ctx->style.panel;
  const ColorState& accentState = ctx->style.button.background;

  float labelSpacing = 4.0f;
  Vec2 labelSize = MeasureTextCached(ctx, label, labelStyle.fontSize);
  float defaultFieldHeight = valueStyle.fontSize + 16.0f;

  // Total widget width: label (40%) + field (60%) laid out horizontally.
  // If there's no label, collapse the label slot entirely.
  bool hasLabel = !label.empty();
  float totalWidth = hasLabel ? 250.0f : 100.0f; // Default width
  Vec2 totalSize(totalWidth, std::max(labelSize.y, defaultFieldHeight));
  LayoutConstraints constraints = ConsumeNextConstraints(SizeConstraint::Fill);
  Vec2 finalSize = ApplyConstraints(ctx, constraints, totalSize);
  totalWidth = finalSize.x;
  float fieldHeight = finalSize.y;

  float labelWidth = hasLabel ? totalWidth * 0.4f : 0.0f;
  float fieldWidth = totalWidth - labelWidth;

  Vec2 widgetPos;
  if (pos.has_value()) {
    widgetPos = ResolveAbsolutePosition(ctx, pos.value(), finalSize);
  } else {
    widgetPos = ctx->cursorPos;
  }

  Vec2 fieldPos(widgetPos.x + labelWidth, widgetPos.y);
  Vec2 fieldSize(fieldWidth, fieldHeight);

  uint32_t id = GenerateId("DRAG_F:", label.c_str());

  // Get or create drag state
  DragWidgetState& state = ctx->dragStates[id];

  float currentValue = value ? *value : 0.0f;
  bool valueChanged = false;

  Vec2 mousePos(ctx->input.MouseX(), ctx->input.MouseY());
  bool hoverField = PointInRect(mousePos, fieldPos, fieldSize) &&
                    !IsMouseInputBlocked(ctx);
  bool leftPressed = ctx->input.IsMousePressed(0);
  bool leftDown = ctx->input.IsMouseDown(0);

  // Set resize-H cursor when hovering the drag field (not in edit mode)
  if (hoverField && !state.isEditing) {
    ctx->desiredCursor = UIContext::CursorType::ResizeH;
  }

  // --- Edit mode (text input via double-click) ---
  if (state.isEditing) {
    // Use the internal TextInput for direct editing
    // We need a temporary string for the text input
    std::string& editStr = state.editText;

    // Check for Enter or click outside to confirm
    bool enterPressed = ctx->input.IsKeyPressed(UIKey::Enter) ||
                        ctx->input.IsKeyPressed(UIKey::KeypadEnter);
    bool clickedOutside = leftPressed && !hoverField;
    bool escapePressed = ctx->input.IsKeyPressed(UIKey::Escape);

    if (enterPressed || clickedOutside) {
      // Parse the edited value
      try {
        float parsed = std::stof(editStr);
        if (min < max) {
          parsed = std::clamp(parsed, min, max);
        }
        currentValue = parsed;
        valueChanged = true;
      } catch (...) {
        // Invalid input, revert - no change
      }
      state.isEditing = false;
      ctx->activeWidgetId = 0;
      ctx->activeWidgetType = ActiveWidgetType::None;
    } else if (escapePressed) {
      // Cancel editing
      state.isEditing = false;
      ctx->activeWidgetId = 0;
      ctx->activeWidgetType = ActiveWidgetType::None;
    } else {
      // Process text input while editing
      ctx->activeWidgetId = id;
      ctx->activeWidgetType = ActiveWidgetType::TextInput;

      // Handle text input (brief 22 fase 4: caret en TextEditState; primer
      // frame → caret al final, como el try_emplace original)
      bool firstEditFrame = ctx->GetWidgetState(id).text == nullptr;
      auto& tsEdit = ctx->GetTextState(id);
      if (firstEditFrame) tsEdit.caret = editStr.size();
      size_t& caret = tsEdit.caret;
      caret = std::min(caret, editStr.size());

      bool ctrlHeld = ctx->input.CtrlDown();

      if (!ctrlHeld) {
        const std::string& inputText = ctx->input.TextInputBuffer();
        if (!inputText.empty()) {
          editStr.insert(caret, inputText);
          caret += inputText.size();
        }
      }

      if (ctx->input.IsKeyPressed(UIKey::Backspace)) {
        if (caret > 0) {
          caret--;
          editStr.erase(caret, 1);
        }
      } else if (ctx->input.IsKeyPressed(UIKey::Delete)) {
        if (caret < editStr.size()) {
          editStr.erase(caret, 1);
        }
      } else if (ctx->input.IsKeyPressed(UIKey::Left)) {
        if (caret > 0) caret--;
      } else if (ctx->input.IsKeyPressed(UIKey::Right)) {
        if (caret < editStr.size()) caret++;
      } else if (ctx->input.IsKeyPressed(UIKey::Home)) {
        caret = 0;
      } else if (ctx->input.IsKeyPressed(UIKey::End)) {
        caret = editStr.size();
      }

      // Ctrl+A: Select all text in the edit field
      if (ctrlHeld && ctx->input.IsKeyPressed(UIKey::A)) {
        caret = editStr.size();
      }
    }
  }
  // --- Normal / Dragging mode ---
  else {
    if (hoverField && leftPressed) {
      uint64_t now = SDL_GetTicks();
      uint64_t elapsed = now - state.lastClickTime;
      state.lastClickTime = now;

      if (elapsed < DOUBLE_CLICK_MS) {
        // Double-click: enter edit mode
        state.isEditing = true;
        char buf[64];
        std::snprintf(buf, sizeof(buf), format ? format : "%.2f", currentValue);
        state.editText = buf;
        // Place caret at end
        ctx->GetTextState(id).caret = state.editText.size(); // brief 22 (fase 4)
        ctx->activeWidgetId = id;
        ctx->activeWidgetType = ActiveWidgetType::TextInput;
      } else {
        // Single click: start dragging
        state.isDragging = true;
        state.dragStartValue = currentValue;
        state.dragStartMouseX = mousePos.x;
        state.dragThresholdPassed = false; // Phase B4
        ctx->activeWidgetId = id;
        ctx->activeWidgetType = ActiveWidgetType::DragWidget;
      }
    }

    if (state.isDragging) {
      if (leftDown) {
        bool shiftHeld = ctx->input.ShiftDown();
        float effectiveSpeed = speed * (shiftHeld ? 0.1f : 1.0f);

        // Phase B4: ignore micro-movements within the drag threshold so a fast click
        // (no real intention to drag) doesn't perturb the value.
        constexpr float DRAG_THRESHOLD_PX = 4.0f;
        float pixelDelta = mousePos.x - state.dragStartMouseX;
        if (!state.dragThresholdPassed) {
          if (std::abs(pixelDelta) >= DRAG_THRESHOLD_PX) {
            state.dragThresholdPassed = true;
            // Reset the start position so the value doesn't jump by the threshold
            // amount when crossing it.
            state.dragStartMouseX = mousePos.x;
          }
        } else {
          float delta = pixelDelta * effectiveSpeed;
          float newValue = state.dragStartValue + delta;

          if (min < max) {
            newValue = std::clamp(newValue, min, max);
          }

          if (std::abs(newValue - currentValue) > 0.00001f) {
            currentValue = newValue;
            valueChanged = true;
          }
        }
      } else {
        // Mouse released: stop dragging
        state.isDragging = false;
        state.dragThresholdPassed = false; // Phase B4
        ctx->activeWidgetId = 0;
        ctx->activeWidgetType = ActiveWidgetType::None;
      }
    }
  }

  // Write back value
  if (value) *value = currentValue;

  // --- Drawing ---

  // Label on the left (only if non-empty)
  if (hasLabel) {
    Vec2 labelPos(widgetPos.x,
                  widgetPos.y + (fieldHeight - labelSize.y) * 0.5f);
    ctx->renderer.DrawText(labelPos, label, labelStyle.color, labelStyle.fontSize);
  }

  // Field background — recessed (dark) / near-white (light) so it stands out.
  Color fieldBg = InputFieldBackground(ctx, hoverField && !state.isEditing);
  if (state.isDragging) {
    // Highlight during drag
    fieldBg = accentState.hover;
    fieldBg.a = 0.25f;
  }

  // Focus ring when editing or dragging
  if (state.isEditing || state.isDragging) {
    DrawFocusRing(ctx, fieldPos, fieldSize, panelStyle.cornerRadius);
  }

  ctx->renderer.DrawRectFilled(fieldPos, fieldSize, fieldBg, panelStyle.cornerRadius);
  // Visible 1px border so the field stands out from the surface (focus ring
  // takes over while editing/dragging).
  if (!state.isEditing && !state.isDragging) {
    ctx->renderer.DrawRect(fieldPos, fieldSize,
                           InputFieldBorder(ctx, hoverField),
                           panelStyle.cornerRadius);
  }

  // Draw value text or edit text
  float textPadding = panelStyle.padding.x * 0.5f;
  // Centrado vertical óptico (mismo cálculo que ComboBox): el ascender
  // del JSON puede ser >1 (ej. Inter 1.08), así que el bbox de fontSize
  // no centra visualmente el texto. Calculamos el centro del bbox visual
  // (top = baseline - capHeight, bottom = baseline) y lo alineamos al medio.
  const float dragAscender = ctx->renderer.GetFontAscender();
  const float dragCapHeight = 0.7f;
  const float dragVisualCenterOffset =
      (dragAscender - dragCapHeight * 0.5f) * valueStyle.fontSize;
  if (state.isEditing) {
    // Draw edit text with caret
    Vec2 textSize = MeasureTextCached(ctx, state.editText, valueStyle.fontSize);
    Vec2 textPos(fieldPos.x + textPadding,
                 fieldPos.y + fieldSize.y * 0.5f - dragVisualCenterOffset);
    ctx->renderer.DrawText(textPos, state.editText, valueStyle.color, valueStyle.fontSize);

    // Draw caret (brief 22 fase 4: test de existencia SIN crear entrada)
    auto wsIt = ctx->widgetStates.find(id);
    size_t caretPos = (wsIt != ctx->widgetStates.end() && wsIt->second.text)
                          ? wsIt->second.text->caret : state.editText.size();
    {
      float blinkAlpha = 0.5f + 0.5f * std::sin(ctx->frame * 0.1f);
      float caretX = MeasureTextCached(ctx, state.editText.substr(0, caretPos), valueStyle.fontSize).x;
      Vec2 caretDrawPos(textPos.x + caretX, fieldPos.y + textPadding * 0.5f);
      Vec2 caretDrawSize(1.5f, fieldSize.y - textPadding);
      Color caretColor = accentState.normal;
      caretColor.a = blinkAlpha;
      ctx->renderer.DrawRectFilled(caretDrawPos, caretDrawSize, caretColor, 0.0f);
    }
  } else {
    // Format and draw current value
    char buf[64];
    std::snprintf(buf, sizeof(buf), format ? format : "%.2f", currentValue);
    std::string valueText(buf);
    Vec2 textSize = MeasureTextCached(ctx, valueText, valueStyle.fontSize);
    // Center value text in the field (centrado vertical óptico, no bbox)
    Vec2 textPos(fieldPos.x + (fieldSize.x - textSize.x) * 0.5f,
                 fieldPos.y + fieldSize.y * 0.5f - dragVisualCenterOffset);
    ctx->renderer.DrawText(textPos, valueText, valueStyle.color, valueStyle.fontSize);
  }

  // Bottom accent line to indicate draggability
  if (!state.isEditing) {
    Color lineColor = state.isDragging ? accentState.normal : accentState.hover;
    lineColor.a = state.isDragging ? 0.8f : (hoverField ? 0.5f : 0.2f);
    float lineY = fieldPos.y + fieldSize.y - 2.0f;
    ctx->renderer.DrawRectFilled(
        Vec2(fieldPos.x + 2.0f, lineY),
        Vec2(fieldSize.x - 4.0f, 2.0f),
        lineColor, 1.0f);
  }

  ctx->lastItemPos = widgetPos;
  AdvanceCursor(ctx, finalSize);
  // Phase B1: publish drag-float state
  {
    bool isActiveDrag = (ctx->activeWidgetId == id &&
                         ctx->activeWidgetType == ActiveWidgetType::DragWidget);
    SetLastItem(id, widgetPos, widgetPos + finalSize,
                hoverField, isActiveDrag, ctx->focusedWidgetId == id, valueChanged);
  }
  return valueChanged;
}

bool DragInt(const std::string& label, int* value, float speed,
             int min, int max, std::optional<Vec2> pos) {
  UIContext* ctx = GetContext();
  if (!ctx) return false;

  int currentValue = value ? *value : 0;
  float asFloat = static_cast<float>(currentValue);
  bool changed = DragFloat(label, &asFloat, speed,
                           static_cast<float>(min), static_cast<float>(max),
                           "%.0f", pos);
  int newInt = static_cast<int>(std::round(asFloat));
  if (min < max) {
    newInt = std::clamp(newInt, min, max);
  }

  if (changed || newInt != currentValue) {
    if (value) *value = newInt;
    return true;
  }
  return false;
}

bool DragFloat3(const std::string& label, float values[3], float speed,
                float min, float max, const char* format,
                std::optional<Vec2> pos) {
  UIContext* ctx = GetContext();
  if (!ctx || !values) return false;

  const TextStyle& labelStyle = ctx->style.GetTextStyle(TypographyStyle::Body);
  Vec2 labelSize = MeasureTextCached(ctx, label, labelStyle.fontSize);

  float defaultFieldHeight = labelStyle.fontSize + 16.0f;
  bool hasLabel = !label.empty();
  float totalWidth = hasLabel ? 400.0f : 300.0f;
  Vec2 totalSize(totalWidth, defaultFieldHeight);
  LayoutConstraints constraints = ConsumeNextConstraints(SizeConstraint::Fill);
  Vec2 finalSize = ApplyConstraints(ctx, constraints, totalSize);
  totalWidth = finalSize.x;
  float fieldHeight = finalSize.y;

  Vec2 widgetPos;
  if (pos.has_value()) {
    widgetPos = ResolveAbsolutePosition(ctx, pos.value(), finalSize);
  } else {
    widgetPos = ctx->cursorPos;
  }

  // Draw label on left (25%) only if present
  float labelWidth = hasLabel ? totalWidth * 0.25f : 0.0f;
  if (hasLabel) {
    Vec2 labelPos(widgetPos.x,
                  widgetPos.y + (fieldHeight - labelSize.y) * 0.5f);
    ctx->renderer.DrawText(labelPos, label, labelStyle.color, labelStyle.fontSize);
  }

  // Three drag fields share the remaining width with small gaps
  float fieldsWidth = totalWidth - labelWidth;
  float gap = 6.0f;
  float singleFieldWidth = (fieldsWidth - gap * 2.0f) / 3.0f;

  bool anyChanged = false;
  bool anyHover = false;

  // Component labels and accent colors for X, Y, Z
  const char* componentLabels[3] = {"X", "Y", "Z"};
  Color componentColors[3] = {
    Color(0.9f, 0.3f, 0.3f, 1.0f),  // Red for X
    Color(0.3f, 0.8f, 0.3f, 1.0f),  // Green for Y
    Color(0.3f, 0.5f, 0.9f, 1.0f),  // Blue for Z
  };

  for (int i = 0; i < 3; ++i) {
    float fieldX = widgetPos.x + labelWidth + i * (singleFieldWidth + gap);
    Vec2 fieldPos(fieldX, widgetPos.y);
    Vec2 fieldSize(singleFieldWidth, fieldHeight);

    // Generate unique ID for each component
    std::string compId = label + "." + componentLabels[i];
    uint32_t id = GenerateId("DRAG_F3:", compId.c_str());

    DragWidgetState& state = ctx->dragStates[id];
    float currentVal = values[i];

    Vec2 mousePos(ctx->input.MouseX(), ctx->input.MouseY());
    bool hoverField = PointInRect(mousePos, fieldPos, fieldSize) &&
                      !IsMouseInputBlocked(ctx);
    if (hoverField) anyHover = true;
    bool leftPressed = ctx->input.IsMousePressed(0);
    bool leftDown = ctx->input.IsMouseDown(0);

    if (hoverField && !state.isEditing) {
      ctx->desiredCursor = UIContext::CursorType::ResizeH;
    }

    bool compChanged = false;

    // --- Edit mode ---
    if (state.isEditing) {
      std::string& editStr = state.editText;
      bool enterPressed = ctx->input.IsKeyPressed(UIKey::Enter) ||
                          ctx->input.IsKeyPressed(UIKey::KeypadEnter);
      bool clickedOutside = leftPressed && !hoverField;
      bool escapePressed = ctx->input.IsKeyPressed(UIKey::Escape);

      if (enterPressed || clickedOutside) {
        try {
          float parsed = std::stof(editStr);
          if (min < max) parsed = std::clamp(parsed, min, max);
          currentVal = parsed;
          compChanged = true;
        } catch (...) {}
        state.isEditing = false;
        if (ctx->activeWidgetId == id) {
          ctx->activeWidgetId = 0;
          ctx->activeWidgetType = ActiveWidgetType::None;
        }
      } else if (escapePressed) {
        state.isEditing = false;
        if (ctx->activeWidgetId == id) {
          ctx->activeWidgetId = 0;
          ctx->activeWidgetType = ActiveWidgetType::None;
        }
      } else {
        ctx->activeWidgetId = id;
        ctx->activeWidgetType = ActiveWidgetType::TextInput;

        // brief 22 (fase 4): caret en TextEditState; primer frame → caret al final
        bool firstEditFrame = ctx->GetWidgetState(id).text == nullptr;
        auto& tsEdit = ctx->GetTextState(id);
        if (firstEditFrame) tsEdit.caret = editStr.size();
        size_t& caret = tsEdit.caret;
        caret = std::min(caret, editStr.size());

        bool ctrlHeld = ctx->input.CtrlDown();

        if (!ctrlHeld) {
          const std::string& inputText = ctx->input.TextInputBuffer();
          if (!inputText.empty()) {
            editStr.insert(caret, inputText);
            caret += inputText.size();
          }
        }
        if (ctx->input.IsKeyPressed(UIKey::Backspace)) {
          if (caret > 0) { caret--; editStr.erase(caret, 1); }
        } else if (ctx->input.IsKeyPressed(UIKey::Delete)) {
          if (caret < editStr.size()) editStr.erase(caret, 1);
        } else if (ctx->input.IsKeyPressed(UIKey::Left)) {
          if (caret > 0) caret--;
        } else if (ctx->input.IsKeyPressed(UIKey::Right)) {
          if (caret < editStr.size()) caret++;
        } else if (ctx->input.IsKeyPressed(UIKey::Home)) {
          caret = 0;
        } else if (ctx->input.IsKeyPressed(UIKey::End)) {
          caret = editStr.size();
        }
      }
    }
    // --- Normal / Dragging ---
    else {
      if (hoverField && leftPressed) {
        uint64_t now = SDL_GetTicks();
        uint64_t elapsed = now - state.lastClickTime;
        state.lastClickTime = now;

        if (elapsed < DOUBLE_CLICK_MS) {
          state.isEditing = true;
          char buf[64];
          std::snprintf(buf, sizeof(buf), format ? format : "%.2f", currentVal);
          state.editText = buf;
          ctx->GetTextState(id).caret = state.editText.size(); // brief 22 (fase 4)
          ctx->activeWidgetId = id;
          ctx->activeWidgetType = ActiveWidgetType::TextInput;
        } else {
          state.isDragging = true;
          state.dragStartValue = currentVal;
          state.dragStartMouseX = mousePos.x;
          state.dragThresholdPassed = false; // Phase B4
          ctx->activeWidgetId = id;
          ctx->activeWidgetType = ActiveWidgetType::DragWidget;
        }
      }

      if (state.isDragging) {
        if (leftDown) {
          bool shiftHeld = ctx->input.ShiftDown();
          float effectiveSpeed = speed * (shiftHeld ? 0.1f : 1.0f);
          // Phase B4: drag threshold
          constexpr float DRAG_THRESHOLD_PX = 4.0f;
          float pixelDelta = mousePos.x - state.dragStartMouseX;
          if (!state.dragThresholdPassed) {
            if (std::abs(pixelDelta) >= DRAG_THRESHOLD_PX) {
              state.dragThresholdPassed = true;
              state.dragStartMouseX = mousePos.x;
            }
          } else {
            float delta = pixelDelta * effectiveSpeed;
            float newVal = state.dragStartValue + delta;
            if (min < max) newVal = std::clamp(newVal, min, max);
            if (std::abs(newVal - currentVal) > 0.00001f) {
              currentVal = newVal;
              compChanged = true;
            }
          }
        } else {
          state.isDragging = false;
          state.dragThresholdPassed = false; // Phase B4
          if (ctx->activeWidgetId == id) {
            ctx->activeWidgetId = 0;
            ctx->activeWidgetType = ActiveWidgetType::None;
          }
        }
      }
    }

    if (compChanged) {
      values[i] = currentVal;
      anyChanged = true;
    }

    // --- Draw component ---
    const PanelStyle& ps = ctx->style.panel;
    const ColorState& accent = ctx->style.button.background;

    Color fieldBg = InputFieldBackground(ctx, hoverField && !state.isEditing);
    if (state.isDragging) {
      fieldBg = accent.hover;
      fieldBg.a = 0.25f;
    }

    if (state.isEditing || state.isDragging) {
      DrawFocusRing(ctx, fieldPos, fieldSize, ps.cornerRadius);
    }

    ctx->renderer.DrawRectFilled(fieldPos, fieldSize, fieldBg, ps.cornerRadius);
    if (!state.isEditing && !state.isDragging) {
      ctx->renderer.DrawRect(fieldPos, fieldSize,
                             InputFieldBorder(ctx, hoverField), ps.cornerRadius);
    }

    float textPadding = ps.padding.x * 0.5f;
    const TextStyle& valStyle = ctx->style.GetTextStyle(TypographyStyle::Body);
    const TextStyle& axisStyle = ctx->style.GetTextStyle(TypographyStyle::BodyStrong);
    // Centrado vertical óptico: usa ascender real + capHeight aproximado
    const float dragAscender = ctx->renderer.GetFontAscender();
    const float dragCapHeight = 0.7f;
    const float dragVisualCenterOffset =
        (dragAscender - dragCapHeight * 0.5f) * valStyle.fontSize;

    // Axis letter prefix (X / Y / Z) in component color, replaces the 3px strip.
    std::string axisStr(1, componentLabels[i][0]);
    Vec2 axisSize = MeasureTextCached(ctx, axisStr, axisStyle.fontSize);
    float prefixGap = 4.0f;
    float prefixAreaW = axisSize.x + textPadding + prefixGap;
    Vec2 axisPos(fieldPos.x + textPadding,
                 fieldPos.y + fieldSize.y * 0.5f - dragVisualCenterOffset);
    ctx->renderer.DrawText(axisPos, axisStr, componentColors[i], axisStyle.fontSize);

    float valueAreaX = fieldPos.x + prefixAreaW;
    float valueAreaW = std::max(0.0f, fieldSize.x - prefixAreaW - textPadding);

    if (state.isEditing) {
      Vec2 textPos(valueAreaX,
                   fieldPos.y + fieldSize.y * 0.5f - dragVisualCenterOffset);
      ctx->renderer.DrawText(textPos, state.editText, valStyle.color, valStyle.fontSize);

      // brief 22 (fase 4): test de existencia SIN crear entrada
      auto wsIt = ctx->widgetStates.find(id);
      size_t caretPos = (wsIt != ctx->widgetStates.end() && wsIt->second.text)
                            ? wsIt->second.text->caret : state.editText.size();
      {
        float blinkAlpha = 0.5f + 0.5f * std::sin(ctx->frame * 0.1f);
        float caretX = MeasureTextCached(ctx, state.editText.substr(0, caretPos), valStyle.fontSize).x;
        Vec2 caretDrawPos(textPos.x + caretX, fieldPos.y + textPadding * 0.5f);
        Vec2 caretDrawSize(1.5f, fieldSize.y - textPadding);
        Color caretColor = accent.normal;
        caretColor.a = blinkAlpha;
        ctx->renderer.DrawRectFilled(caretDrawPos, caretDrawSize, caretColor, 0.0f);
      }
    } else {
      char buf[64];
      std::snprintf(buf, sizeof(buf), format ? format : "%.2f", currentVal);
      std::string valText(buf);
      Vec2 textSize = MeasureTextCached(ctx, valText, valStyle.fontSize);
      Vec2 textPos(valueAreaX + (valueAreaW - textSize.x) * 0.5f,
                   fieldPos.y + fieldSize.y * 0.5f - dragVisualCenterOffset);
      ctx->renderer.DrawText(textPos, valText, valStyle.color, valStyle.fontSize);
    }

    // Bottom accent line
    if (!state.isEditing) {
      Color lineColor = componentColors[i];
      lineColor.a = state.isDragging ? 0.8f : (hoverField ? 0.5f : 0.2f);
      float lineY = fieldPos.y + fieldSize.y - 2.0f;
      ctx->renderer.DrawRectFilled(
          Vec2(fieldPos.x + 2.0f, lineY),
          Vec2(fieldSize.x - 4.0f, 2.0f),
          lineColor, 1.0f);
    }
  }

  ctx->lastItemPos = widgetPos;
  AdvanceCursor(ctx, finalSize);
  // Phase B1: publish drag-float3 state (use base id so the whole row queries together)
  {
    uint32_t baseId = GenerateId("DRAGF3:", label.c_str());
    SetLastItem(baseId, widgetPos, widgetPos + finalSize,
                anyHover, false /* multi-component, no single active */,
                ctx->focusedWidgetId == baseId, anyChanged);
  }
  return anyChanged;
}

static bool ComboBoxImpl(const std::string &label, int *currentItem,
                         const std::vector<std::string> &items,
                         const std::vector<uint32_t> *icons,
                         float width, std::optional<Vec2> pos,
                         bool drawHeader);

bool ComboBox(const std::string &label, int *currentItem,
              const std::vector<std::string> &items, float width,
              std::optional<Vec2> pos) {
  return ComboBoxImpl(label, currentItem, items, nullptr, width, pos, true);
}

bool ComboBox(const std::string &label, int *currentItem,
              const std::vector<std::pair<std::string, uint32_t>> &items,
              float width, std::optional<Vec2> pos) {
  std::vector<std::string> labels;
  std::vector<uint32_t> ics;
  labels.reserve(items.size());
  ics.reserve(items.size());
  for (const auto &p : items) { labels.push_back(p.first); ics.push_back(p.second); }
  return ComboBoxImpl(label, currentItem, labels, &ics, width, pos, true);
}

bool ComboBoxNoLabel(const std::string &id, int *currentItem,
                     const std::vector<std::string> &items, float width,
                     std::optional<Vec2> pos) {
  return ComboBoxImpl(id, currentItem, items, nullptr, width, pos, false);
}

bool ComboBoxNoLabel(const std::string &id, int *currentItem,
                     const std::vector<std::pair<std::string, uint32_t>> &items,
                     float width, std::optional<Vec2> pos) {
  std::vector<std::string> labels;
  std::vector<uint32_t> ics;
  labels.reserve(items.size());
  ics.reserve(items.size());
  for (const auto &p : items) { labels.push_back(p.first); ics.push_back(p.second); }
  return ComboBoxImpl(id, currentItem, labels, &ics, width, pos, false);
}

static bool ComboBoxImpl(const std::string &label, int *currentItem,
                         const std::vector<std::string> &items,
                         const std::vector<uint32_t> *icons,
                         float width, std::optional<Vec2> pos,
                         bool drawHeader) {
  UIContext *ctx = GetContext();
  if (!ctx || items.empty())
    return false;

  uint32_t id = GenerateId("COMBO:", label.c_str());

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

  // Constantes de slot para icono y chevron (compartidas con dibujo)
  const float fieldIconSize = itemStyle.fontSize;
  const float fieldIconGap = 6.0f;
  // Chevron Lucide a tamaño de texto para coherencia visual con los iconos
  // del item seleccionado y el resto de la UI.
  const float arrowSize = itemStyle.fontSize;
  // Gap texto↔chevron igual al padding lateral para simetría visual:
  // (paddingX | texto | arrowGap | chevron | paddingX) con los tres espacios iguales.
  const float arrowGap = panelStyle.padding.x;
  const float arrowSlot = arrowSize + arrowGap;

  // Calcular SIEMPRE el ancho natural (texto del item más ancho + iconSlot
  // + paddings + chevron). Se usa tanto para auto-width como para garantizar
  // un mínimo después de aplicar constraints (Fill no debe achicarlo de más).
  float maxItemWidth = 0.0f;
  for (size_t i = 0; i < items.size(); ++i) {
    Vec2 itemTextSize = MeasureTextCached(ctx, items[i], itemStyle.fontSize);
    float itemIconSlot = 0.0f;
    if (icons && i < icons->size() && (*icons)[i] != 0u) {
      itemIconSlot = fieldIconSize + fieldIconGap;
    }
    float itemWidth = itemIconSlot + itemTextSize.x;
    if (itemWidth > maxItemWidth) maxItemWidth = itemWidth;
  }
  const float naturalWidth = panelStyle.padding.x * 2.0f + maxItemWidth + arrowSlot;
  if (width <= 0.0f) {
    width = naturalWidth;
  }

  Vec2 labelSize = drawHeader
                       ? MeasureTextCached(ctx, label, labelStyle.fontSize)
                       : Vec2(0.0f, 0.0f);
  float headerOffset = drawHeader ? (labelSize.y + labelSpacing) : 0.0f;
  float defaultFieldHeight = itemStyle.fontSize + 16.0f;
  Vec2 fieldSize(width, defaultFieldHeight);

  Vec2 totalSize(fieldSize.x, headerOffset + fieldSize.y);
  LayoutConstraints constraints = ConsumeNextConstraints(SizeConstraint::Fill);
  Vec2 finalSize = ApplyConstraints(ctx, constraints, totalSize);

  // Resolver posición: usar pos si se proporciona, sino usar cursor
  // directamente
  Vec2 widgetPos;
  if (pos.has_value()) {
    widgetPos = ResolveAbsolutePosition(ctx, pos.value(), finalSize);
  } else {
    widgetPos = ctx->cursorPos;
  }
  Vec2 fieldPos(widgetPos.x, widgetPos.y + headerOffset);
  if (finalSize.x > 0.0f) {
    fieldSize.x = finalSize.x;
  }
  // Respetar la altura del constraint: el campo ocupa lo que sobre tras el header.
  fieldSize.y = std::max(0.0f, finalSize.y - headerOffset);
  float fieldHeight = fieldSize.y;

  float mouseX = ctx->input.MouseX();
  float mouseY = ctx->input.MouseY();
  bool hoverField = IsMouseOver(ctx, fieldPos, fieldSize);

  // Estado del dropdown
  bool& boolSlot = ctx->GetWidgetState(id).boolVal; // brief 22 (fase 3)
  bool isOpen = boolSlot;

  // Solo un ComboBox abierto a la vez: si otro pasó a ser el abierto, éste
  // se considera cerrado.
  if (isOpen && ctx->openComboId != 0 && ctx->openComboId != id) {
    isOpen = false;
  }

  // Nota: si este campo cae bajo el dropdown de OTRO combo abierto, hoverField
  // ya es false (IsMouseOver aplica el bloqueo de input por overlay), así que
  // no hace falta una comprobación extra aquí: el click no abre el de debajo.
  bool clicked = hoverField && ctx->input.IsMousePressed(0);
  if (clicked) {
    isOpen = !isOpen;
    // Al abrir, este combo pasa a ser el único abierto (cierra los demás).
    ctx->openComboId = isOpen ? id : 0;
  }

  // Geometría del dropdown (puede abrir hacia arriba o hacia abajo según el
  // espacio disponible). Se calcula una sola vez y se reutiliza para el
  // hit-test de "click fuera" y para el encolado del render diferido, así
  // ambos coinciden incluso cuando el dropdown abre hacia arriba.
  float comboItemH = itemStyle.fontSize + panelStyle.padding.y;
  Vec2 comboViewport = ctx->renderer.GetViewportSize();
  float comboDesired = static_cast<float>(items.size()) * comboItemH;
  float comboSpaceBelow = comboViewport.y - (fieldPos.y + fieldSize.y) - 8.0f;
  float comboSpaceAbove = fieldPos.y - 8.0f;
  bool comboOpenUp =
      (comboDesired > comboSpaceBelow && comboSpaceAbove > comboSpaceBelow);
  float comboAvail = comboOpenUp ? comboSpaceAbove : comboSpaceBelow;
  int comboMaxRows = std::max(1, static_cast<int>(comboAvail / comboItemH));
  int comboVisibleRows = std::min(static_cast<int>(items.size()), comboMaxRows);
  float comboDropHeight = static_cast<float>(comboVisibleRows) * comboItemH;
  float comboDropY = comboOpenUp ? (fieldPos.y - comboDropHeight)
                                 : (fieldPos.y + fieldSize.y);
  Vec2 comboDropPos(fieldPos.x, comboDropY);
  Vec2 comboDropSize(fieldSize.x, comboDropHeight);

  // Publica el rect del dropdown para que los widgets de debajo bloqueen el
  // click cuando este combo es el abierto.
  if (isOpen) {
    ctx->openComboDropdownPos = comboDropPos;
    ctx->openComboDropdownSize = comboDropSize;
  }

  // Cerrar si se hace click fuera (del campo y del dropdown real)
  if (isOpen && ctx->input.IsMousePressed(0) && !hoverField) {
    bool hoverDropdown =
        (mouseX >= comboDropPos.x && mouseX <= comboDropPos.x + comboDropSize.x &&
         mouseY >= comboDropPos.y && mouseY <= comboDropPos.y + comboDropSize.y);
    if (!hoverDropdown) {
      isOpen = false;
    }
  }

  // Keyboard navigation when dropdown is open
  auto& highlightEntry = ctx->GetWidgetState(id).intVal; // brief 22 (fase 3)
  if (isOpen) {
    if (ctx->input.IsKeyPressed(UIKey::Down)) {
      highlightEntry = std::min(highlightEntry + 1, static_cast<int>(items.size()) - 1);
    }
    if (ctx->input.IsKeyPressed(UIKey::Up)) {
      highlightEntry = std::max(highlightEntry - 1, 0);
    }
    if (ctx->input.IsKeyPressed(UIKey::Enter) ||
        ctx->input.IsKeyPressed(UIKey::Space)) {
      if (currentItem) *currentItem = highlightEntry;
      isOpen = false;
    }
    if (ctx->input.IsKeyPressed(UIKey::Escape)) {
      isOpen = false;
    }
  } else {
    // Also activate with Enter/Space when has focus and dropdown is closed
    if (hasFocus && (ctx->input.IsKeyPressed(UIKey::Enter) ||
                     ctx->input.IsKeyPressed(UIKey::Space))) {
      highlightEntry = selectedIndex;
      isOpen = true;
    }
    // Up/Down while closed: directly change selection
    if (hasFocus && ctx->input.IsKeyPressed(UIKey::Down)) {
      if (currentItem && *currentItem < static_cast<int>(items.size()) - 1) {
        (*currentItem)++;
      }
    }
    if (hasFocus && ctx->input.IsKeyPressed(UIKey::Up)) {
      if (currentItem && *currentItem > 0) {
        (*currentItem)--;
      }
    }
  }

  // Actualizar estado
  boolSlot = isOpen;
  // Si este combo dejó de estar abierto (click fuera, ESC/Enter, o selección
  // de item vía dropdown diferido), liberar el slot único de "abierto".
  if (!isOpen && ctx->openComboId == id) {
    ctx->openComboId = 0;
  }

  // Campo con fondo distintivo (recessed en oscuro / casi blanco en claro).
  Color fieldBg = InputFieldBackground(ctx, hoverField);

  // Solo mostrar borde cuando tiene focus
  if (hasFocus) {
    DrawFocusRing(ctx, fieldPos, fieldSize, panelStyle.cornerRadius);
    fieldBg = accentState.hover;
    fieldBg.a = 0.15f;
  }
  ctx->renderer.DrawRectFilled(fieldPos, fieldSize, fieldBg,
                               panelStyle.cornerRadius);
  if (!hasFocus) {
    ctx->renderer.DrawRect(fieldPos, fieldSize,
                           InputFieldBorder(ctx, hoverField),
                           panelStyle.cornerRadius);
  }

  // Texto seleccionado (con icono opcional del item activo)
  uint32_t selectedCp = (icons && static_cast<size_t>(selectedIndex) < icons->size())
                          ? (*icons)[selectedIndex] : 0u;
  float fieldIconSlot = (selectedCp != 0u) ? (fieldIconSize + fieldIconGap) : 0.0f;

  // Padding interno adaptativo: distribuye el espacio sobrante (después del
  // texto y el chevron) en TRES huecos iguales — izquierda, gap texto↔chevron,
  // y derecha. Así el control siempre se ve simétrico aunque el width sea
  // estrecho (FixedSize) o muy holgado (Fill). Se cota a [4, panel.padding.x]
  // para no quedar pegado en estrechos ni con paddings exagerados en anchos.
  float availableChrome = fieldSize.x - maxItemWidth - arrowSize;
  float perPadding = std::clamp(availableChrome / 3.0f, 4.0f, panelStyle.padding.x);
  Vec2 textPadding(perPadding, panelStyle.padding.y);

  if (selectedCp != 0u) {
    DrawWidgetIcon(ctx, fieldPos, fieldSize, selectedCp, itemStyle.color,
                   fieldIconSize, textPadding.x, fieldIconGap);
  }

  // Texto: clip al área disponible para no solaparse con el chevron.
  // Centrado vertical exacto. DrawText pone la baseline en
  //   baseline = pos.y + ascender * fontSize
  // y el TOP visual de un glifo en cap-height en
  //   top    = baseline - capHeight * fontSize.
  // Para texto sin descenders (la mayoría: "Default", "Layer", etc.)
  // el bbox visual va de top a baseline, y su centro es:
  //   centerOffset = (ascender - capHeight/2) * fontSize
  // Centramos ese punto en el medio del field:
  //   pos.y = fieldCenter - centerOffset
  // Esto tolera fonts con ascender atípico (ej. Inter usa ~1.08, no 1.0).
  const float ascender = ctx->renderer.GetFontAscender();
  const float capHeight = 0.7f;  // EM units; típico para sans-serif
  const float visualCenterOffset = (ascender - capHeight * 0.5f) * itemStyle.fontSize;
  Vec2 textPos(fieldPos.x + textPadding.x + fieldIconSlot,
               fieldPos.y + fieldSize.y * 0.5f - visualCenterOffset);
  float textAreaWidth = std::max(0.0f, fieldSize.x - 2.0f * textPadding.x - fieldIconSlot - arrowSize);
  ctx->renderer.PushClipRect(
      Vec2(fieldPos.x + textPadding.x + fieldIconSlot, fieldPos.y),
      Vec2(textAreaWidth, fieldSize.y));
  ctx->renderer.DrawText(textPos, selectedText, itemStyle.color,
                         itemStyle.fontSize);
  ctx->renderer.PopClipRect();

  // Chevron Lucide: DrawWidgetIcon ya centra verticalmente respecto al rect.
  DrawWidgetIcon(ctx, fieldPos, fieldSize, Icons::ChevronDown, itemStyle.color,
                 arrowSize, fieldSize.x - arrowSize - textPadding.x, 0.0f);

  // Dibujar label (sólo en variantes con header)
  if (drawHeader) {
    ctx->renderer.DrawText(widgetPos, label, labelStyle.color,
                           labelStyle.fontSize);
  }

  // Check if a deferred dropdown reported a change for this combo
  bool valueChanged = false;
  bool& cbChanged = ctx->GetWidgetState(id).comboChanged; // brief 22 (fase 3)
  if (cbChanged) {
    valueChanged = true;
    cbChanged = false; // reset (equivalente al antiguo erase del flag de cambio)
    // Invoke valueChanged callback
    std::string idStr = "COMBO:" + label;
    auto cbIt = ctx->valueChangedCallbacks.find(idStr);
    if (cbIt != ctx->valueChangedCallbacks.end()) cbIt->second(idStr, currentItem);
  }

  // Queue dropdown for deferred rendering (to ensure it appears on top).
  // Reusa la geometría calculada arriba (abre arriba/abajo, recortada a filas).
  if (isOpen) {
    UIContext::DeferredComboDropdown dropdown;
    dropdown.fieldPos = fieldPos;
    dropdown.fieldSize = fieldSize;
    dropdown.dropdownPos = comboDropPos;
    dropdown.dropdownSize = comboDropSize;
    dropdown.items = items;
    if (icons) dropdown.iconCodepoints = *icons;
    dropdown.selectedIndex = selectedIndex;
    dropdown.comboId = id;
    dropdown.highlightIndex = highlightEntry;
    dropdown.currentItemPtr = currentItem;
    dropdown.fieldHeight = comboItemH;

    ctx->deferredComboDropdowns.push_back(dropdown);
  }

  // Update open state
  boolSlot = isOpen;

  ctx->lastItemPos = widgetPos;
  AdvanceCursor(ctx, finalSize);
  // Phase B1: publish combo box state
  SetLastItem(id, widgetPos, widgetPos + finalSize,
              hoverField, isOpen, hasFocus, valueChanged);
  return valueChanged;
}

// Phase C4: Searchable ComboBox. Filters items by case-insensitive substring,
// then delegates rendering to the regular ComboBox using the filtered list.
// Maps the filtered selection back to the original index.
bool ComboBoxSearchable(const std::string &label, int *currentItem,
                        const std::vector<std::string> &items, float width,
                        std::optional<Vec2> pos) {
  UIContext *ctx = GetContext();
  if (!ctx || items.empty()) return false;

  uint32_t searchId = GenerateId("COMBO_SEARCH:", label.c_str());
  std::string &filter = ctx->GetWidgetState(searchId).stringVal; // brief 22 (fase 3)

  auto toLower = [](std::string s) {
    for (auto &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
  };
  std::string flo = toLower(filter);

  std::vector<std::string> filtered;
  std::vector<int> indexMap;
  filtered.reserve(items.size());
  indexMap.reserve(items.size());
  for (int i = 0; i < static_cast<int>(items.size()); ++i) {
    if (flo.empty() || toLower(items[i]).find(flo) != std::string::npos) {
      filtered.push_back(items[i]);
      indexMap.push_back(i);
    }
  }
  if (filtered.empty()) {
    filtered.push_back("(no match)");
    indexMap.push_back(currentItem ? *currentItem : 0);
  }

  // Show inline search field; advances cursor before the combo
  TextInput("##search_" + label, &filter, width);

  // Map currentItem to filtered index
  int filteredCur = 0;
  if (currentItem) {
    for (int i = 0; i < static_cast<int>(indexMap.size()); ++i) {
      if (indexMap[i] == *currentItem) { filteredCur = i; break; }
    }
  }
  int prev = filteredCur;
  bool changed = ComboBox(label, &filteredCur, filtered, width, pos);
  if (filteredCur != prev || changed) {
    if (currentItem && filteredCur >= 0 && filteredCur < static_cast<int>(indexMap.size())) {
      *currentItem = indexMap[filteredCur];
    }
    return true;
  }
  return false;
}

bool ComboBoxSearchable(const std::string &label, int *currentItem,
                        const std::vector<std::pair<std::string, uint32_t>> &items,
                        float width, std::optional<Vec2> pos) {
  UIContext *ctx = GetContext();
  if (!ctx || items.empty()) return false;

  uint32_t searchId = GenerateId("COMBO_SEARCH:", label.c_str());
  std::string &filter = ctx->GetWidgetState(searchId).stringVal; // brief 22 (fase 3)

  auto toLower = [](std::string s) {
    for (auto &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
  };
  std::string flo = toLower(filter);

  std::vector<std::pair<std::string, uint32_t>> filtered;
  std::vector<int> indexMap;
  filtered.reserve(items.size());
  indexMap.reserve(items.size());
  for (int i = 0; i < static_cast<int>(items.size()); ++i) {
    if (flo.empty() || toLower(items[i].first).find(flo) != std::string::npos) {
      filtered.push_back(items[i]);
      indexMap.push_back(i);
    }
  }
  if (filtered.empty()) {
    filtered.push_back({"(no match)", 0u});
    indexMap.push_back(currentItem ? *currentItem : 0);
  }

  TextInput("##search_" + label, &filter, width);

  int filteredCur = 0;
  if (currentItem) {
    for (int i = 0; i < static_cast<int>(indexMap.size()); ++i) {
      if (indexMap[i] == *currentItem) { filteredCur = i; break; }
    }
  }
  int prev = filteredCur;
  bool changed = ComboBox(label, &filteredCur, filtered, width, pos);
  if (filteredCur != prev || changed) {
    if (currentItem && filteredCur >= 0 && filteredCur < static_cast<int>(indexMap.size())) {
      *currentItem = indexMap[filteredCur];
    }
    return true;
  }
  return false;
}

void RenderDeferredDropdowns() {
  UIContext *ctx = GetContext();
  if (!ctx) return;

  const PanelStyle &panelStyle = ctx->style.panel;
  const TextStyle &itemStyle = ctx->style.GetTextStyle(TypographyStyle::Body);
  const ColorState &accentState = ctx->style.button.background;

  float mouseX = ctx->input.MouseX();
  float mouseY = ctx->input.MouseY();

  // Los items del combo se procesan aquí (encima de todo): exime su propio
  // hover/click del bloqueo de input por overlay mientras dura este bloque.
  ctx->insideOverlayRender = true;

  // Render each queued dropdown
  for (auto &dropdown : ctx->deferredComboDropdowns) {
    Vec2 dropdownPos = dropdown.dropdownPos;
    Vec2 dropdownSize = dropdown.dropdownSize;
    float fieldHeight = dropdown.fieldHeight;

    // Flush batch before drawing dropdown to ensure it's on top
    ctx->renderer.FlushBatch();

    // Draw dropdown background with elevation (a dropdown is a flyout)
    ctx->renderer.DrawRectWithElevation(dropdownPos, dropdownSize,
                                        panelStyle.background,
                                        panelStyle.cornerRadius,
                                        Elevation::Z::Flyout);

    // Use acrylic effect for dropdown (Fluent Design)
    ctx->renderer.DrawRectAcrylic(dropdownPos, dropdownSize,
                                  panelStyle.background,
                                  panelStyle.cornerRadius, 0.95f);

    // Draw border
    Color dropdownBorder = FluentColors::BorderDark;
    dropdownBorder.a = 0.8f;
    ctx->renderer.DrawRect(dropdownPos, dropdownSize, dropdownBorder,
                           panelStyle.cornerRadius);

    // Draw dropdown items. Only draw the rows that fit inside the box so a
    // clamped dropdown (list taller than the available space) never paints a
    // row outside its background/border.
    bool itemClicked = false;
    int clickedIndex = -1;

    size_t visibleCount = dropdown.items.size();
    if (fieldHeight > 0.0f) {
      size_t fit = static_cast<size_t>(
          (dropdownSize.y + 0.5f) / fieldHeight);
      visibleCount = std::min(visibleCount, fit);
    }

    for (size_t i = 0; i < visibleCount; ++i) {
      Vec2 itemPos(dropdownPos.x,
                   dropdownPos.y + static_cast<float>(i) * fieldHeight);
      Vec2 itemSize(dropdownSize.x, fieldHeight);

      bool hoverItem = IsMouseOver(ctx, itemPos, itemSize);

      // Highlight hovered or keyboard-selected item. The first/last row must
      // round the corners that touch the box edge so the highlight doesn't
      // poke past the dropdown's rounded corners. Since the renderer rounds all
      // four corners uniformly, we round then square the interior edge back off.
      bool isHighlighted = hoverItem || (static_cast<int>(i) == dropdown.highlightIndex);
      if (isHighlighted) {
        float r = panelStyle.cornerRadius;
        bool firstRow = (i == 0);
        bool lastRow = (i + 1 == visibleCount);
        if (r > 0.0f && (firstRow || lastRow)) {
          ctx->renderer.DrawRectFilled(itemPos, itemSize, accentState.hover, r);
          if (lastRow && !firstRow) {
            // Square the top edge (interior, meets the row above).
            ctx->renderer.DrawRectFilled(itemPos, Vec2(itemSize.x, r),
                                         accentState.hover, 0.0f);
          } else if (firstRow && !lastRow) {
            // Square the bottom edge (interior, meets the row below).
            ctx->renderer.DrawRectFilled(
                Vec2(itemPos.x, itemPos.y + itemSize.y - r),
                Vec2(itemSize.x, r), accentState.hover, 0.0f);
          }
          // Single visible row: keep all four corners rounded (matches box).
        } else {
          ctx->renderer.DrawRectFilled(itemPos, itemSize, accentState.hover, 0.0f);
        }
      }

      // Per-row icon overrides the selection dot when present.
      uint32_t rowCp = (i < dropdown.iconCodepoints.size())
                         ? dropdown.iconCodepoints[i] : 0u;
      if (rowCp != 0u) {
        float rowIconSize = itemStyle.fontSize;
        Color rowIconColor = (static_cast<int>(i) == dropdown.selectedIndex)
                               ? accentState.normal
                               : itemStyle.color;
        DrawWidgetIcon(ctx, itemPos, itemSize, rowCp, rowIconColor,
                       rowIconSize, panelStyle.padding.x, 0.0f);
      } else if (static_cast<int>(i) == dropdown.selectedIndex) {
        float dotR = 3.0f;
        Vec2 dotCenter(itemPos.x + 8.0f, itemPos.y + fieldHeight * 0.5f);
        ctx->renderer.DrawCircle(dotCenter, dotR, accentState.normal, true);
      }

      // Draw item text centered vertically in the item row.
      // Reserve the same 20px gutter regardless of icon presence so rows align.
      float textY = itemPos.y + (fieldHeight - itemStyle.fontSize) * 0.5f;
      Vec2 itemTextPos(itemPos.x + panelStyle.padding.x + 20.0f, textY);
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
      if (clickedIndex != dropdown.selectedIndex) {
        ctx->GetWidgetState(dropdown.comboId).comboChanged = true; // brief 22 (fase 3)
      }
      *dropdown.currentItemPtr = clickedIndex;
      // Close the dropdown by setting its state to false
      ctx->GetWidgetState(dropdown.comboId).boolVal = false; // brief 22 (fase 3)
    }
  }

  ctx->insideOverlayRender = false;

  // Clear the deferred ComboBox dropdowns queue
  ctx->deferredComboDropdowns.clear();

  // Render deferred menu dropdowns
  for (auto &dropdown : ctx->deferredMenuDropdowns) {
    // Draw dropdown background with elevation and contrast (sin borde)
    Color dropdownBg = AdjustContainerBackground(ctx->style.panel.background, ctx->style.isDarkTheme);

    ctx->renderer.DrawRectWithElevation(
        dropdown.dropdownPos, dropdown.dropdownSize,
        dropdownBg, 4.0f, Elevation::Z::Flyout);

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

            // Reserve a leading icon slot if requested
            float iconSize = textStyle.fontSize;
            float iconGap = 6.0f;
            float iconSlot = (item.iconCodepoint != 0u) ? (iconSize + iconGap) : 0.0f;

            if (item.iconCodepoint != 0u) {
                DrawWidgetIcon(ctx, item.pos, item.size, item.iconCodepoint,
                               item.textColor, iconSize, panelStyle.padding.x, iconGap);
            }

            // Draw item text
            Vec2 textSize = MeasureTextCached(ctx, item.label, textStyle.fontSize);
            Vec2 textPos(item.pos.x + panelStyle.padding.x + iconSlot,
                         item.pos.y + (item.size.y - textSize.y) * 0.5f);

            ctx->renderer.DrawText(textPos, item.label, item.textColor, textStyle.fontSize);
        }
    }
  }

  // Clear the deferred menu dropdowns queue
  ctx->deferredMenuDropdowns.clear();

  // Render deferred tooltips (always on top)
  if (!ctx->deferredTooltips.empty()) {
    ctx->renderer.FlushBatch();
    auto savedClipStack = ctx->renderer.GetClipStack();
    // Temporarily clear clip stack for tooltips
    while(!ctx->renderer.GetClipStack().empty()) {
        ctx->renderer.PopClipRect();
    }

    for (const auto &tooltip : ctx->deferredTooltips) {
      float lineH = tooltip.fontSize * 1.3f;
      float alpha = tooltip.opacity;

      // Split text into lines
      std::vector<std::string> lines;
      {
        size_t start = 0;
        while (true) {
          size_t nl = tooltip.text.find('\n', start);
          if (nl == std::string::npos) {
            lines.push_back(tooltip.text.substr(start));
            break;
          }
          lines.push_back(tooltip.text.substr(start, nl - start));
          start = nl + 1;
        }
      }

      // Compute tooltip size
      float maxLineW = 0.0f;
      for (const auto& line : lines) {
        float lw = MeasureTextCached(ctx, line, tooltip.fontSize).x;
        maxLineW = std::max(maxLineW, std::min(lw, 300.0f));
      }
      Vec2 tooltipSize(maxLineW + 16.0f, static_cast<float>(lines.size()) * lineH + 10.0f);

      // Fondo con alto contraste
      Color bg = panelStyle.background;
      bg.a = alpha;

      // Sombra suave por elevación (un tooltip flota en z=Flyout); se atenúa
      // con el alpha del fade-in/out del tooltip.
      ctx->renderer.DrawElevationShadow(tooltip.pos, tooltipSize, 4.0f,
                                        Elevation::Z::Flyout, alpha);

      // Contenedor
      ctx->renderer.DrawRectFilled(tooltip.pos, tooltipSize, bg, 4.0f);
      Color borderColor(0.5f, 0.5f, 0.5f, alpha);
      ctx->renderer.DrawRect(tooltip.pos, tooltipSize, borderColor, 4.0f);

      // Draw each line
      Color textColor = itemStyle.color;
      textColor.a = alpha;
      for (size_t i = 0; i < lines.size(); ++i) {
        Vec2 textPos(tooltip.pos.x + 8.0f, tooltip.pos.y + 5.0f + i * lineH);
        ctx->renderer.DrawText(textPos, lines[i], textColor, tooltip.fontSize);
      }
    }

    ctx->renderer.FlushBatch();
    // Restore clip stack
    for (const auto& rect : savedClipStack) {
        ctx->renderer.PushClipRect(Vec2(rect.x, rect.y), Vec2(rect.width, rect.height));
    }
    
    ctx->deferredTooltips.clear();
  }

  // Liberar el rect de bloqueo del menú al final del frame, una vez procesados
  // todos los widgets de fondo. Si MenuItem cerró el menú a mitad de frame, el
  // bloqueo siguió vigente durante este frame (evitando el clickthrough) y aquí
  // se limpia para el siguiente.
  if (ctx->openMenuId != 0) {
    auto it = ctx->menuStates.find(ctx->openMenuId);
    if (it == ctx->menuStates.end() || !it->second.open) {
      ctx->openMenuId = 0;
    }
  }
}

// ════════════════════════════════════════════════════════════════════════════
// NumberBox (brief 14, section 4)
// ════════════════════════════════════════════════════════════════════════════
// Numeric field with +/- spinners and validation. Reuses the internal TextInput
// for editing (buffer in WidgetState.stringVal); parses on Enter/blur, clamps, reformats.
// Spinners repeat while held; the mouse wheel over the control steps ±step.
bool NumberBox(const std::string &label, double *value, double min, double max,
               double step, const char *format, std::optional<Vec2> pos) {
  UIContext *ctx = GetContext();
  if (!ctx || !value)
    return false;
  if (!format)
    format = "%.0f";

  uint32_t nbId = GenerateId("NUMBOX:", label.c_str());
  // Mismo id que el TextInput interno (mismo label, mismo scope de PushID).
  uint32_t txtId = GenerateId("TXT:", label.c_str());

  auto formatVal = [&](double v) {
    char b[64];
    std::snprintf(b, sizeof(b), format, v);
    return std::string(b);
  };

  // Buffer de edición (lo muta el TextInput interno) y flag de edición.
  std::string &buf = ctx->GetWidgetState(nbId).stringVal; // brief 22 (fase 3)
  bool &editing = ctx->GetWidgetState(nbId).boolVal;

  // Geometría: campo + spinners reservados a la derecha.
  bool inVertical =
      !ctx->layoutStack.empty() && ctx->layoutStack.back().isVertical;
  float totalW = S(160.0f);
  if (inVertical) {
    float av = GetCurrentAvailableSpace(ctx).x;
    if (av > 1.0f)
      totalW = av;
  }
  totalW = std::max(totalW, S(90.0f));
  float spinnerW = S(20.0f);
  float gap = S(2.0f);
  float fieldWidth = std::max(S(40.0f), totalW - spinnerW - gap);

  // Mientras NO se edita, el buffer refleja el valor (sincronía externa).
  if (!editing)
    buf = formatVal(*value);

  // Campo de texto interno (anatomía estándar: label arriba). Anchura fija para
  // reservar hueco a los spinners.
  LayoutConstraints fc;
  fc.width = SizeConstraint::Auto;
  fc.fixedWidth = fieldWidth;
  SetNextConstraints(fc);
  TextInput(label, &buf, fieldWidth, false, pos, nullptr, 0);

  Vec2 widgetPos = ctx->lastItemPos;
  Vec2 totalItemSize = ctx->lastItemSize;

  // Recalcular la geometría del campo (label arriba, campo abajo) — debe coincidir
  // con TextInput: labelStyle = Subtitle, labelSpacing = 4, alto = bodyFont+16.
  const TextStyle &labelStyle =
      ctx->style.GetTextStyle(TypographyStyle::Subtitle);
  const TextStyle &bodyStyle = ctx->style.GetTextStyle(TypographyStyle::Body);
  float labelSpacing = 4.0f;
  Vec2 labelSize = MeasureTextCached(ctx, label, labelStyle.fontSize);
  float fieldH = bodyStyle.fontSize + 16.0f;
  float fieldTop = widgetPos.y + labelSize.y + labelSpacing;
  // Si TextInput expandió la altura (constraint Fill vertical), anclar el campo
  // a la parte baja del item total.
  if (totalItemSize.y - fieldH > labelSize.y + labelSpacing + 0.5f)
    fieldTop = widgetPos.y + (totalItemSize.y - fieldH);

  // --- Commit en Enter/blur ---
  bool nowActive = (ctx->activeWidgetId == txtId &&
                    ctx->activeWidgetType == ActiveWidgetType::TextInput);
  // Blur por click fuera del control: el TextInput single-line solo desactiva con
  // Enter, así que aquí detectamos un click fuera del campo+spinners para confirmar.
  Vec2 mp(ctx->input.MouseX(), ctx->input.MouseY());
  bool clickAway = editing && ctx->input.IsMousePressed(0) &&
                   !PointInRect(mp, Vec2(widgetPos.x, fieldTop),
                                Vec2(totalW, fieldH));
  bool changed = false;
  if (nowActive && !clickAway) {
    editing = true;
  } else if (editing) {
    // Perdió el foco / pulsó Enter / click fuera → confirmar.
    editing = false;
    if (clickAway && ctx->activeWidgetId == txtId) {
      ctx->activeWidgetId = 0;
      ctx->activeWidgetType = ActiveWidgetType::None;
    }
    const char *s = buf.c_str();
    char *endp = nullptr;
    double parsed = std::strtod(s, &endp);
    if (endp != s) {
      double nv = std::clamp(parsed, min, max);
      if (nv != *value) {
        *value = nv;
        changed = true;
      }
    }
    // Re-formatear (también revierte un texto inválido).
    buf = formatVal(*value);
  }

  // --- Spinners +/- (repetición al mantener) ---
  Vec2 spinPos(widgetPos.x + fieldWidth + gap, fieldTop);
  Vec2 upRect(spinPos.x, spinPos.y);
  Vec2 upSize(spinnerW, std::floor(fieldH * 0.5f));
  Vec2 dnRect(spinPos.x, spinPos.y + upSize.y);
  Vec2 dnSize(spinnerW, fieldH - upSize.y);

  auto spinner = [&](const Vec2 &bp, const Vec2 &bs, bool isUp,
                     const char *rkeyName) -> bool {
    bool hov = IsMouseOver(ctx, bp, bs);
    ctx->renderer.DrawRectFilled(bp, bs, InputFieldBackground(ctx, hov), 0.0f);
    uint32_t cp = isUp ? Icons::Plus : Icons::Minus;
    float glyphSize = bs.y * 0.7f;
    DrawWidgetIcon(ctx, bp, bs, cp, bodyStyle.color, glyphSize,
                   (bs.x - glyphSize) * 0.5f, 0.0f);
    uint32_t rkey = GenerateId(rkeyName, label.c_str());
    float &timer = ctx->GetWidgetState(rkey).floatVal; // brief 22 (fase 3)
    bool pressed = hov && ctx->input.IsMousePressed(0);
    bool down = hov && ctx->input.IsMouseDown(0);
    bool tick = false;
    if (pressed) {
      tick = true;
      timer = -0.35f; // retardo inicial antes de la auto-repetición
    } else if (down) {
      timer += ctx->deltaTime;
      if (timer >= 0.05f) {
        tick = true;
        timer = 0.0f;
      }
    } else {
      timer = 0.0f;
    }
    return tick;
  };

  int delta = 0;
  if (IsRectInViewport(ctx, spinPos, Vec2(spinnerW, fieldH))) {
    if (spinner(upRect, upSize, true, "NBSPIN_UP:"))
      delta += 1;
    if (spinner(dnRect, dnSize, false, "NBSPIN_DN:"))
      delta -= 1;
    ctx->renderer.DrawRect(spinPos, Vec2(spinnerW, fieldH),
                           InputFieldBorder(ctx, false), 0.0f);
  }

  // --- Rueda del ratón sobre todo el control (campo + spinners) ---
  Vec2 wheelPos(widgetPos.x, fieldTop);
  Vec2 wheelSize(totalW, fieldH);
  if (!ctx->scrollConsumedThisFrame && !IsMouseInputBlocked(ctx) &&
      PointInRect(Vec2(ctx->input.MouseX(), ctx->input.MouseY()), wheelPos,
                  wheelSize)) {
    float wy = ctx->input.MouseWheelY();
    if (std::abs(wy) > 0.001f) {
      delta += (wy > 0.0f) ? 1 : -1;
      ctx->scrollConsumedThisFrame = true;
    }
  }

  if (delta != 0) {
    double nv = std::clamp(*value + delta * step, min, max);
    if (nv != *value) {
      *value = nv;
      changed = true;
    }
    buf = formatVal(*value);
    editing = false;
    if (ctx->activeWidgetId == txtId) {
      ctx->activeWidgetId = 0;
      ctx->activeWidgetType = ActiveWidgetType::None;
    }
  }

  // Accesibilidad: rol Slider/SpinButton, value formateado.
  ctx->widgetTree.FindOrCreate(nbId, ctx->frame, [&]() {
    auto node = std::make_unique<WidgetNode>(nbId);
    node->accessibleRole = WidgetNode::AccessibleRole::Slider;
    node->accessibleName = label;
    return node;
  });
  if (auto *node = ctx->widgetTree.FindById(nbId))
    node->accessibleValue = formatVal(*value);

  return changed;
}

// ============================================================================
// brief 17 — Texto y contenido rico
// SelectableText / PasswordBox / AutoSuggestBox / TokenizingTextBox.
// (HyperlinkButton lives in BasicWidgets.cpp; MarkdownView in MarkdownWidgets.cpp.)
//
// These reuse the shared selection-boundary helpers (WordBounds/LineBounds),
// the hit-test primitives (FindCaretPosition/GetGlyphAdvance) and the SO
// clipboard (InputState::Set/GetClipboardText). AutoSuggestBox/TokenizingTextBox
// build their popups on BeginFlyout (brief 14) and chips on BeginWrapPanel
// (brief 19), and reuse the internal TextInput for editing — which already
// claims IME per-field via ctx->imeOwnerId (brief 18).
// ============================================================================

// SelectableText — read-only text the user can select and copy. Selection is
// kept by id in WidgetState.intVal (anchor + caret byte offsets). Highlight is drawn
// before the glyphs. Mouse: down sets anchor, drag moves caret, double-click =
// word, triple-click = line. Keyboard (when focused): Shift+Left/Right extend,
// Ctrl+A select all, Ctrl+C / Ctrl+Insert copy to the OS clipboard.
void SelectableText(const std::string &id, const std::string &text,
                    float fontSize, bool wrap, std::optional<Vec2> pos) {
  UIContext *ctx = GetContext();
  if (!ctx)
    return;

  const TextStyle &bodyStyle = ctx->style.GetTextStyle(TypographyStyle::Body);
  float fs = fontSize > 0.0f ? fontSize : bodyStyle.fontSize;
  Color textColor = bodyStyle.color;
  float lineH = fs * 1.4f;

  // Resolve wrap width (logical layout width). 0 = no wrap.
  float wrapW = 0.0f;
  if (wrap) {
    Vec2 avail = GetCurrentAvailableSpace(ctx);
    wrapW = avail.x;
    if (wrapW <= 0.0f)
      wrapW = ctx->renderer.GetViewportSize().x - ctx->cursorPos.x;
  }

  // Build the visual line layout as byte ranges [start,end). We do our own greedy
  // word-wrap (instead of MeasureTextWrapped) so the SAME line breaks are used for
  // both the highlight rects and the per-line DrawText below — guaranteeing they
  // line up exactly. Every byte belongs to exactly one line; a trailing '\n' stays
  // at the end of its line and is stripped for display.
  std::vector<std::pair<size_t, size_t>> lines;
  {
    size_t i = 0, lineStart = 0, n = text.size();
    float lineW = 0.0f;
    while (i < n) {
      if (text[i] == '\n') {
        lines.push_back({lineStart, i + 1});
        ++i;
        lineStart = i;
        lineW = 0.0f;
        continue;
      }
      size_t tokStart = i;
      while (i < n && text[i] != ' ' && text[i] != '\t' && text[i] != '\n')
        ++i;
      size_t wordEnd = i;
      while (i < n && (text[i] == ' ' || text[i] == '\t'))
        ++i;
      size_t tokEnd = i;
      float wordW = MeasureTextCached(ctx, text.substr(tokStart, wordEnd - tokStart), fs).x;
      float tokW = MeasureTextCached(ctx, text.substr(tokStart, tokEnd - tokStart), fs).x;
      if (wrap && wrapW > 0.0f && lineW > 0.0f && lineW + wordW > wrapW) {
        lines.push_back({lineStart, tokStart});
        lineStart = tokStart;
        lineW = 0.0f;
      }
      lineW += tokW;
    }
    lines.push_back({lineStart, n});
  }
  int numLines = static_cast<int>(lines.size());

  auto lineDisp = [&](int ln) -> std::string {
    size_t s = lines[ln].first, e = lines[ln].second;
    if (e > s && text[e - 1] == '\n')
      --e;
    return text.substr(s, e - s);
  };

  float maxW = 0.0f;
  for (int ln = 0; ln < numLines; ++ln)
    maxW = std::max(maxW, MeasureTextCached(ctx, lineDisp(ln), fs).x);
  float totalH = static_cast<float>(numLines) * lineH;
  Vec2 totalSize(wrap && wrapW > 0.0f ? wrapW : maxW, totalH);

  LayoutConstraints constraints =
      ConsumeNextConstraints(wrap ? SizeConstraint::Fill : SizeConstraint::Auto);
  Vec2 finalSize = ApplyConstraints(ctx, constraints, totalSize);
  Vec2 widgetPos = pos.has_value()
                       ? ResolveAbsolutePosition(ctx, pos.value(), finalSize)
                       : ctx->cursorPos;

  uint32_t wid = GenerateId("SELTXT:", id.c_str());
  ctx->focusableWidgets.push_back(wid);

  // Selection range (anchor + caret) as byte offsets in WidgetState.intVal (2 sub-keys).
  int &anchorI = ctx->GetWidgetState(wid).intVal; // brief 22 (fase 3)
  int &caretI = ctx->GetWidgetState(AnimSlot(wid, 1)).intVal;
  bool &dragging = ctx->GetWidgetState(AnimSlot(wid, 2)).boolVal;
  auto clampIdx = [&](int v) -> size_t {
    if (v < 0)
      v = 0;
    return std::min(static_cast<size_t>(v), text.size());
  };
  size_t anchor = clampIdx(anchorI);
  size_t caret = clampIdx(caretI);

  // x within a single display line -> byte offset (glyph-advance hit-test at fs).
  auto caretInLine = [&](const std::string &ln, float targetX) -> size_t {
    float acc = 0.0f;
    const char *p = ln.data();
    const char *e = p + ln.size();
    size_t bp = 0;
    while (p < e) {
      uint32_t cp = DecodeUTF8(p, e);
      if (cp == 0)
        break;
      float w = ctx->renderer.GetGlyphAdvance(cp, fs);
      if (targetX < acc + w * 0.5f)
        return bp;
      acc += w;
      bp = static_cast<size_t>(p - ln.data());
    }
    return ln.size();
  };
  auto hitTest = [&](const Vec2 &m) -> size_t {
    int ln = static_cast<int>((m.y - widgetPos.y) / lineH);
    ln = std::clamp(ln, 0, numLines - 1);
    std::string disp = lineDisp(ln);
    float localX = m.x - widgetPos.x;
    return lines[ln].first + caretInLine(disp, std::max(localX, 0.0f));
  };

  Vec2 mousePos(ctx->input.MouseX(), ctx->input.MouseY());
  bool hover = IsMouseOver(ctx, widgetPos, finalSize);
  if (hover)
    ctx->desiredCursor = UIContext::CursorType::IBeam;
  bool focused = (ctx->focusedWidgetId == wid);

  if (ctx->input.IsMousePressed(0)) {
    if (hover) {
      ctx->focusedWidgetId = wid;
      focused = true;
      size_t hit = hitTest(mousePos);
      auto &ci = ctx->GetTextState(wid).clickInfo; // brief 22 (fase 4)
      uint64_t now = SDL_GetTicks();
      const uint64_t MS = 350;
      const float DIST = 4.0f;
      bool multi = (now - ci.lastClickTime) <= MS &&
                   std::abs(mousePos.x - ci.lastClickPos.x) <= DIST &&
                   std::abs(mousePos.y - ci.lastClickPos.y) <= DIST;
      ci.clickCount = multi ? std::min(ci.clickCount + 1, 3) : 1;
      ci.lastClickTime = now;
      ci.lastClickPos = mousePos;
      bool shiftClk = ctx->input.ShiftDown();
      if (shiftClk) {
        caret = hit; // extend from existing anchor
        ci.clickCount = 1;
      } else if (ci.clickCount == 2) {
        size_t ws, we;
        WordBounds(text, hit, ws, we);
        anchor = ws;
        caret = we;
      } else if (ci.clickCount >= 3) {
        size_t ls, le;
        LineBounds(text, hit, ls, le);
        anchor = ls;
        caret = le;
      } else {
        anchor = caret = hit;
      }
      dragging = true;
    }
  }
  if (!ctx->input.IsMouseDown(0))
    dragging = false;
  else if (dragging && !ctx->input.IsMousePressed(0))
    caret = hitTest(mousePos);

  if (focused) {
    bool ctrlHeld = ctx->input.CtrlDown();
    bool shiftHeld = ctx->input.ShiftDown();
    bool hasSel = anchor != caret;
    if (ctrlHeld && ctx->input.IsKeyPressed(UIKey::A)) {
      anchor = 0;
      caret = text.size();
    } else if (ctrlHeld && (ctx->input.IsKeyPressed(UIKey::C) ||
                            ctx->input.IsKeyPressed(UIKey::Insert))) {
      if (hasSel) {
        size_t s = std::min(anchor, caret), e = std::max(anchor, caret);
        ctx->input.SetClipboardText(text.substr(s, e - s));
      }
    } else if (ctx->input.IsKeyPressed(UIKey::Left)) {
      if (!shiftHeld && hasSel) {
        caret = anchor = std::min(anchor, caret);
      } else {
        if (caret > 0)
          caret = Utf8PrevCodepoint(text, caret);
        if (!shiftHeld)
          anchor = caret;
      }
    } else if (ctx->input.IsKeyPressed(UIKey::Right)) {
      if (!shiftHeld && hasSel) {
        caret = anchor = std::max(anchor, caret);
      } else {
        if (caret < text.size())
          caret = Utf8NextCodepoint(text, caret);
        if (!shiftHeld)
          anchor = caret;
      }
    }
  }

  anchorI = static_cast<int>(anchor);
  caretI = static_cast<int>(caret);

  if (IsRectInViewport(ctx, widgetPos, finalSize)) {
    // Highlight (accent, translucent) BEFORE the glyphs.
    if (anchor != caret) {
      size_t selS = std::min(anchor, caret), selE = std::max(anchor, caret);
      Color sel = ctx->style.button.background.normal;
      sel.a = 0.35f;
      for (int ln = 0; ln < numLines; ++ln) {
        size_t ls = lines[ln].first, le = lines[ln].second;
        size_t a = std::max(selS, ls);
        size_t b = std::min(selE, le);
        if (b <= a)
          continue;
        size_t dispEnd = (le > ls && text[le - 1] == '\n') ? le - 1 : le;
        size_t da = std::min(a, dispEnd);
        size_t db = std::min(b, dispEnd);
        float x0 = MeasureTextCached(ctx, text.substr(ls, da - ls), fs).x;
        float x1 = MeasureTextCached(ctx, text.substr(ls, db - ls), fs).x;
        if (b > dispEnd)
          x1 += fs * 0.3f; // selection runs through the line break
        float top = widgetPos.y + static_cast<float>(ln) * lineH;
        ctx->renderer.DrawRectFilled(Vec2(widgetPos.x + x0, top + 1.0f),
                                     Vec2(std::max(0.0f, x1 - x0), lineH - 2.0f),
                                     sel, 2.0f);
      }
    }
    for (int ln = 0; ln < numLines; ++ln) {
      float top = widgetPos.y + static_cast<float>(ln) * lineH;
      ctx->renderer.DrawText(Vec2(widgetPos.x, top + (lineH - fs) * 0.5f),
                             lineDisp(ln), textColor, fs);
    }
  }

  ctx->lastItemPos = widgetPos;
  if (pos.has_value())
    ctx->lastItemSize = finalSize;
  else
    AdvanceCursor(ctx, finalSize);
  SetLastItem(wid, widgetPos, widgetPos + finalSize, hover, dragging, focused,
              false);
}

// PasswordBox — single-line masked field. Like a single-line TextInput but draws
// '•' per codepoint, with an eye toggle to reveal. Copy/cut are intentionally
// disabled so the secret cannot leave via the clipboard. Paste IS allowed.
// Claims IME per-field on focus (ctx->imeOwnerId), like TextInput.
bool PasswordBox(const std::string &id, std::string *value,
                 const std::string &placeholder, std::optional<Vec2> pos) {
  UIContext *ctx = GetContext();
  if (!ctx)
    return false;

  const TextStyle &ts = ctx->style.GetTextStyle(TypographyStyle::Body);
  const PanelStyle &panelStyle = ctx->style.panel;
  const ColorState &accentState = ctx->style.button.background;
  float fs = ts.fontSize;
  float fieldH = fs + 16.0f;

  Vec2 totalSize(220.0f, fieldH);
  LayoutConstraints c = ConsumeNextConstraints(SizeConstraint::Fill);
  Vec2 finalSize = ApplyConstraints(ctx, c, totalSize);
  finalSize.y = std::max(finalSize.y, fieldH);
  Vec2 widgetPos = pos.has_value()
                       ? ResolveAbsolutePosition(ctx, pos.value(), finalSize)
                       : ctx->cursorPos;
  Vec2 fieldPos = widgetPos;
  Vec2 fieldSize(finalSize.x, fieldH);

  uint32_t wid = GenerateId("PWD:", id.c_str());
  ctx->focusableWidgets.push_back(wid);

  std::string *textPtr = value;
  if (!textPtr) {
    textPtr = &ctx->GetWidgetState(wid).stringVal; // brief 22 (fase 3)
  }
  std::string &textRef = *textPtr;

  // brief 22 (fase 4): caret/anchor en TextEditState. Primer frame → caret al
  // final (try_emplace original); anchor default ya es SIZE_MAX.
  bool firstTextFrame = ctx->GetWidgetState(wid).text == nullptr;
  auto &tstate = ctx->GetTextState(wid);
  if (firstTextFrame) tstate.caret = textRef.size();
  size_t &caret = tstate.caret;
  caret = std::min(caret, textRef.size());
  size_t &selAnchor = tstate.anchor;
  bool &reveal = ctx->GetWidgetState(AnimSlot(wid, 3)).boolVal; // brief 22 (fase 3)

  auto HasSelection = [&]() { return selAnchor != SIZE_MAX && selAnchor != caret; };
  auto SelStart = [&]() -> size_t { return HasSelection() ? std::min(selAnchor, caret) : caret; };
  auto SelEnd = [&]() -> size_t { return HasSelection() ? std::max(selAnchor, caret) : caret; };
  auto ClearSel = [&]() { selAnchor = SIZE_MAX; };
  auto DelSel = [&]() {
    if (!HasSelection())
      return;
    size_t s = SelStart(), e = SelEnd();
    textRef.erase(s, e - s);
    caret = s;
    ClearSel();
  };

  // codepoint <-> byte mapping (masking is per-codepoint, not per-byte).
  auto byteToCp = [&](size_t b) -> size_t {
    const char *p = textRef.data();
    const char *end = textRef.data() + textRef.size();
    const char *stop = textRef.data() + std::min(b, textRef.size());
    size_t n = 0;
    while (p < stop) {
      DecodeUTF8(p, end);
      ++n;
    }
    return n;
  };
  auto cpToByte = [&](size_t k) -> size_t {
    const char *p = textRef.data();
    const char *end = textRef.data() + textRef.size();
    size_t n = 0;
    while (p < end && n < k) {
      DecodeUTF8(p, end);
      ++n;
    }
    return static_cast<size_t>(p - textRef.data());
  };
  auto totalCp = [&]() -> size_t { return byteToCp(textRef.size()); };
  const std::string BULLET = "\xE2\x80\xA2"; // U+2022, 3 bytes
  auto displayStr = [&]() -> std::string {
    if (reveal)
      return textRef;
    std::string s;
    size_t n = totalCp();
    s.reserve(n * BULLET.size());
    for (size_t i = 0; i < n; ++i)
      s += BULLET;
    return s;
  };
  // display-byte length of the prefix up to byte caret `b`.
  auto dispPrefix = [&](size_t b) -> std::string {
    if (reveal)
      return textRef.substr(0, std::min(b, textRef.size()));
    std::string s;
    size_t k = byteToCp(b);
    for (size_t i = 0; i < k; ++i)
      s += BULLET;
    return s;
  };
  auto hitToByte = [&](float localX) -> size_t {
    if (reveal)
      return FindCaretPosition(textRef, localX, ctx);
    float bw = ctx->renderer.GetGlyphAdvance(0x2022, fs);
    size_t k = bw > 0.0f ? static_cast<size_t>(std::max(0.0f, (localX + bw * 0.5f) / bw)) : 0;
    k = std::min(k, totalCp());
    return cpToByte(k);
  };

  float textPadding = panelStyle.padding.x * 0.5f;
  float eyeW = fieldH;
  Vec2 eyePos(fieldPos.x + fieldSize.x - eyeW, fieldPos.y);
  Vec2 eyeSize(eyeW, fieldSize.y);
  float textAreaRight = fieldPos.x + fieldSize.x - eyeW;

  Vec2 mousePos(ctx->input.MouseX(), ctx->input.MouseY());
  bool blocked = IsMouseInputBlocked(ctx);
  bool hoverField = PointInRect(mousePos, fieldPos, Vec2(fieldSize.x - eyeW, fieldSize.y)) && !blocked;
  bool hoverEye = PointInRect(mousePos, eyePos, eyeSize) && !blocked;
  if (hoverField)
    ctx->desiredCursor = UIContext::CursorType::IBeam;

  bool leftPressed = ctx->input.IsMousePressed(0);
  bool leftDown = ctx->input.IsMouseDown(0);

  if (leftPressed) {
    if (hoverEye) {
      reveal = !reveal;
    } else if (hoverField) {
      ctx->activeWidgetId = wid;
      ctx->activeWidgetType = ActiveWidgetType::TextInput;
      ctx->focusedWidgetId = wid;
      float localX = mousePos.x - (fieldPos.x + textPadding);
      size_t nc = hitToByte(std::max(localX, 0.0f));
      if (ctx->input.ShiftDown() && selAnchor != SIZE_MAX) {
        caret = nc;
      } else {
        caret = nc;
        selAnchor = caret;
        ClearSel();
      }
    } else if (ctx->activeWidgetId == wid &&
               ctx->activeWidgetType == ActiveWidgetType::TextInput) {
      ctx->activeWidgetId = 0;
      ctx->activeWidgetType = ActiveWidgetType::None;
    }
  }
  if (!leftPressed && leftDown && ctx->activeWidgetId == wid &&
      ctx->activeWidgetType == ActiveWidgetType::TextInput) {
    float localX = mousePos.x - (fieldPos.x + textPadding);
    caret = hitToByte(std::max(localX, 0.0f));
  }

  bool hasFocus = ctx->activeWidgetId == wid &&
                  ctx->activeWidgetType == ActiveWidgetType::TextInput;
  bool valueChanged = false;

  // brief 18.4: claim IME per-field while focused; release on blur.
  if (ctx->window) {
    if (hasFocus) {
      if (ctx->imeOwnerId != wid) {
        SDL_StartTextInput(static_cast<SDL_Window*>(ctx->window));
        ctx->imeOwnerId = wid;
      }
      SDL_Rect area{static_cast<int>(fieldPos.x), static_cast<int>(fieldPos.y),
                    static_cast<int>(fieldSize.x), static_cast<int>(fieldSize.y)};
      SDL_SetTextInputArea(static_cast<SDL_Window*>(ctx->window), &area, 0);
    } else if (ctx->imeOwnerId == wid) {
      SDL_StopTextInput(static_cast<SDL_Window*>(ctx->window));
      ctx->imeOwnerId = 0;
    }
  }

  if (hasFocus) {
    bool ctrlHeld = ctx->input.CtrlDown();
    bool shiftHeld = ctx->input.ShiftDown();

    if (ctrlHeld && ctx->input.IsKeyPressed(UIKey::A)) {
      selAnchor = 0;
      caret = textRef.size();
    }
    // Ctrl+C / Ctrl+X are intentionally NOT handled (do not leak the secret).
    else if (ctrlHeld && ctx->input.IsKeyPressed(UIKey::V)) {
      std::string clip = ctx->input.GetClipboardText();
      for (char &ch : clip)
        if (ch == '\n' || ch == '\r')
          ch = ' ';
      if (!clip.empty()) {
        if (HasSelection())
          DelSel();
        textRef.insert(caret, clip);
        caret += clip.size();
        valueChanged = true;
      }
    } else {
      if (!ctrlHeld) {
        const std::string &inputText = ctx->input.TextInputBuffer();
        if (!inputText.empty()) {
          if (HasSelection())
            DelSel();
          textRef.insert(caret, inputText);
          caret += inputText.size();
          valueChanged = true;
        }
      }
      if (ctx->input.IsKeyPressed(UIKey::Backspace)) {
        if (HasSelection()) {
          DelSel();
          valueChanged = true;
        } else if (caret > 0) {
          size_t prev = Utf8PrevCodepoint(textRef, caret);
          textRef.erase(prev, caret - prev);
          caret = prev;
          valueChanged = true;
        }
      } else if (ctx->input.IsKeyPressed(UIKey::Delete)) {
        if (HasSelection()) {
          DelSel();
          valueChanged = true;
        } else if (caret < textRef.size()) {
          size_t next = Utf8NextCodepoint(textRef, caret);
          textRef.erase(caret, next - caret);
          valueChanged = true;
        }
      } else if (ctx->input.IsKeyPressed(UIKey::Left)) {
        if (shiftHeld && selAnchor == SIZE_MAX)
          selAnchor = caret;
        else if (!shiftHeld)
          ClearSel();
        if (caret > 0)
          caret = Utf8PrevCodepoint(textRef, caret);
      } else if (ctx->input.IsKeyPressed(UIKey::Right)) {
        if (shiftHeld && selAnchor == SIZE_MAX)
          selAnchor = caret;
        else if (!shiftHeld)
          ClearSel();
        if (caret < textRef.size())
          caret = Utf8NextCodepoint(textRef, caret);
      } else if (ctx->input.IsKeyPressed(UIKey::Home)) {
        if (shiftHeld && selAnchor == SIZE_MAX)
          selAnchor = caret;
        else if (!shiftHeld)
          ClearSel();
        caret = 0;
      } else if (ctx->input.IsKeyPressed(UIKey::End)) {
        if (shiftHeld && selAnchor == SIZE_MAX)
          selAnchor = caret;
        else if (!shiftHeld)
          ClearSel();
        caret = textRef.size();
      } else if (ctx->input.IsKeyPressed(UIKey::Enter) ||
                 ctx->input.IsKeyPressed(UIKey::KeypadEnter)) {
        ctx->activeWidgetId = 0;
        ctx->activeWidgetType = ActiveWidgetType::None;
      }
    }
  }

  // ---- draw ----
  Color bgColor = InputFieldBackground(ctx, hoverField && !hasFocus);
  if (hasFocus) {
    DrawFocusRing(ctx, fieldPos, fieldSize, panelStyle.cornerRadius);
    bgColor = accentState.hover;
    bgColor.a = 0.15f;
  }
  ctx->renderer.DrawRectFilled(fieldPos, fieldSize, bgColor, panelStyle.cornerRadius);
  ctx->renderer.DrawInsetShadow(fieldPos, fieldSize, panelStyle.cornerRadius, 2.0f,
                                Color(0.0f, 0.0f, 0.0f, 0.16f));
  if (!hasFocus)
    ctx->renderer.DrawRect(fieldPos, fieldSize, InputFieldBorder(ctx, hoverField),
                           panelStyle.cornerRadius);

  std::string disp = displayStr();
  Vec2 textPos(fieldPos.x + textPadding,
               fieldPos.y + (fieldSize.y - fs) * 0.5f);

  ctx->renderer.PushClipRect(Vec2(fieldPos.x + textPadding, fieldPos.y),
                             Vec2(std::max(0.0f, textAreaRight - fieldPos.x - textPadding * 2.0f),
                                  fieldSize.y));
  if (hasFocus && HasSelection()) {
    float sx = MeasureTextCached(ctx, dispPrefix(SelStart()), fs).x;
    float ex = MeasureTextCached(ctx, dispPrefix(SelEnd()), fs).x;
    Color sel = accentState.normal;
    sel.a = 0.35f;
    ctx->renderer.DrawRectFilled(Vec2(textPos.x + sx, fieldPos.y + 2.0f),
                                 Vec2(std::max(0.0f, ex - sx), fieldSize.y - 4.0f),
                                 sel, 2.0f);
  }
  if (textRef.empty() && !hasFocus && !placeholder.empty()) {
    Color ph = ts.color;
    ph.a *= 0.4f;
    ctx->renderer.DrawText(textPos, placeholder, ph, fs);
  } else {
    ctx->renderer.DrawText(textPos, disp, ts.color, fs);
  }
  if (hasFocus && !HasSelection()) {
    float caretX = textPos.x + MeasureTextCached(ctx, dispPrefix(caret), fs).x;
    float blink = 0.5f + 0.5f * std::sin(ctx->frame * 0.1f);
    Color cc = accentState.normal;
    cc.a = blink;
    ctx->renderer.DrawRectFilled(Vec2(caretX, fieldPos.y + textPadding * 0.5f),
                                 Vec2(1.5f, fieldSize.y - textPadding), cc, 0.0f);
  }
  ctx->renderer.PopClipRect();

  // Eye toggle (Lucide). Eye = revealed, EyeOff = masked.
  Color eyeColor = ts.color;
  eyeColor.a = hoverEye ? 1.0f : 0.6f;
  DrawWidgetIcon(ctx, eyePos, eyeSize, reveal ? Icons::Eye : Icons::EyeOff,
                 eyeColor, fs, (eyeW - fs) * 0.5f, 0.0f);

  ctx->lastItemPos = widgetPos;
  if (pos.has_value())
    ctx->lastItemSize = finalSize;
  else
    AdvanceCursor(ctx, finalSize);
  SetLastItem(wid, widgetPos, widgetPos + finalSize, hoverField, hasFocus,
              hasFocus, valueChanged);
  return valueChanged;
}

// Shared: case-insensitive substring search; returns byte offset or npos.
static size_t CaseInsensitiveFind(const std::string &hay, const std::string &needle) {
  if (needle.empty())
    return std::string::npos;
  auto lower = [](char ch) {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  };
  if (needle.size() > hay.size())
    return std::string::npos;
  for (size_t i = 0; i + needle.size() <= hay.size(); ++i) {
    size_t j = 0;
    for (; j < needle.size(); ++j)
      if (lower(hay[i + j]) != lower(needle[j]))
        break;
    if (j == needle.size())
      return i;
  }
  return std::string::npos;
}

// Draw a suggestion row with the matched substring (of `query`) highlighted in
// the accent color. Returns true when the row is clicked.
static bool DrawSuggestionRow(UIContext *ctx, const std::string &label,
                              const std::string &query, float rowW, float rowH,
                              float fs, bool highlighted) {
  Vec2 rowPos = ctx->cursorPos;
  Vec2 rowSize(rowW, rowH);
  const ColorState &accent = ctx->style.button.background;
  bool hover = IsMouseOver(ctx, rowPos, rowSize);
  if (hover || highlighted) {
    Color hl = accent.hover;
    ctx->renderer.DrawRectFilled(rowPos, rowSize, hl, 4.0f);
  }
  const TextStyle &ts = ctx->style.GetTextStyle(TypographyStyle::Body);
  Vec2 tp(rowPos.x + 8.0f, rowPos.y + (rowH - fs) * 0.5f);
  ctx->renderer.DrawText(tp, label, ts.color, fs);
  size_t m = CaseInsensitiveFind(label, query);
  if (m != std::string::npos) {
    float mx = MeasureTextCached(ctx, label.substr(0, m), fs).x;
    ctx->renderer.DrawText(Vec2(tp.x + mx, tp.y), label.substr(m, query.size()),
                           accent.normal, fs);
  }
  AdvanceCursor(ctx, rowSize);
  return hover && ctx->input.IsMousePressed(0);
}

// AutoSuggestBox — search field + popup of suggestions (TextInput + BeginFlyout).
// Up/Down move the highlight, Enter picks it, Esc closes, click picks. Returns
// the chosen suggestion this frame (or "").
std::string AutoSuggestBox(
    const std::string &id, std::string *text,
    const std::function<std::vector<std::string>(const std::string &)> &suggestionsFn,
    const std::string &placeholder) {
  UIContext *ctx = GetContext();
  if (!ctx)
    return "";

  uint32_t wid = GenerateId("ASB:", id.c_str());
  std::string *textPtr = text;
  if (!textPtr) {
    textPtr = &ctx->GetWidgetState(wid).stringVal; // brief 22 (fase 3)
  }

  // Inner field is scoped by id so its empty/hidden label produces a stable id.
  PushID(id.c_str());
  TextInput("##field", textPtr, 240.0f, false, std::nullopt,
            placeholder.empty() ? nullptr : placeholder.c_str());
  uint32_t tid = GenerateId("TXT:", "##field");
  PopID();

  Vec2 mn, mx;
  GetItemRect(&mn, &mx);
  Rect anchorRect(mn, mx - mn);

  bool fieldFocused = (ctx->activeWidgetId == tid &&
                       ctx->activeWidgetType == ActiveWidgetType::TextInput);

  std::vector<std::string> sugg = suggestionsFn ? suggestionsFn(*textPtr)
                                                : std::vector<std::string>{};
  std::string flyId = "ASB_FLY:" + id;
  uint32_t hlKey = GenerateId("ASB_HL:", id.c_str());
  int &hl = ctx->GetWidgetState(hlKey).intVal; // brief 22 (fase 3)

  std::string result;

  if (fieldFocused && !sugg.empty())
    OpenFlyout(flyId);
  else if (sugg.empty())
    CloseFlyout(flyId);

  if (IsFlyoutOpen(flyId) && !sugg.empty()) {
    int n = static_cast<int>(sugg.size());
    if (hl >= n)
      hl = n - 1;
    if (ctx->input.IsKeyPressed(UIKey::Down))
      hl = (hl + 1) % n;
    else if (ctx->input.IsKeyPressed(UIKey::Up))
      hl = (hl <= 0) ? n - 1 : hl - 1;
    if ((ctx->input.IsKeyPressed(UIKey::Enter) ||
         ctx->input.IsKeyPressed(UIKey::KeypadEnter)) &&
        hl >= 0 && hl < n) {
      result = sugg[hl];
      *textPtr = result;
      CloseFlyout(flyId);
    }
  } else {
    hl = -1;
  }

  if (IsFlyoutOpen(flyId)) {
    if (BeginFlyout(flyId, anchorRect, FlyoutPlacement::BottomEdgeAlignedLeft)) {
      const TextStyle &ts = ctx->style.GetTextStyle(TypographyStyle::Body);
      float fs = ts.fontSize;
      float rowH = fs + 12.0f;
      float rowW = std::max(anchorRect.size.x - ctx->style.panel.padding.x * 2.0f, 80.0f);
      for (int i = 0; i < static_cast<int>(sugg.size()); ++i) {
        if (DrawSuggestionRow(ctx, sugg[i], *textPtr, rowW, rowH, fs, i == hl)) {
          result = sugg[i];
          *textPtr = result;
          CloseFlyout(flyId);
        }
      }
      EndFlyout();
    }
  }

  return result;
}

// TokenizingTextBox / Chips — multi-value entry. Chips (pills with an "x") wrap
// via BeginWrapPanel, followed by an inline text field. Enter / comma confirm a
// token; Backspace on an empty field deletes the last chip; clicking "x" deletes
// that chip. Optional suggestions via BeginFlyout. Returns true when the token
// list changes.
bool TokenizingTextBox(
    const std::string &id, std::vector<std::string> *tokens,
    const std::string &placeholder,
    const std::function<std::vector<std::string>(const std::string &)> &suggestionsFn) {
  UIContext *ctx = GetContext();
  if (!ctx || !tokens)
    return false;

  const TextStyle &ts = ctx->style.GetTextStyle(TypographyStyle::Body);
  const ColorState &accent = ctx->style.button.background;
  float fs = ts.fontSize;
  bool changed = false;

  uint32_t bufKey = GenerateId("TTB_BUF:", id.c_str());
  std::string &buf = ctx->GetWidgetState(bufKey).stringVal; // brief 22 (fase 3)

  auto trim = [](std::string s) {
    size_t a = s.find_first_not_of(" \t");
    size_t b = s.find_last_not_of(" \t");
    if (a == std::string::npos)
      return std::string();
    return s.substr(a, b - a + 1);
  };

  int deleteIndex = -1;
  uint32_t tid = 0;

  BeginWrapPanel("ttb_" + id);

  // Chips
  for (size_t i = 0; i < tokens->size(); ++i) {
    PushID(static_cast<int>(i));
    const std::string &tok = (*tokens)[i];
    float padX = 8.0f;
    float gap = 4.0f;
    float xSize = fs * 0.85f;
    Vec2 tsz = MeasureTextCached(ctx, tok, fs);
    float chipH = fs + 8.0f;
    float chipW = padX + tsz.x + gap + xSize + padX;
    Vec2 chipSize(chipW, chipH);

    LayoutConstraints cc = ConsumeNextConstraints(SizeConstraint::Auto);
    Vec2 chipFinal = ApplyConstraints(ctx, cc, chipSize);
    Vec2 chipPos = ctx->cursorPos;

    Color pill = accent.normal;
    pill.a = 0.25f;
    ctx->renderer.DrawRectFilled(chipPos, chipFinal, pill, chipH * 0.5f);
    ctx->renderer.DrawText(Vec2(chipPos.x + padX, chipPos.y + (chipFinal.y - fs) * 0.5f),
                           tok, ts.color, fs);
    // "x" delete zone
    Vec2 xPos(chipPos.x + padX + tsz.x + gap, chipPos.y);
    Vec2 xZone(xSize + padX, chipFinal.y);
    bool xHover = IsMouseOver(ctx, xPos, xZone);
    Color xCol = ts.color;
    xCol.a = xHover ? 1.0f : 0.6f;
    DrawWidgetIcon(ctx, xPos, xZone, Icons::X, xCol, xSize, 0.0f, 0.0f);
    if (xHover && ctx->input.IsMousePressed(0))
      deleteIndex = static_cast<int>(i);

    ctx->lastItemPos = chipPos;
    AdvanceCursor(ctx, chipFinal);
    PopID();
  }

  // Inline text field (scoped so its hidden label gives a stable id).
  PushID("field");
  SetNextConstraints(FixedSize(140.0f, fs + 16.0f));
  TextInput("##ttb_field", &buf, 140.0f, false, std::nullopt,
            placeholder.empty() ? nullptr : placeholder.c_str());
  tid = GenerateId("TXT:", "##ttb_field");
  PopID();

  Vec2 fmn, fmx;
  GetItemRect(&fmn, &fmx);
  Rect fieldRect(fmn, fmx - fmn);

  EndWrapPanel();

  bool fieldFocused = (ctx->activeWidgetId == tid &&
                       ctx->activeWidgetType == ActiveWidgetType::TextInput);

  // Apply chip deletion.
  if (deleteIndex >= 0 && deleteIndex < static_cast<int>(tokens->size())) {
    tokens->erase(tokens->begin() + deleteIndex);
    changed = true;
  }

  auto addToken = [&](const std::string &raw) {
    std::string t = trim(raw);
    if (!t.empty()) {
      tokens->push_back(t);
      changed = true;
    }
  };

  // Comma anywhere in the buffer splits into tokens (keep the trailing remainder).
  if (buf.find(',') != std::string::npos) {
    std::string remainder;
    size_t start = 0;
    for (size_t i = 0; i <= buf.size(); ++i) {
      if (i == buf.size() || buf[i] == ',') {
        std::string part = buf.substr(start, i - start);
        if (i == buf.size())
          remainder = part; // last segment has no trailing comma yet
        else
          addToken(part);
        start = i + 1;
      }
    }
    buf = remainder;
  }

  bool enter = ctx->input.IsKeyPressed(UIKey::Enter) ||
               ctx->input.IsKeyPressed(UIKey::KeypadEnter);
  if (fieldFocused && enter && !trim(buf).empty()) {
    addToken(buf);
    buf.clear();
    // keep editing the field after committing
    ctx->activeWidgetId = tid;
    ctx->activeWidgetType = ActiveWidgetType::TextInput;
  }

  // Backspace on an empty field removes the last chip.
  if (fieldFocused && buf.empty() &&
      ctx->input.IsKeyPressed(UIKey::Backspace) && !tokens->empty()) {
    tokens->pop_back();
    changed = true;
  }

  // Optional suggestions popup.
  if (suggestionsFn) {
    std::vector<std::string> sugg = suggestionsFn(buf);
    std::string flyId = "TTB_FLY:" + id;
    if (fieldFocused && !buf.empty() && !sugg.empty())
      OpenFlyout(flyId);
    else if (buf.empty() || sugg.empty())
      CloseFlyout(flyId);
    if (IsFlyoutOpen(flyId) && !sugg.empty()) {
      if (BeginFlyout(flyId, fieldRect, FlyoutPlacement::BottomEdgeAlignedLeft)) {
        float rowH = fs + 12.0f;
        float rowW = std::max(fieldRect.size.x, 120.0f);
        for (const auto &s : sugg) {
          if (DrawSuggestionRow(ctx, s, buf, rowW, rowH, fs, false)) {
            addToken(s);
            buf.clear();
            CloseFlyout(flyId);
          }
        }
        EndFlyout();
      }
    }
  }

  return changed;
}

} // namespace FluentUI
