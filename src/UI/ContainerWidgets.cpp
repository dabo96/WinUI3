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

bool BeginPanel(const std::string &id, const Vec2 &desiredSize,
                bool reserveLayoutSpace, std::optional<bool> useAcrylic,
                std::optional<float> acrylicOpacity, std::optional<Vec2> pos) {
  UIContext *ctx = GetContext();
  if (!ctx)
    return false;

  std::string key = "PANEL:" + id;
  uint32_t panelId = GenerateId(key.c_str());
  auto &state = ctx->panelStates[panelId];
  if (!state.initialized) {
    state.size = desiredSize;
    if (state.size.x <= 0.0f)
      state.size.x = 300.0f;
    if (state.size.y <= 0.0f)
      state.size.y = 200.0f;

    // Resolver posición: usar pos si se proporciona, sino usar cursor
    // directamente NO usar ResolveAbsolutePosition aquí porque puede aplicar
    // offsets incorrectos cuando hay un layout activo. Usar ctx->cursorPos
    // directamente como los widgets básicos.
    if (pos.has_value()) {
      state.position = ResolveAbsolutePosition(ctx, pos.value(), state.size);
      state.useAbsolutePos = true;
    } else {
      state.position = ctx->cursorPos;
      state.useAbsolutePos = false;
    }

    // Configurar acrylic: usar parámetros si se proporcionan, sino usar el
    // estilo global
    const PanelStyle &panelStyle = ctx->style.panel;
    state.useAcrylic =
        useAcrylic.has_value() ? useAcrylic.value() : panelStyle.useAcrylic;
    state.acrylicOpacity = acrylicOpacity.has_value()
                               ? acrylicOpacity.value()
                               : panelStyle.acrylicOpacity;

    state.initialized = true;
    state.reservedLayoutSize = state.size;
    state.expandedLayoutSize = state.size;
  } else {
    // Permitir cambiar configuración de acrylic en tiempo de ejecución
    if (useAcrylic.has_value()) {
      state.useAcrylic = useAcrylic.value();
    }
    if (acrylicOpacity.has_value()) {
      state.acrylicOpacity = acrylicOpacity.value();
    }

    // IMPORTANTE: Actualizar posición del panel en cada frame si NO usa posición absoluta
    // Esto permite que el panel se mueva con el scroll de contenedores padre (TabView, ScrollView)
    // La posición es: cursor del layout + offset del drag del usuario
    if (!state.useAbsolutePos && !state.dragging && !state.resizing) {
      state.position = ctx->cursorPos + state.dragPositionOffset;
    }
  }

  const PanelStyle &panelStyle = ctx->style.panel;
  float titleHeight =
      panelStyle.headerText.fontSize + panelStyle.padding.y * 2.0f;

  if (reserveLayoutSpace) {
    if (desiredSize.x > 0.0f) {
      state.reservedLayoutSize.x = desiredSize.x;
      state.expandedLayoutSize.x = desiredSize.x;
    }
    if (desiredSize.y > 0.0f) {
      state.reservedLayoutSize.y =
          state.minimized ? titleHeight : desiredSize.y;
      state.expandedLayoutSize.y = desiredSize.y;
    }
  }

  Vec2 mousePos(ctx->input.MouseX(), ctx->input.MouseY());
  bool leftDown = ctx->input.IsMouseDown(0);
  bool leftPressed = ctx->input.IsMousePressed(0);

  Vec2 titleSize(state.size.x, titleHeight);

  Vec2 minimizeButtonSize(titleHeight - panelStyle.padding.y * 1.5f,
                          titleHeight - panelStyle.padding.y * 1.5f);
  Vec2 minimizeButtonPos(state.position.x + state.size.x -
                             minimizeButtonSize.x - panelStyle.padding.x,
                         state.position.y +
                             (titleHeight - minimizeButtonSize.y) * 0.5f);

  if (leftPressed &&
      PointInRect(mousePos, minimizeButtonPos, minimizeButtonSize)) {
    state.minimized = !state.minimized;
    state.dragging = false;
    state.resizing = false;
  }

  // Manejar drag
  if (state.dragging) {
    if (!leftDown) {
      state.dragging = false;
      // Guardar el offset de la posición arrastrada relativo al cursor del layout
      if (!state.useAbsolutePos) {
        state.dragPositionOffset = state.position - ctx->cursorPos;
        state.hasBeenDragged = true;
      }
    } else {
      state.position = Vec2(mousePos.x - state.dragOffset.x,
                            mousePos.y - state.dragOffset.y);
    }
  }

  // Manejar resize
  if (state.resizing) {
    if (!leftDown) {
      state.resizing = false;
    } else {
      Vec2 delta(mousePos.x - state.resizeStartMouse.x,
                 mousePos.y - state.resizeStartMouse.y);
      Vec2 newSize = Vec2(state.resizeStartSize.x + delta.x,
                          state.resizeStartSize.y + delta.y);
      float minWidth = 120.0f;
      float minHeight = titleHeight + 40.0f;
      state.size.x = std::max(newSize.x, minWidth);
      state.size.y = std::max(newSize.y, minHeight);
    }
  }

  // Iniciar drag o resize
  if (!state.dragging && !state.resizing) {
    if (leftPressed && PointInRect(mousePos, state.position, titleSize)) {
      state.dragging = true;
      state.dragOffset =
          Vec2(mousePos.x - state.position.x, mousePos.y - state.position.y);
    } else if (!state.minimized) {
      Vec2 resizeHandleSize(12.0f, 12.0f);
      Vec2 resizeHandlePos(state.position.x + state.size.x - resizeHandleSize.x,
                           state.position.y + state.size.y -
                               resizeHandleSize.y);
      if (leftPressed &&
          PointInRect(mousePos, resizeHandlePos, resizeHandleSize)) {
        state.resizing = true;
        state.resizeStartMouse = mousePos;
        state.resizeStartSize = state.size;
      }
    }
  }

  Vec2 viewport = ctx->renderer.GetViewportSize();
  float maxPosX = std::max(0.0f, viewport.x - state.size.x);
  float maxPosY = std::max(
      0.0f, viewport.y - (state.minimized ? titleHeight : state.size.y));
  state.position.x = std::clamp(state.position.x, 0.0f, maxPosX);
  state.position.y = std::clamp(state.position.y, 0.0f, maxPosY);
  state.size.x = std::clamp(state.size.x, 100.0f, viewport.x);
  state.size.y = std::clamp(state.size.y, titleHeight, viewport.y);

  // Registrar área ocupada del panel ANTES de dibujarlo para que otros widgets
  // lo vean
  Vec2 panelSize =
      state.minimized ? Vec2(state.size.x, titleHeight) : state.size;
  RegisterOccupiedArea(ctx, state.position, panelSize);

  Color panelColor = panelStyle.background;
  Color titleColor = panelStyle.headerBackground;

  if (!state.minimized) {
    if (panelStyle.shadowOpacity > 0.0f) {
      ctx->renderer.DrawRectFilled(
          state.position + Vec2(0.0f, panelStyle.shadowOffsetY), state.size,
          Color(0.0f, 0.0f, 0.0f, panelStyle.shadowOpacity),
          panelStyle.cornerRadius);
    }

    // Usar efecto acrylic según la configuración específica del panel
    if (state.useAcrylic) {
      ctx->renderer.DrawRectAcrylic(state.position, state.size, panelColor,
                                    panelStyle.cornerRadius,
                                    state.acrylicOpacity);
    } else {
      ctx->renderer.DrawRectFilled(state.position, state.size, panelColor,
                                   panelStyle.cornerRadius);
    }
  } else {
    if (state.useAcrylic) {
      ctx->renderer.DrawRectAcrylic(
          state.position, Vec2(state.size.x, titleHeight), panelColor,
          panelStyle.cornerRadius, state.acrylicOpacity);
    } else {
      ctx->renderer.DrawRectFilled(state.position,
                                   Vec2(state.size.x, titleHeight), panelColor,
                                   panelStyle.cornerRadius);
    }
  }

  ctx->renderer.DrawRectFilled(state.position, titleSize, titleColor,
                               panelStyle.cornerRadius);
  // Sin borde visible - solo contraste de fondo como en Windows 11 Settings

  Vec2 titleTextPos(state.position.x + panelStyle.padding.x,
                    state.position.y +
                        (titleHeight - panelStyle.headerText.fontSize) * 0.5f);
  ctx->renderer.DrawText(titleTextPos, id, panelStyle.headerText.color,
                         panelStyle.headerText.fontSize);

  bool minimizeHover =
      PointInRect(mousePos, minimizeButtonPos, minimizeButtonSize);
  bool buttonPressed = minimizeHover && ctx->input.IsMouseDown(0);
  bool panelMinimized = state.minimized;
  auto resolveButtonColor = [&](const ColorState &stateColors) -> Color {
    if (panelMinimized) {
      return stateColors.disabled;
    }
    if (buttonPressed)
      return stateColors.pressed;
    if (minimizeHover)
      return stateColors.hover;
    return stateColors.normal;
  };
  ctx->renderer.DrawRectFilled(minimizeButtonPos, minimizeButtonSize,
                               resolveButtonColor(panelStyle.titleButton),
                               panelStyle.cornerRadius * 0.4f);
  std::string symbol = state.minimized ? "+" : "-";
  Vec2 symbolSize =
      MeasureTextCached(ctx,symbol, panelStyle.headerText.fontSize * 0.8f);
  Vec2 symbolPos(
      minimizeButtonPos.x + (minimizeButtonSize.x - symbolSize.x) * 0.5f,
      minimizeButtonPos.y + (minimizeButtonSize.y - symbolSize.y) * 0.5f);
  ctx->renderer.DrawText(symbolPos, symbol, Color(1.0f, 1.0f, 1.0f, 1.0f),
                         panelStyle.headerText.fontSize * 0.8f);

  if (!state.minimized) {
    Vec2 resizeHandleSize(12.0f, 12.0f);
    Vec2 resizeHandlePos(state.position.x + state.size.x - resizeHandleSize.x,
                         state.position.y + state.size.y - resizeHandleSize.y);
    ctx->renderer.DrawRectFilled(resizeHandlePos, resizeHandleSize,
                                 panelStyle.titleButton.hover,
                                 panelStyle.cornerRadius * 0.3f);
  }

  Vec2 layoutOrigin = ctx->cursorPos;
  PanelFrameContext frameCtx{};
  frameCtx.id = panelId;
  frameCtx.layoutOrigin = layoutOrigin;
  frameCtx.titleHeight = titleHeight;
  frameCtx.clipPushed = false;
  frameCtx.layoutPushed = false;
  // Si se usa posición absoluta, no reservar espacio en el layout
  if (state.useAbsolutePos) {
    frameCtx.reserveLayout = false;
  } else {
    frameCtx.reserveLayout = reserveLayoutSpace;
  }
  if (!state.minimized) {
    state.expandedLayoutSize = state.size;
  }
  if (!state.minimized) {
    state.expandedLayoutSize = state.size;
    state.reservedLayoutSize = state.size;
  }
  Vec2 reservedSize = state.reservedLayoutSize;
  if (state.minimized) {
    reservedSize = Vec2(state.expandedLayoutSize.x, titleHeight);
  }
  frameCtx.reservedLayoutSize = reservedSize;
  frameCtx.savedCursor = ctx->cursorPos;
  frameCtx.savedLastItemPos = ctx->lastItemPos;
  frameCtx.savedLastItemSize = ctx->lastItemSize;
  if (!reserveLayoutSpace && !ctx->layoutStack.empty()) {
    const LayoutStack &parentStack = ctx->layoutStack.back();
    frameCtx.parentCursor = parentStack.cursor;
    frameCtx.parentContentSize = parentStack.contentSize;
    frameCtx.parentAvailable = parentStack.availableSpace;
    frameCtx.parentItemCount = parentStack.itemCount;
  }

  bool shouldRenderContent = !state.minimized;

  if (shouldRenderContent) {
    Vec2 clipPos(state.position.x + panelStyle.padding.x,
                 state.position.y + titleHeight + panelStyle.padding.y);
    Vec2 clipSize(state.size.x - panelStyle.padding.x * 2.0f,
                  state.size.y - titleHeight - panelStyle.padding.y * 2.0f);
    clipSize.x = std::max(0.0f, clipSize.x);
    clipSize.y = std::max(0.0f, clipSize.y);
    if (clipSize.x > 0.0f && clipSize.y > 0.0f) {
      ctx->renderer.PushClipRect(clipPos, clipSize);
      frameCtx.clipPos = clipPos;
      frameCtx.clipSize = clipSize;
      frameCtx.clipPushed = true;
    }

    float contentWidth =
        std::max(0.0f, state.size.x - panelStyle.padding.x * 2.0f);
    float contentHeight = std::max(0.0f, state.size.y - titleHeight -
                                             panelStyle.padding.y * 2.0f);
    Vec2 contentOrigin(state.position.x + panelStyle.padding.x,
                       state.position.y + titleHeight + panelStyle.padding.y);

    // Guardar el cursor original antes de configurar el layout del panel
    Vec2 savedCursorBeforeLayout = ctx->cursorPos;

    // Configurar el cursor dentro del panel
    ctx->cursorPos = contentOrigin;
    ctx->lastItemPos = contentOrigin;
    ctx->lastItemSize = Vec2(0.0f, 0.0f);

    // Empujar offset del área de contenido (similar a BeginScrollView)
    ctx->offsetStack.push_back(contentOrigin);

    // Iniciar layout vertical para organizar los widgets dentro del panel
    BeginVertical(ctx->style.spacing, Vec2(contentWidth, contentHeight), Vec2(0.0f, 0.0f));
    frameCtx.layoutPushed = true;
  }

  ctx->panelStack.push_back(frameCtx);

  // Manejar el cursor según el estado del panel y reserveLayoutSpace
  // IMPORTANTE: Si el panel está visible (shouldRenderContent = true),
  // NO tocar cursorPos aquí porque ya está configurado correctamente arriba
  // Si se usa posición absoluta, NO avanzar el cursor
  // NOTA: No avanzar el cursor aquí cuando el panel está minimizado
  // EndPanel se encargará de avanzar el cursor correctamente usando displayedSize
  if (!shouldRenderContent && !state.useAbsolutePos) {
    // Panel minimizado: solo resetear cursor a layoutOrigin
    // El avance se hará en EndPanel
    ctx->cursorPos = layoutOrigin;
  }
  // Si shouldRenderContent es true, el cursor ya está correctamente configurado
  // en contentOrigin arriba (línea 1008), no hacer nada más

  return shouldRenderContent;
}

