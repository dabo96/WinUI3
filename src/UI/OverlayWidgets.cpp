#include "UI/Widgets.h"
#include "UI/WidgetHelpers.h"
#include "Theme/FluentTheme.h"
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

void ProgressBar(float fraction, const Vec2 &size, const std::string &overlay,
                 std::optional<Vec2> pos) {
  UIContext *ctx = GetContext();
  if (!ctx)
    return;

  Vec2 desiredSize = size;
  if (desiredSize.x <= 0.0f)
    desiredSize.x = 200.0f;
  if (desiredSize.y <= 0.0f)
    desiredSize.y = 20.0f;

  fraction = std::clamp(fraction, 0.0f, 1.0f);

  LayoutConstraints constraints = ConsumeNextConstraints(SizeConstraint::Fill);
  Vec2 barSize = ApplyConstraints(ctx, constraints, desiredSize);
  barSize.x = std::max(barSize.x, 1.0f);
  barSize.y = std::max(barSize.y, 4.0f);

  bool hasAbsolutePos = pos.has_value();
  Vec2 barPos;
  if (hasAbsolutePos) {
    barPos = ResolveAbsolutePosition(ctx, pos.value(), barSize);
  } else {
    barPos = ctx->cursorPos;
  }

  const PanelStyle &panelStyle = ctx->style.panel;
  const ColorState &accentState = ctx->style.button.background;
  const TextStyle &captionStyle =
      ctx->style.GetTextStyle(TypographyStyle::Caption);

  if (IsRectInViewport(ctx, barPos, barSize)) {
    ctx->renderer.DrawRectFilled(barPos, barSize, panelStyle.background,
                                 panelStyle.cornerRadius);

    Vec2 fillSize(barSize.x * fraction, barSize.y);
    ctx->renderer.DrawRectFilled(barPos, fillSize, accentState.normal,
                                 panelStyle.cornerRadius);

    if (!overlay.empty()) {
      Vec2 textSize = MeasureTextCached(ctx, overlay, captionStyle.fontSize);
      Vec2 textPos(barPos.x + (barSize.x - textSize.x) * 0.5f,
                   barPos.y + (barSize.y - textSize.y) * 0.5f);
      ctx->renderer.DrawText(textPos, overlay, captionStyle.color,
                             captionStyle.fontSize);
    }
  }

  ctx->lastItemPos = barPos;

  if (!hasAbsolutePos) {
    AdvanceCursor(ctx, barSize);
  } else {
    ctx->lastItemSize = barSize;
  }
}

void Tooltip(const std::string &text, float delay) {
  UIContext *ctx = GetContext();
  if (!ctx || text.empty())
    return;

  // Verificar si el mouse está sobre el último widget dibujado
  bool mouseOverWidget = false;
  if (ctx->lastItemSize.x > 0.0f || ctx->lastItemSize.y > 0.0f) {
    mouseOverWidget = IsMouseOver(ctx, ctx->lastItemPos, ctx->lastItemSize);
  }

  auto &tooltip = ctx->tooltipState;

  if (mouseOverWidget) {
    ctx->anyTooltipHoveredThisFrame = true;
    
    // Si cambiamos de widget, reiniciar el contador
    if (tooltip.lastHoveredWidgetId != ctx->lastGeneratedId) {
        tooltip.hoverTime = 0.0f;
        tooltip.visible = false;
        tooltip.lastHoveredWidgetId = ctx->lastGeneratedId;
    }

    tooltip.hoverTime += ctx->deltaTime;
    
    if (tooltip.hoverTime >= delay) {
      tooltip.visible = true;
      tooltip.text = text;

      float mouseX = ctx->input.MouseX();
      float mouseY = ctx->input.MouseY();

      const TextStyle &textStyle = ctx->style.GetTextStyle(TypographyStyle::Caption);

      // Multi-line: split on \n, compute per-line sizes with max-width constraint
      static constexpr float MAX_TOOLTIP_WIDTH = 300.0f;
      float lineH = textStyle.fontSize * 1.3f;
      float maxLineW = 0.0f;
      int lineCount = 1;
      {
        size_t pos = 0;
        while ((pos = text.find('\n', pos)) != std::string::npos) {
          lineCount++;
          pos++;
        }
      }
      // Measure each line
      {
        size_t start = 0;
        for (int i = 0; i < lineCount; ++i) {
          size_t end = text.find('\n', start);
          if (end == std::string::npos) end = text.size();
          std::string line = text.substr(start, end - start);
          float lw = MeasureTextCached(ctx, line, textStyle.fontSize).x;
          maxLineW = std::max(maxLineW, std::min(lw, MAX_TOOLTIP_WIDTH));
          start = end + 1;
        }
      }

      Vec2 tooltipSize(maxLineW + 16.0f, lineCount * lineH + 10.0f);

      // Posición: un poco debajo y a la derecha del cursor
      Vec2 tooltipPos(mouseX + 16.0f, mouseY + 16.0f);
      Vec2 viewport = ctx->renderer.GetViewportSize();

      if (tooltipPos.x + tooltipSize.x > viewport.x) tooltipPos.x = mouseX - tooltipSize.x - 8.0f;
      if (tooltipPos.y + tooltipSize.y > viewport.y) tooltipPos.y = mouseY - tooltipSize.y - 8.0f;

      tooltip.position = tooltipPos;

      // Fade-in: ramp opacity over 0.15s after becoming visible
      float fadeTime = tooltip.hoverTime - delay;
      float opacity = std::min(1.0f, fadeTime / 0.15f);

      // Encolar para renderizado diferido
      UIContext::DeferredTooltip deferred;
      deferred.text = text;
      deferred.pos = tooltipPos;
      deferred.fontSize = textStyle.fontSize;
      deferred.opacity = opacity;
      ctx->deferredTooltips.push_back(deferred);
    }
  }
}

