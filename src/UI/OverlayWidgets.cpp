#include "UI/Widgets.h"
#include "UI/WidgetHelpers.h"
#include "Theme/FluentTheme.h"
#include "core/Animation.h"
#include "core/Context.h"
#include "core/Renderer.h"
#include "core/Elevation.h"
#include "core/WidgetNode.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <memory>
#include <optional>
#include <vector>

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

// ============================================================================
// Flyout genérico (brief 14, sección 5) + MenuFlyout (sección 6)
// ============================================================================

// Resuelve la posición del flyout respecto al anchor según placement, con flip
// al lado opuesto si no cabe en el viewport y clamp final a los bordes.
static Vec2 ComputeFlyoutPosition(const Rect &anchor, Vec2 size,
                                  FlyoutPlacement placement, Vec2 viewport) {
  const float gap = 4.0f;
  auto centerX = [&]() { return anchor.pos.x + (anchor.size.x - size.x) * 0.5f; };
  auto centerY = [&]() { return anchor.pos.y + (anchor.size.y - size.y) * 0.5f; };

  Vec2 pos;
  switch (placement) {
  case FlyoutPlacement::Bottom:
    pos = Vec2(centerX(), anchor.Bottom() + gap);
    break;
  case FlyoutPlacement::Top:
    pos = Vec2(centerX(), anchor.Top() - size.y - gap);
    break;
  case FlyoutPlacement::Left:
    pos = Vec2(anchor.Left() - size.x - gap, centerY());
    break;
  case FlyoutPlacement::Right:
    pos = Vec2(anchor.Right() + gap, centerY());
    break;
  case FlyoutPlacement::BottomEdgeAlignedLeft:
    pos = Vec2(anchor.Left(), anchor.Bottom() + gap);
    break;
  case FlyoutPlacement::BottomEdgeAlignedRight:
    pos = Vec2(anchor.Right() - size.x, anchor.Bottom() + gap);
    break;
  case FlyoutPlacement::TopEdgeAlignedLeft:
    pos = Vec2(anchor.Left(), anchor.Top() - size.y - gap);
    break;
  }

  bool bottomish = placement == FlyoutPlacement::Bottom ||
                   placement == FlyoutPlacement::BottomEdgeAlignedLeft ||
                   placement == FlyoutPlacement::BottomEdgeAlignedRight;
  bool topish = placement == FlyoutPlacement::Top ||
                placement == FlyoutPlacement::TopEdgeAlignedLeft;

  // Flip vertical (Bottom <-> Top) si no cabe en el lado preferido.
  if (bottomish && pos.y + size.y > viewport.y &&
      anchor.Top() - size.y - gap >= 0.0f) {
    pos.y = anchor.Top() - size.y - gap;
  } else if (topish && pos.y < 0.0f &&
             anchor.Bottom() + gap + size.y <= viewport.y) {
    pos.y = anchor.Bottom() + gap;
  }
  // Flip horizontal para Left/Right.
  if (placement == FlyoutPlacement::Left && pos.x < 0.0f &&
      anchor.Right() + gap + size.x <= viewport.x) {
    pos.x = anchor.Right() + gap;
  } else if (placement == FlyoutPlacement::Right &&
             pos.x + size.x > viewport.x &&
             anchor.Left() - size.x - gap >= 0.0f) {
    pos.x = anchor.Left() - size.x - gap;
  }

  // Clamp a los bordes del viewport.
  if (size.x < viewport.x)
    pos.x = std::clamp(pos.x, 0.0f, viewport.x - size.x);
  else
    pos.x = 0.0f;
  if (size.y < viewport.y)
    pos.y = std::clamp(pos.y, 0.0f, viewport.y - size.y);
  else
    pos.y = 0.0f;
  return pos;
}