void EndPanel() {
  UIContext *ctx = GetContext();
  if (!ctx)
    return;
  if (ctx->panelStack.empty())
    return;

  PanelFrameContext frameCtx = ctx->panelStack.back();
  ctx->panelStack.pop_back();
  auto it = ctx->panelStates.find(frameCtx.id);
  if (it == ctx->panelStates.end())
    return;

  // Sacar offset del área de contenido (similar a EndScrollView)
  if (!ctx->offsetStack.empty()) {
    ctx->offsetStack.pop_back();
  }

  // Cerrar el layout vertical si fue creado
  if (frameCtx.layoutPushed) {
    EndVertical(false); // No avanzar el cursor del padre aquí
  }

  if (frameCtx.clipPushed) {
    ctx->renderer.PopClipRect();
  }

  auto &state = it->second;
  float titleHeight = frameCtx.titleHeight;
  Vec2 displayedSize(state.size.x,
                     state.minimized ? titleHeight : state.size.y);

  // Only restore cursor if NOT reserving layout space
  // When reserving layout space, we want the cursor to advance from current position
  if (!frameCtx.reserveLayout || state.useAbsolutePos) {
    ctx->cursorPos = frameCtx.savedCursor;
    ctx->lastItemPos = frameCtx.savedLastItemPos;
    ctx->lastItemSize = frameCtx.savedLastItemSize;
  }

  // Only restore parent layout state if NOT reserving layout space
  if (!ctx->layoutStack.empty() && (!frameCtx.reserveLayout || state.useAbsolutePos)) {
    LayoutStack &parentStack = ctx->layoutStack.back();
    parentStack.cursor = frameCtx.parentCursor;
    parentStack.contentSize = frameCtx.parentContentSize;
    parentStack.availableSpace = frameCtx.parentAvailable;
    parentStack.itemCount = frameCtx.parentItemCount;
  }

  // Avanzar cursor si se reserva espacio en layout
  if (frameCtx.reserveLayout && !state.useAbsolutePos) {
    // Usar displayedSize que ya tiene en cuenta si el panel está minimizado
    Vec2 advanceSize = displayedSize;

    // Set lastItemPos to panel position before advancing
    ctx->lastItemPos = state.position;
    AdvanceCursor(ctx, advanceSize);
  }
}

