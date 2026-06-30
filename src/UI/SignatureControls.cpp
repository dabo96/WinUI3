// Brief 14 — Controles de firma de Fluent (parte no-overlay).
// ToggleSwitch, Expander (card colapsable), SplitButton / DropDownButton y
// RatingControl. Archivo nuevo para no colisionar con BasicWidgets.cpp.
// NumberBox vive en InputWidgets.cpp; TeachingTip / ContentDialog en
// OverlayWidgets.cpp. Flyout / MenuFlyout (14-A) ya existen en OverlayWidgets.cpp.
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
#include <optional>
#include <vector>

namespace FluentUI {

// Lerp lineal de color (motion del brief 10 degradado a interpolación simple).
static Color LerpColor(const Color &a, const Color &b, float t) {
  t = std::clamp(t, 0.0f, 1.0f);
  return Color(a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t,
               a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t);
}

// Aproxima un muelle/transición con un acercamiento exponencial al objetivo.
// Devuelve el nuevo valor. (Brief 10 motion → ease simple, sin overshoot.)
static float ApproachAnim(float current, float target, float dt, float speed = 14.0f) {
  float t = std::clamp(dt * speed, 0.0f, 1.0f);
  float next = current + (target - current) * t;
  if (std::abs(next - target) < 0.001f)
    next = target;
  return next;
}

// ════════════════════════════════════════════════════════════════════════════
// 1) ToggleSwitch
// ════════════════════════════════════════════════════════════════════════════
bool ToggleSwitch(const std::string &label, bool *value, const std::string &onText,
                  const std::string &offText, std::optional<Vec2> pos) {
  UIContext *ctx = GetContext();
  if (!ctx)
    return false;

  uint32_t id = GenerateId("TOGGLE:", label.c_str());
  ctx->focusableWidgets.push_back(id);

  auto boolEntry = ctx->boolStates.try_emplace(id, false);
  bool current = value ? *value : boolEntry.first->second;

  const TextStyle &labelStyle = ctx->style.GetTextStyle(TypographyStyle::Body);
  const PanelStyle &panelStyle = ctx->style.panel;
  Color accent = ctx->style.accentColor;

  float trackW = S(40.0f), trackH = S(20.0f);
  float thumbR = trackH * 0.5f - S(2.0f);

  // Header (label) opcional encima del switch, como en WinUI.
  bool hasHeader = !label.empty();
  Vec2 headerSize = hasHeader ? MeasureTextCached(ctx, label, labelStyle.fontSize)
                              : Vec2(0.0f, 0.0f);
  float headerH = hasHeader ? headerSize.y + S(4.0f) : 0.0f;

  const std::string &statusText = current ? onText : offText;
  Vec2 statusSize = statusText.empty()
                        ? Vec2(0.0f, 0.0f)
                        : MeasureTextCached(ctx, statusText, labelStyle.fontSize);
  float statusSlot = statusText.empty() ? 0.0f : (S(8.0f) + statusSize.x);

  float rowH = std::max(trackH, labelStyle.fontSize);
  Vec2 totalSize(std::max(headerSize.x, trackW + statusSlot), headerH + rowH);

  LayoutConstraints constraints = ConsumeNextConstraints();
  Vec2 finalSize = ApplyConstraints(ctx, constraints, totalSize);

  bool hasAbsolutePos = pos.has_value();
  Vec2 widgetPos = hasAbsolutePos
                       ? ResolveAbsolutePosition(ctx, pos.value(), finalSize)
                       : ctx->cursorPos;

  Vec2 trackPos(widgetPos.x, widgetPos.y + headerH + (rowH - trackH) * 0.5f);

  // Hit area = track + status text (no toda la fila Fill).
  Vec2 hitPos(widgetPos.x, widgetPos.y + headerH);
  Vec2 hitSize(trackW + statusSlot, rowH);
  bool hover = IsMouseOver(ctx, hitPos, hitSize);

  bool toggled = false;
  if (hover && ctx->input.IsMousePressed(0)) {
    current = !current;
    toggled = true;
  }
  if (ctx->focusedWidgetId == id &&
      (ctx->input.IsKeyPressed(SDL_SCANCODE_SPACE) ||
       ctx->input.IsKeyPressed(SDL_SCANCODE_RETURN))) {
    current = !current;
    toggled = true;
  }

  if (value)
    *value = current;
  else
    boolEntry.first->second = current;

  if (hover)
    ctx->desiredCursor = UIContext::CursorType::Hand;

  // Animación 0→1 del thumb (floatStates[AnimSlot(id,0)]).
  float &anim = ctx->floatStates[AnimSlot(id, 0)];
  float target = current ? 1.0f : 0.0f;
  // Primera vez: arrancar ya en el estado correcto (sin animación fantasma).
  auto animInit = ctx->boolStates.try_emplace(AnimSlot(id, 1), false);
  if (!animInit.first->second) {
    anim = target;
    animInit.first->second = true;
  }
  anim = ApproachAnim(anim, target, ctx->deltaTime);

  if (IsRectInViewport(ctx, widgetPos, finalSize)) {
    // Header.
    if (hasHeader) {
      ctx->renderer.DrawText(widgetPos, label, labelStyle.color,
                             labelStyle.fontSize);
    }

    if (ctx->focusedWidgetId == id) {
      DrawFocusRing(ctx, trackPos, Vec2(trackW, trackH), trackH * 0.5f);
    }

    // Track: OFF neutro con borde, ON acento. Color interpolado por anim.
    Color trackOff = panelStyle.headerBackground;
    Color trackColor = LerpColor(trackOff, accent, anim);
    ctx->renderer.DrawRectFilled(trackPos, Vec2(trackW, trackH), trackColor,
                                 trackH * 0.5f);
    if (anim < 0.5f) {
      Color border = panelStyle.borderColor;
      border.a = 0.9f;
      ctx->renderer.DrawRect(trackPos, Vec2(trackW, trackH),
                             LerpColor(border, accent, anim), trackH * 0.5f);
    }

    // Thumb circular blanco que se desplaza de un extremo al otro.
    float cxMin = trackPos.x + trackH * 0.5f;
    float cxMax = trackPos.x + trackW - trackH * 0.5f;
    float cx = cxMin + (cxMax - cxMin) * anim;
    float cy = trackPos.y + trackH * 0.5f;
    Color thumbColor = anim > 0.5f ? Color(1.0f, 1.0f, 1.0f, 1.0f)
                                   : labelStyle.color;
    ctx->renderer.DrawCircle(Vec2(cx, cy), thumbR, thumbColor, true);

    // Texto on/off a la derecha.
    if (!statusText.empty()) {
      Vec2 stPos(trackPos.x + trackW + S(8.0f),
                 trackPos.y + (trackH - statusSize.y) * 0.5f);
      ctx->renderer.DrawText(stPos, statusText, labelStyle.color,
                             labelStyle.fontSize);
    }
  }

  // Accesibilidad: rol CheckBox/Switch, value = "on"/"off".
  ctx->widgetTree.FindOrCreate(id, ctx->frame, [&]() {
    auto node = std::make_unique<WidgetNode>(id);
    node->accessibleRole = WidgetNode::AccessibleRole::CheckBox;
    node->accessibleName = label;
    return node;
  });
  if (auto *node = ctx->widgetTree.FindById(id))
    node->accessibleValue = current ? "on" : "off";

  ctx->lastItemPos = widgetPos;
  if (!hasAbsolutePos)
    AdvanceCursor(ctx, finalSize);
  else
    ctx->lastItemSize = finalSize;

  SetLastItem(id, widgetPos, widgetPos + finalSize, hover,
              hover && ctx->input.IsMouseDown(0), ctx->focusedWidgetId == id,
              toggled);
  return toggled;
}

// ════════════════════════════════════════════════════════════════════════════
// 2) Expander (card colapsable con chevron y cuerpo animado/clipado)
// ════════════════════════════════════════════════════════════════════════════
namespace {
struct ExpanderFrame {
  uint32_t id = 0;
  Vec2 cardPos{0.0f, 0.0f};
  float cardWidth = 0.0f;
  float headerH = 0.0f;
  float bodyPad = 0.0f;
  Vec2 savedCursor{0.0f, 0.0f};
  Vec2 savedLastItemPos{0.0f, 0.0f};
  Vec2 savedLastItemSize{0.0f, 0.0f};
  bool clipPushed = false;
  bool layoutPushed = false;
  float animHeight = 0.0f; // altura visible animada del cuerpo (clip)
};
// Pila local (immediate-mode, single-threaded). No necesita vivir en UIContext.
static std::vector<ExpanderFrame> g_expanderStack;
} // namespace

bool BeginExpander(const std::string &id, const std::string &header,
                   uint32_t icon, bool *expanded) {
  UIContext *ctx = GetContext();
  if (!ctx)
    return false;

  uint32_t wid = GenerateId("EXPANDER:", id.c_str());
  ctx->focusableWidgets.push_back(wid);

  auto openEntry = ctx->boolStates.try_emplace(wid, false);
  bool isOpen = expanded ? *expanded : openEntry.first->second;

  const TextStyle &headerStyle =
      ctx->style.GetTextStyle(TypographyStyle::BodyStrong);
  const PanelStyle &panelStyle = ctx->style.panel;

  float padX = panelStyle.padding.x;
  float headerH = headerStyle.fontSize + S(20.0f);
  float chevronSize = headerStyle.fontSize;
  float iconGap = S(8.0f);

  Vec2 desired(0.0f, headerH);
  LayoutConstraints constraints = ConsumeNextConstraints(SizeConstraint::Fill);
  Vec2 finalSize = ApplyConstraints(ctx, constraints, desired);
  if (finalSize.x <= 0.0f) {
    Vec2 avail = GetCurrentAvailableSpace(ctx);
    finalSize.x = avail.x > 0.0f ? avail.x : 280.0f;
  }
  float cardWidth = finalSize.x;
  Vec2 cardPos = ctx->cursorPos;

  // --- Interacción del header ---
  Vec2 headerPos = cardPos;
  Vec2 headerSize(cardWidth, headerH);
  Vec2 mouse(ctx->input.MouseX(), ctx->input.MouseY());
  bool hover = PointInRect(mouse, headerPos, headerSize) &&
               IsRectInViewport(ctx, headerPos, headerSize) &&
               !IsMouseInputBlocked(ctx);
  bool clicked = hover && ctx->input.IsMousePressed(0);
  if (ctx->focusedWidgetId == wid &&
      (ctx->input.IsKeyPressed(SDL_SCANCODE_SPACE) ||
       ctx->input.IsKeyPressed(SDL_SCANCODE_RETURN)))
    clicked = true;
  if (clicked) {
    isOpen = !isOpen;
    openEntry.first->second = isOpen;
    if (expanded)
      *expanded = isOpen;
  }
  if (hover)
    ctx->desiredCursor = UIContext::CursorType::Hand;

  // --- Animación de la altura del cuerpo ---
  // measured = altura del contenido medida en EndExpander el frame anterior.
  float measured = ctx->floatStates[AnimSlot(wid, 1)];
  float &animH = ctx->floatStates[AnimSlot(wid, 0)];
  if (isOpen) {
    animH = ApproachAnim(animH, measured, ctx->deltaTime);
  } else {
    animH = 0.0f; // colapso instantáneo (degradación de motion brief 10)
  }
  float bodyPad = panelStyle.padding.y;
  // Primera apertura sin medir aún: forzar un frame de construcción (clip a 0)
  // para medir el contenido; los frames siguientes animan la apertura.
  bool needMeasure = isOpen && measured <= 0.0f;
  bool bodyVisible = isOpen && (animH > 1.0f || needMeasure);
  float effectiveBodyH = needMeasure ? 0.0f : animH;
  float visibleBodyH = bodyVisible ? (effectiveBodyH + bodyPad * 2.0f) : 0.0f;
  float cardHeight = headerH + visibleBodyH;

  // --- Dibujo de la card (fondo + elevación + borde) ---
  if (IsRectInViewport(ctx, cardPos, Vec2(cardWidth, cardHeight))) {
    Color cardBg = AdjustContainerBackground(panelStyle.background,
                                             ctx->style.isDarkTheme);
    ctx->renderer.DrawElevationShadow(cardPos, Vec2(cardWidth, cardHeight),
                                      panelStyle.cornerRadius, Elevation::Z::Card);
    ctx->renderer.DrawRectFilled(cardPos, Vec2(cardWidth, cardHeight), cardBg,
                                 panelStyle.cornerRadius);

    // Header con highlight de hover.
    if (hover) {
      Color hl = panelStyle.headerBackground;
      hl.a = 0.6f;
      ctx->renderer.DrawRectFilled(headerPos, headerSize, hl,
                                   panelStyle.cornerRadius);
    }
    if (ctx->focusedWidgetId == wid)
      DrawFocusRing(ctx, cardPos, Vec2(cardWidth, cardHeight),
                    panelStyle.cornerRadius);

    // Icono opcional + título a la izquierda.
    float textX = cardPos.x + padX;
    if (icon != 0u) {
      DrawWidgetIcon(ctx, headerPos, headerSize, icon, headerStyle.color,
                     headerStyle.fontSize, padX, iconGap);
      textX += headerStyle.fontSize + iconGap;
    }
    Vec2 titlePos(textX, headerPos.y + (headerH - headerStyle.fontSize) * 0.5f);
    ctx->renderer.DrawText(titlePos, header, headerStyle.color,
                           headerStyle.fontSize);

    // Chevron a la derecha: Down (expandido) / Right (colapsado) — "rota" 90°.
    uint32_t chevronCp = isOpen ? Icons::ChevronDown : Icons::ChevronRight;
    float chevX = cardPos.x + cardWidth - padX - chevronSize;
    DrawWidgetIcon(ctx, Vec2(chevX, headerPos.y), Vec2(chevronSize, headerH),
                   chevronCp, headerStyle.color, chevronSize, 0.0f, 0.0f);

    // Línea separadora header/cuerpo cuando hay cuerpo visible.
    if (bodyVisible) {
      Color sep = panelStyle.borderColor;
      sep.a = 0.5f;
      ctx->renderer.DrawLine(
          Vec2(cardPos.x + padX, cardPos.y + headerH),
          Vec2(cardPos.x + cardWidth - padX, cardPos.y + headerH), sep, 1.0f);
    }
    Color border = panelStyle.borderColor;
    border.a = 0.8f;
    ctx->renderer.DrawRect(cardPos, Vec2(cardWidth, cardHeight), border,
                           panelStyle.cornerRadius);
  }

  // Accesibilidad: rol Group, expanded.
  ctx->widgetTree.FindOrCreate(wid, ctx->frame, [&]() {
    auto node = std::make_unique<WidgetNode>(wid);
    node->accessibleRole = WidgetNode::AccessibleRole::Group;
    node->accessibleName = header;
    return node;
  });
  if (auto *node = ctx->widgetTree.FindById(wid))
    node->accessibleExpanded = isOpen;

  // Si no hay cuerpo visible: no abrimos scope (EndExpander no se llamará).
  if (!bodyVisible) {
    ctx->lastItemPos = cardPos;
    AdvanceCursor(ctx, Vec2(cardWidth, cardHeight));
    SetLastItem(wid, cardPos, cardPos + Vec2(cardWidth, cardHeight), hover,
                false, ctx->focusedWidgetId == wid, clicked);
    return false;
  }

  // --- Abrir scope del cuerpo (push layout/clip, como Panel) ---
  ExpanderFrame fr;
  fr.id = wid;
  fr.cardPos = cardPos;
  fr.cardWidth = cardWidth;
  fr.headerH = headerH;
  fr.bodyPad = bodyPad;
  fr.savedCursor = ctx->cursorPos;
  fr.savedLastItemPos = ctx->lastItemPos;
  fr.savedLastItemSize = ctx->lastItemSize;
  fr.animHeight = effectiveBodyH;

  Vec2 bodyOrigin(cardPos.x + padX, cardPos.y + headerH + bodyPad);
  float bodyW = std::max(0.0f, cardWidth - padX * 2.0f);
  // Clip al área visible animada (revelado/ocultado suave).
  ctx->renderer.PushClipRect(Vec2(cardPos.x, cardPos.y + headerH),
                             Vec2(cardWidth, visibleBodyH));
  fr.clipPushed = true;

  ctx->cursorPos = bodyOrigin;
  ctx->lastItemPos = bodyOrigin;
  ctx->lastItemSize = Vec2(0.0f, 0.0f);
  ctx->offsetStack.push_back(bodyOrigin);
  BeginVertical(ctx->style.spacing, Vec2(bodyW, 0.0f), Vec2(0.0f, 0.0f));
  fr.layoutPushed = true;
  g_expanderStack.push_back(fr);
  PushID(id.c_str());

  return true;
}

void EndExpander() {
  UIContext *ctx = GetContext();
  if (!ctx || g_expanderStack.empty())
    return;

  ExpanderFrame fr = g_expanderStack.back();
  g_expanderStack.pop_back();
  PopID();

  // Medir la altura real del contenido (para la animación del frame siguiente).
  float measured = 0.0f;
  if (fr.layoutPushed && !ctx->layoutStack.empty()) {
    auto &stack = ctx->layoutStack.back();
    float cursorH = stack.cursor.y - stack.contentStart.y;
    if (stack.itemCount > 0 && stack.spacing > 0.0f)
      cursorH -= stack.spacing;
    EndVertical(false);
    measured = std::max(cursorH, ctx->lastItemSize.y);
  }
  if (!ctx->offsetStack.empty())
    ctx->offsetStack.pop_back();
  if (fr.clipPushed)
    ctx->renderer.PopClipRect();

  ctx->floatStates[AnimSlot(fr.id, 1)] = measured;

  // Restaurar estado del padre y avanzar el cursor por la card completa.
  ctx->cursorPos = fr.savedCursor;
  ctx->lastItemPos = fr.cardPos;
  float visibleBodyH = fr.animHeight + fr.bodyPad * 2.0f;
  Vec2 cardSize(fr.cardWidth, fr.headerH + visibleBodyH);
  AdvanceCursor(ctx, cardSize);
}

// ════════════════════════════════════════════════════════════════════════════
// 3) SplitButton / DropDownButton
// ════════════════════════════════════════════════════════════════════════════
static std::vector<MenuEntry> CommandsToEntries(const std::vector<CommandItem> &cmds) {
  std::vector<MenuEntry> entries;
  entries.reserve(cmds.size());
  for (const auto &c : cmds) {
    MenuEntry e;
    e.label = c.label;
    e.icon = c.icon;
    e.enabled = c.enabled;
    e.onInvoke = c.onInvoke;
    entries.push_back(std::move(e));
  }
  return entries;
}

int SplitButton(const std::string &label, uint32_t icon,
                std::function<void()> onPrimary,
                const std::vector<CommandItem> &menu) {
  UIContext *ctx = GetContext();
  if (!ctx)
    return 0;

  uint32_t id = GenerateId("SPLITBTN:", label.c_str());
  ctx->focusableWidgets.push_back(id);

  const ButtonStyle &bs = ctx->GetEffectiveButtonStyle();
  float font = bs.text.fontSize;
  Vec2 textSize = MeasureTextCached(ctx, label, font);
  float iconSlot = icon != 0u ? font + S(6.0f) : 0.0f;
  float primaryW = textSize.x + iconSlot + bs.padding.x * 2.0f;
  float chevW = S(28.0f);
  float height = std::max(textSize.y, font) + bs.padding.y * 2.0f;
  Vec2 totalSize(primaryW + chevW, height);

  LayoutConstraints constraints = ConsumeNextConstraints();
  Vec2 finalSize = ApplyConstraints(ctx, constraints, totalSize);
  Vec2 widgetPos = ctx->cursorPos;

  Vec2 primPos = widgetPos;
  Vec2 primSize(primaryW, finalSize.y);
  Vec2 chevPos(widgetPos.x + primaryW, widgetPos.y);
  Vec2 chevSize(chevW, finalSize.y);

  bool primHover = IsMouseOver(ctx, primPos, primSize);
  bool chevHover = IsMouseOver(ctx, chevPos, chevSize);
  bool primClicked = primHover && ctx->input.IsMousePressed(0);
  bool chevClicked = chevHover && ctx->input.IsMousePressed(0);

  bool hasFocus = ctx->focusedWidgetId == id;
  if (hasFocus) {
    if (ctx->input.IsKeyPressed(SDL_SCANCODE_RETURN) ||
        ctx->input.IsKeyPressed(SDL_SCANCODE_SPACE))
      primClicked = true;
    if (ctx->input.IsKeyPressed(SDL_SCANCODE_DOWN))
      chevClicked = true;
  }

  auto stateColor = [&](const ColorState &st, bool hover) -> Color {
    return hover ? (ctx->input.IsMouseDown(0) ? st.pressed : st.hover) : st.normal;
  };

  if (IsRectInViewport(ctx, widgetPos, finalSize)) {
    ctx->renderer.DrawElevationShadow(widgetPos, finalSize, bs.cornerRadius,
                                      (primHover || chevHover)
                                          ? Elevation::Z::ButtonHover
                                          : Elevation::Z::ButtonRest);
    if (hasFocus)
      DrawFocusRing(ctx, widgetPos, finalSize, bs.cornerRadius);
    // Fondo de cada zona.
    ctx->renderer.DrawRectFilled(primPos, primSize,
                                 stateColor(bs.background, primHover),
                                 bs.cornerRadius);
    ctx->renderer.DrawRectFilled(chevPos, chevSize,
                                 stateColor(bs.background, chevHover),
                                 bs.cornerRadius);
    // Divisor entre primaria y chevron.
    Color div = bs.foreground.normal;
    div.a = 0.35f;
    ctx->renderer.DrawLine(Vec2(chevPos.x, widgetPos.y + S(5.0f)),
                           Vec2(chevPos.x, widgetPos.y + finalSize.y - S(5.0f)),
                           div, 1.0f);
    if (bs.borderWidth > 0.0f)
      ctx->renderer.DrawRect(widgetPos, finalSize, bs.border.normal,
                             bs.cornerRadius);

    // Contenido de la zona primaria (icono + label centrados).
    Color fg = bs.foreground.normal;
    float blockW = textSize.x + iconSlot;
    float blockX = primPos.x + (primaryW - blockW) * 0.5f;
    if (icon != 0u) {
      DrawWidgetIcon(ctx, primPos, primSize, icon, fg, font, blockX - primPos.x,
                     S(6.0f));
    }
    Vec2 textPos(blockX + iconSlot, primPos.y + (finalSize.y - textSize.y) * 0.5f);
    if (!label.empty())
      ctx->renderer.DrawText(textPos, label, fg, font);

    // Chevron-down de la zona desplegable, centrado.
    float chevGlyphX = chevPos.x + (chevW - font) * 0.5f;
    DrawWidgetIcon(ctx, chevPos, chevSize, Icons::ChevronDown, fg, font,
                   chevGlyphX - chevPos.x, 0.0f);
  }

  ctx->lastItemPos = widgetPos;
  AdvanceCursor(ctx, finalSize);

  std::string menuId = "SPLITMENU:" + label;
  int result = 0;
  if (primClicked) {
    if (onPrimary)
      onPrimary();
    result = 1;
  }
  if (chevClicked) {
    OpenFlyout(menuId);
    result = 2;
  }

  // El MenuFlyout se evalúa cada frame (se dibuja solo si está abierto).
  std::vector<MenuEntry> entries = CommandsToEntries(menu);
  MenuFlyout(menuId, Rect(widgetPos, finalSize), entries);

  SetLastItem(id, widgetPos, widgetPos + finalSize, primHover || chevHover,
              false, hasFocus, result != 0);
  return result;
}

void DropDownButton(const std::string &label, uint32_t icon,
                    const std::vector<CommandItem> &menu) {
  UIContext *ctx = GetContext();
  if (!ctx)
    return;

  uint32_t id = GenerateId("DROPBTN:", label.c_str());
  ctx->focusableWidgets.push_back(id);

  const ButtonStyle &bs = ctx->GetEffectiveButtonStyle();
  float font = bs.text.fontSize;
  Vec2 textSize = MeasureTextCached(ctx, label, font);
  float iconSlot = icon != 0u ? font + S(6.0f) : 0.0f;
  float chevSlot = font + S(6.0f);
  Vec2 totalSize(textSize.x + iconSlot + chevSlot + bs.padding.x * 2.0f,
                 std::max(textSize.y, font) + bs.padding.y * 2.0f);

  LayoutConstraints constraints = ConsumeNextConstraints();
  Vec2 finalSize = ApplyConstraints(ctx, constraints, totalSize);
  Vec2 widgetPos = ctx->cursorPos;

  bool hover = IsMouseOver(ctx, widgetPos, finalSize);
  bool clicked = hover && ctx->input.IsMousePressed(0);
  bool hasFocus = ctx->focusedWidgetId == id;
  if (hasFocus && (ctx->input.IsKeyPressed(SDL_SCANCODE_RETURN) ||
                   ctx->input.IsKeyPressed(SDL_SCANCODE_SPACE) ||
                   ctx->input.IsKeyPressed(SDL_SCANCODE_DOWN)))
    clicked = true;

  if (IsRectInViewport(ctx, widgetPos, finalSize)) {
    ctx->renderer.DrawElevationShadow(widgetPos, finalSize, bs.cornerRadius,
                                      hover ? Elevation::Z::ButtonHover
                                            : Elevation::Z::ButtonRest);
    if (hasFocus)
      DrawFocusRing(ctx, widgetPos, finalSize, bs.cornerRadius);
    Color bg = hover ? (ctx->input.IsMouseDown(0) ? bs.background.pressed
                                                  : bs.background.hover)
                     : bs.background.normal;
    ctx->renderer.DrawRectFilled(widgetPos, finalSize, bg, bs.cornerRadius);
    if (bs.borderWidth > 0.0f)
      ctx->renderer.DrawRect(widgetPos, finalSize, bs.border.normal,
                             bs.cornerRadius);
    Color fg = bs.foreground.normal;
    float x = widgetPos.x + bs.padding.x;
    if (icon != 0u) {
      DrawWidgetIcon(ctx, widgetPos, finalSize, icon, fg, font, bs.padding.x,
                     S(6.0f));
      x += font + S(6.0f);
    }
    Vec2 textPos(x, widgetPos.y + (finalSize.y - textSize.y) * 0.5f);
    if (!label.empty())
      ctx->renderer.DrawText(textPos, label, fg, font);
    // Chevron al final.
    DrawWidgetIcon(ctx, widgetPos, finalSize, Icons::ChevronDown, fg, font,
                   finalSize.x - bs.padding.x - font, 0.0f);
  }

  ctx->lastItemPos = widgetPos;
  AdvanceCursor(ctx, finalSize);

  std::string menuId = "DROPMENU:" + label;
  if (clicked)
    OpenFlyout(menuId);
  std::vector<MenuEntry> entries = CommandsToEntries(menu);
  MenuFlyout(menuId, Rect(widgetPos, finalSize), entries);

  SetLastItem(id, widgetPos, widgetPos + finalSize, hover, false, hasFocus,
              clicked);
}

// ════════════════════════════════════════════════════════════════════════════
// 8) RatingControl
// ════════════════════════════════════════════════════════════════════════════
bool RatingControl(const std::string &id, int *value, int maxStars,
                   bool allowHalf) {
  UIContext *ctx = GetContext();
  if (!ctx || maxStars < 1)
    return false;

  uint32_t wid = GenerateId("RATING:", id.c_str());
  ctx->focusableWidgets.push_back(wid);

  // value se interpreta en "medias estrellas" cuando allowHalf (0..2*maxStars),
  // y en estrellas enteras (0..maxStars) en caso contrario.
  int maxUnits = allowHalf ? maxStars * 2 : maxStars;
  int current = value ? *value : ctx->intStates[wid];
  current = std::clamp(current, 0, maxUnits);

  float starSize = S(22.0f);
  float gap = S(4.0f);
  Vec2 totalSize(maxStars * starSize + (maxStars - 1) * gap, starSize);

  LayoutConstraints constraints = ConsumeNextConstraints();
  Vec2 finalSize = ApplyConstraints(ctx, constraints, totalSize);
  Vec2 widgetPos = ctx->cursorPos;

  Vec2 mouse(ctx->input.MouseX(), ctx->input.MouseY());
  bool hover = PointInRect(mouse, widgetPos, Vec2(totalSize.x, starSize)) &&
               !IsMouseInputBlocked(ctx);

  // Preview por hover: calcular unidades bajo el cursor.
  int previewUnits = -1;
  if (hover) {
    float rel = mouse.x - widgetPos.x;
    for (int i = 0; i < maxStars; ++i) {
      float sx = i * (starSize + gap);
      if (rel >= sx && rel < sx + starSize) {
        if (allowHalf) {
          bool firstHalf = (rel - sx) < starSize * 0.5f;
          previewUnits = i * 2 + (firstHalf ? 1 : 2);
        } else {
          previewUnits = i + 1;
        }
        break;
      }
    }
    if (rel >= totalSize.x)
      previewUnits = maxUnits;
    ctx->desiredCursor = UIContext::CursorType::Hand;
  }

  bool changed = false;
  if (hover && previewUnits >= 0 && ctx->input.IsMousePressed(0)) {
    // Click sobre la unidad ya seleccionada la limpia (toggle a 0).
    current = (current == previewUnits) ? 0 : previewUnits;
    changed = true;
  }

  // Teclado: flechas ±1 unidad.
  if (ctx->focusedWidgetId == wid) {
    if (ctx->input.IsKeyPressed(SDL_SCANCODE_RIGHT) ||
        ctx->input.IsKeyPressed(SDL_SCANCODE_UP)) {
      current = std::min(current + 1, maxUnits);
      changed = true;
    } else if (ctx->input.IsKeyPressed(SDL_SCANCODE_LEFT) ||
               ctx->input.IsKeyPressed(SDL_SCANCODE_DOWN)) {
      current = std::max(current - 1, 0);
      changed = true;
    }
  }

  if (value)
    *value = current;
  else
    ctx->intStates[wid] = current;

  // Valor a dibujar: preview si hay hover, si no el actual.
  int displayUnits = (hover && previewUnits >= 0) ? previewUnits : current;

  Color goldFull(0.98f, 0.74f, 0.18f, 1.0f);
  Color empty = ctx->style.panel.borderColor;
  empty.a = 0.85f;

  if (IsRectInViewport(ctx, widgetPos, finalSize)) {
    if (ctx->focusedWidgetId == wid)
      DrawFocusRing(ctx, widgetPos, Vec2(totalSize.x, starSize), 4.0f);
    for (int i = 0; i < maxStars; ++i) {
      Vec2 starPos(widgetPos.x + i * (starSize + gap), widgetPos.y);
      Vec2 starBox(starSize, starSize);
      int unitsForStar = allowHalf ? (i + 1) * 2 : (i + 1);
      uint32_t glyph;
      Color color;
      if (allowHalf && displayUnits == i * 2 + 1) {
        glyph = Icons::StarHalf;
        color = goldFull;
      } else if (displayUnits >= unitsForStar) {
        glyph = Icons::Star;
        color = goldFull;
      } else {
        glyph = Icons::Star;
        color = empty;
      }
      DrawWidgetIcon(ctx, starPos, starBox, glyph, color, starSize, 0.0f, 0.0f);
    }
  }

  // Accesibilidad: rol Slider, value = "X of N".
  ctx->widgetTree.FindOrCreate(wid, ctx->frame, [&]() {
    auto node = std::make_unique<WidgetNode>(wid);
    node->accessibleRole = WidgetNode::AccessibleRole::Slider;
    node->accessibleName = id;
    return node;
  });
  if (auto *node = ctx->widgetTree.FindById(wid)) {
    char buf[32];
    if (allowHalf)
      std::snprintf(buf, sizeof(buf), "%.1f of %d", current * 0.5f, maxStars);
    else
      std::snprintf(buf, sizeof(buf), "%d of %d", current, maxStars);
    node->accessibleValue = buf;
  }

  ctx->lastItemPos = widgetPos;
  AdvanceCursor(ctx, finalSize);
  SetLastItem(wid, widgetPos, widgetPos + finalSize, hover, false,
              ctx->focusedWidgetId == wid, changed);
  return changed;
}

} // namespace FluentUI
