#include "UI/Widgets.h"
#include "UI/WidgetHelpers.h"
#include "Theme/FluentTheme.h"
#include "Theme/Material.h"
#include "core/Animation.h"
#include "core/Context.h"
#include "core/Renderer.h"
#include "core/Elevation.h"
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

bool Button(const std::string &label, uint32_t iconCodepoint, const Vec2 &size, std::optional<Vec2> pos, bool enabled);

bool Button(const std::string &label, const Vec2 &size, std::optional<Vec2> pos, bool enabled) {
  return Button(label, 0u, size, pos, enabled);
}

bool Button(const std::string &label, uint32_t iconCodepoint, const Vec2 &size, std::optional<Vec2> pos, bool enabled) {
  UIContext *ctx = GetContext();
  if (!ctx)
    return false;

  const ButtonStyle &buttonStyle = ctx->GetEffectiveButtonStyle();

  Vec2 textSize = MeasureTextCached(ctx, label, buttonStyle.text.fontSize);
  if ((textSize.x <= 0.0f || textSize.y <= 0.0f) && !label.empty()) {
    textSize = Vec2(label.length() * buttonStyle.text.fontSize * 0.6f,
                    buttonStyle.text.fontSize);
  }

  // Icon-only button: collapse text size to the line height so the button is square.
  bool iconOnly = (iconCodepoint != 0u) && label.empty();
  float iconSize = buttonStyle.text.fontSize;
  float iconGap = (iconCodepoint != 0u && !label.empty()) ? 6.0f : 0.0f;
  float iconSlot = (iconCodepoint != 0u) ? (iconSize + iconGap) : 0.0f;

  if (iconOnly) {
    textSize = Vec2(0.0f, iconSize);
  }

  Vec2 desiredSize = size;
  if (desiredSize.x <= 0.0f) {
    if (iconOnly) {
      // Square: same width and height, both derived from line height + padding.
      desiredSize.x = iconSize + buttonStyle.padding.y * 2.0f;
    } else {
      desiredSize.x = textSize.x + iconSlot + buttonStyle.padding.x * 2.0f;
    }
  }
  if (desiredSize.y <= 0.0f) {
    desiredSize.y = std::max(textSize.y, iconSize) + buttonStyle.padding.y * 2.0f;
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
      enabled && !IsMouseInputBlocked(ctx) &&
      (mouseX >= btnPos.x && mouseX < (btnPos.x + btnSize.x) &&
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

  // Brief 07: source the button surface (fill, border, corner, elevation, reveal)
  // from a data-driven material instead of ad-hoc maths. ResolveButtonMaterial reads
  // the EFFECTIVE button style so push/pop style stacks still apply; the result is
  // identical to the previous inline logic (regression-free parity). Foreground/text
  // color is not part of the surface material and keeps coming from buttonStyle.
  WidgetState btnState = !enabled ? WidgetState::Disabled
                       : pressed  ? WidgetState::Pressed
                       : hover    ? WidgetState::Hover
                                  : WidgetState::Rest;
  FluentMaterial btnMat = ResolveButtonMaterial(buttonStyle, btnState);

  // Perf 1.2: Register animation slots for GC tracking (only widgets that use animations)
  RegisterAnimSlots(buttonId);

  // Obtener o crear animaciones de color
  auto &bgAnim = ctx->colorAnimations[AnimSlot(buttonId, 0)];
  auto &fgAnim = ctx->colorAnimations[AnimSlot(buttonId, 1)];
  auto &borderAnim = ctx->colorAnimations[AnimSlot(buttonId, 2)];

  // Inicializar animaciones si es necesario (primera vez)
  if (!bgAnim.IsInitialized()) {
    bgAnim.SetImmediate(btnMat.fill);
  }
  if (!fgAnim.IsInitialized()) {
    fgAnim.SetImmediate(getTargetColor(buttonStyle.foreground));
  }
  if (!borderAnim.IsInitialized()) {
    borderAnim.SetImmediate(btnMat.border);
  }

  // Actualizar objetivos de animación (fill/border desde el material; fg via Style)
  bgAnim.SetTarget(btnMat.fill, 0.2f,
                   Easing::EaseOutCubic);
  fgAnim.SetTarget(getTargetColor(buttonStyle.foreground), 0.2f,
                   Easing::EaseOutCubic);
  borderAnim.SetTarget(btnMat.border, 0.2f,
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
    if (buttonStyle.shadowOpacity > 0.0f) {
      // Sombra por elevación (z) reactiva al estado, resuelta por el material
      // (rest=plano, hover elevado, pressed algo hundido, disabled plano).
      // shadowOpacity del tema actúa de interruptor (los temas "flat" lo ponen a 0).
      ctx->renderer.DrawElevationShadow(btnPos, btnSize, btnMat.radius, btnMat.elevationZ);
    }

    if (hasFocus && enabled) {
      DrawFocusRing(ctx, btnPos, btnSize, btnMat.radius);
    }

    // Reveal highlight (brief 04): enabled buttons react to the nearby cursor across
    // their whole surface. The material carries the intensity; consumed by the next
    // DrawRect* call.
    ctx->renderer.SetNextRevealIntensity(btnMat.revealIntensity);
    ctx->renderer.DrawRectFilled(btnPos, btnSize, bgColor, btnMat.radius);
    if (btnMat.borderWidth > 0.0f) {
      ctx->renderer.DrawRect(btnPos, btnSize, borderColor, btnMat.radius);
    }

    Vec2 contentPos(btnPos.x + buttonStyle.padding.x,
                    btnPos.y + buttonStyle.padding.y);

    // Center the (icon + gap + text) block horizontally inside the button.
    float contentWidth = textSize.x + iconSlot;
    float blockX = btnPos.x + (btnSize.x - contentWidth) * 0.5f;
    blockX = std::max(blockX, btnPos.x + 2.0f);

    if (iconCodepoint != 0u) {
      DrawWidgetIcon(ctx, btnPos, btnSize, iconCodepoint, fgColor, iconSize,
                     blockX - btnPos.x, iconGap);
    }

    Vec2 textPos(
        blockX + iconSlot,
        contentPos.y +
            (btnSize.y - buttonStyle.padding.y * 2.0f - textSize.y) * 0.5f - 1.0f); // Pequeño ajuste visual

    if (!label.empty()) {
      ctx->renderer.DrawText(textPos, label, fgColor, buttonStyle.text.fontSize);
    }

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

  // Phase B1: publish item state for IsItemActivated/Edited/etc.
  SetLastItem(buttonId, btnPos, btnPos + btnSize, hover, pressed, hasFocus, enabled && clicked);

  return enabled && clicked;
}

static bool SegmentedControlImpl(const std::string &id,
                                 const std::vector<std::string> &options,
                                 const std::vector<uint32_t> *icons,
                                 int *activeIndex,
                                 std::optional<Vec2> pos);

bool SegmentedControl(const std::string &id,
                      const std::vector<std::pair<std::string, uint32_t>> &options,
                      int *activeIndex,
                      std::optional<Vec2> pos) {
  std::vector<std::string> labels;
  std::vector<uint32_t> ics;
  labels.reserve(options.size());
  ics.reserve(options.size());
  for (const auto &p : options) { labels.push_back(p.first); ics.push_back(p.second); }
  return SegmentedControlImpl(id, labels, &ics, activeIndex, pos);
}

bool SegmentedControl(const std::string &id,
                      const std::vector<std::string> &options,
                      int *activeIndex,
                      std::optional<Vec2> pos) {
  return SegmentedControlImpl(id, options, nullptr, activeIndex, pos);
}

static bool SegmentedControlImpl(const std::string &id,
                                 const std::vector<std::string> &options,
                                 const std::vector<uint32_t> *icons,
                                 int *activeIndex,
                                 std::optional<Vec2> pos) {
  UIContext *ctx = GetContext();
  if (!ctx || options.empty())
    return false;

  // Fluent 2 SegmentedControl: a single rounded container holding equal-width
  // pill segments. Selected segment uses the brand accent fill; inactive
  // segments are transparent with body-color text.
  const TextStyle &textStyle = ctx->style.GetTextStyle(TypographyStyle::Body);
  const PanelStyle &panelStyle = ctx->style.panel;
  Color accent = ctx->style.accentColor;

  float vPadding = 5.0f;
  float hPadding = 12.0f;
  float gap = 2.0f;
  float height = textStyle.fontSize + vPadding * 2.0f; // ≈26 with 14px body
  float radius = 4.0f;                                  // borderRadiusMedium

  // Compute per-segment widths from icon + text + padding.
  std::vector<float> segWidths(options.size());
  float segIconSize = textStyle.fontSize;
  float segIconGap = options.empty() ? 0.0f : 6.0f;
  float totalW = 0.0f;
  for (size_t i = 0; i < options.size(); ++i) {
    Vec2 ts = MeasureTextCached(ctx, options[i], textStyle.fontSize);
    uint32_t cp = (icons && i < icons->size()) ? (*icons)[i] : 0u;
    bool iconOnly = (cp != 0u) && options[i].empty();
    float iconSlot = 0.0f;
    if (cp != 0u) {
      iconSlot = segIconSize + (options[i].empty() ? 0.0f : segIconGap);
    }
    if (iconOnly) {
      // Square segment for icon-only entries
      segWidths[i] = segIconSize + hPadding * 2.0f;
    } else {
      segWidths[i] = ts.x + iconSlot + hPadding * 2.0f;
    }
    totalW += segWidths[i];
  }
  totalW += gap * (options.size() - 1);

  Vec2 desired(totalW, height);
  LayoutConstraints constraints = ConsumeNextConstraints();
  Vec2 finalSize = ApplyConstraints(ctx, constraints, desired);

  Vec2 widgetPos = pos.has_value()
                       ? ResolveAbsolutePosition(ctx, pos.value(), finalSize)
                       : ctx->cursorPos;

  // Container background (subtle inset)
  Color containerBg = panelStyle.headerBackground;
  ctx->renderer.DrawRectFilled(widgetPos, finalSize, containerBg, radius);

  int currentIndex = activeIndex ? *activeIndex : 0;
  if (currentIndex < 0 || currentIndex >= (int)options.size()) currentIndex = 0;

  bool changed = false;
  float cursorX = widgetPos.x;
  for (size_t i = 0; i < options.size(); ++i) {
    Vec2 segPos(cursorX, widgetPos.y);
    Vec2 segSize(segWidths[i], finalSize.y);
    bool hover = IsMouseOver(ctx, segPos, segSize);
    bool isActive = (int)i == currentIndex;

    if (hover && ctx->input.IsMousePressed(0) && !isActive) {
      currentIndex = (int)i;
      changed = true;
    }

    Color segBg(0, 0, 0, 0);
    Color textColor = textStyle.color;
    if (isActive) {
      segBg = accent;
      textColor = Color(1.0f, 1.0f, 1.0f, 1.0f);
    } else if (hover) {
      // Overlay tinted by theme so the hover is visible on light backgrounds too.
      segBg = ctx->style.isDarkTheme ? Color(1.0f, 1.0f, 1.0f, 0.08f)
                                     : Color(0.0f, 0.0f, 0.0f, 0.06f);
    }
    if (segBg.a > 0.001f) {
      ctx->renderer.DrawRectFilled(segPos, segSize, segBg, radius);
    }

    Vec2 ts = MeasureTextCached(ctx, options[i], textStyle.fontSize);
    uint32_t cp = (icons && i < icons->size()) ? (*icons)[i] : 0u;
    float iconSlot = 0.0f;
    if (cp != 0u) {
      iconSlot = segIconSize + (options[i].empty() ? 0.0f : segIconGap);
    }
    float blockWidth = ts.x + iconSlot;
    float blockX = segPos.x + (segSize.x - blockWidth) * 0.5f;
    if (cp != 0u) {
      DrawWidgetIcon(ctx, segPos, segSize, cp, textColor, segIconSize,
                     blockX - segPos.x, options[i].empty() ? 0.0f : segIconGap);
    }
    if (!options[i].empty()) {
      Vec2 textPos(blockX + iconSlot,
                   segPos.y + (segSize.y - ts.y) * 0.5f);
      ctx->renderer.DrawText(textPos, options[i], textColor, textStyle.fontSize);
    }

    cursorX += segWidths[i] + gap;
  }

  if (activeIndex) *activeIndex = currentIndex;

  // Generate stable id used as a hash key; not currently used but reserved
  // for future state (focus, animations).
  uint32_t segId = GenerateId("SEG:", id.c_str());
  (void)segId;

  ctx->lastItemPos = widgetPos;
  AdvanceCursor(ctx, finalSize);
  return changed;
}

bool Button(const std::string &label, ButtonSize variant,
            std::optional<Vec2> pos, bool enabled) {
  UIContext *ctx = GetContext();
  if (!ctx)
    return false;

  // Build an override style based on Fluent 2 size variants.
  // Heights derive from font + padding*2: Small≈24, Medium≈32, Large≈40.
  ButtonStyle s = ctx->GetEffectiveButtonStyle();
  switch (variant) {
    case ButtonSize::Small:
      s.padding = Vec2(8.0f, 3.0f);   // spacingHorizontalS × Fluent S
      s.text.fontSize = 12.0f;        // fontSizeBase200
      s.cornerRadius = 2.0f;          // <32px → borderRadiusSmall
      break;
    case ButtonSize::Medium:
      s.padding = Vec2(12.0f, 5.0f);  // spacingHorizontalM × Fluent M
      s.text.fontSize = 14.0f;        // fontSizeBase300
      s.cornerRadius = 4.0f;          // borderRadiusMedium
      break;
    case ButtonSize::Large:
      s.padding = Vec2(16.0f, 8.0f);  // spacingHorizontalL × Fluent L
      s.text.fontSize = 16.0f;        // fontSizeBase400
      s.cornerRadius = 4.0f;
      break;
  }
  ctx->buttonStyleStack.push_back(s);
  bool clicked = Button(label, Vec2(0.0f, 0.0f), pos, enabled);
  ctx->buttonStyleStack.pop_back();
  return clicked;
}

bool Button(const std::string &label, uint32_t iconCodepoint, ButtonSize variant,
            std::optional<Vec2> pos, bool enabled) {
  UIContext *ctx = GetContext();
  if (!ctx) return false;

  ButtonStyle s = ctx->GetEffectiveButtonStyle();
  switch (variant) {
    case ButtonSize::Small:
      s.padding = Vec2(8.0f, 3.0f);
      s.text.fontSize = 12.0f;
      s.cornerRadius = 2.0f;
      break;
    case ButtonSize::Medium:
      s.padding = Vec2(12.0f, 5.0f);
      s.text.fontSize = 14.0f;
      s.cornerRadius = 4.0f;
      break;
    case ButtonSize::Large:
      s.padding = Vec2(16.0f, 8.0f);
      s.text.fontSize = 16.0f;
      s.cornerRadius = 4.0f;
      break;
  }
  ctx->buttonStyleStack.push_back(s);
  bool clicked = Button(label, iconCodepoint, Vec2(0.0f, 0.0f), pos, enabled);
  ctx->buttonStyleStack.pop_back();
  return clicked;
}

bool IconButton(uint32_t iconCodepoint, float size, std::optional<Vec2> pos, bool enabled) {
  UIContext *ctx = GetContext();
  if (!ctx) return false;
  Vec2 sz(size, size);
  return Button(std::string(), iconCodepoint, sz, pos, enabled);
}

void Label(const std::string &text, uint32_t iconCodepoint,
           std::optional<Vec2> position, TypographyStyle variant, bool disabled);

void Label(const std::string &text, std::optional<Vec2> position,
           TypographyStyle variant, bool disabled) {
  Label(text, 0u, position, variant, disabled);
}

void Label(const std::string &text, uint32_t iconCodepoint,
           std::optional<Vec2> position, TypographyStyle variant, bool disabled) {
  UIContext *ctx = GetContext();
  if (!ctx)
    return;

  const TextStyle &textStyle = ctx->style.GetTextStyle(variant);
  Color color = disabled ? ctx->style.label.disabledColor : textStyle.color;

  Vec2 measured = MeasureTextCached(ctx, text, textStyle.fontSize);
  if ((measured.x <= 0.0f || measured.y <= 0.0f) && !text.empty()) {
    measured =
        Vec2(text.length() * textStyle.fontSize * 0.6f, textStyle.fontSize);
  }
  if (measured.y <= 0.0f) measured.y = textStyle.fontSize;

  float iconSize = textStyle.fontSize;
  float iconGap = (iconCodepoint != 0u && !text.empty()) ? 6.0f : 0.0f;
  float iconSlot = (iconCodepoint != 0u) ? (iconSize + iconGap) : 0.0f;

  Vec2 contentSize(measured.x + iconSlot, std::max(measured.y, iconSize));

  // En layouts horizontales con altura conocida (ej. toolbar), estirar la
  // altura del label a la del slot para que quede vertical-centrado igual
  // que el resto de widgets (botones, drag, combo) de la misma fila.
  bool inHorizontal = !ctx->layoutStack.empty() && !ctx->layoutStack.back().isVertical;
  if (inHorizontal) {
    float lineH = ctx->layoutStack.back().availableSpace.y;
    if (lineH > contentSize.y) {
      contentSize.y = lineH;
    }
  }

  LayoutConstraints constraints = ConsumeNextConstraints();
  Vec2 finalSize = ApplyConstraints(ctx, constraints, contentSize);

  bool hasAbsolutePos = position.has_value();
  Vec2 pos;
  if (hasAbsolutePos) {
    pos = ResolveAbsolutePosition(ctx, position.value(), finalSize);
  } else {
    pos = ctx->cursorPos;
  }

  if (IsRectInViewport(ctx, pos, finalSize)) {
    if (iconCodepoint != 0u) {
      DrawWidgetIcon(ctx, pos, finalSize, iconCodepoint, color, iconSize, 0.0f, iconGap);
    }
    if (!text.empty()) {
      Vec2 textPos(pos.x + iconSlot,
                   pos.y + (finalSize.y - measured.y) * 0.5f);
      ctx->renderer.DrawText(textPos, text, color, textStyle.fontSize);
    }
  }
  ctx->lastItemPos = pos;

  if (!hasAbsolutePos) {
    AdvanceCursor(ctx, finalSize);
  } else {
    ctx->lastItemSize = finalSize;
  }
}

void IconLabel(uint32_t iconCodepoint, float size, std::optional<Color> color,
               std::optional<Vec2> position) {
  UIContext *ctx = GetContext();
  if (!ctx || iconCodepoint == 0u) return;

  const TextStyle &textStyle = ctx->style.GetTextStyle(TypographyStyle::Body);
  float iconSize = size > 0.0f ? size : textStyle.fontSize;
  Color iconColor = color.has_value() ? color.value() : textStyle.color;

  Vec2 finalSize(iconSize, iconSize);
  bool hasAbsolutePos = position.has_value();
  Vec2 pos;
  if (hasAbsolutePos) {
    pos = ResolveAbsolutePosition(ctx, position.value(), finalSize);
  } else {
    pos = ctx->cursorPos;
  }

  if (IsRectInViewport(ctx, pos, finalSize)) {
    DrawWidgetIcon(ctx, pos, finalSize, iconCodepoint, iconColor, iconSize, 0.0f, 0.0f);
  }
  ctx->lastItemPos = pos;
  if (!hasAbsolutePos) {
    AdvanceCursor(ctx, finalSize);
  } else {
    ctx->lastItemSize = finalSize;
  }
}

// Phase C1: Greedy word-wrap label.
// maxWidth = 0 → use available width from the current layout cursor (Fill behavior).
void LabelWrapped(const std::string &text, float maxWidth,
                  std::optional<Vec2> position, TypographyStyle variant,
                  bool disabled) {
  UIContext *ctx = GetContext();
  if (!ctx) return;

  const TextStyle &textStyle = ctx->style.GetTextStyle(variant);
  Color color = disabled ? ctx->style.label.disabledColor : textStyle.color;

  // Resolve effective wrap width
  float wrapW = maxWidth;
  if (wrapW <= 0.0f) {
    // Use remaining space in current vertical layout, or container width
    if (!ctx->layoutStack.empty()) {
      wrapW = ctx->layoutStack.back().availableSpace.x;
    }
    if (wrapW <= 0.0f) wrapW = 200.0f;
  }

  // Greedy wrap: split on spaces, accumulate words until line exceeds wrapW.
  std::vector<std::string> lines;
  std::string current;
  size_t i = 0;
  const size_t n = text.size();
  while (i < n) {
    // Hard newline
    if (text[i] == '\n') {
      lines.push_back(current);
      current.clear();
      ++i;
      continue;
    }
    // Extract next word (run of non-space, non-newline chars)
    size_t wStart = i;
    while (i < n && text[i] != ' ' && text[i] != '\n') ++i;
    std::string word = text.substr(wStart, i - wStart);

    // Tentatively add separator + word
    std::string tentative = current.empty() ? word : current + " " + word;
    Vec2 sz = MeasureTextCached(ctx, tentative, textStyle.fontSize);
    if (sz.x <= wrapW || current.empty()) {
      current = tentative;
    } else {
      lines.push_back(current);
      current = word;
    }

    // Skip a single space (multiple spaces collapse)
    while (i < n && text[i] == ' ') ++i;
  }
  if (!current.empty()) lines.push_back(current);
  if (lines.empty()) lines.push_back("");

  // Compute total size
  float lineH = textStyle.fontSize * 1.4f;
  float maxLineW = 0.0f;
  for (const auto& ln : lines) {
    Vec2 m = MeasureTextCached(ctx, ln, textStyle.fontSize);
    maxLineW = std::max(maxLineW, m.x);
  }
  Vec2 totalSize(std::min(maxLineW, wrapW), lineH * static_cast<float>(lines.size()));

  LayoutConstraints constraints = ConsumeNextConstraints();
  Vec2 finalSize = ApplyConstraints(ctx, constraints, totalSize);

  bool hasAbsolutePos = position.has_value();
  Vec2 pos = hasAbsolutePos
      ? ResolveAbsolutePosition(ctx, position.value(), finalSize)
      : ctx->cursorPos;

  if (IsRectInViewport(ctx, pos, finalSize)) {
    Vec2 cursor = pos;
    for (const auto& ln : lines) {
      ctx->renderer.DrawText(cursor, ln, color, textStyle.fontSize);
      cursor.y += lineH;
    }
  }
  ctx->lastItemPos = pos;

  if (!hasAbsolutePos) {
    AdvanceCursor(ctx, finalSize);
  } else {
    ctx->lastItemSize = finalSize;
  }
}

// ─── Phase C8: Rich text label with inline markup ─────────────────────────
namespace {

struct RichSpan {
  std::string text;
  Color color;
  float size;
  bool bold = false;
  bool italic = false;
  bool isLink = false;
  std::string linkUrl;
};

// Hex parser for #RRGGBB or #RRGGBBAA
static bool ParseHexColor(const std::string &s, Color &out) {
  size_t start = 0;
  if (!s.empty() && s[0] == '#') start = 1;
  if (s.size() - start != 6 && s.size() - start != 8) return false;
  auto hex = [](char c) -> int {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
  };
  int v[8] = {0};
  size_t len = s.size() - start;
  for (size_t i = 0; i < len; ++i) {
    int h = hex(s[start + i]);
    if (h < 0) return false;
    v[i] = h;
  }
  out.r = (v[0] * 16 + v[1]) / 255.0f;
  out.g = (v[2] * 16 + v[3]) / 255.0f;
  out.b = (v[4] * 16 + v[5]) / 255.0f;
  out.a = (len == 8) ? (v[6] * 16 + v[7]) / 255.0f : 1.0f;
  return true;
}

// Tokenize markup into spans. Tags supported: <b> <i> <color=...> <size=N> <a href="url">
static std::vector<RichSpan> ParseMarkup(const std::string &markup,
                                         const Color &baseColor,
                                         float baseSize) {
  std::vector<RichSpan> out;
  std::vector<Color> colorStack{baseColor};
  std::vector<float> sizeStack{baseSize};
  int boldDepth = 0;
  int italicDepth = 0;
  std::string linkUrl;

  std::string buf;
  auto flush = [&]() {
    if (!buf.empty()) {
      RichSpan s;
      s.text = buf;
      s.color = colorStack.back();
      s.size = sizeStack.back();
      s.bold = boldDepth > 0;
      s.italic = italicDepth > 0;
      s.isLink = !linkUrl.empty();
      s.linkUrl = linkUrl;
      out.push_back(std::move(s));
      buf.clear();
    }
  };

  size_t i = 0, n = markup.size();
  while (i < n) {
    if (markup[i] == '<') {
      size_t j = markup.find('>', i + 1);
      if (j == std::string::npos) { buf.push_back(markup[i]); ++i; continue; }
      std::string tag = markup.substr(i + 1, j - i - 1);
      flush();

      // Closing tag?
      if (!tag.empty() && tag[0] == '/') {
        std::string name = tag.substr(1);
        if (name == "b" && boldDepth > 0) --boldDepth;
        else if (name == "i" && italicDepth > 0) --italicDepth;
        else if (name == "color" && colorStack.size() > 1) colorStack.pop_back();
        else if (name == "size" && sizeStack.size() > 1) sizeStack.pop_back();
        else if (name == "a") linkUrl.clear();
      } else {
        // Opening tag
        if (tag == "b") {
          ++boldDepth;
        } else if (tag == "i") {
          ++italicDepth;
        } else if (tag.rfind("color=", 0) == 0) {
          Color c = colorStack.back();
          if (ParseHexColor(tag.substr(6), c)) colorStack.push_back(c);
          else colorStack.push_back(colorStack.back());
        } else if (tag.rfind("size=", 0) == 0) {
          try {
            float s = std::stof(tag.substr(5));
            sizeStack.push_back(s > 1.0f ? s : sizeStack.back());
          } catch (...) {
            sizeStack.push_back(sizeStack.back());
          }
        } else if (tag.rfind("a ", 0) == 0 || tag == "a") {
          // Parse href="..."
          size_t hp = tag.find("href=");
          if (hp != std::string::npos) {
            size_t qs = hp + 5;
            char q = '"';
            if (qs < tag.size() && (tag[qs] == '"' || tag[qs] == '\'')) {
              q = tag[qs]; ++qs;
            }
            size_t qe = tag.find(q, qs);
            if (qe != std::string::npos) {
              linkUrl = tag.substr(qs, qe - qs);
            }
          }
          if (linkUrl.empty()) linkUrl = "#";
        }
      }
      i = j + 1;
    } else {
      buf.push_back(markup[i]);
      ++i;
    }
  }
  flush();
  return out;
}

} // namespace

void LabelRich(const std::string &markup, float maxWidth,
               std::optional<Vec2> position, TypographyStyle variant,
               std::function<void(const std::string &url)> onLinkClicked) {
  UIContext *ctx = GetContext();
  if (!ctx) return;

  const TextStyle &textStyle = ctx->style.GetTextStyle(variant);
  Color baseColor = textStyle.color;
  float baseSize = textStyle.fontSize;

  std::vector<RichSpan> spans = ParseMarkup(markup, baseColor, baseSize);

  // Resolve effective wrap width (0 = no wrap)
  float wrapW = maxWidth;
  if (wrapW <= 0.0f && !ctx->layoutStack.empty()) {
    wrapW = ctx->layoutStack.back().availableSpace.x;
  }

  // Token = a contiguous chunk inside a span split on whitespace.
  // Each token carries its own style. A token can break across lines.
  struct Token {
    std::string text;
    bool isSpace = false;
    bool isNewline = false;
    Color color;
    float size = 16.0f;
    bool bold = false, italic = false;
    bool isLink = false;
    std::string linkUrl;
    Vec2 measured;
  };

  std::vector<Token> tokens;
  for (const auto &s : spans) {
    size_t i = 0, n = s.text.size();
    while (i < n) {
      if (s.text[i] == '\n') {
        Token t; t.text = "\n"; t.isNewline = true; t.color = s.color;
        t.size = s.size; t.bold = s.bold; t.italic = s.italic;
        t.isLink = s.isLink; t.linkUrl = s.linkUrl;
        tokens.push_back(t); ++i; continue;
      }
      if (s.text[i] == ' ' || s.text[i] == '\t') {
        Token t; t.text = " "; t.isSpace = true; t.color = s.color;
        t.size = s.size; t.bold = s.bold; t.italic = s.italic;
        t.isLink = s.isLink; t.linkUrl = s.linkUrl;
        t.measured = MeasureTextCached(ctx, " ", s.size);
        tokens.push_back(t); ++i;
        while (i < n && (s.text[i] == ' ' || s.text[i] == '\t')) ++i;
        continue;
      }
      size_t start = i;
      while (i < n && s.text[i] != ' ' && s.text[i] != '\t' && s.text[i] != '\n') ++i;
      Token t; t.text = s.text.substr(start, i - start);
      t.color = s.color; t.size = s.size; t.bold = s.bold; t.italic = s.italic;
      t.isLink = s.isLink; t.linkUrl = s.linkUrl;
      t.measured = MeasureTextCached(ctx, t.text, s.size);
      tokens.push_back(t);
    }
  }

  // Lay out tokens into lines respecting wrapW.
  struct LineEntry {
    std::vector<size_t> tokenIndices;
    float width = 0.0f;
    float height = 0.0f;
  };
  std::vector<LineEntry> lines;
  LineEntry cur;
  for (size_t k = 0; k < tokens.size(); ++k) {
    const Token &t = tokens[k];
    if (t.isNewline) {
      if (cur.height <= 0.0f) cur.height = t.size * 1.4f;
      lines.push_back(std::move(cur));
      cur = {};
      continue;
    }
    float w = t.measured.x;
    if (wrapW > 0.0f && !t.isSpace && cur.width + w > wrapW &&
        !cur.tokenIndices.empty()) {
      lines.push_back(std::move(cur));
      cur = {};
      // Skip if the wrap candidate is just a leading space — fold instead
      if (t.isSpace) continue;
    }
    if (t.isSpace && cur.tokenIndices.empty()) continue;
    cur.tokenIndices.push_back(k);
    cur.width += w;
    cur.height = std::max(cur.height, t.size * 1.4f);
  }
  if (!cur.tokenIndices.empty() || cur.height > 0.0f) lines.push_back(std::move(cur));
  if (lines.empty()) lines.push_back({});

  // Total size
  float totalH = 0.0f, maxLineW = 0.0f;
  for (auto &l : lines) {
    if (l.height <= 0.0f) l.height = baseSize * 1.4f;
    totalH += l.height;
    maxLineW = std::max(maxLineW, l.width);
  }
  Vec2 totalSize(wrapW > 0.0f ? std::min(maxLineW, wrapW) : maxLineW, totalH);

  LayoutConstraints constraints = ConsumeNextConstraints();
  Vec2 finalSize = ApplyConstraints(ctx, constraints, totalSize);

  bool hasAbsolutePos = position.has_value();
  Vec2 pos = hasAbsolutePos
      ? ResolveAbsolutePosition(ctx, position.value(), finalSize)
      : ctx->cursorPos;

  // Render
  if (IsRectInViewport(ctx, pos, finalSize)) {
    float yCursor = pos.y;
    for (const auto &line : lines) {
      float xCursor = pos.x;
      for (size_t idx : line.tokenIndices) {
        const Token &t = tokens[idx];
        float baseline = yCursor + (line.height - t.size) * 0.5f;
        if (t.isLink) {
          // Underline + link colour (use accent; honour explicit color overrides)
          Color linkCol = (t.color.r == baseColor.r &&
                          t.color.g == baseColor.g &&
                          t.color.b == baseColor.b)
              ? Color(0.30f, 0.60f, 1.0f, 1.0f)
              : t.color;
          ctx->renderer.DrawText(Vec2(xCursor, baseline), t.text, linkCol, t.size);
          float underY = baseline + t.size * 0.95f;
          ctx->renderer.DrawLine(Vec2(xCursor, underY),
                                 Vec2(xCursor + t.measured.x, underY),
                                 linkCol, 1.0f);

          // Hover/click detection
          Vec2 lpos(xCursor, baseline);
          Vec2 lsize(t.measured.x, t.size);
          if (IsMouseOver(ctx, lpos, lsize)) {
            ctx->desiredCursor = UIContext::CursorType::Hand;
            if (ctx->input.IsMousePressed(0) && onLinkClicked) {
              onLinkClicked(t.linkUrl);
            }
          }
        } else if (t.bold) {
          // Fake bold by drawing twice with a 0.5px offset
          ctx->renderer.DrawText(Vec2(xCursor, baseline), t.text, t.color, t.size);
          ctx->renderer.DrawText(Vec2(xCursor + 0.5f, baseline), t.text, t.color, t.size);
        } else {
          ctx->renderer.DrawText(Vec2(xCursor, baseline), t.text, t.color, t.size);
        }
        xCursor += t.measured.x;
      }
      yCursor += line.height;
    }
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

  // Detect if we're inside a horizontal layout
  bool inHorizontal = !ctx->layoutStack.empty() && !ctx->layoutStack.back().isVertical;

  if (inHorizontal) {
    // Vertical separator: thin line, full height of the layout
    float lineHeight = ctx->layoutStack.back().contentSize.y;
    if (lineHeight <= 0.0f) lineHeight = ctx->layoutStack.back().availableSpace.y;
    if (lineHeight <= 0.0f) lineHeight = 20.0f; // fallback
    float sepWidth = separator.thickness;

    Vec2 pos = ctx->cursorPos;
    Vec2 drawPos = pos + Vec2(separator.padding * 0.5f, 0.0f);
    Vec2 drawSize(sepWidth, lineHeight);
    if (IsRectInViewport(ctx, drawPos, drawSize)) {
      ctx->renderer.DrawRectFilled(drawPos, drawSize, separator.color, 0.0f);
    }

    Vec2 totalSize(sepWidth + separator.padding, lineHeight);
    ctx->lastItemPos = pos;
    AdvanceCursor(ctx, totalSize);
  } else {
    // Horizontal separator (default): full width line
    Vec2 available = GetCurrentAvailableSpace(ctx);
    float separatorWidth = available.x > 0.0f ? available.x
                 : ctx->renderer.GetViewportSize().x - ctx->cursorPos.x * 2.0f;
    Vec2 desired(separatorWidth, separator.thickness);

    LayoutConstraints constraints = ConsumeNextConstraints(SizeConstraint::Fill);
    Vec2 finalSize = ApplyConstraints(ctx, constraints, desired);
    finalSize.y = std::max(finalSize.y, separator.thickness);

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
  // brief 21: scope grid cell content by the grid id (paired with gridStack).
  PushID(id.c_str());

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
  // brief 21: pop the scope pushed in BeginGrid.
  PopID();

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
  LayoutConstraints constraints = ConsumeNextConstraints(SizeConstraint::Fill);
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

    // Phase C6: eyedropper button overlay (small square at right of hex)
    {
      Vec2 eyePos(hexPos.x + hexSize.x - PREVIEW_H, hexPos.y);
      Vec2 eyeSize(PREVIEW_H, PREVIEW_H);
      bool hoverEye = (mx >= eyePos.x && mx < eyePos.x + eyeSize.x &&
                       my >= eyePos.y && my < eyePos.y + eyeSize.y);
      Color eyeBg = state.eyedropperActive
          ? ctx->style.button.background.hover
          : (hoverEye ? Color(0.2f, 0.2f, 0.2f, 0.7f) : Color(0.13f, 0.13f, 0.13f, 0.5f));
      ctx->renderer.DrawRectFilled(eyePos, eyeSize, eyeBg, 3.0f);
      ctx->renderer.DrawRect(eyePos, eyeSize, Color(0.3f, 0.3f, 0.3f, 0.7f), 3.0f);
      ctx->renderer.DrawText(
          Vec2(eyePos.x + 4.0f, eyePos.y + (eyeSize.y - 13.0f) * 0.5f),
          "Pick", ctx->style.GetTextStyle(TypographyStyle::Body).color, 11.0f);
      if (hoverEye && mousePressed) {
        state.eyedropperActive = !state.eyedropperActive;
      }
      if (state.eyedropperActive) {
        ctx->desiredCursor = UIContext::CursorType::Hand;
        // Click anywhere outside the picker captures the pixel
        bool insidePicker = (mx >= widgetPos.x && mx < widgetPos.x + totalSize.x &&
                             my >= widgetPos.y && my < widgetPos.y + totalSize.y);
        if (mousePressed && !insidePicker) {
          int px = static_cast<int>(mx);
          int py = static_cast<int>(my);
          float dpi = ctx->renderer.GetDPIScale();
          Color picked = ctx->renderer.ReadPixel(static_cast<int>(px * dpi),
                                                  static_cast<int>(py * dpi));
          if (picked.a > 0.0f) {
            picked.ToHSV(state.hue, state.saturation, state.value);
            state.alpha = picked.a;
            changed = true;
          }
          state.eyedropperActive = false;
        }
        if (ctx->input.IsKeyPressed(SDL_SCANCODE_ESCAPE)) {
          state.eyedropperActive = false;
        }
      }
    }
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

  // Phase B1: publish color-picker state
  {
    bool hover = (mx >= widgetPos.x && mx < widgetPos.x + totalSize.x &&
                  my >= widgetPos.y && my < widgetPos.y + totalSize.y);
    bool active = (ctx->activeWidgetId == pickerId);
    SetLastItem(pickerId, widgetPos, widgetPos + totalSize,
                hover, active, ctx->focusedWidgetId == pickerId, changed);
  }

  return changed;
}

} // namespace FluentUI