bool BeginScrollView(const std::string &id, const Vec2 &size,
                     Vec2 *scrollOffset, std::optional<Vec2> pos) {
  UIContext *ctx = GetContext();
  if (!ctx)
    return false;

  // scrollConsumedThisFrame is now reset in NewFrame()

  std::string key = "SCROLLVIEW:" + id;
  uint32_t scrollViewId = GenerateId(key.c_str());
  auto &state = ctx->scrollViewStates[scrollViewId];

  if (!state.initialized) {
    state.scrollOffset = Vec2(0.0f, 0.0f);
    state.contentSize = Vec2(0.0f, 0.0f);
    state.viewSize = size;
    state.initialized = true;
  } else {
    state.viewSize = size;
  }

  // Si se proporciona scrollOffset externo, usarlo y actualizar el estado
  if (scrollOffset) {
    state.scrollOffset = *scrollOffset;
  }

  float scrollbarWidth = SCROLLBAR_WIDTH_LARGE;

  // Resolver posición
  Vec2 scrollViewPos;
  Vec2 fullSize(size.x, size.y);
  if (pos.has_value()) {
    scrollViewPos = ResolveAbsolutePosition(ctx, pos.value(), fullSize);
    state.useAbsolutePos = true;
    state.absolutePos = scrollViewPos;
  } else {
    scrollViewPos = ctx->cursorPos;
    state.useAbsolutePos = false;
  }

  state.position = scrollViewPos;

  // Registrar área ocupada solo si NO se usa posición absoluta
  if (!state.useAbsolutePos) {
    RegisterOccupiedArea(ctx, scrollViewPos, fullSize);
  }

  const PanelStyle &panelStyle = ctx->style.panel;

  // Dibujar fondo del scroll view con contraste más pronunciado - sin borde
  Color scrollBg = AdjustContainerBackground(panelStyle.background, ctx->style.isDarkTheme);
  ctx->renderer.DrawRectFilled(scrollViewPos, size, scrollBg,
                               panelStyle.cornerRadius);

  // Calcular área de contenido (reservar espacio para scrollbar)
  Vec2 availableSize(size.x - scrollbarWidth, size.y - scrollbarWidth);
  Vec2 contentAreaPos(scrollViewPos.x + panelStyle.padding.x,
                      scrollViewPos.y + panelStyle.padding.y);
  Vec2 contentAreaSize(availableSize.x - panelStyle.padding.x * 2.0f,
                       availableSize.y - panelStyle.padding.y * 2.0f);

  // Manejar scroll con rueda del mouse
  // Solo procesar scroll si el mouse está sobre el área de contenido de este ScrollView
  // y este ScrollView es el más anidado (está en el tope de la pila después de agregarlo)
  // Verificar hover sobre el área de contenido (no solo el ScrollView completo)
  bool hoverContentArea = IsMouseOver(ctx, contentAreaPos, contentAreaSize);

  // Agregar este ScrollView a la pila primero para poder verificar si es el más anidado
  // (temporalmente, antes de agregarlo al final)

  // Solo procesar scroll si el hover está sobre el área de contenido Y
  // el scroll no ha sido consumido aún (esto asegura que solo el primero en procesarse lo haga)
  // Como los ScrollViews más anidados se procesan primero, esto naturalmente hace que solo
  // el más anidado procese el scroll
  if (hoverContentArea && !ctx->scrollConsumedThisFrame) {
    float wheelY = ctx->input.MouseWheelY();
    if (std::abs(wheelY) > 0.001f) {
      float scrollSpeed = SCROLL_SPEED;
      // El límite se calculará en EndScrollView cuando sepamos contentSize
      state.scrollOffset.y -= wheelY * scrollSpeed;
      state.scrollOffset.y = std::max(0.0f, state.scrollOffset.y);
      ctx->scrollConsumedThisFrame = true; // Marcar como consumido
    }
    float wheelX = ctx->input.MouseWheelX();
    if (std::abs(wheelX) > 0.001f) {
      float scrollSpeed = SCROLL_SPEED;
      state.scrollOffset.x -= wheelX * scrollSpeed;
      state.scrollOffset.x = std::max(0.0f, state.scrollOffset.x);
      ctx->scrollConsumedThisFrame = true; // Marcar como consumido
    }
  }

  // Scrollbar interaction is handled in EndScrollView to avoid duplication.
  // The scroll offset from the previous frame is used for content positioning.

  // Aplicar clipping al área de contenido
  ctx->renderer.PushClipRect(contentAreaPos, contentAreaSize);

  // Guardar cursor y estado antes de modificarlo
  Vec2 savedCursor = ctx->cursorPos;
  Vec2 savedLastItemPos = ctx->lastItemPos;
  Vec2 savedLastItemSize = ctx->lastItemSize;

  // Configurar el cursor para el contenido (ajustado por scroll)
  ctx->cursorPos = contentAreaPos - state.scrollOffset;
  ctx->lastItemPos = ctx->cursorPos;
  ctx->lastItemSize = Vec2(0.0f, 0.0f);

  // Empujar offset del área de contenido
  ctx->offsetStack.push_back(contentAreaPos);

  // Iniciar layout vertical para organizar los widgets dentro del ScrollView
  BeginVertical(ctx->style.spacing, Vec2(contentAreaSize.x, 0.0f), Vec2(0.0f, 0.0f));

  // Guardar contexto para EndScrollView
  ScrollViewFrameContext frameCtx{};
  frameCtx.id = scrollViewId;
  frameCtx.position = scrollViewPos;
  frameCtx.size = size;
  frameCtx.contentAreaPos = contentAreaPos;
  frameCtx.contentAreaSize = contentAreaSize;
  frameCtx.availableSize = availableSize;
  frameCtx.scrollbarWidth = scrollbarWidth;
  frameCtx.layoutPushed = true;
  frameCtx.useAbsolutePos = state.useAbsolutePos;
  frameCtx.savedCursor = savedCursor;
  frameCtx.savedLastItemPos = savedLastItemPos;
  frameCtx.savedLastItemSize = savedLastItemSize;
  ctx->scrollViewStack.push_back(frameCtx);

  // Actualizar scrollOffset externo si se proporciona
  if (scrollOffset) {
    *scrollOffset = state.scrollOffset;
  }

  return true;
}