bool BeginFlyout(const std::string &id, const Rect &anchorRect,
                 FlyoutPlacement placement) {
  UIContext *ctx = GetContext();
  if (!ctx)
    return false;

  uint32_t flyoutId = GenerateId("FLYOUT:", id.c_str());
  auto &state = ctx->flyoutStates[flyoutId];

  if (!state.open) {
    if (ctx->activeFlyoutId == flyoutId) {
      ctx->activeFlyoutId = 0;
      ctx->insideFlyout = false;
    }
    return false;
  }

  state.anchor = anchorRect;
  Vec2 viewport = ctx->renderer.GetViewportSize();
  const PanelStyle &panelStyle = ctx->style.panel;

  // ¿Conocemos ya el tamaño del contenido? (medido el frame anterior)
  bool known = state.measuredSize.x >= 1.0f && state.measuredSize.y >= 1.0f;

  // Cierre por click-fuera (usando el rect dibujado el frame anterior) o Esc.
  Vec2 mouse(ctx->input.MouseX(), ctx->input.MouseY());
  if (known) {
    Rect flyRect(state.position, state.measuredSize);
    bool pressedOutside =
        (ctx->input.IsMousePressed(0) || ctx->input.IsMousePressed(2)) &&
        !flyRect.Contains(mouse) && !anchorRect.Contains(mouse);
    if (pressedOutside) {
      state.open = false;
      if (ctx->activeFlyoutId == flyoutId)
        ctx->activeFlyoutId = 0;
      ctx->insideFlyout = false;
      return false;
    }
  }
  if (ctx->input.IsKeyPressed(SDL_SCANCODE_ESCAPE)) {
    state.open = false;
    if (ctx->activeFlyoutId == flyoutId)
      ctx->activeFlyoutId = 0;
    ctx->insideFlyout = false;
    return false;
  }

  // Single-open: este flyout captura el input este frame.
  ctx->activeFlyoutId = flyoutId;
  ctx->insideFlyout = true;

  // Posición: si el tamaño es conocido, posicionamos con flip/clamp. En el primer
  // frame (tamaño desconocido) usamos una estimación y dejamos el contenido en el
  // viewport restante; se reposiciona ajustado el frame siguiente (auto-grow).
  Vec2 cardSize = known ? state.measuredSize : Vec2(0.0f, 0.0f);
  Vec2 estimate = known ? cardSize : Vec2(200.0f, 100.0f);
  Vec2 pos = ComputeFlyoutPosition(anchorRect, estimate, placement, viewport);
  state.position = pos;

  Vec2 layoutSize = cardSize;
  if (!known) {
    layoutSize = Vec2(std::max(1.0f, viewport.x - pos.x),
                      std::max(1.0f, viewport.y - pos.y));
  }

  // Capa de overlay + reset de clipping (igual que Modal) para dibujar al frente.
  ctx->renderer.SetLayer(RenderLayer::Overlay);
  state.savedClipStack = ctx->renderer.GetClipStack();
  ctx->renderer.FlushBatch();
  while (!ctx->renderer.GetClipStack().empty())
    ctx->renderer.PopClipRect();

  // Fondo: card con sombra (z=Flyout). Solo cuando el tamaño es conocido para
  // evitar un flash a pantalla completa el primer frame.
  if (known) {
    Color cardBg = panelStyle.background;
    cardBg.a = 1.0f;
    ctx->renderer.DrawRectFilled(pos, cardSize, cardBg, panelStyle.cornerRadius);
    ctx->renderer.DrawRectWithElevation(pos, cardSize, cardBg,
                                        panelStyle.cornerRadius,
                                        Elevation::Z::Flyout);
    Color border = FluentColors::BorderDark;
    border.a = 0.7f;
    ctx->renderer.DrawRect(pos, cardSize, border, panelStyle.cornerRadius);
  }

  // Guardar estado del padre para EndFlyout.
  state.savedCursorPos = ctx->cursorPos;
  state.savedLastItemPos = ctx->lastItemPos;
  state.savedLastItemSize = ctx->lastItemSize;
  state.savedLayoutStackSize = ctx->layoutStack.size();

  // Clip al área de la card + layout vertical del contenido (con padding).
  ctx->renderer.PushClipRect(pos, layoutSize);
  Vec2 contentPos(pos.x + panelStyle.padding.x, pos.y + panelStyle.padding.y);
  ctx->cursorPos = contentPos;
  ctx->lastItemPos = contentPos;
  ctx->lastItemSize = Vec2(0.0f, 0.0f);
  BeginVertical(ctx->style.spacing, std::nullopt, Vec2(0.0f, 0.0f));

  return true;
}