bool BeginContextMenu(const std::string &id) {
  UIContext *ctx = GetContext();
  if (!ctx)
    return false;

  uint32_t menuId = GenerateId("CTXMENU:", id.c_str());
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
      state.scrollOffset = 0.0f; // Reset scroll al abrir
      ctx->activeContextMenuId = menuId;
    }
  }

  // Si el menú no está abierto, no renderizar nada
  if (!state.open) {
    return false;
  }

  Vec2 menuPos = state.position;

  // Asegurarse de que el menú no se salga de la ventana
  Vec2 viewport = ctx->renderer.GetViewportSize();
  float menuPadding = 8.0f;
  if (menuPos.x + 150.0f > viewport.x) {
    menuPos.x = viewport.x - 150.0f;
  }
  if (menuPos.y + 100.0f > viewport.y) {
    menuPos.y = viewport.y - 100.0f;
  }
  if (menuPos.y < menuPadding) menuPos.y = menuPadding;
  state.position = menuPos;

  // Altura máxima disponible en el viewport
  float maxHeight = viewport.y - menuPos.y - menuPadding;

  // GUARDAR estado del padre para restaurar en EndContextMenu
  state.savedCursorPos = ctx->cursorPos;
  state.savedLastItemPos = ctx->lastItemPos;
  state.savedLastItemSize = ctx->lastItemSize;
  state.savedClipStack = ctx->renderer.GetClipStack();
  state.savedLayoutStackSize = ctx->layoutStack.size();

  // IMPORTANTE: Resetear clipping para que el menú aparezca por encima de todo
  ctx->renderer.FlushBatch();
  while(!ctx->renderer.GetClipStack().empty()) {
      ctx->renderer.PopClipRect();
  }

  // Calcular tamaño visual (clamped a maxHeight)
  float visualHeight = state.contentSize.y > 0.0f
      ? std::min(state.contentSize.y, maxHeight)
      : state.size.y;
  float visualWidth = state.size.x > 0.0f ? state.size.x : 150.0f;

  // Manejar scroll si el contenido excede la altura visual
  bool needsScroll = state.contentSize.y > maxHeight && state.contentSize.y > 0.0f;
  if (needsScroll) {
    float maxScroll = state.contentSize.y - maxHeight;
    // Scroll con rueda del mouse cuando el mouse está sobre el menú
    float mx = ctx->input.MouseX();
    float my = ctx->input.MouseY();
    bool mouseOverMenu = mx >= menuPos.x && mx <= menuPos.x + visualWidth &&
                         my >= menuPos.y && my <= menuPos.y + visualHeight;
    if (mouseOverMenu && !ctx->scrollConsumedThisFrame) {
      float wy = ctx->input.MouseWheelY();
      if (std::abs(wy) > 0.001f) {
        state.scrollOffset = std::clamp(state.scrollOffset - wy * SCROLL_SPEED, 0.0f, maxScroll);
        ctx->scrollConsumedThisFrame = true;
      }
    }
    state.scrollOffset = std::clamp(state.scrollOffset, 0.0f, maxScroll);
  } else {
    state.scrollOffset = 0.0f;
  }

  // Draw background using visual size
  if (visualWidth > 0.0f && visualHeight > 0.0f) {
    ctx->renderer.DrawRectWithElevation(menuPos, Vec2(visualWidth, visualHeight),
                                        ctx->style.panel.background, 4.0f,
                                        Elevation::Z::Flyout);
    Color borderColor = FluentColors::BorderDark;
    borderColor.a = 0.7f;
    ctx->renderer.DrawRect(menuPos, Vec2(visualWidth, visualHeight), borderColor, 4.0f);
  }

  // Clip del contenido del menú a la altura visual
  ctx->renderer.PushClipRect(menuPos, Vec2(visualWidth, visualHeight));

  // Marcar que estamos dentro de un context menu (bloquear input en widgets de fondo)
  ctx->insideContextMenu = true;

  // Iniciar layout — aplicar scroll offset
  ctx->cursorPos = Vec2(menuPos.x, menuPos.y - state.scrollOffset);
  ctx->lastItemPos = ctx->cursorPos;
  ctx->lastItemSize = Vec2(0.0f, 0.0f);
  BeginVertical(0.0f, std::nullopt, Vec2(0.0f, 0.0f));

  return true;
}