void EndScrollView() {
  UIContext *ctx = GetContext();
  if (!ctx)
    return;

  if (ctx->scrollViewStack.empty())
    return;

  ScrollViewFrameContext frameCtx = ctx->scrollViewStack.back();
  ctx->scrollViewStack.pop_back();

  auto it = ctx->scrollViewStates.find(frameCtx.id);
  if (it == ctx->scrollViewStates.end())
    return;

  auto &state = it->second;

  // Cerrar el layout vertical y obtener el tamaño del contenido
  if (frameCtx.layoutPushed && !ctx->layoutStack.empty()) {
    LayoutStack &layout = ctx->layoutStack.back();
    // El contentSize es el tamaño total del contenido en el layout
    state.contentSize = layout.contentSize;
    EndVertical(false); // No avanzar el cursor del padre aquí
  }

  // Sacar offset del área de contenido
  if (!ctx->offsetStack.empty()) {
    ctx->offsetStack.pop_back();
  }

  // Remover clipping
  ctx->renderer.PopClipRect();

  // Restaurar cursor original antes de avanzarlo
  // Esto evita que el scrollOffset afecte a widgets fuera del ScrollView
  ctx->cursorPos = frameCtx.savedCursor;
  ctx->lastItemPos = frameCtx.savedLastItemPos;
  ctx->lastItemSize = frameCtx.savedLastItemSize;

  const PanelStyle &panelStyle = ctx->style.panel;

  // Calcular si necesitamos barras de scroll
  bool needsVerticalScrollbar = state.contentSize.y > frameCtx.contentAreaSize.y;
  bool needsHorizontalScrollbar = state.contentSize.x > frameCtx.contentAreaSize.x;

  // Clamp scroll offset a rangos válidos
  float maxScrollY = std::max(0.0f, state.contentSize.y - frameCtx.contentAreaSize.y);
  float maxScrollX = std::max(0.0f, state.contentSize.x - frameCtx.contentAreaSize.x);
  state.scrollOffset.y = std::clamp(state.scrollOffset.y, 0.0f, maxScrollY);
  state.scrollOffset.x = std::clamp(state.scrollOffset.x, 0.0f, maxScrollX);

  // Manejar input del mouse para las barras de scroll
  float mouseX = ctx->input.MouseX();
  float mouseY = ctx->input.MouseY();
  bool leftDown = ctx->input.IsMouseDown(0);
  bool leftPressed = ctx->input.IsMousePressed(0);

  // Barra de scroll vertical
  if (needsVerticalScrollbar) {
    Vec2 vScrollbarPos(frameCtx.position.x + frameCtx.size.x - frameCtx.scrollbarWidth,
                       frameCtx.position.y);
    Vec2 vScrollbarSize(frameCtx.scrollbarWidth,
                        frameCtx.size.y -
                            (needsHorizontalScrollbar ? frameCtx.scrollbarWidth : 0.0f));

    float scrollRatio = std::clamp(state.contentSize.y > 0.0f
                                       ? frameCtx.contentAreaSize.y / state.contentSize.y
                                       : 1.0f,
                                   0.0f, 1.0f);
    float thumbHeight = vScrollbarSize.y * scrollRatio;
    thumbHeight = std::max(thumbHeight, 20.0f); // Mínimo tamaño del thumb
    float maxThumbPos = vScrollbarSize.y - thumbHeight;
    float thumbPos =
        maxThumbPos * std::clamp(state.scrollOffset.y / maxScrollY, 0.0f, 1.0f);

    Vec2 thumbPosAbsolute(vScrollbarPos.x, vScrollbarPos.y + thumbPos);
    Vec2 thumbSize(frameCtx.scrollbarWidth, thumbHeight);

    // Detectar hover y drag en la barra vertical
    bool hoverThumb = (mouseX >= thumbPosAbsolute.x &&
                       mouseX <= thumbPosAbsolute.x + thumbSize.x &&
                       mouseY >= thumbPosAbsolute.y &&
                       mouseY <= thumbPosAbsolute.y + thumbSize.y);
    bool hoverTrack = (mouseX >= vScrollbarPos.x &&
                       mouseX <= vScrollbarPos.x + vScrollbarSize.x &&
                       mouseY >= vScrollbarPos.y &&
                       mouseY <= vScrollbarPos.y + vScrollbarSize.y);

    if (state.draggingScrollbar && state.draggingScrollbarType == 1) {
      if (!leftDown) {
        state.draggingScrollbar = false;
        state.draggingScrollbarType = 0;
      } else {
        float mouseDelta = mouseY - state.dragStartMouse.y;
        float scrollDelta = (mouseDelta / maxThumbPos) * maxScrollY;
        state.scrollOffset.y =
            std::clamp(state.dragStartScroll + scrollDelta, 0.0f, maxScrollY);
      }
    } else if (leftPressed && hoverThumb) {
      state.draggingScrollbar = true;
      state.draggingScrollbarType = 1;
      state.dragStartMouse = Vec2(mouseX, mouseY);
      state.dragStartScroll = state.scrollOffset.y;
    } else if (leftPressed && hoverTrack) {
      // Click en el track: saltar a esa posición
      float clickPos = mouseY - vScrollbarPos.y;
      float scrollRatioClick = clickPos / vScrollbarSize.y;
      state.scrollOffset.y = std::clamp(scrollRatioClick * maxScrollY, 0.0f, maxScrollY);
    }

    // Dibujar barra de scroll vertical
    Color scrollbarBg =
        hoverTrack ? panelStyle.headerBackground : panelStyle.background;
    ctx->renderer.DrawRectFilled(vScrollbarPos, vScrollbarSize, scrollbarBg,
                                 0.0f);

    Color thumbColor = hoverThumb || state.draggingScrollbar
                           ? ctx->style.button.background.hover
                           : ctx->style.button.background.normal;
    ctx->renderer.DrawRectFilled(thumbPosAbsolute, thumbSize, thumbColor, 4.0f);
  }

  // Barra de scroll horizontal
  if (needsHorizontalScrollbar) {
    Vec2 hScrollbarPos(frameCtx.position.x,
                       frameCtx.position.y + frameCtx.size.y - frameCtx.scrollbarWidth);
    Vec2 hScrollbarSize(frameCtx.size.x -
                            (needsVerticalScrollbar ? frameCtx.scrollbarWidth : 0.0f),
                        frameCtx.scrollbarWidth);

    float scrollRatio = std::clamp(state.contentSize.x > 0.0f
                                       ? frameCtx.contentAreaSize.x / state.contentSize.x
                                       : 1.0f,
                                   0.0f, 1.0f);
    float thumbWidth = hScrollbarSize.x * scrollRatio;
    thumbWidth = std::max(thumbWidth, 20.0f); // Mínimo tamaño del thumb
    float maxThumbPos = hScrollbarSize.x - thumbWidth;
    float thumbPos =
        maxThumbPos * std::clamp(state.scrollOffset.x / maxScrollX, 0.0f, 1.0f);

    Vec2 thumbPosAbsolute(hScrollbarPos.x + thumbPos, hScrollbarPos.y);
    Vec2 thumbSize(thumbWidth, frameCtx.scrollbarWidth);

    // Detectar hover y drag en la barra horizontal
    bool hoverThumb = (mouseX >= thumbPosAbsolute.x &&
                       mouseX <= thumbPosAbsolute.x + thumbSize.x &&
                       mouseY >= thumbPosAbsolute.y &&
                       mouseY <= thumbPosAbsolute.y + thumbSize.y);
    bool hoverTrack = (mouseX >= hScrollbarPos.x &&
                       mouseX <= hScrollbarPos.x + hScrollbarSize.x &&
                       mouseY >= hScrollbarPos.y &&
                       mouseY <= hScrollbarPos.y + hScrollbarSize.y);

    if (state.draggingScrollbar && state.draggingScrollbarType == 2) {
      if (!leftDown) {
        state.draggingScrollbar = false;
        state.draggingScrollbarType = 0;
      } else {
        float mouseDelta = mouseX - state.dragStartMouse.x;
        float scrollDelta = (mouseDelta / maxThumbPos) * maxScrollX;
        state.scrollOffset.x =
            std::clamp(state.dragStartScroll + scrollDelta, 0.0f, maxScrollX);
      }
    } else if (leftPressed && hoverThumb) {
      state.draggingScrollbar = true;
      state.draggingScrollbarType = 2;
      state.dragStartMouse = Vec2(mouseX, mouseY);
      state.dragStartScroll = state.scrollOffset.x;
    } else if (leftPressed && hoverTrack) {
      // Click en el track: saltar a esa posición
      float clickPos = mouseX - hScrollbarPos.x;
      float scrollRatioClick = clickPos / hScrollbarSize.x;
      state.scrollOffset.x = std::clamp(scrollRatioClick * maxScrollX, 0.0f, maxScrollX);
    }

    // Dibujar barra de scroll horizontal
    Color scrollbarBg =
        hoverTrack ? panelStyle.headerBackground : panelStyle.background;
    ctx->renderer.DrawRectFilled(hScrollbarPos, hScrollbarSize, scrollbarBg,
                                 0.0f);

    Color thumbColor = hoverThumb || state.draggingScrollbar
                           ? ctx->style.button.background.hover
                           : ctx->style.button.background.normal;
    ctx->renderer.DrawRectFilled(thumbPosAbsolute, thumbSize, thumbColor, 4.0f);
  }

  // Avanzar cursor si NO se usa posición absoluta
  // El cursor ya fue restaurado arriba, así que avanzar desde allí
  if (!frameCtx.useAbsolutePos) {
    // Usar la posición del ScrollView como lastItemPos para el avance correcto
    ctx->lastItemPos = frameCtx.position;
    // El cursor ya está restaurado, así que AdvanceCursor avanzará desde él
    AdvanceCursor(ctx, frameCtx.size);
  } else {
    // Si usa posición absoluta, solo restaurar el cursor sin avanzarlo
    // El cursor ya fue restaurado arriba
  }
}