void EndFlyout() {
  UIContext *ctx = GetContext();
  if (!ctx)
    return;
  if (ctx->activeFlyoutId == 0)
    return;
  auto it = ctx->flyoutStates.find(ctx->activeFlyoutId);
  if (it == ctx->flyoutStates.end())
    return;
  auto &state = it->second;

  // Medir el contenido (para auto-grow / posición del frame siguiente).
  Vec2 measuredContent{0.0f, 0.0f};
  if (ctx->layoutStack.size() > state.savedLayoutStackSize) {
    EndVertical(false);
    measuredContent = ctx->lastItemSize;
  }
  const PanelStyle &panelStyle = ctx->style.panel;
  Vec2 fullSize(measuredContent.x + panelStyle.padding.x * 2.0f,
                measuredContent.y + panelStyle.padding.y * 2.0f);
  if (fullSize.x > 1.0f && fullSize.y > 1.0f)
    state.measuredSize = fullSize;

  // Quitar clip del contenido + salir de la capa de overlay.
  ctx->renderer.PopClipRect();
  ctx->renderer.SetLayer(RenderLayer::Default);

  // Restaurar estado del padre.
  ctx->cursorPos = state.savedCursorPos;
  ctx->lastItemPos = state.savedLastItemPos;
  ctx->lastItemSize = state.savedLastItemSize;

  // Restaurar clip stack original.
  ctx->renderer.FlushBatch();
  while (!ctx->renderer.GetClipStack().empty())
    ctx->renderer.PopClipRect();
  for (const auto &rect : state.savedClipStack) {
    ctx->renderer.PushClipRect(Vec2((float)rect.x, (float)rect.y),
                               Vec2((float)rect.width, (float)rect.height));
  }
  state.savedClipStack.clear();

  // insideFlyout=false pero activeFlyoutId sigue activo: bloquea el fondo bajo el
  // rect del flyout el resto del frame (se re-fija cada BeginFlyout y se limpia
  // al cerrar). TODO: submenús (varios flyouts apilados) en una revisión futura.
  ctx->insideFlyout = false;
}

void OpenFlyout(const std::string &id) {
  UIContext *ctx = GetContext();
  if (!ctx)
    return;
  uint32_t flyoutId = GenerateId("FLYOUT:", id.c_str());
  // Single-open: cerrar cualquier otro flyout abierto.
  for (auto &kv : ctx->flyoutStates) {
    if (kv.first != flyoutId)
      kv.second.open = false;
  }
  auto &state = ctx->flyoutStates[flyoutId];
  if (!state.open) {
    state.open = true;
    state.measuredSize = Vec2(0.0f, 0.0f); // forzar re-medir/reposicionar
  }
  ctx->activeFlyoutId = flyoutId;
}

void CloseFlyout(const std::string &id) {
  UIContext *ctx = GetContext();
  if (!ctx)
    return;
  uint32_t flyoutId = GenerateId("FLYOUT:", id.c_str());
  auto it = ctx->flyoutStates.find(flyoutId);
  if (it != ctx->flyoutStates.end())
    it->second.open = false;
  if (ctx->activeFlyoutId == flyoutId)
    ctx->activeFlyoutId = 0;
}