bool ContextMenuItem(const std::string &label, uint32_t iconCodepoint, bool enabled);

bool ContextMenuItem(const std::string &label, bool enabled) {
  return ContextMenuItem(label, 0u, enabled);
}

bool ContextMenuItem(const std::string &label, uint32_t iconCodepoint, bool enabled) {
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

  Vec2 textSize = MeasureTextCached(ctx,label, textStyle.fontSize);
  float iconSize = textStyle.fontSize;
  float iconGap = 6.0f;
  float iconSlot = (iconCodepoint != 0u) ? (iconSize + iconGap) : 0.0f;
  float itemWidth = textSize.x + iconSlot + panelStyle.padding.x * 2.0f + 32.0f;

  Vec2 itemSize(itemWidth, itemHeight);
  Vec2 itemPos = ctx->cursorPos;

  float mouseX = ctx->input.MouseX();
  float mouseY = ctx->input.MouseY();
  bool hover = enabled && IsMouseOver(ctx, itemPos, itemSize);
  bool clicked = hover && ctx->input.IsMousePressed(0);

  // Dibujar fondo del item
  Color itemBg = hover ? panelStyle.headerBackground : panelStyle.background;
  if (!enabled) {
    itemBg = Color(itemBg.r * 0.5f, itemBg.g * 0.5f, itemBg.b * 0.5f, itemBg.a);
  }
  ctx->renderer.DrawRectFilled(itemPos, itemSize, itemBg, 0.0f);

  Color textColor =
      enabled ? textStyle.color
              : Color(textStyle.color.r * 0.5f, textStyle.color.g * 0.5f,
                      textStyle.color.b * 0.5f, textStyle.color.a);

  if (iconCodepoint != 0u) {
    DrawWidgetIcon(ctx, itemPos, itemSize, iconCodepoint, textColor,
                   iconSize, panelStyle.padding.x, iconGap);
  }

  // Dibujar texto del item
  Vec2 textPos(itemPos.x + panelStyle.padding.x + iconSlot,
               itemPos.y + (itemHeight - textSize.y) * 0.5f);
  ctx->renderer.DrawText(textPos, label, textColor, textStyle.fontSize);

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
  float separatorWidth = 120.0f; // minimum, will be expanded by EndContextMenu

  Vec2 separatorSize(separatorWidth, separatorHeight + separatorPadding * 2.0f);
  Vec2 separatorPos = ctx->cursorPos;

  // Dibujar línea separadora
  Vec2 lineStart(separatorPos.x + separatorPadding,
                 separatorPos.y + separatorPadding);
  Vec2 lineEnd(separatorPos.x + separatorWidth - separatorPadding,
               separatorPos.y + separatorPadding);
  ctx->renderer.DrawLine(lineStart, lineEnd, panelStyle.borderColor,
                         separatorHeight);

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
    ctx->insideContextMenu = false;
    // Pop the context menu's vertical layout if it was pushed
    if (ctx->layoutStack.size() > it->second.savedLayoutStackSize) {
      EndVertical(false);
    }
    // Restaurar estado del padre
    ctx->cursorPos = it->second.savedCursorPos;
    ctx->lastItemPos = it->second.savedLastItemPos;
    ctx->lastItemSize = it->second.savedLastItemSize;
    // Restaurar clip stack
    ctx->renderer.FlushBatch();
    while(!ctx->renderer.GetClipStack().empty()) {
        ctx->renderer.PopClipRect();
    }
    for (const auto& rect : it->second.savedClipStack) {
        ctx->renderer.PushClipRect(Vec2((float)rect.x, (float)rect.y),
                                   Vec2((float)rect.width, (float)rect.height));
    }
    it->second.savedClipStack.clear();
    return;
  }

  auto &state = it->second;

  // Calcular el tamaño total del contenido (incluyendo scroll offset para obtener el tamaño real)
  float fullContentHeight = ctx->cursorPos.y - (state.position.y - state.scrollOffset);
  if (fullContentHeight < 10.0f) fullContentHeight = 10.0f;

  // Get content width from layout stack before closing it
  float contentWidth = 120.0f; // minimum
  if (!ctx->layoutStack.empty()) {
    contentWidth = std::max(contentWidth, ctx->layoutStack.back().contentSize.x);
    EndVertical(false);
  }

  // Guardar tamaño de contenido real y tamaño visual
  state.contentSize = Vec2(contentWidth, fullContentHeight);
  Vec2 viewport = ctx->renderer.GetViewportSize();
  float menuPadding = 8.0f;
  float maxHeight = viewport.y - state.position.y - menuPadding;
  float visualHeight = std::min(fullContentHeight, maxHeight);
  state.size = Vec2(contentWidth, visualHeight);

  // Pop el clip rect del menú
  ctx->renderer.PopClipRect();

  // Dibujar scrollbar si el contenido excede la altura visual
  if (fullContentHeight > visualHeight && visualHeight > 20.0f) {
    float scrollbarWidth = 4.0f;
    float scrollbarX = state.position.x + contentWidth - scrollbarWidth - 2.0f;
    float scrollRatio = visualHeight / fullContentHeight;
    float thumbHeight = std::max(20.0f, visualHeight * scrollRatio);
    float maxScroll = fullContentHeight - visualHeight;
    float scrollProgress = maxScroll > 0.0f ? state.scrollOffset / maxScroll : 0.0f;
    float thumbY = state.position.y + scrollProgress * (visualHeight - thumbHeight);

    bool darkMenu = ctx->style.isDarkTheme;
    Color scrollbarBg = darkMenu ? Color(1.0f, 1.0f, 1.0f, 0.1f)
                                 : Color(0.0f, 0.0f, 0.0f, 0.08f);
    ctx->renderer.DrawRectFilled(Vec2(scrollbarX, state.position.y),
                                 Vec2(scrollbarWidth, visualHeight), scrollbarBg, 2.0f);
    Color scrollbarThumb = darkMenu ? Color(1.0f, 1.0f, 1.0f, 0.3f)
                                    : Color(0.0f, 0.0f, 0.0f, 0.28f);
    ctx->renderer.DrawRectFilled(Vec2(scrollbarX, thumbY),
                                 Vec2(scrollbarWidth, thumbHeight), scrollbarThumb, 2.0f);
  }

  // Restaurar flag de context menu
  ctx->insideContextMenu = false;

  // RESTAURAR estado del padre: cursor, lastItem, y clipping
  ctx->cursorPos = state.savedCursorPos;
  ctx->lastItemPos = state.savedLastItemPos;
  ctx->lastItemSize = state.savedLastItemSize;

  // Restaurar clip stack original
  ctx->renderer.FlushBatch();
  while(!ctx->renderer.GetClipStack().empty()) {
      ctx->renderer.PopClipRect();
  }
  for (const auto& rect : state.savedClipStack) {
      ctx->renderer.PushClipRect(Vec2((float)rect.x, (float)rect.y),
                                 Vec2((float)rect.width, (float)rect.height));
  }
  state.savedClipStack.clear();
}