bool BeginTabView(const std::string &id, int *activeTab,
                  const std::vector<std::string> &tabLabels, const Vec2 &size,
                  std::optional<Vec2> pos) {
  UIContext *ctx = GetContext();
  if (!ctx || tabLabels.empty())
    return false;

  std::string key = "TABVIEW:" + id;
  uint32_t tabViewId = GenerateId(key.c_str());
  auto &state = ctx->tabViewStates[tabViewId];

  if (!state.initialized) {
    state.activeTab = activeTab ? *activeTab : 0;
    state.initialized = true;
  } else {
    if (activeTab) {
      state.activeTab = *activeTab;
    }
  }

  const PanelStyle &panelStyle = ctx->style.panel;
  const TextStyle &tabTextStyle =
      ctx->style.GetTextStyle(TypographyStyle::Body);
  float tabHeight = tabTextStyle.fontSize + panelStyle.padding.y * 2.0f;
  float tabSpacing = 4.0f;

  // Calcular tamaño de la barra de pestañas
  float tabBarWidth = 0.0f;
  for (const auto &label : tabLabels) {
    Vec2 labelSize = MeasureTextCached(ctx,label, tabTextStyle.fontSize);
    tabBarWidth += labelSize.x + panelStyle.padding.x * 2.0f + tabSpacing;
  }
  tabBarWidth -= tabSpacing; // Remover el último spacing

  Vec2 tabViewSize = size;
  if (tabViewSize.x <= 0.0f) {
    tabViewSize.x = std::max(tabBarWidth + panelStyle.padding.x * 2.0f, 300.0f);
  }
  if (tabViewSize.y <= 0.0f) {
    tabViewSize.y = 300.0f;
  }

  // Resolver posición: usar pos si se proporciona, sino usar cursor
  Vec2 tabViewPos;
  if (pos.has_value()) {
    tabViewPos = pos.value();
    state.useAbsolutePos = true;
    state.absolutePos = pos.value();
  } else {
    tabViewPos = ctx->cursorPos;
    tabViewPos = ResolveAbsolutePosition(ctx, tabViewPos, tabViewSize);
    state.useAbsolutePos = false;
  }
  // Registrar área ocupada solo si NO se usa posición absoluta
  if (!state.useAbsolutePos) {
    RegisterOccupiedArea(ctx, tabViewPos, tabViewSize);
  }

  state.tabBarSize = Vec2(tabViewSize.x, tabHeight);

  // Configurar área de contenido PRIMERO (debajo de la barra de pestañas)
  // Usar un padding más pequeño dentro del TabView para evitar problemas de
  // clipping
  float innerPadding = 8.0f;
  Vec2 contentPos(tabViewPos.x + innerPadding,
                  tabViewPos.y + tabHeight + innerPadding);
  Vec2 contentSize(tabViewSize.x - innerPadding * 2.0f,
                   tabViewSize.y - tabHeight - innerPadding * 2.0f);

  // Asegurar que el contentSize sea válido
  if (contentSize.x < 0.0f)
    contentSize.x = 0.0f;
  if (contentSize.y < 0.0f)
    contentSize.y = 0.0f;

  // Dibujar fondo del TabView PRIMERO - esto debe renderizarse antes que los
  // widgets - con contraste sin borde
  Color tabViewBg = AdjustContainerBackground(panelStyle.background, ctx->style.isDarkTheme);
  ctx->renderer.DrawRectFilled(tabViewPos, tabViewSize, tabViewBg,
                               panelStyle.cornerRadius);

  // Dibujar barra de pestañas - con contraste sin borde
  Vec2 tabBarPos = tabViewPos;
  Vec2 tabBarBgPos = tabBarPos;
  Vec2 tabBarBgSize(tabViewSize.x, tabHeight);
  Color tabBarBg = AdjustContainerBackground(panelStyle.headerBackground, ctx->style.isDarkTheme);
  ctx->renderer.DrawRectFilled(tabBarBgPos, tabBarBgSize, tabBarBg, 0.0f);

  // Dibujar pestañas
  float currentX = tabBarPos.x + panelStyle.padding.x;
  float mouseX = ctx->input.MouseX();
  float mouseY = ctx->input.MouseY();
  bool leftPressed = ctx->input.IsMousePressed(0);

  for (size_t i = 0; i < tabLabels.size(); ++i) {
    Vec2 labelSize =
        MeasureTextCached(ctx,tabLabels[i], tabTextStyle.fontSize);
    Vec2 tabSize(labelSize.x + panelStyle.padding.x * 2.0f, tabHeight);
    Vec2 tabPos(currentX, tabBarPos.y);

    bool hover = IsMouseOver(ctx, tabPos, tabSize);
    bool isActive = (static_cast<int>(i) == state.activeTab);

    // Dibujar fondo de la pestaña - sin borde, solo contraste
    Color tabBg;
    if (isActive) {
      // Tab activa: usar el mismo color que el fondo del TabView para que no haya borde visible
      tabBg = tabViewBg;
    } else {
      // Tab inactiva: usar el mismo color que la barra de tabs
      tabBg = tabBarBg;
    }
    if (hover && !isActive) {
      tabBg = Color(tabBg.r * 1.1f, tabBg.g * 1.1f, tabBg.b * 1.1f, tabBg.a);
    }
    ctx->renderer.DrawRectFilled(tabPos, tabSize, tabBg, 0.0f);

    // Dibujar indicador de pestaña activa (línea inferior)
    if (isActive) {
      Vec2 indicatorStart(tabPos.x, tabPos.y + tabSize.y - 2.0f);
      Vec2 indicatorEnd(tabPos.x + tabSize.x, tabPos.y + tabSize.y - 2.0f);
      Color indicatorColor = ctx->style.button.background.normal;
      ctx->renderer.DrawLine(indicatorStart, indicatorEnd, indicatorColor,
                             3.0f);
    }

    // Dibujar texto de la pestaña
    Vec2 textPos(tabPos.x + panelStyle.padding.x,
                 tabPos.y + (tabHeight - labelSize.y) * 0.5f);
    Color textColor =
        isActive
            ? tabTextStyle.color
            : Color(tabTextStyle.color.r * 0.8f, tabTextStyle.color.g * 0.8f,
                    tabTextStyle.color.b * 0.8f, tabTextStyle.color.a);
    ctx->renderer.DrawText(textPos, tabLabels[i], textColor,
                           tabTextStyle.fontSize);

    // Manejar click en la pestaña
    if (hover && leftPressed) {
      state.activeTab = static_cast<int>(i);
      if (activeTab) {
        *activeTab = state.activeTab;
      }
    }

    currentX += tabSize.x + tabSpacing;
  }

  // IMPORTANTE: Flush batch aquí para asegurar que el fondo del TabView se
  // renderice ANTES de aplicar el clipping y dibujar los widgets
  ctx->renderer.FlushBatch();


  // Configurar cursor para el contenido de la pestaña activa
  ctx->cursorPos = contentPos;
  ctx->lastItemPos = contentPos;
  ctx->lastItemSize = Vec2(0.0f, 0.0f);

  // Configurar scroll del contenido
  state.viewSize = contentSize;

  // Scrollbar interaction is handled in EndTabView to avoid duplication.

  // Obtener scrollOffset para el tab activo (cada tab tiene su propio scrollOffset)
  Vec2& tabScrollOffset = state.tabScrollOffsets[state.activeTab];

  // Ajustar cursor con scroll actual (vertical)
  Vec2 scrolledContentPos =
      Vec2(contentPos.x, contentPos.y - tabScrollOffset.y);

  // Aplicar clipping al área de contenido ANTES de configurar el layout
  // Esto asegura que los widgets solo se dibujen dentro del área visible
  ctx->renderer.PushClipRect(contentPos, contentSize);

  // Iniciar layout vertical para el contenido con padding cero explícito
  // Esto debe hacerse DESPUÉS del clipping
  ctx->cursorPos = scrolledContentPos;
  ctx->lastItemPos = scrolledContentPos;
  ctx->lastItemSize = Vec2(0.0f, 0.0f);
  BeginVertical(ctx->style.spacing, std::make_optional(contentSize),
                std::make_optional(Vec2(0.0f, 0.0f)));

  // Guardar frame del tab para calcular tamaño de contenido al cerrar
  ctx->tabFrameStack.push_back(
      TabContentFrame{tabViewId, contentPos, contentSize, ctx->cursorPos});

  // Actualizar activeTab externo
  if (activeTab) {
    *activeTab = state.activeTab;
  }

  return true;
}

