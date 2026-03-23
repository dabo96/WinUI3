#include "UI/Widgets.h"
#include "UI/WidgetHelpers.h"
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

bool Checkbox(const std::string &label, bool *value, std::optional<Vec2> pos) {
  UIContext *ctx = GetContext();
  if (!ctx)
    return false;

  Vec2 boxSize(S(20.0f), S(20.0f));
  const TextStyle &textStyle = ctx->style.GetTextStyle(TypographyStyle::Body);
  Vec2 textSize = MeasureTextCached(ctx,label, textStyle.fontSize);
  Vec2 totalSize(boxSize.x + S(8.0f) + textSize.x,
                 std::max(boxSize.y, textSize.y));
  LayoutConstraints constraints = ConsumeNextConstraints();
  Vec2 layoutSize = ApplyConstraints(ctx, constraints, totalSize);

  // Resolver posición: usar pos si se proporciona, sino usar cursor
  // directamente
  Vec2 boxPos;
  if (pos.has_value()) {
    boxPos = ResolveAbsolutePosition(ctx, pos.value(), layoutSize);
  } else {
    boxPos = ctx->cursorPos;
  }

  uint32_t id = GenerateId("CHK:", label.c_str());

  auto boolEntry = ctx->boolStates.try_emplace(id, false);
  bool currentValue = value ? *value : boolEntry.first->second;

  // Expand hitbox to include label area
  bool hover = IsMouseOver(ctx, boxPos, layoutSize);

  bool toggled = false;
  if (hover && ctx->input.IsMousePressed(0)) {
    currentValue = !currentValue;
    toggled = true;
  }

  if (value)
    *value = currentValue;
  else
    boolEntry.first->second = currentValue;

  // Invoke valueChanged callback if toggled
  if (toggled) {
    std::string idStr = "CHK:" + label;
    auto cbIt = ctx->valueChangedCallbacks.find(idStr);
    if (cbIt != ctx->valueChangedCallbacks.end()) cbIt->second(idStr, value);
  }

  const PanelStyle &panelStyle = ctx->style.panel;
  const ColorState &toggleAccent = ctx->style.button.background;

  // Fondo del checkbox - contraste sutil sin borde
  bool hoverBox = IsMouseOver(ctx, boxPos, boxSize);
  Color boxFill = panelStyle.background;
  if (hoverBox) {
    boxFill = panelStyle.headerBackground;
  }
  // Hacer el fondo más distintivo según el tema para mejor visibilidad (sin borde)
  boxFill = AdjustContainerBackground(boxFill, ctx->style.isDarkTheme);

  // Focus ring for keyboard navigation
  if (ctx->focusedWidgetId == id) {
    DrawFocusRing(ctx, boxPos, boxSize, panelStyle.cornerRadius * 0.5f);
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

  ctx->lastItemPos = boxPos;
  AdvanceCursor(ctx, layoutSize);
  return toggled;
}

bool RadioButton(const std::string &label, int *value, int optionValue,
                 const std::string &group, std::optional<Vec2> pos) {
  UIContext *ctx = GetContext();
  if (!ctx)
    return false;

  Vec2 circleSize(20.0f, 20.0f);
  const TextStyle &textStyle = ctx->style.GetTextStyle(TypographyStyle::Body);
  Vec2 textSize = MeasureTextCached(ctx,label, textStyle.fontSize);
  Vec2 totalSize(circleSize.x + 8.0f + textSize.x,
                 std::max(circleSize.y, textSize.y));
  LayoutConstraints constraints = ConsumeNextConstraints();
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

  // Obtener valor actual del grupo
  int currentValue = value ? *value : 0;
  bool isSelected = (currentValue == optionValue);

  // Expand hitbox to include label area
  bool hover = IsMouseOver(ctx, circlePos, layoutSize);

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
  float mx = ctx->input.MouseX();
  float my = ctx->input.MouseY();
  float dx = mx - circleCenter.x;
  float dy = my - circleCenter.y;
  bool hoverCircle = (dx * dx + dy * dy) <= radius * radius;
  Color circleFill = panelStyle.background;
  if (hoverCircle) {
    circleFill = panelStyle.headerBackground;
  }
  // Hacer el fondo más distintivo según el tema para mejor visibilidad (sin borde)
  circleFill = AdjustContainerBackground(circleFill, ctx->style.isDarkTheme);

  // Focus ring for keyboard navigation
  if (ctx->focusedWidgetId == id) {
    DrawFocusRing(ctx, circlePos, circleSize, radius);
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
                 std::optional<Vec2> pos) {
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

  Vec2 labelSize = MeasureTextCached(ctx,label, labelStyle.fontSize);
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
  if (pos.has_value()) {
    widgetPos = ResolveAbsolutePosition(ctx, pos.value(), finalSize);
  } else {
    widgetPos = ctx->cursorPos;
  }
  Vec2 trackPos(widgetPos.x, widgetPos.y + labelSize.y + labelSpacing);

  uint32_t id = GenerateId("SLDR_F:", label.c_str());

  auto floatEntry = ctx->floatStates.try_emplace(id, minValue);
  float currentValue = value ? *value : floatEntry.first->second;
  currentValue = std::clamp(currentValue, minValue, maxValue);

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
    floatEntry.first->second = currentValue;

  // Invoke valueChanged callback if slider moved
  if (valueChanged) {
    std::string idStr = "SLDR_F:" + label;
    auto cbIt = ctx->valueChangedCallbacks.find(idStr);
    if (cbIt != ctx->valueChangedCallbacks.end()) cbIt->second(idStr, value);
  }

  float fraction = CalculateSliderFraction(currentValue, minValue, maxValue);

  if (!format) {
    format = "%.2f";
  }
  char valueBuffer[64];
  std::snprintf(valueBuffer, sizeof(valueBuffer), format, currentValue);
  std::string valueText(valueBuffer);
  Vec2 valueSize = MeasureTextCached(ctx,valueText, valueStyle.fontSize);
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

  // Focus ring for keyboard navigation
  if (ctx->focusedWidgetId == id) {
    DrawFocusRing(ctx, trackPos, trackSize, panelStyle.cornerRadius);
  }

  Color trackColor =
      hover ? panelStyle.headerBackground : panelStyle.background;
  ctx->renderer.DrawRectFilled(trackPos, trackSize, trackColor,
                               panelStyle.cornerRadius);

  Vec2 fillSize(trackSize.x * fraction, trackSize.y);
  ctx->renderer.DrawRectFilled(trackPos, fillSize, accentState.normal,
                               panelStyle.cornerRadius);

  ctx->renderer.DrawText(widgetPos, label, labelStyle.color,
                         labelStyle.fontSize);

  ctx->renderer.DrawText(valuePos, valueText, valueStyle.color,
                         valueStyle.fontSize);

  ctx->lastItemPos = widgetPos;
  AdvanceCursor(ctx, finalSize);
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

bool TextInput(const std::string &label, std::string *value, float width,
               bool multiline, std::optional<Vec2> pos, const char* placeholder, size_t maxLength) {
  UIContext *ctx = GetContext();
  if (!ctx)
    return false;
  if (width <= 0.0f)
    width = 200.0f;

  float labelSpacing = 4.0f;

  const TextStyle &labelStyle =
      ctx->style.GetTextStyle(TypographyStyle::Subtitle);
  const TextStyle &inputTextStyle =
      ctx->style.GetTextStyle(TypographyStyle::Body);
  const PanelStyle &panelStyle = ctx->style.panel;
  const ColorState &accentState = ctx->style.button.background;

  float singleLineHeight = inputTextStyle.fontSize + panelStyle.padding.y * 2.0f;
  float inputHeight = multiline ? std::max(100.0f, singleLineHeight * 4.0f) : singleLineHeight;
  Vec2 labelSize = MeasureTextCached(ctx,label, labelStyle.fontSize);
  Vec2 fieldSize(width, inputHeight);

  Vec2 totalSize(fieldSize.x, labelSize.y + labelSpacing + fieldSize.y);
  LayoutConstraints constraints = ConsumeNextConstraints();
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

  // Vertical scroll offset for multiline (stored in floatStates with offset key)
  uint32_t mlScrollKey = id ^ 0xA5A5A5A5u; // Distinct key for multiline vertical scroll
  float &mlScroll = ctx->floatStates[mlScrollKey];

  // Selection anchor (SIZE_MAX = no selection)
  auto anchorIt = ctx->selectionAnchors.try_emplace(id, SIZE_MAX);
  size_t &selAnchor = anchorIt.first->second;

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
  bool hover = PointInRect(mousePos, fieldPos, fieldSize);
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
      if (multiline) {
        // Multiline: determine which line was clicked, then find caret within that line
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
        caret = lineStartOff + posInLine;
      } else {
        float localX = mousePos.x - (fieldPos.x + panelStyle.padding.x) + scroll;
        caret = FindCaretPosition(textRef, std::max(localX, 0.0f), ctx);
      }
      selAnchor = caret; // Set anchor for potential drag selection
      ClearSelection();
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

  // Capture pre-edit state for undo (snapshot before any modifications)
  std::string preEditText = textRef;
  size_t preEditCaret = caret;

  // Query keyboard modifier state
  SDL_Keymod modState = SDL_GetModState();
  bool ctrlHeld = (modState & SDL_KMOD_CTRL) != 0;
  bool shiftHeld = (modState & SDL_KMOD_SHIFT) != 0;

  if (hasFocus) {
    // Ctrl+A: Select all
    if (ctrlHeld && ctx->input.IsKeyPressed(SDL_SCANCODE_A)) {
      selAnchor = 0;
      caret = textRef.size();
    }
    // Ctrl+C: Copy
    else if (ctrlHeld && ctx->input.IsKeyPressed(SDL_SCANCODE_C)) {
      if (HasSelection()) {
        std::string selected = textRef.substr(SelectionStart(), SelectionEnd() - SelectionStart());
        SDL_SetClipboardText(selected.c_str());
      }
    }
    // Ctrl+X: Cut
    else if (ctrlHeld && ctx->input.IsKeyPressed(SDL_SCANCODE_X)) {
      if (HasSelection()) {
        std::string selected = textRef.substr(SelectionStart(), SelectionEnd() - SelectionStart());
        SDL_SetClipboardText(selected.c_str());
        DeleteSelection();
        valueChanged = true;
      }
    }
    // Ctrl+V: Paste
    else if (ctrlHeld && ctx->input.IsKeyPressed(SDL_SCANCODE_V)) {
      const char* clip = SDL_GetClipboardText();
      if (clip && clip[0] != '\0') {
        if (HasSelection()) {
          DeleteSelection();
          valueChanged = true;
        }
        std::string clipStr(clip);
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
    else if (ctrlHeld && ctx->input.IsKeyPressed(SDL_SCANCODE_Z) && !shiftHeld) {
      auto& undoState = ctx->textUndoStates[id];
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
    else if ((ctrlHeld && ctx->input.IsKeyPressed(SDL_SCANCODE_Y)) ||
             (ctrlHeld && shiftHeld && ctx->input.IsKeyPressed(SDL_SCANCODE_Z))) {
      auto& undoState = ctx->textUndoStates[id];
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
          if (HasSelection()) {
            DeleteSelection();
          }
          // Enforce maxLength limit
          std::string toInsert = inputText;
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

      if (ctx->input.IsKeyPressed(SDL_SCANCODE_BACKSPACE)) {
        if (HasSelection()) {
          DeleteSelection();
          valueChanged = true;
        } else if (caret > 0) {
          size_t prev = Utf8PrevCodepoint(textRef, caret);
          textRef.erase(prev, caret - prev);
          caret = prev;
          valueChanged = true;
        }
      } else if (ctx->input.IsKeyPressed(SDL_SCANCODE_DELETE)) {
        if (HasSelection()) {
          DeleteSelection();
          valueChanged = true;
        } else if (caret < textRef.size()) {
          size_t next = Utf8NextCodepoint(textRef, caret);
          textRef.erase(caret, next - caret);
          valueChanged = true;
        }
      } else if (ctx->input.IsKeyPressed(SDL_SCANCODE_LEFT)) {
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
      } else if (ctx->input.IsKeyPressed(SDL_SCANCODE_RIGHT)) {
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
      } else if (ctx->input.IsKeyPressed(SDL_SCANCODE_HOME)) {
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
      } else if (ctx->input.IsKeyPressed(SDL_SCANCODE_END)) {
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
      } else if (multiline && (ctx->input.IsKeyPressed(SDL_SCANCODE_UP) ||
                                ctx->input.IsKeyPressed(SDL_SCANCODE_DOWN))) {
        // Up/Down arrow navigation for multiline
        if (shiftHeld) {
          if (selAnchor == SIZE_MAX) selAnchor = caret;
        } else {
          ClearSelection();
        }
        int curLine, curCol;
        FindLineCol(textRef, caret, curLine, curCol);
        int totalLines = CountLines(textRef);
        if (ctx->input.IsKeyPressed(SDL_SCANCODE_UP)) {
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
      } else if (multiline && (ctx->input.IsKeyPressed(SDL_SCANCODE_RETURN) ||
                                ctx->input.IsKeyPressed(SDL_SCANCODE_KP_ENTER))) {
        // In multiline mode, Enter inserts a newline
        if (HasSelection()) {
          DeleteSelection();
        }
        textRef.insert(caret, 1, '\n');
        caret++;
        valueChanged = true;
      } else if (!multiline && (ctx->input.IsKeyPressed(SDL_SCANCODE_RETURN) ||
                                ctx->input.IsKeyPressed(SDL_SCANCODE_KP_ENTER))) {
        ctx->activeWidgetId = 0;
        ctx->activeWidgetType = ActiveWidgetType::None;
      }
    }
  }

  // Push pre-edit state to undo stack when text actually changed
  if (valueChanged && preEditText != textRef) {
    ctx->textUndoStates[id].PushUndo(preEditText, preEditCaret, ctx->frame);
  }

  // Invoke valueChanged callback if text changed
  if (valueChanged) {
    std::string idStr = "TXT:" + label;
    auto cbIt = ctx->valueChangedCallbacks.find(idStr);
    if (cbIt != ctx->valueChangedCallbacks.end()) cbIt->second(idStr, textPtr);
  }

  ctx->renderer.DrawText(widgetPos, label, labelStyle.color,
                         labelStyle.fontSize);

  // Fondo más distintivo para TextInput - sin borde visible (solo focus)
  Color bgColor = panelStyle.background;
  bool isDarkTheme = ctx->style.isDarkTheme;
  if (isDarkTheme) {
    bgColor = Color(bgColor.r * 1.12f, bgColor.g * 1.12f, bgColor.b * 1.12f, 1.0f);
  } else {
    bgColor = Color(bgColor.r * 0.94f, bgColor.g * 0.94f, bgColor.b * 0.94f, 1.0f);
  }

  // Solo mostrar borde cuando tiene focus
  if (hasFocus) {
    DrawFocusRing(ctx, fieldPos, fieldSize, panelStyle.cornerRadius);
    bgColor = accentState.hover;
    bgColor.a = 0.15f;
  }

  ctx->renderer.DrawRectFilled(fieldPos, fieldSize, bgColor,
                               panelStyle.cornerRadius);

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
        SDL_SetTextInputArea(ctx->window, &inputArea, 0);
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
  return valueChanged;
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
  float fieldHeight = valueStyle.fontSize + panelStyle.padding.y * 2.0f;

  // Total widget width: label (40%) + field (60%) laid out horizontally
  float totalWidth = 250.0f; // Default width
  Vec2 totalSize(totalWidth, std::max(labelSize.y, fieldHeight));
  LayoutConstraints constraints = ConsumeNextConstraints();
  Vec2 finalSize = ApplyConstraints(ctx, constraints, totalSize);
  totalWidth = finalSize.x;

  float labelWidth = totalWidth * 0.4f;
  float fieldWidth = totalWidth * 0.6f;

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
  bool hoverField = PointInRect(mousePos, fieldPos, fieldSize);
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
    bool enterPressed = ctx->input.IsKeyPressed(SDL_SCANCODE_RETURN) ||
                        ctx->input.IsKeyPressed(SDL_SCANCODE_KP_ENTER);
    bool clickedOutside = leftPressed && !hoverField;
    bool escapePressed = ctx->input.IsKeyPressed(SDL_SCANCODE_ESCAPE);

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

      // Handle text input
      auto caretIt = ctx->caretPositions.try_emplace(id, editStr.size());
      size_t& caret = caretIt.first->second;
      caret = std::min(caret, editStr.size());

      SDL_Keymod modState = SDL_GetModState();
      bool ctrlHeld = (modState & SDL_KMOD_CTRL) != 0;

      if (!ctrlHeld) {
        const std::string& inputText = ctx->input.TextInputBuffer();
        if (!inputText.empty()) {
          editStr.insert(caret, inputText);
          caret += inputText.size();
        }
      }

      if (ctx->input.IsKeyPressed(SDL_SCANCODE_BACKSPACE)) {
        if (caret > 0) {
          caret--;
          editStr.erase(caret, 1);
        }
      } else if (ctx->input.IsKeyPressed(SDL_SCANCODE_DELETE)) {
        if (caret < editStr.size()) {
          editStr.erase(caret, 1);
        }
      } else if (ctx->input.IsKeyPressed(SDL_SCANCODE_LEFT)) {
        if (caret > 0) caret--;
      } else if (ctx->input.IsKeyPressed(SDL_SCANCODE_RIGHT)) {
        if (caret < editStr.size()) caret++;
      } else if (ctx->input.IsKeyPressed(SDL_SCANCODE_HOME)) {
        caret = 0;
      } else if (ctx->input.IsKeyPressed(SDL_SCANCODE_END)) {
        caret = editStr.size();
      }

      // Ctrl+A: Select all text in the edit field
      if (ctrlHeld && ctx->input.IsKeyPressed(SDL_SCANCODE_A)) {
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
        ctx->caretPositions[id] = state.editText.size();
        ctx->activeWidgetId = id;
        ctx->activeWidgetType = ActiveWidgetType::TextInput;
      } else {
        // Single click: start dragging
        state.isDragging = true;
        state.dragStartValue = currentValue;
        state.dragStartMouseX = mousePos.x;
        ctx->activeWidgetId = id;
        ctx->activeWidgetType = ActiveWidgetType::DragWidget;
      }
    }

    if (state.isDragging) {
      if (leftDown) {
        SDL_Keymod modState = SDL_GetModState();
        bool shiftHeld = (modState & SDL_KMOD_SHIFT) != 0;
        float effectiveSpeed = speed * (shiftHeld ? 0.1f : 1.0f);

        float delta = (mousePos.x - state.dragStartMouseX) * effectiveSpeed;
        float newValue = state.dragStartValue + delta;

        if (min < max) {
          newValue = std::clamp(newValue, min, max);
        }

        if (std::abs(newValue - currentValue) > 0.00001f) {
          currentValue = newValue;
          valueChanged = true;
        }
      } else {
        // Mouse released: stop dragging
        state.isDragging = false;
        ctx->activeWidgetId = 0;
        ctx->activeWidgetType = ActiveWidgetType::None;
      }
    }
  }

  // Write back value
  if (value) *value = currentValue;

  // --- Drawing ---

  // Label on the left
  Vec2 labelPos(widgetPos.x,
                widgetPos.y + (fieldHeight - labelSize.y) * 0.5f);
  ctx->renderer.DrawText(labelPos, label, labelStyle.color, labelStyle.fontSize);

  // Field background
  bool isDark = ctx->style.isDarkTheme;
  Color fieldBg = panelStyle.background;
  if (isDark) {
    fieldBg = Color(fieldBg.r * 1.12f, fieldBg.g * 1.12f, fieldBg.b * 1.12f, 1.0f);
  } else {
    fieldBg = Color(fieldBg.r * 0.94f, fieldBg.g * 0.94f, fieldBg.b * 0.94f, 1.0f);
  }

  if (state.isDragging) {
    // Highlight during drag
    fieldBg = accentState.hover;
    fieldBg.a = 0.25f;
  } else if (hoverField && !state.isEditing) {
    fieldBg = panelStyle.headerBackground;
  }

  // Focus ring when editing or dragging
  if (state.isEditing || state.isDragging) {
    DrawFocusRing(ctx, fieldPos, fieldSize, panelStyle.cornerRadius);
  }

  ctx->renderer.DrawRectFilled(fieldPos, fieldSize, fieldBg, panelStyle.cornerRadius);

  // Draw value text or edit text
  float textPadding = panelStyle.padding.x * 0.5f;
  if (state.isEditing) {
    // Draw edit text with caret
    Vec2 textSize = MeasureTextCached(ctx, state.editText, valueStyle.fontSize);
    Vec2 textPos(fieldPos.x + textPadding,
                 fieldPos.y + (fieldSize.y - valueStyle.fontSize) * 0.5f);
    ctx->renderer.DrawText(textPos, state.editText, valueStyle.color, valueStyle.fontSize);

    // Draw caret
    auto caretIt = ctx->caretPositions.find(id);
    size_t caretPos = (caretIt != ctx->caretPositions.end()) ? caretIt->second : state.editText.size();
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
    // Center value text in the field
    Vec2 textPos(fieldPos.x + (fieldSize.x - textSize.x) * 0.5f,
                 fieldPos.y + (fieldSize.y - valueStyle.fontSize) * 0.5f);
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

  float fieldHeight = labelStyle.fontSize + ctx->style.panel.padding.y * 2.0f;
  float totalWidth = 400.0f;
  Vec2 totalSize(totalWidth, fieldHeight);
  LayoutConstraints constraints = ConsumeNextConstraints();
  Vec2 finalSize = ApplyConstraints(ctx, constraints, totalSize);
  totalWidth = finalSize.x;

  Vec2 widgetPos;
  if (pos.has_value()) {
    widgetPos = ResolveAbsolutePosition(ctx, pos.value(), finalSize);
  } else {
    widgetPos = ctx->cursorPos;
  }

  // Draw label on left (30%)
  float labelWidth = totalWidth * 0.25f;
  Vec2 labelPos(widgetPos.x,
                widgetPos.y + (fieldHeight - labelSize.y) * 0.5f);
  ctx->renderer.DrawText(labelPos, label, labelStyle.color, labelStyle.fontSize);

  // Three drag fields share the remaining 75% with small gaps
  float fieldsWidth = totalWidth * 0.75f;
  float gap = 4.0f;
  float singleFieldWidth = (fieldsWidth - gap * 2.0f) / 3.0f;

  bool anyChanged = false;

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
    bool hoverField = PointInRect(mousePos, fieldPos, fieldSize);
    bool leftPressed = ctx->input.IsMousePressed(0);
    bool leftDown = ctx->input.IsMouseDown(0);

    if (hoverField && !state.isEditing) {
      ctx->desiredCursor = UIContext::CursorType::ResizeH;
    }

    bool compChanged = false;

    // --- Edit mode ---
    if (state.isEditing) {
      std::string& editStr = state.editText;
      bool enterPressed = ctx->input.IsKeyPressed(SDL_SCANCODE_RETURN) ||
                          ctx->input.IsKeyPressed(SDL_SCANCODE_KP_ENTER);
      bool clickedOutside = leftPressed && !hoverField;
      bool escapePressed = ctx->input.IsKeyPressed(SDL_SCANCODE_ESCAPE);

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

        auto caretIt = ctx->caretPositions.try_emplace(id, editStr.size());
        size_t& caret = caretIt.first->second;
        caret = std::min(caret, editStr.size());

        SDL_Keymod modState = SDL_GetModState();
        bool ctrlHeld = (modState & SDL_KMOD_CTRL) != 0;

        if (!ctrlHeld) {
          const std::string& inputText = ctx->input.TextInputBuffer();
          if (!inputText.empty()) {
            editStr.insert(caret, inputText);
            caret += inputText.size();
          }
        }
        if (ctx->input.IsKeyPressed(SDL_SCANCODE_BACKSPACE)) {
          if (caret > 0) { caret--; editStr.erase(caret, 1); }
        } else if (ctx->input.IsKeyPressed(SDL_SCANCODE_DELETE)) {
          if (caret < editStr.size()) editStr.erase(caret, 1);
        } else if (ctx->input.IsKeyPressed(SDL_SCANCODE_LEFT)) {
          if (caret > 0) caret--;
        } else if (ctx->input.IsKeyPressed(SDL_SCANCODE_RIGHT)) {
          if (caret < editStr.size()) caret++;
        } else if (ctx->input.IsKeyPressed(SDL_SCANCODE_HOME)) {
          caret = 0;
        } else if (ctx->input.IsKeyPressed(SDL_SCANCODE_END)) {
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
          ctx->caretPositions[id] = state.editText.size();
          ctx->activeWidgetId = id;
          ctx->activeWidgetType = ActiveWidgetType::TextInput;
        } else {
          state.isDragging = true;
          state.dragStartValue = currentVal;
          state.dragStartMouseX = mousePos.x;
          ctx->activeWidgetId = id;
          ctx->activeWidgetType = ActiveWidgetType::DragWidget;
        }
      }

      if (state.isDragging) {
        if (leftDown) {
          SDL_Keymod modState = SDL_GetModState();
          bool shiftHeld = (modState & SDL_KMOD_SHIFT) != 0;
          float effectiveSpeed = speed * (shiftHeld ? 0.1f : 1.0f);
          float delta = (mousePos.x - state.dragStartMouseX) * effectiveSpeed;
          float newVal = state.dragStartValue + delta;
          if (min < max) newVal = std::clamp(newVal, min, max);
          if (std::abs(newVal - currentVal) > 0.00001f) {
            currentVal = newVal;
            compChanged = true;
          }
        } else {
          state.isDragging = false;
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
    bool isDark = ctx->style.isDarkTheme;

    Color fieldBg = ps.background;
    if (isDark) {
      fieldBg = Color(fieldBg.r * 1.12f, fieldBg.g * 1.12f, fieldBg.b * 1.12f, 1.0f);
    } else {
      fieldBg = Color(fieldBg.r * 0.94f, fieldBg.g * 0.94f, fieldBg.b * 0.94f, 1.0f);
    }

    if (state.isDragging) {
      fieldBg = accent.hover;
      fieldBg.a = 0.25f;
    } else if (hoverField && !state.isEditing) {
      fieldBg = ps.headerBackground;
    }

    if (state.isEditing || state.isDragging) {
      DrawFocusRing(ctx, fieldPos, fieldSize, ps.cornerRadius);
    }

    ctx->renderer.DrawRectFilled(fieldPos, fieldSize, fieldBg, ps.cornerRadius);

    // Color indicator strip on left edge
    ctx->renderer.DrawRectFilled(
        fieldPos, Vec2(3.0f, fieldSize.y), componentColors[i], ps.cornerRadius);

    float textPadding = ps.padding.x * 0.5f;
    const TextStyle& valStyle = ctx->style.GetTextStyle(TypographyStyle::Body);

    if (state.isEditing) {
      Vec2 textPos(fieldPos.x + textPadding + 4.0f,
                   fieldPos.y + (fieldSize.y - valStyle.fontSize) * 0.5f);
      ctx->renderer.DrawText(textPos, state.editText, valStyle.color, valStyle.fontSize);

      auto caretIt = ctx->caretPositions.find(id);
      size_t caretPos = (caretIt != ctx->caretPositions.end()) ? caretIt->second : state.editText.size();
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
      Vec2 textPos(fieldPos.x + (fieldSize.x - textSize.x) * 0.5f,
                   fieldPos.y + (fieldSize.y - valStyle.fontSize) * 0.5f);
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
  return anyChanged;
}

bool ComboBox(const std::string &label, int *currentItem,
              const std::vector<std::string> &items, float width,
              std::optional<Vec2> pos) {
  UIContext *ctx = GetContext();
  if (!ctx || items.empty())
    return false;
  if (width <= 0.0f)
    width = 200.0f;

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

  Vec2 labelSize = MeasureTextCached(ctx,label, labelStyle.fontSize);
  float fieldHeight = itemStyle.fontSize + panelStyle.padding.y * 2.0f;
  Vec2 fieldSize(width, fieldHeight);

  Vec2 totalSize(fieldSize.x, labelSize.y + labelSpacing + fieldSize.y);
  LayoutConstraints constraints = ConsumeNextConstraints();
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
  }

  float mouseX = ctx->input.MouseX();
  float mouseY = ctx->input.MouseY();
  bool hoverField = IsMouseOver(ctx, fieldPos, fieldSize);

  // Estado del dropdown
  auto boolEntry = ctx->boolStates.try_emplace(id, false);
  bool isOpen = boolEntry.first->second;

  bool clicked = hoverField && ctx->input.IsMousePressed(0);
  if (clicked) {
    isOpen = !isOpen;
  }

  // Cerrar si se hace click fuera
  if (isOpen && ctx->input.IsMousePressed(0) && !hoverField) {
    float itemH = itemStyle.fontSize + panelStyle.padding.y;
    float dropH = static_cast<float>(items.size()) * itemH;
    bool hoverDropdown =
        (mouseX >= fieldPos.x && mouseX <= fieldPos.x + fieldSize.x &&
         mouseY >= fieldPos.y + fieldSize.y &&
         mouseY <= fieldPos.y + fieldSize.y + dropH);
    if (!hoverDropdown) {
      isOpen = false;
    }
  }

  // Keyboard navigation when dropdown is open
  auto& highlightEntry = ctx->intStates[id];
  if (isOpen) {
    if (ctx->input.IsKeyPressed(SDL_SCANCODE_DOWN)) {
      highlightEntry = std::min(highlightEntry + 1, static_cast<int>(items.size()) - 1);
    }
    if (ctx->input.IsKeyPressed(SDL_SCANCODE_UP)) {
      highlightEntry = std::max(highlightEntry - 1, 0);
    }
    if (ctx->input.IsKeyPressed(SDL_SCANCODE_RETURN) ||
        ctx->input.IsKeyPressed(SDL_SCANCODE_SPACE)) {
      if (currentItem) *currentItem = highlightEntry;
      isOpen = false;
    }
    if (ctx->input.IsKeyPressed(SDL_SCANCODE_ESCAPE)) {
      isOpen = false;
    }
  } else {
    // Also activate with Enter/Space when has focus and dropdown is closed
    if (hasFocus && (ctx->input.IsKeyPressed(SDL_SCANCODE_RETURN) ||
                     ctx->input.IsKeyPressed(SDL_SCANCODE_SPACE))) {
      highlightEntry = selectedIndex;
      isOpen = true;
    }
    // Up/Down while closed: directly change selection
    if (hasFocus && ctx->input.IsKeyPressed(SDL_SCANCODE_DOWN)) {
      if (currentItem && *currentItem < static_cast<int>(items.size()) - 1) {
        (*currentItem)++;
      }
    }
    if (hasFocus && ctx->input.IsKeyPressed(SDL_SCANCODE_UP)) {
      if (currentItem && *currentItem > 0) {
        (*currentItem)--;
      }
    }
  }

  // Actualizar estado
  boolEntry.first->second = isOpen;

  // Dibujar campo con fondo más distintivo - sin borde visible
  Color fieldBg = panelStyle.background;
  if (hoverField) {
    fieldBg = panelStyle.headerBackground;
  }
  // Hacer el fondo más distintivo según el tema (sin borde)
  bool isDarkTheme = ctx->style.isDarkTheme;
  if (isDarkTheme) {
    fieldBg = Color(fieldBg.r * 1.12f, fieldBg.g * 1.12f, fieldBg.b * 1.12f, 1.0f);
  } else {
    fieldBg = Color(fieldBg.r * 0.94f, fieldBg.g * 0.94f, fieldBg.b * 0.94f, 1.0f);
  }

  // Solo mostrar borde cuando tiene focus
  if (hasFocus) {
    DrawFocusRing(ctx, fieldPos, fieldSize, panelStyle.cornerRadius);
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

  // Check if a deferred dropdown reported a change for this combo
  bool valueChanged = false;
  auto changedIt = ctx->comboBoxChanged.find(id);
  if (changedIt != ctx->comboBoxChanged.end() && changedIt->second) {
    valueChanged = true;
    ctx->comboBoxChanged.erase(changedIt);
    // Invoke valueChanged callback
    std::string idStr = "COMBO:" + label;
    auto cbIt = ctx->valueChangedCallbacks.find(idStr);
    if (cbIt != ctx->valueChangedCallbacks.end()) cbIt->second(idStr, currentItem);
  }

  // Queue dropdown for deferred rendering (to ensure it appears on top)
  if (isOpen) {
    // Each dropdown item: text height + vertical padding for spacing
    float itemH = itemStyle.fontSize + panelStyle.padding.y;
    float dropdownHeight = static_cast<float>(items.size()) * itemH;

    // Clamp to viewport
    Vec2 viewport = ctx->renderer.GetViewportSize();
    float maxDropH = viewport.y - (fieldPos.y + fieldSize.y) - 8.0f;
    dropdownHeight = std::min(dropdownHeight, std::max(maxDropH, itemH));

    UIContext::DeferredComboDropdown dropdown;
    dropdown.fieldPos = fieldPos;
    dropdown.fieldSize = fieldSize;
    dropdown.dropdownPos = Vec2(fieldPos.x, fieldPos.y + fieldSize.y);
    dropdown.dropdownSize = Vec2(fieldSize.x, dropdownHeight);
    dropdown.items = &items;
    dropdown.selectedIndex = selectedIndex;
    dropdown.comboId = id;
    dropdown.highlightIndex = highlightEntry;
    dropdown.currentItemPtr = currentItem;
    dropdown.fieldHeight = itemH;

    ctx->deferredComboDropdowns.push_back(dropdown);
  }

  // Update open state
  boolEntry.first->second = isOpen;

  ctx->lastItemPos = widgetPos;
  AdvanceCursor(ctx, finalSize);
  return valueChanged;
}

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

    for (size_t i = 0; i < dropdown.items->size(); ++i) {
      Vec2 itemPos(dropdownPos.x,
                   dropdownPos.y + static_cast<float>(i) * fieldHeight);
      Vec2 itemSize(dropdownSize.x, fieldHeight);

      bool hoverItem = IsMouseOver(ctx, itemPos, itemSize);

      // Highlight hovered or keyboard-selected item
      bool isHighlighted = hoverItem || (static_cast<int>(i) == dropdown.highlightIndex);
      if (isHighlighted) {
        ctx->renderer.DrawRectFilled(itemPos, itemSize, accentState.hover, 0.0f);
      }

      // Draw selection indicator for current item
      if (static_cast<int>(i) == dropdown.selectedIndex) {
        float dotR = 3.0f;
        Vec2 dotCenter(itemPos.x + 8.0f, itemPos.y + fieldHeight * 0.5f);
        ctx->renderer.DrawCircle(dotCenter, dotR, accentState.normal, true);
      }

      // Draw item text centered vertically in the item row
      float textY = itemPos.y + (fieldHeight - itemStyle.fontSize) * 0.5f;
      Vec2 itemTextPos(itemPos.x + panelStyle.padding.x + 20.0f, textY);
      ctx->renderer.DrawText(itemTextPos, (*dropdown.items)[i], itemStyle.color,
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
        ctx->comboBoxChanged[dropdown.comboId] = true;
      }
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
    Color dropdownBg = AdjustContainerBackground(ctx->style.panel.background, ctx->style.isDarkTheme);

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

      // Dibujar sombra simple
      ctx->renderer.DrawRectFilled(tooltip.pos + Vec2(2, 2), tooltipSize, Color(0,0,0,0.3f * alpha), 4.0f);

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
}

} // namespace FluentUI
