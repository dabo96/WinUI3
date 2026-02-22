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

  LayoutConstraints constraints = ConsumeNextConstraints();
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

  RegisterOccupiedArea(ctx, barPos, barSize);
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
      Vec2 textSize = MeasureTextCached(ctx,text, textStyle.fontSize);
      Vec2 tooltipSize(textSize.x + textStyle.fontSize,
                       textSize.y + textStyle.fontSize);

      // Calcular posición (arriba y a la derecha del mouse, asegurándose de no
      // salir de la ventana)
      float mouseX = ctx->input.MouseX();
      float mouseY = ctx->input.MouseY();
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
    Vec2 textSize = MeasureTextCached(ctx,tooltip.text, textStyle.fontSize);
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

  Vec2 textSize = MeasureTextCached(ctx,label, textStyle.fontSize);
  itemWidth = std::max(itemWidth, textSize.x + panelStyle.padding.x * 2.0f);

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

} // namespace FluentUI