void EndTabView() {
  UIContext *ctx = GetContext();
  if (!ctx)
    return;

  // Cerrar el layout vertical para finalizar y conocer el cursor final (tamaño
  // de contenido)
  Vec2 contentEndCursor = ctx->cursorPos;
  if (!ctx->layoutStack.empty()) {
    EndVertical(false);
  }

  // Tomar frame guardado
  if (!ctx->tabFrameStack.empty()) {
    TabContentFrame frame = ctx->tabFrameStack.back();
    ctx->tabFrameStack.pop_back();

    // Calcular tamaño de contenido real
    Vec2 contentSize;
    contentSize.x =
        frame.contentAreaSize.x; // por ahora igual al ancho del área
    contentSize.y = std::max(frame.contentAreaSize.y,
                             contentEndCursor.y - frame.contentStartCursor.y);

    // Actualizar estado del TabView
    UIContext *ctx2 = GetContext();
    if (ctx2) {
      auto it = ctx2->tabViewStates.find(frame.tabViewId);
      if (it != ctx2->tabViewStates.end()) {
        auto &st = it->second;
        st.contentSize = contentSize;

        // Manejar input para scroll vertical
        float mouseX = ctx2->input.MouseX();
        float mouseY = ctx2->input.MouseY();
        bool hoverContent =
            (mouseX >= frame.contentAreaPos.x &&
             mouseX <= frame.contentAreaPos.x + frame.contentAreaSize.x &&
             mouseY >= frame.contentAreaPos.y &&
             mouseY <= frame.contentAreaPos.y + frame.contentAreaSize.y);
        float wheelY = ctx2->input.MouseWheelY();
        if (hoverContent && std::abs(wheelY) > 0.001f &&
            contentSize.y > frame.contentAreaSize.y && !ctx->scrollConsumedThisFrame) {
          float scrollSpeed = SCROLL_SPEED;
          // Usar scrollOffset del tab activo
          Vec2& tabScrollOffset = st.tabScrollOffsets[st.activeTab];
          tabScrollOffset.y = std::clamp(
              tabScrollOffset.y - wheelY * scrollSpeed, 0.0f,
              std::max(0.0f, contentSize.y - frame.contentAreaSize.y));
          ctx->scrollConsumedThisFrame = true; // Marcar como consumido
        }

        // Dibujar barra de scroll vertical si es necesaria
        if (contentSize.y > frame.contentAreaSize.y) {
          float scrollbarWidth = SCROLLBAR_WIDTH;
          Vec2 barPos(frame.contentAreaPos.x + frame.contentAreaSize.x -
                          scrollbarWidth,
                      frame.contentAreaPos.y);
          Vec2 barSize(scrollbarWidth, frame.contentAreaSize.y);
          Vec2& tabScrollOffset = st.tabScrollOffsets[st.activeTab];
          DrawScrollbar(ctx2, barPos, barSize, contentSize.y,
                        frame.contentAreaSize.y, tabScrollOffset.y,
                        st.draggingScrollbar, st.dragStartMouse,
                        st.dragStartScroll, st.draggingScrollbar, true);
        }

        // Advance cursor for non-absolute TabViews
        if (!st.useAbsolutePos) {
          Vec2 tabViewSize(st.tabBarSize.x,
                           st.tabBarSize.y + frame.contentAreaSize.y +
                               16.0f); // 2 * innerPadding
          ctx->lastItemPos = frame.contentAreaPos - Vec2(8.0f, st.tabBarSize.y + 8.0f);
          AdvanceCursor(ctx, tabViewSize);
        }
      }
    }
  }

  // Remover clipping al final
  ctx->renderer.PopClipRect();
}

} // namespace FluentUI