bool IsFlyoutOpen(const std::string &id) {
  UIContext *ctx = GetContext();
  if (!ctx)
    return false;
  uint32_t flyoutId = GenerateId("FLYOUT:", id.c_str());
  auto it = ctx->flyoutStates.find(flyoutId);
  return it != ctx->flyoutStates.end() && it->second.open;
}

void MenuFlyout(const std::string &id, const Rect &anchorRect,
                const std::vector<MenuEntry> &entries) {
  UIContext *ctx = GetContext();
  if (!ctx)
    return;
  if (!BeginFlyout(id, anchorRect, FlyoutPlacement::BottomEdgeAlignedLeft))
    return;

  const PanelStyle &panelStyle = ctx->style.panel;
  const TextStyle &textStyle = ctx->style.GetTextStyle(TypographyStyle::Body);
  float fontSize = textStyle.fontSize;
  float padX = panelStyle.padding.x;
  float rowH = fontSize + panelStyle.padding.y * 2.0f;
  float iconSize = fontSize;
  float iconGap = 8.0f;
  float accelGap = 24.0f;

  bool anyIcon = false, anyCheck = false;
  for (const auto &e : entries) {
    if (e.icon)
      anyIcon = true;
    if (e.checkable)
      anyCheck = true;
  }
  float leftSlot = (anyIcon || anyCheck) ? (iconSize + iconGap) : 0.0f;

  // Ancho uniforme de fila: icono + label + (acelerador) + (chevron submenú).
  float maxRow = 120.0f;
  for (const auto &e : entries) {
    if (e.separator)
      continue;
    float w = padX + leftSlot + MeasureTextCached(ctx, e.label, fontSize).x + padX;
    if (!e.accelerator.empty())
      w += accelGap + MeasureTextCached(ctx, e.accelerator, fontSize).x;
    if (!e.submenu.empty())
      w += iconGap + iconSize;
    maxRow = std::max(maxRow, w);
  }

  // Highlight de teclado persistente (-1 = ninguno).
  uint32_t hlKey = GenerateId("FLYOUTMENU_HL:", id.c_str());
  int hl = -1;
  {
    auto hlIt = ctx->intStates.find(hlKey);
    if (hlIt != ctx->intStates.end())
      hl = hlIt->second;
  }

  std::vector<int> selectable;
  for (int i = 0; i < (int)entries.size(); ++i)
    if (!entries[i].separator && entries[i].enabled)
      selectable.push_back(i);

  bool downP = ctx->input.IsKeyPressed(SDL_SCANCODE_DOWN);
  bool upP = ctx->input.IsKeyPressed(SDL_SCANCODE_UP);
  bool enterP = ctx->input.IsKeyPressed(SDL_SCANCODE_RETURN) ||
                ctx->input.IsKeyPressed(SDL_SCANCODE_KP_ENTER);
  if (!selectable.empty() && (downP || upP)) {
    int curPos = -1;
    for (int k = 0; k < (int)selectable.size(); ++k)
      if (selectable[k] == hl) {
        curPos = k;
        break;
      }
    if (downP)
      curPos = (curPos + 1) % (int)selectable.size();
    else
      curPos = (curPos <= 0) ? (int)selectable.size() - 1 : curPos - 1;
    hl = selectable[curPos];
  }

  int invokeIndex = -1;
  for (int i = 0; i < (int)entries.size(); ++i) {
    const MenuEntry &e = entries[i];
    Vec2 rowPos = ctx->cursorPos;

    if (e.separator) {
      Vec2 sepSize(maxRow, 1.0f + 8.0f);
      ctx->renderer.DrawLine(Vec2(rowPos.x + padX, rowPos.y + 4.0f),
                             Vec2(rowPos.x + maxRow - padX, rowPos.y + 4.0f),
                             panelStyle.borderColor, 1.0f);
      AdvanceCursor(ctx, sepSize);
      continue;
    }

    Vec2 rowSize(maxRow, rowH);
    bool hover = e.enabled && IsMouseOver(ctx, rowPos, rowSize);
    if (hover)
      hl = i; // el ratón fija el highlight
    bool highlighted = e.enabled && hl == i;
    bool clicked = hover && ctx->input.IsMousePressed(0);

    if (highlighted) {
      ctx->renderer.DrawRectFilled(rowPos, rowSize, panelStyle.headerBackground,
                                   4.0f);
    }

    Color textColor =
        e.enabled ? textStyle.color
                  : Color(textStyle.color.r * 0.5f, textStyle.color.g * 0.5f,
                          textStyle.color.b * 0.5f, textStyle.color.a);

    // Columna izquierda: check (si checkable+checked) o icono.
    if (e.checkable && e.checked) {
      float cx = rowPos.x + padX + iconSize * 0.5f;
      float cy = rowPos.y + rowH * 0.5f;
      float s = iconSize * 0.32f;
      ctx->renderer.DrawLine(Vec2(cx - s, cy), Vec2(cx - s * 0.2f, cy + s),
                             textColor, 1.5f);
      ctx->renderer.DrawLine(Vec2(cx - s * 0.2f, cy + s), Vec2(cx + s, cy - s),
                             textColor, 1.5f);
    } else if (e.icon) {
      DrawWidgetIcon(ctx, rowPos, rowSize, e.icon, textColor, iconSize, padX,
                     iconGap);
    }

    // Label.
    Vec2 textPos(rowPos.x + padX + leftSlot,
                 rowPos.y + (rowH - fontSize) * 0.5f);
    ctx->renderer.DrawText(textPos, e.label, textColor, fontSize);

    float subSlot = e.submenu.empty() ? 0.0f : (iconGap + iconSize);

    // Acelerador alineado a la derecha.
    if (!e.accelerator.empty()) {
      Vec2 accSize = MeasureTextCached(ctx, e.accelerator, fontSize);
      Vec2 accPos(rowPos.x + maxRow - padX - subSlot - accSize.x,
                  rowPos.y + (rowH - fontSize) * 0.5f);
      Color accColor = textColor;
      accColor.a *= 0.6f;
      ctx->renderer.DrawText(accPos, e.accelerator, accColor, fontSize);
    }

    // Chevron de submenú (v1: indicador visual; expansión = TODO).
    if (!e.submenu.empty()) {
      float ax = rowPos.x + maxRow - padX - iconSize * 0.6f;
      float ay = rowPos.y + rowH * 0.5f;
      float s = iconSize * 0.25f;
      ctx->renderer.DrawLine(Vec2(ax - s, ay - s), Vec2(ax + s, ay), textColor,
                             1.5f);
      ctx->renderer.DrawLine(Vec2(ax + s, ay), Vec2(ax - s, ay + s), textColor,
                             1.5f);
    }

    AdvanceCursor(ctx, rowSize);

    if (clicked || (enterP && hl == i && e.enabled))
      invokeIndex = i;
  }

  ctx->intStates[hlKey] = hl;

  // Invocar fuera del bucle de dibujo.
  if (invokeIndex >= 0) {
    const MenuEntry &e = entries[invokeIndex];
    if (e.submenu.empty()) {
      if (e.onInvoke)
        e.onInvoke();
      CloseFlyout(id); // hoja → cerrar el menú
    }
    // TODO: submenús — abrir flyout anidado al invocar/hover una entrada con submenu.
  }

  EndFlyout();
}