bool BeginModal(const std::string &id, const std::string &title, uint32_t iconCodepoint,
                bool *open, const Vec2 &size);

bool BeginModal(const std::string &id, const std::string &title, bool *open,
                const Vec2 &size) {
  return BeginModal(id, title, 0u, open, size);
}

bool BeginModal(const std::string &id, const std::string &title, uint32_t iconCodepoint,
                bool *open, const Vec2 &size) {
  UIContext *ctx = GetContext();
  if (!ctx)
    return false;

  uint32_t modalId = GenerateId("MODAL:", id.c_str());
  auto &state = ctx->modalStates[modalId];

  // Si open es nullptr o está en false, no mostrar el modal
  if (!open || !*open) {
    state.open = false;
    // Liberar el bloqueo global de input si este era el modal activo.
    if (ctx->activeModalId == modalId) {
      ctx->activeModalId = 0;
      ctx->insideModal = false;
    }
    return false;
  }

  const PanelStyle &panelStyle = ctx->style.panel;
  float titleHeight =
      ctx->style.panel.headerText.fontSize + ctx->style.panel.padding.y * 2.0f;
  float verticalPadding = panelStyle.padding.y * 2.0f;

  if (!state.initialized) {
    state.minSize = size;
    state.size = size;
    // Auto-grow from first frame if needed (content not yet measured)
    Vec2 viewport = ctx->renderer.GetViewportSize();
    state.position =
        Vec2((viewport.x - state.size.x) * 0.5f, (viewport.y - state.size.y) * 0.5f);
    state.initialized = true;
  } else {
    // Keep user-requested size as minimum, but grow to fit content
    state.minSize = size;
    float requiredHeight = titleHeight + state.contentSize.y + verticalPadding;
    float newHeight = std::max(size.y, requiredHeight);

    // Re-center vertically if height changed
    if (std::abs(newHeight - state.size.y) > 1.0f && !state.dragging) {
      float deltaH = newHeight - state.size.y;
      state.position.y -= deltaH * 0.5f;
    }
    state.size.y = newHeight;
    state.size.x = size.x; // Width stays as requested
  }

  state.open = *open;

  // Modal activo: captura el input. Bloquea el fondo (vía IsMouseOver) mientras
  // su propio contenido queda exento con insideModal=true.
  ctx->activeModalId = modalId;
  ctx->insideModal = true;

  // Push modal ID to stack so EndModal knows which modal to close
  ctx->modalStack.push_back(modalId);

  // ACTIVAR CAPA DE OVERLAY para que el modal se dibuje encima de todo
  ctx->renderer.SetLayer(RenderLayer::Overlay);

  // GUARDAR CLIPPING: Guardar el stack actual para restaurarlo en EndModal
  state.savedClipStack = ctx->renderer.GetClipStack();

  // IMPORTANTE: FlushBatch y reset de clipping para que el modal esté al frente
  ctx->renderer.FlushBatch();
  while(!ctx->renderer.GetClipStack().empty()) {
      ctx->renderer.PopClipRect();
  }

  Vec2 viewport = ctx->renderer.GetViewportSize();
  Vec2 mousePos(ctx->input.MouseX(), ctx->input.MouseY());
  bool leftPressed = ctx->input.IsMousePressed(0);
  bool leftDown = ctx->input.IsMouseDown(0);

  // Clamp position to viewport
  state.position.x = std::clamp(state.position.x, 0.0f, std::max(0.0f, viewport.x - state.size.x));
  state.position.y = std::clamp(state.position.y, 0.0f, std::max(0.0f, viewport.y - state.size.y));

  // Dibujar overlay oscuro detrás del modal (backdrop)
  Color overlayColor(0.0f, 0.0f, 0.0f, 0.5f);
  ctx->renderer.DrawRectFilled(Vec2(0, 0), viewport, overlayColor, 0.0f);

  // Forzar que el backdrop se dibuje antes que el modal
  ctx->renderer.FlushBatch();

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

  // Dibujar modal con elevation elevada y FONDO TOTALMENTE OPACO
  Color modalBg = panelStyle.background;
  modalBg.a = 1.0f;

  // 1. Dibujar un rectángulo sólido base sin bordes redondeados para asegurar opacidad total
  ctx->renderer.DrawRectFilled(state.position, state.size, modalBg, 0.0f);

  // 2. Dibujar la elevación y el fondo con estilo (con bordes redondeados)
  ctx->renderer.DrawRectWithElevation(state.position, state.size,
                                      modalBg,
                                      panelStyle.cornerRadius, Elevation::Z::Dialog);

  // Dibujar título con el color de cabecera del tema
  Vec2 titleBgPos = state.position;
  Vec2 titleBgSize(state.size.x, titleHeight);

  ctx->renderer.DrawRectFilled(titleBgPos, titleBgSize,
                               panelStyle.headerBackground,
                               panelStyle.cornerRadius);

  // Rectángulo para enderezar las esquinas inferiores del título
  ctx->renderer.DrawRectFilled(Vec2(titleBgPos.x, titleBgPos.y + titleHeight * 0.5f),
                               Vec2(titleBgSize.x, titleHeight * 0.5f),
                               panelStyle.headerBackground, 0.0f);

  // Dibujar un borde para el modal
  ctx->renderer.DrawRect(state.position, state.size, panelStyle.borderColor,
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
  float xSize = std::round(closeButtonSize * 0.25f);
  Vec2 xCenter(std::round(closeButtonPos.x + closeButtonSize * 0.5f),
               std::round(closeButtonPos.y + closeButtonSize * 0.5f));

  float thickness = 1.5f;
  ctx->renderer.DrawLine(Vec2(xCenter.x - xSize, xCenter.y - xSize),
                         Vec2(xCenter.x + xSize, xCenter.y + xSize),
                         Color(1.0f, 1.0f, 1.0f, 1.0f), thickness);
  ctx->renderer.DrawLine(Vec2(xCenter.x - xSize, xCenter.y + xSize),
                         Vec2(xCenter.x + xSize, xCenter.y - xSize),
                         Color(1.0f, 1.0f, 1.0f, 1.0f), thickness);

  // Manejar click en el botón de cerrar
  if (hoverClose && ctx->input.IsMousePressed(0)) {
    *open = false;
    state.open = false;
  }

  // Dibujar texto del título (con icono opcional al inicio)
  float titleIconSize = panelStyle.headerText.fontSize;
  float titleIconGap = 6.0f;
  float titleIconSlot = (iconCodepoint != 0u) ? (titleIconSize + titleIconGap) : 0.0f;
  if (iconCodepoint != 0u) {
    DrawWidgetIcon(ctx, state.position, Vec2(state.size.x, titleHeight),
                   iconCodepoint, panelStyle.headerText.color, titleIconSize,
                   panelStyle.padding.x, titleIconGap);
  }
  Vec2 titleTextPos(state.position.x + panelStyle.padding.x + titleIconSlot,
                    state.position.y +
                        (titleHeight - panelStyle.headerText.fontSize) * 0.5f);
  ctx->renderer.DrawText(titleTextPos, title, panelStyle.headerText.color,
                         panelStyle.headerText.fontSize);

  // Configurar área de contenido
  Vec2 contentPos(state.position.x + panelStyle.padding.x,
                  state.position.y + titleHeight + panelStyle.padding.y);
  Vec2 contentSize(state.size.x - panelStyle.padding.x * 2.0f,
                   state.size.y - titleHeight - verticalPadding);

  // Aplicar clipping al área de contenido
  ctx->renderer.PushClipRect(contentPos, contentSize);

  // Configurar cursor para el contenido del modal
  ctx->cursorPos = contentPos;
  ctx->lastItemPos = contentPos;
  ctx->lastItemSize = Vec2(0.0f, 0.0f);

  // Iniciar layout vertical para el contenido
  BeginVertical(ctx->style.spacing, Vec2(contentSize.x, 0.0f), Vec2(0.0f, 0.0f));

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

  // Get the modal ID from the stack
  uint32_t modalId = 0;
  if (!ctx->modalStack.empty()) {
    modalId = ctx->modalStack.back();
    ctx->modalStack.pop_back();
  }

  // Fin del contenido del modal: el resto de widgets de fondo vuelve a estar
  // bloqueado (activeModalId sigue activo hasta que el modal se cierre).
  ctx->insideModal = false;

  // Measure content size before closing layout
  Vec2 measuredContent{0.0f, 0.0f};
  if (!ctx->layoutStack.empty()) {
    EndVertical(false);
    measuredContent = ctx->lastItemSize;
  }

  // Update content size in modal state for next frame auto-sizing
  if (modalId != 0) {
    auto it = ctx->modalStates.find(modalId);
    if (it != ctx->modalStates.end()) {
      it->second.contentSize = measuredContent;
    }
  }

  // Remover clipping del área de contenido del modal
  ctx->renderer.PopClipRect();

  // REMOVER CAPA DE OVERLAY
  ctx->renderer.SetLayer(RenderLayer::Default);

  // RESTAURAR CLIPPING ORIGINAL using the specific modal's saved stack
  if (modalId != 0) {
    auto it = ctx->modalStates.find(modalId);
    if (it != ctx->modalStates.end() && !it->second.savedClipStack.empty()) {
      ctx->renderer.FlushBatch();
      for (const auto& rect : it->second.savedClipStack) {
        ctx->renderer.PushClipRect(Vec2((float)rect.x, (float)rect.y), Vec2((float)rect.width, (float)rect.height));
      }
      it->second.savedClipStack.clear();
    }
  }
}

} // namespace FluentUI
