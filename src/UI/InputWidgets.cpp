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

bool Checkbox(const std::string &label, bool *value, std::optional<Vec2> pos) {
  UIContext *ctx = GetContext();
  if (!ctx)
    return false;

  Vec2 boxSize(20.0f, 20.0f);
  const TextStyle &textStyle = ctx->style.GetTextStyle(TypographyStyle::Body);
  Vec2 textSize = MeasureTextCached(ctx,label, textStyle.fontSize);
  Vec2 totalSize(boxSize.x + 8.0f + textSize.x,
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

  std::string idStr = "CHK:" + label;
  uint32_t id = GenerateId(idStr.c_str());

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

  RegisterOccupiedArea(ctx, boxPos, layoutSize);

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

  std::string groupKey = group.empty() ? "DEFAULT_RADIO_GROUP" : group;
  std::string idStr = "RADIO:" + groupKey + ":" + label;
  uint32_t id = GenerateId(idStr.c_str());

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

  std::string idStr = "SLDR_F:" + label;
  uint32_t id = GenerateId(idStr.c_str());

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
               float width, std::optional<Vec2> pos) {
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

static size_t FindCaretPosition(const std::string &text, float targetX,
                                UIContext *ctx) {
  const TextStyle &textStyle = ctx->style.GetTextStyle(TypographyStyle::Body);
  float accumulated = 0.0f;
  for (size_t i = 0; i < text.size(); ++i) {
    std::string ch(1, text[i]);
    float charWidth = MeasureTextCached(ctx,ch, textStyle.fontSize).x;
    if (targetX < accumulated + charWidth * 0.5f) {
      return i;
    }
    accumulated += charWidth;
  }
  return text.size();
}

bool TextInput(const std::string &label, std::string *value, float width,
               bool multiline, std::optional<Vec2> pos) {
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

  Vec2 textSize = MeasureTextCached(ctx,textRef, inputTextStyle.fontSize);
  float textPadding = panelStyle.padding.x * 0.5f;
  float availableWidth = fieldSize.x - textPadding * 2.0f;
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
              std::optional<Vec2> pos) {
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
    // Verificar si el click está fuera del dropdown también
    float dropdownHeight =
        std::min(static_cast<float>(items.size()) * fieldHeight, DROPDOWN_MAX_HEIGHT);
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
  }

  // Queue dropdown for deferred rendering (to ensure it appears on top)
  if (isOpen) {
    float dropdownHeight =
        std::min(static_cast<float>(items.size()) * fieldHeight, DROPDOWN_MAX_HEIGHT);

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

      bool hoverItem = IsMouseOver(ctx, itemPos, itemSize);

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
}

} // namespace FluentUI