// ════════════════════════════════════════════════════════════════════════════
// TeachingTip / coachmark (brief 14, section 7)
// ════════════════════════════════════════════════════════════════════════════
bool TeachingTip(const std::string &id, const Rect &targetRect,
                 const std::string &title, const std::string &body,
                 const std::string &actionText) {
  UIContext *ctx = GetContext();
  if (!ctx)
    return false;

  uint32_t seenKey = GenerateId("TEACHTIP_SEEN:", id.c_str());
  bool &seen = ctx->boolStates[seenKey];
  if (seen)
    return false;

  std::string flyoutId = "TEACHTIP:" + id;
  // Abrir una sola vez. Si el flyout se cerró (click fuera/Esc) y ya se había
  // abierto, marcar "ya visto" y no volver a mostrar.
  uint32_t openedKey = GenerateId("TEACHTIP_OPENED:", id.c_str());
  bool &openedOnce = ctx->boolStates[openedKey];
  if (!IsFlyoutOpen(flyoutId)) {
    if (!openedOnce) {
      OpenFlyout(flyoutId);
      openedOnce = true;
    } else {
      seen = true;
      return false;
    }
  }

  const PanelStyle &panelStyle = ctx->style.panel;
  bool actioned = false;

  // Colocar el flyout debajo del target (con flip si no cabe).
  if (BeginFlyout(flyoutId, targetRect, FlyoutPlacement::Bottom)) {
    Label(title, std::nullopt, TypographyStyle::BodyStrong);
    Spacing(2.0f);
    LabelWrapped(body, 240.0f);
    Spacing(6.0f);
    BeginHorizontal(8.0f);
    if (!actionText.empty() && Button(actionText)) {
      actioned = true;
      seen = true;
      CloseFlyout(flyoutId);
    }
    if (Button("Got it")) {
      seen = true;
      CloseFlyout(flyoutId);
    }
    EndHorizontal();
    EndFlyout();
  }

  // Dibujar el "beak" (flecha) apuntando al target, sobre la capa de overlay y
  // fuera del clip de la card (queda en el hueco entre el target y el flyout).
  uint32_t fid = GenerateId("FLYOUT:", flyoutId.c_str());
  auto it = ctx->flyoutStates.find(fid);
  if (it != ctx->flyoutStates.end() && it->second.open &&
      it->second.measuredSize.y > 1.0f) {
    Vec2 fpos = it->second.position;
    Vec2 fsize = it->second.measuredSize;
    Color beakColor = panelStyle.background;
    beakColor.a = 1.0f;
    float tcx = targetRect.pos.x + targetRect.size.x * 0.5f;
    float cx = std::clamp(tcx, fpos.x + 10.0f, fpos.x + fsize.x - 10.0f);
    const float k = 7.0f;
    ctx->renderer.SetLayer(RenderLayer::Overlay);
    ctx->renderer.FlushBatch();
    if (fpos.y >= targetRect.Bottom()) {
      // Flyout debajo del target → pico hacia arriba.
      float by = fpos.y;
      ctx->renderer.DrawTriangleFilled(Vec2(cx - k, by), Vec2(cx + k, by),
                                       Vec2(cx, by - k), beakColor);
    } else {
      // Flyout encima → pico hacia abajo.
      float by = fpos.y + fsize.y;
      ctx->renderer.DrawTriangleFilled(Vec2(cx - k, by), Vec2(cx + k, by),
                                       Vec2(cx, by + k), beakColor);
    }
    ctx->renderer.SetLayer(RenderLayer::Default);
  }

  return actioned;
}

// ════════════════════════════════════════════════════════════════════════════
// ContentDialog (brief 14, section 9)
// ════════════════════════════════════════════════════════════════════════════
DialogResult ContentDialog(const std::string &id, bool *open,
                           const std::string &title,
                           std::function<void()> body,
                           const std::string &primaryText,
                           const std::string &secondaryText,
                           const std::string &closeText) {
  UIContext *ctx = GetContext();
  if (!ctx || !open || !*open)
    return DialogResult::None;

  bool escPressed = ctx->input.IsKeyPressed(SDL_SCANCODE_ESCAPE);
  bool enterPressed = ctx->input.IsKeyPressed(SDL_SCANCODE_RETURN) ||
                      ctx->input.IsKeyPressed(SDL_SCANCODE_KP_ENTER);

  // Foco inicial en el botón primario la primera vez que se abre. No mantenemos
  // una referencia al mapa: body() es arbitrario y podría insertar en boolStates
  // (rehash) e invalidarla.
  uint32_t focusInitKey = GenerateId("CDLG_FOCUS:", id.c_str());
  bool focusInit = false;
  {
    auto fit = ctx->boolStates.find(focusInitKey);
    if (fit != ctx->boolStates.end())
      focusInit = fit->second;
  }
  if (!focusInit && !primaryText.empty()) {
    ctx->focusedWidgetId = GenerateId("BTN:", primaryText.c_str());
    ctx->boolStates[focusInitKey] = true;
  }

  // Trap de foco: recordar dónde empiezan los widgets enfocables del diálogo.
  size_t focusStart = ctx->focusableWidgets.size();

  bool wantPrimary = false, wantSecondary = false, wantClose = false;
  Vec2 dlgSize(S(420.0f), S(180.0f));

  if (BeginModal(id, title, open, dlgSize)) {
    if (body)
      body();
    Spacing(S(10.0f));
    BeginHorizontal(S(8.0f));
    if (!primaryText.empty()) {
      // Botón primario acentuado (color de acento, texto blanco).
      ButtonStyle accentStyle = ctx->GetEffectiveButtonStyle();
      Color ac = ctx->style.accentColor;
      accentStyle.background.normal = ac;
      accentStyle.background.hover =
          Color(std::min(1.0f, ac.r * 1.12f), std::min(1.0f, ac.g * 1.12f),
                std::min(1.0f, ac.b * 1.12f), ac.a);
      accentStyle.background.pressed =
          Color(ac.r * 0.88f, ac.g * 0.88f, ac.b * 0.88f, ac.a);
      Color white(1.0f, 1.0f, 1.0f, 1.0f);
      accentStyle.foreground.normal = white;
      accentStyle.foreground.hover = white;
      accentStyle.foreground.pressed = white;
      PushButtonStyle(accentStyle);
      if (Button(primaryText))
        wantPrimary = true;
      PopButtonStyle();
    }
    if (!secondaryText.empty() && Button(secondaryText))
      wantSecondary = true;
    if (!closeText.empty() && Button(closeText))
      wantClose = true;
    EndHorizontal();
    EndModal();
  }

  // Trap de foco: dejar en focusableWidgets sólo los controles del diálogo, de
  // modo que Tab (procesado al inicio del frame siguiente) cicle dentro de él.
  if (focusStart > 0 && focusStart <= ctx->focusableWidgets.size()) {
    ctx->focusableWidgets.erase(ctx->focusableWidgets.begin(),
                                ctx->focusableWidgets.begin() +
                                    static_cast<long>(focusStart));
    if (ctx->focusIndex >= static_cast<int>(focusStart))
      ctx->focusIndex -= static_cast<int>(focusStart);
    else
      ctx->focusIndex = 0;
  }

  // Resolver el resultado. Enter = Primary, Esc = Close (el Modal ya cerró por
  // Esc/X poniendo *open=false, que también tratamos como Close).
  DialogResult result = DialogResult::None;
  if (wantPrimary || (enterPressed && !primaryText.empty()))
    result = DialogResult::Primary;
  else if (wantSecondary)
    result = DialogResult::Secondary;
  else if (wantClose || escPressed || !*open)
    result = DialogResult::Close;

  if (result != DialogResult::None) {
    *open = false;
    ctx->boolStates[focusInitKey] = false; // re-armar el foco para la próxima
  }

  // Accesibilidad: rol Dialog.
  uint32_t dlgId = GenerateId("CONTENTDIALOG:", id.c_str());
  ctx->widgetTree.FindOrCreate(dlgId, ctx->frame, [&]() {
    auto node = std::make_unique<WidgetNode>(dlgId);
    node->accessibleRole = WidgetNode::AccessibleRole::Dialog;
    node->accessibleName = title;
    return node;
  });

  return result;
}

} // namespace FluentUI
