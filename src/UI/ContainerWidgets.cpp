#include "UI/Widgets.h"
#include "UI/WidgetHelpers.h"
#include "UI/Icons.h"
#include "Theme/FluentTheme.h"
#include "Theme/Material.h"
#include "core/Animation.h"
#include "core/Context.h"
#include "core/Renderer.h"
#include "core/Elevation.h"
#include "core/UIKey.h"
#include "core/WidgetNodes.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <optional>

namespace FluentUI {

bool BeginPanel(const std::string &id, uint32_t iconCodepoint, const Vec2 &desiredSize,
                bool reserveLayoutSpace, std::optional<bool> useAcrylic,
                std::optional<float> acrylicOpacity, std::optional<Vec2> pos, float maxHeight);

bool BeginPanel(const std::string &id, const Vec2 &desiredSize,
                bool reserveLayoutSpace, std::optional<bool> useAcrylic,
                std::optional<float> acrylicOpacity, std::optional<Vec2> pos, float maxHeight) {
  return BeginPanel(id, 0u, desiredSize, reserveLayoutSpace, useAcrylic,
                    acrylicOpacity, pos, maxHeight);
}

bool BeginPanel(const std::string &id, uint32_t iconCodepoint, const Vec2 &desiredSize,
                bool reserveLayoutSpace, std::optional<bool> useAcrylic,
                std::optional<float> acrylicOpacity, std::optional<Vec2> pos, float maxHeight) {
  UIContext *ctx = GetContext();
  if (!ctx)
    return false;

  uint32_t panelId = GenerateId("PANEL:", id.c_str());

  // Register in widget tree (Phase 1 — coexists with old panelStates map)
  auto* panelNode = static_cast<PanelNode*>(
      ctx->widgetTree.FindOrCreate(panelId, ctx->frame, [&]() {
          auto node = std::make_unique<PanelNode>(panelId);
          node->debugName = id;
          return node;
      })
  );
  ctx->widgetTree.PushParent(panelNode);

  auto &state = ctx->panelStates[panelId];
  const PanelStyle &panelStyle = ctx->GetEffectivePanelStyle();
  float titleHeight =
      panelStyle.headerText.fontSize + panelStyle.padding.y * 2.0f;
  float verticalPadding = panelStyle.padding.y * 2.0f;

  if (!state.initialized) {
    state.size = desiredSize;
    if (state.size.x <= 0.0f) {
      Vec2 avail = GetCurrentAvailableSpace(ctx);
      state.size.x = avail.x > 0.0f ? avail.x : 300.0f;
    }
    if (state.size.y <= 0.0f) {
        state.size.y = titleHeight + 40.0f;
    }

    if (pos.has_value()) {
      state.position = ResolveAbsolutePosition(ctx, pos.value(), state.size);
      state.useAbsolutePos = true;
    } else {
      state.position = ctx->cursorPos;
      state.useAbsolutePos = false;
    }

    state.useAcrylic = useAcrylic.has_value() ? useAcrylic.value() : panelStyle.useAcrylic;
    state.acrylicOpacity = acrylicOpacity.has_value() ? acrylicOpacity.value() : panelStyle.acrylicOpacity;
    state.initialized = true;
  } else {
    // Update size from caller if not manually resized
    if (!state.resizing && !state.hasBeenManuallyResized) {
        if (desiredSize.x > 0.0f) state.size.x = desiredSize.x;
        // NOTE: height is governed by EndPanel's content-based auto-grow, which
        // already treats desiredSize.y as a *minimum* (userHeight). Overwriting
        // size.y here every frame would pin the panel to desiredSize.y and clip
        // any content taller than it (e.g. a compact panel that should grow).
    }
  }

  // SINCRONIZACIÓN DE POSICIÓN
  // For absolute-positioned panels: sync from caller only if NOT dragged by user
  if (pos.has_value() && state.useAbsolutePos && !state.dragging && !state.hasBeenDragged) {
    state.position = ResolveAbsolutePosition(ctx, pos.value(), state.size);
  } else if (!state.useAbsolutePos && !state.dragging && !state.resizing) {
    state.position = ctx->cursorPos;
    if (state.hasBeenDragged) {
      state.position = state.position + state.dragPositionOffset;
    }
  }

  Vec2 mousePos(ctx->input.MouseX(), ctx->input.MouseY());
  bool leftDown = ctx->input.IsMouseDown(0);
  bool leftPressed = ctx->input.IsMousePressed(0);

  Vec2 titleSize(state.size.x, titleHeight);
  Vec2 minimizeButtonSize(titleHeight - panelStyle.padding.y * 1.5f, titleHeight - panelStyle.padding.y * 1.5f);
  Vec2 minimizeButtonPos(state.position.x + state.size.x - minimizeButtonSize.x - panelStyle.padding.x,
                         state.position.y + (titleHeight - minimizeButtonSize.y) * 0.5f);

  if (leftPressed && PointInRect(mousePos, minimizeButtonPos, minimizeButtonSize)) {
    state.minimized = !state.minimized;
    state.dragging = false; state.resizing = false;
  }

  // Drag & Resize
  if (state.dragging) {
    if (!leftDown) {
      state.dragging = false;
      if (!state.useAbsolutePos) state.dragPositionOffset = state.position - ctx->cursorPos;
      state.hasBeenDragged = true;
    } else {
      state.position = Vec2(mousePos.x - state.dragOffset.x, mousePos.y - state.dragOffset.y);
    }
  }

  if (state.resizing && !state.minimized) {
    if (!leftDown) {
      state.resizing = false;
      state.hasBeenManuallyResized = true;
      state.manualSize = state.size;
    } else {
      Vec2 delta = mousePos - state.resizeStartMouse;
      state.size.x = std::max(state.resizeStartSize.x + delta.x, 120.0f);
      state.size.y = std::max(state.resizeStartSize.y + delta.y, titleHeight + 20.0f);
    }
  }

  if (!state.dragging && !state.resizing && leftPressed) {
    if (PointInRect(mousePos, state.position, titleSize) && !PointInRect(mousePos, minimizeButtonPos, minimizeButtonSize)) {
      state.dragging = true;
      state.dragOffset = mousePos - state.position;
    } else if (!state.minimized) {
      Vec2 hPos(state.position.x + state.size.x - 12, state.position.y + state.size.y - 12);
      if (PointInRect(mousePos, hPos, Vec2(12, 12))) {
        state.resizing = true;
        state.resizeStartMouse = mousePos;
        state.resizeStartSize = state.size;
      }
    }
  }

  // Set resize cursor when hovering resize handle
  if (!state.minimized) {
    Vec2 hPos(state.position.x + state.size.x - 12, state.position.y + state.size.y - 12);
    if (state.resizing || PointInRect(mousePos, hPos, Vec2(12, 12))) {
      ctx->desiredCursor = UIContext::CursorType::ResizeNWSE;
    } else if (state.dragging || PointInRect(mousePos, state.position, Vec2(state.size.x, titleHeight))) {
      ctx->desiredCursor = UIContext::CursorType::Hand;
    }
  }

  // PRE-GROW: ajustar la altura a lo medido el frame anterior ANTES de empujar el
  // clip, para que el área de contenido (y por tanto el clip) ya quepa todo este
  // mismo frame. EndPanel también ajusta, pero solo surte efecto al frame
  // siguiente, lo que dejaba el contenido recortado un frame (o de forma
  // persistente si quedaba justo bajo el umbral de la scrollbar). Sin maxHeight el
  // panel crece libremente para contener su contenido; con maxHeight se limita y
  // EndPanel dibuja la scrollbar. Se respeta el redimensionado manual del usuario.
  if (!state.resizing && !state.hasBeenManuallyResized && state.contentSize.y > 0.0f) {
    float fitHeight = titleHeight + state.contentSize.y + verticalPadding;
    float target = (desiredSize.y > 0.0f) ? std::max(fitHeight, desiredSize.y) : fitHeight;
    if (maxHeight > 0.0f) {
      float minUsable = titleHeight + verticalPadding + state.contentSize.y * 0.5f;
      target = std::min(target, std::max(maxHeight, minUsable));
    }
    if (target > state.size.y) state.size.y = target;
  }

  // CULLING: Para contenedores como Panel, somos muy permisivos.
  // Solo descartamos si está completamente fuera del viewport global para ahorrar rendimiento,
  // pero dejamos que el Clipping Stack maneje el recorte fino en scroll.
  Vec2 viewportSize = ctx->renderer.GetViewportSize();
  Vec2 drawSize = state.minimized ? Vec2(state.size.x, titleHeight) : state.size;
  bool isVisible = (state.position.x + state.size.x > 0 && state.position.x < viewportSize.x &&
                    state.position.y + drawSize.y > 0 && state.position.y < viewportSize.y);
  
  if (isVisible) {
    // Brief 07: the panel/card body (fill, corner, elevation) comes from a
    // data-driven material instead of reading PanelStyle fields ad hoc. Identical
    // result to before (z=Card, fill=panel.background, radius=panel.cornerRadius).
    FluentMaterial panelMat = ResolvePanelMaterial(panelStyle, WidgetState::Rest);
    // Dibujo del panel
    if (!state.minimized && panelStyle.shadowOpacity > 0.0f) {
      // Sombra por elevación: un panel/card flota en z=Card. shadowOpacity del
      // tema actúa de interruptor (los temas "flat" lo ponen a 0).
      ctx->renderer.DrawElevationShadow(state.position, drawSize,
                                        panelMat.radius, panelMat.elevationZ);
    }
    Color bg = panelMat.fill;
    if (state.useAcrylic) ctx->renderer.DrawRectAcrylic(state.position, drawSize, bg, panelMat.radius, state.acrylicOpacity);
    else ctx->renderer.DrawRectFilled(state.position, drawSize, bg, panelMat.radius);

    ctx->renderer.DrawRectFilled(state.position, titleSize, panelStyle.headerBackground, panelMat.radius);
    {
      float titleIconSize = panelStyle.headerText.fontSize;
      float titleIconGap = 6.0f;
      float titleIconSlot = (iconCodepoint != 0u) ? (titleIconSize + titleIconGap) : 0.0f;
      Vec2 titlePadStart = state.position + panelStyle.padding * 0.5f;
      if (iconCodepoint != 0u) {
        DrawWidgetIcon(ctx, state.position, titleSize, iconCodepoint,
                       panelStyle.headerText.color, titleIconSize,
                       panelStyle.padding.x * 0.5f, titleIconGap);
      }
      ctx->renderer.DrawText(Vec2(titlePadStart.x + titleIconSlot, titlePadStart.y),
                             id, panelStyle.headerText.color,
                             panelStyle.headerText.fontSize);
    }
    
    // Botón de minimizar/restaurar.
    bool bh = PointInRect(mousePos, minimizeButtonPos, minimizeButtonSize);
    Color bc = (bh && leftDown) ? panelStyle.titleButton.pressed : (bh ? panelStyle.titleButton.hover : panelStyle.titleButton.normal);
    ctx->renderer.DrawRectFilled(minimizeButtonPos, minimizeButtonSize, bc, panelStyle.cornerRadius * 0.4f);
    {
      // El glifo + / - se dibuja con primitivas (barras) en lugar de un glifo
      // Lucide: el trazo de "minus" es tan fino que, reescalado al tamaño del
      // botón, queda casi invisible. La geometría garantiza visibilidad
      // independientemente del atlas de iconos (mismo criterio que el TreeView).
      Color glyphColor(1.0f, 1.0f, 1.0f, 0.9f);
      float cx = minimizeButtonPos.x + minimizeButtonSize.x * 0.5f;
      float cy = minimizeButtonPos.y + minimizeButtonSize.y * 0.5f;
      float halfLen = std::round(minimizeButtonSize.x * 0.22f); // medio largo de la barra
      float thick = std::max(2.0f, std::round(minimizeButtonSize.x * 0.10f));
      // Barra horizontal (presente en + y en -)
      ctx->renderer.DrawRectFilled(Vec2(cx - halfLen, cy - thick * 0.5f),
                                   Vec2(halfLen * 2.0f, thick), glyphColor, 0.0f);
      // Barra vertical solo cuando está minimizado (icono "+" para restaurar)
      if (state.minimized) {
        ctx->renderer.DrawRectFilled(Vec2(cx - thick * 0.5f, cy - halfLen),
                                     Vec2(thick, halfLen * 2.0f), glyphColor, 0.0f);
      }
    }
  }

  // Preparar contexto para hijos
  PanelFrameContext frameCtx{};
  frameCtx.id = panelId;
  frameCtx.titleHeight = titleHeight;
  frameCtx.isVisible = isVisible;
  frameCtx.userHeight = desiredSize.y;
  frameCtx.maxHeight = maxHeight;
  frameCtx.reserveLayout = state.useAbsolutePos ? false : reserveLayoutSpace;
  frameCtx.savedCursor = ctx->cursorPos;
  frameCtx.savedLastItemPos = ctx->lastItemPos;
  frameCtx.savedLastItemSize = ctx->lastItemSize;
  frameCtx.parentAvailable = GetCurrentAvailableSpace(ctx);

  bool shouldRenderContent = !state.minimized && isVisible;
  if (shouldRenderContent) {
    Vec2 contentOrigin = state.position + Vec2(panelStyle.padding.x, titleHeight + panelStyle.padding.y);
    float cw = std::max(0.0f, state.size.x - panelStyle.padding.x * 2.0f);
    float ch = std::max(0.0f, state.size.y - titleHeight - panelStyle.padding.y * 2.0f);
    
    // Manejar scroll interno si el contenido es mayor que el panel
    // No interceptar scroll si hay un context menu activo (el menú tiene prioridad)
    if (state.contentSize.y > ch && IsMouseOver(ctx, contentOrigin, Vec2(cw, ch)) && !ctx->scrollConsumedThisFrame && ctx->activeContextMenuId == 0) {
        float wy = ctx->input.MouseWheelY();
        if (std::abs(wy) > 0.001f) {
            float maxScroll = std::max(0.0f, state.contentSize.y - ch);
            float oldScroll = state.scrollOffset.y;
            state.scrollOffset.y = std::clamp(state.scrollOffset.y - wy * S(SCROLL_SPEED), 0.0f, maxScroll);
            // Only consume if we actually scrolled — propagate to parent when at limits
            if (std::abs(state.scrollOffset.y - oldScroll) > 0.001f) {
                ctx->scrollConsumedThisFrame = true;
            }
        }
    }

    // Extend the content clip slightly on left/top/bottom so widget edges aren't
    // shaved by the clip boundary (left: focus rings/borders; bottom: the last
    // item's rounded corner / shadow when the panel auto-grows to fit exactly).
    // The gutter lands inside the panel's padding (12px), so it never bleeds out.
    // The right edge is left untouched so content keeps clearing the scrollbar.
    float gutter = S(CONTENT_LEFT_GUTTER);
    ctx->renderer.PushClipRect(Vec2(contentOrigin.x - gutter, contentOrigin.y - gutter),
                               Vec2(cw + gutter, ch + gutter * 2.0f));
    frameCtx.clipPushed = true;

    // Aplicar el scroll offset al cursor del contenido
    ctx->cursorPos = contentOrigin - Vec2(0, state.scrollOffset.y);
    ctx->offsetStack.push_back(contentOrigin);
    // Reserve room on the right so trailing content clears the scrollbar (drawn
    // 6px wide at the content's right edge in EndPanel when content overflows).
    float sbReserve = S(6.0f) + S(SCROLLBAR_GUTTER);
    BeginVertical(ctx->style.spacing, Vec2(std::max(0.0f, cw - sbReserve), 0.0f), Vec2(0,0));
    frameCtx.layoutPushed = true;
    ctx->panelStack.push_back(frameCtx);
    // brief 21: scope child widgets by the panel id so identical labels in
    // different panels get distinct ids (no "##" suffix needed). Pushed only on
    // the content-rendering path (BeginPanel returned true → EndPanel will run).
    PushID(id.c_str());
  } else if (!state.useAbsolutePos && reserveLayoutSpace) {
    AdvanceCursor(ctx, state.minimized ? Vec2(state.size.x, titleHeight) : state.size);
  }

  return shouldRenderContent;
}

void EndPanel() {
  UIContext *ctx = GetContext();
  if (!ctx || ctx->panelStack.empty()) return;

  // Pop from widget tree (Phase 1)
  ctx->widgetTree.PopParent();

  PanelFrameContext frameCtx = ctx->panelStack.back();
  ctx->panelStack.pop_back();
  // brief 21: pop the scope pushed in BeginPanel (paired with panelStack).
  PopID();
  auto it = ctx->panelStates.find(frameCtx.id);
  if (it == ctx->panelStates.end()) return;
  auto &state = it->second;

  if (!ctx->offsetStack.empty()) ctx->offsetStack.pop_back();
  if (frameCtx.layoutPushed) {
    // Measure content height from cursor displacement (most accurate for dynamic content)
    auto &stack = ctx->layoutStack.back();
    float cursorContentH = stack.cursor.y - stack.contentStart.y;
    if (stack.itemCount > 0 && stack.spacing > 0.0f)
      cursorContentH -= stack.spacing; // remove trailing spacing
    EndVertical(false);
    // Use cursor-based measurement — it tracks actual content each frame,
    // properly handling dynamic content (widgets appearing/disappearing)
    state.contentSize = ctx->lastItemSize;
    if (cursorContentH > state.contentSize.y)
      state.contentSize.y = cursorContentH;
  }
  if (frameCtx.clipPushed) ctx->renderer.PopClipRect();

  // AJUSTE DINÁMICO DE TAMAÑO O SCROLLBAR
  float verticalPadding = ctx->style.panel.padding.y * 2.0f;
  float requiredHeight = frameCtx.titleHeight + state.contentSize.y + verticalPadding;

  if (!state.resizing) {
      if (state.hasBeenManuallyResized) {
          // El usuario redimensionó manualmente: respetar ese tamaño
          state.size.y = state.manualSize.y;
      } else {
          // Auto-ajustar al contenido, pero respetar límites del contenedor padre
          float newHeight = requiredHeight;
          if (frameCtx.userHeight > 0.0f) {
              newHeight = std::max(newHeight, frameCtx.userHeight);
          }

          // NOTE: previously clamped newHeight to parentAvailable.y here, but that
          // clipped panel content inconsistently: an early panel (while the parent
          // still had space, parentAvailable.y > 0) got capped and lost content,
          // while a later panel (parent space exhausted to 0, so the >0 guard
          // skipped the clamp) grew freely. For "auto-size to content" a panel must
          // always fit its content; bound a panel with an explicit maxHeight (which
          // enables internal scrolling) instead of the parent's remaining space.

          if (frameCtx.maxHeight > 0.0f) {
              // Ensure maxHeight can at least fit title + minimal content
              float minUsable = frameCtx.titleHeight + verticalPadding + state.contentSize.y * 0.5f;
              float effectiveMax = std::max(frameCtx.maxHeight, minUsable);
              newHeight = std::min(newHeight, effectiveMax);
          }
          state.size.y = newHeight;
      }
  }

  // Asegurar scroll offset válido
  float availableHeight = state.size.y - (frameCtx.titleHeight + verticalPadding);
  float maxScroll = std::max(0.0f, state.contentSize.y - availableHeight);
  state.scrollOffset.y = std::clamp(state.scrollOffset.y, 0.0f, maxScroll);

  // Dibujar scrollbar si el contenido excede el área visible del panel. Esto solo
  // ocurre cuando el panel NO puede crecer para contenerlo (maxHeight fijado o
  // redimensionado manual): el pre-grow de BeginPanel cubre el caso auto. Umbral
  // de 1px para no dejar contenido recortado sin scrollbar.
  if (frameCtx.isVisible && !state.minimized && maxScroll > 1.0f) {
      Vec2 contentAreaPos = state.position + Vec2(ctx->style.panel.padding.x, frameCtx.titleHeight + ctx->style.panel.padding.y);
      float cw = std::max(0.0f, state.size.x - ctx->style.panel.padding.x * 2.0f);
      Vec2 barPos(contentAreaPos.x + cw - 6.0f, contentAreaPos.y);
      Vec2 barSize(6.0f, availableHeight);

      DrawScrollbar(ctx, barPos, barSize, state.contentSize.y, availableHeight, state.scrollOffset.y,
                    state.draggingScrollbar, state.dragStartMouse, state.dragStartScroll, state.draggingScrollbar, true);
  }

  if (!frameCtx.reserveLayout || state.useAbsolutePos) {
    ctx->cursorPos = frameCtx.savedCursor;
    ctx->lastItemPos = frameCtx.savedLastItemPos;
    ctx->lastItemSize = frameCtx.savedLastItemSize;
  }

  if (frameCtx.reserveLayout && !state.useAbsolutePos) {
    ctx->lastItemPos = state.position;
    AdvanceCursor(ctx, state.minimized ? Vec2(state.size.x, frameCtx.titleHeight) : state.size);
  }
}

bool CollapsingHeader(const std::string &label, bool *open,
                      uint32_t iconCodepoint, std::optional<Vec2> pos) {
  UIContext *ctx = GetContext();
  if (!ctx) return false;

  uint32_t id = GenerateId("COLLAPSE:", label.c_str());
  ctx->focusableWidgets.push_back(id);

  auto entry = ctx->boolStates.try_emplace(id, false);
  bool &storedOpen = entry.first->second;
  bool isOpen = open ? *open : storedOpen;

  const TextStyle &labelStyle = ctx->style.GetTextStyle(TypographyStyle::BodyStrong);
  const PanelStyle &panelStyle = ctx->style.panel;
  const ColorState &accentState = ctx->style.button.background;

  const float headerHeight = labelStyle.fontSize + 16.0f;
  const float chevronSize = labelStyle.fontSize;
  const float iconGap = 6.0f;
  const float padX = panelStyle.padding.x * 0.5f;
  const float indentAmount = 16.0f;

  // Reset any indent left by a previous CollapsingHeader at this layout depth so
  // this header itself sits flush at the parent's left edge. Restoring the
  // available width keeps Fill widgets sized against the full parent.
  bool inVerticalLayout = !ctx->layoutStack.empty() && ctx->layoutStack.back().isVertical;
  if (inVerticalLayout) {
    LayoutStack &stack = ctx->layoutStack.back();
    if (stack.collapseIndent != 0.0f) {
      stack.availableSpace.x += stack.collapseIndent;
      stack.collapseIndent = 0.0f;
      ctx->cursorPos.x = stack.contentStart.x;
    }
  }

  Vec2 desiredSize(0.0f, headerHeight);
  LayoutConstraints constraints = ConsumeNextConstraints(SizeConstraint::Fill);
  Vec2 finalSize = ApplyConstraints(ctx, constraints, desiredSize);
  if (finalSize.x <= 0.0f) {
    Vec2 avail = GetCurrentAvailableSpace(ctx);
    finalSize.x = avail.x > 0.0f ? avail.x : 200.0f;
  }
  if (finalSize.y <= 0.0f) finalSize.y = headerHeight;

  Vec2 widgetPos;
  if (pos.has_value()) {
    widgetPos = ResolveAbsolutePosition(ctx, pos.value(), finalSize);
  } else {
    widgetPos = ctx->cursorPos;
  }

  Vec2 mousePos(ctx->input.MouseX(), ctx->input.MouseY());
  bool hover = PointInRect(mousePos, widgetPos, finalSize) &&
               IsRectInViewport(ctx, widgetPos, finalSize) &&
               !IsMouseInputBlocked(ctx);
  bool leftDown = ctx->input.IsMouseDown(0);
  bool clicked = hover && ctx->input.IsMousePressed(0);

  // Keyboard activation when focused.
  if (ctx->focusedWidgetId == id &&
      (ctx->input.IsKeyPressed(UIKey::Space) ||
       ctx->input.IsKeyPressed(UIKey::Enter))) {
    clicked = true;
  }

  if (clicked) {
    isOpen = !isOpen;
    storedOpen = isOpen;
    if (open) *open = isOpen;
  }

  if (hover) ctx->desiredCursor = UIContext::CursorType::Hand;

  // Background — subtle row that highlights on hover/press.
  Color bg = panelStyle.headerBackground;
  if (hover && leftDown) {
    bg = accentState.pressed; bg.a = 0.35f;
  } else if (hover) {
    bg = accentState.hover; bg.a = 0.18f;
  }
  ctx->renderer.DrawRectFilled(widgetPos, finalSize, bg, panelStyle.cornerRadius);

  if (ctx->focusedWidgetId == id) {
    DrawFocusRing(ctx, widgetPos, finalSize, panelStyle.cornerRadius);
  }

  // Chevron at left. brief 18.5: in RTL a collapsed header points left.
  uint32_t chevronCp = isOpen ? Icons::ChevronDown
                              : MirrorDirectionalIcon(ctx, Icons::ChevronRight);
  DrawWidgetIcon(ctx, widgetPos, finalSize, chevronCp,
                 labelStyle.color, chevronSize, padX, iconGap);

  float labelStartX = widgetPos.x + padX + chevronSize + iconGap;

  if (iconCodepoint != 0u) {
    DrawWidgetIcon(ctx, Vec2(labelStartX, widgetPos.y),
                   Vec2(labelStyle.fontSize, finalSize.y), iconCodepoint,
                   labelStyle.color, labelStyle.fontSize, 0.0f, iconGap);
    labelStartX += labelStyle.fontSize + iconGap;
  }

  // Label, vertically centered using the same optical-center formula as ComboBox.
  const float ascender = ctx->renderer.GetFontAscender();
  const float capHeight = 0.7f;
  const float visualCenterOffset = (ascender - capHeight * 0.5f) * labelStyle.fontSize;
  Vec2 labelPos(labelStartX, widgetPos.y + finalSize.y * 0.5f - visualCenterOffset);
  ctx->renderer.DrawText(labelPos, label, labelStyle.color, labelStyle.fontSize);

  ctx->lastItemPos = widgetPos;
  AdvanceCursor(ctx, finalSize);

  // Push indent for subsequent siblings if expanded; otherwise leave at 0.
  if (inVerticalLayout) {
    LayoutStack &stack = ctx->layoutStack.back();
    stack.collapseIndent = isOpen ? indentAmount : 0.0f;
    if (stack.collapseIndent != 0.0f) {
      stack.availableSpace.x = std::max(0.0f, stack.availableSpace.x - stack.collapseIndent);
    }
    // Apply the new indent to the cursor immediately so the very next widget
    // picks it up without needing another AdvanceCursor cycle.
    ctx->cursorPos.x = stack.contentStart.x + stack.collapseIndent;
  }

  SetLastItem(id, widgetPos, widgetPos + finalSize, hover, false,
              ctx->focusedWidgetId == id, clicked);
  return isOpen;
}

bool BeginScrollView(const std::string &id, const Vec2 &size,
                     Vec2 *scrollOffset, std::optional<Vec2> pos) {
  UIContext *ctx = GetContext();
  if (!ctx)
    return false;

  uint32_t scrollViewId = GenerateId("SCROLLVIEW:", id.c_str());

  // Register in widget tree (Phase 1)
  auto* scrollNode = static_cast<ScrollViewNode*>(
      ctx->widgetTree.FindOrCreate(scrollViewId, ctx->frame, [&]() {
          auto node = std::make_unique<ScrollViewNode>(scrollViewId);
          node->debugName = id;
          return node;
      })
  );
  ctx->widgetTree.PushParent(scrollNode);

  auto &state = ctx->scrollViewStates[scrollViewId];

  // Resolve {0,0} size from available space (like Splitter does)
  Vec2 resolvedSize = size;
  if (resolvedSize.x <= 0.0f) {
    Vec2 avail = GetCurrentAvailableSpace(ctx);
    resolvedSize.x = avail.x > 0.0f ? avail.x : 400.0f;
  }
  if (resolvedSize.y <= 0.0f) {
    Vec2 avail = GetCurrentAvailableSpace(ctx);
    resolvedSize.y = avail.y > 0.0f ? avail.y : 300.0f;
  }

  if (!state.initialized) {
    state.scrollOffset = Vec2(0.0f, 0.0f);
    state.contentSize = Vec2(0.0f, 0.0f);
    state.viewSize = resolvedSize;
    state.initialized = true;
  } else {
    state.viewSize = resolvedSize;
  }

  // Auto-grow: if the resolved size is too small for the content,
  // expand to at least half the content size (with scrollbar for the rest)
  if (state.contentSize.y > 0.0f && resolvedSize.y < state.contentSize.y) {
    float minHeight = state.contentSize.y * 0.5f;
    if (resolvedSize.y < minHeight) {
      Vec2 avail = GetCurrentAvailableSpace(ctx);
      float maxGrow = avail.y > 0.0f ? avail.y : resolvedSize.y;
      resolvedSize.y = std::min(minHeight, maxGrow);
    }
  }

  if (scrollOffset) state.scrollOffset = *scrollOffset;

  float scrollbarWidth = S(SCROLLBAR_WIDTH_LARGE);
  Vec2 scrollViewPos;
  if (pos.has_value()) {
    scrollViewPos = ResolveAbsolutePosition(ctx, pos.value(), resolvedSize);
    state.useAbsolutePos = true;
    state.absolutePos = scrollViewPos;
  } else {
    scrollViewPos = ctx->cursorPos;
    state.useAbsolutePos = false;
  }
  state.position = scrollViewPos;

  const PanelStyle &panelStyle = ctx->GetEffectivePanelStyle();
  Color scrollBg = AdjustContainerBackground(panelStyle.background, ctx->style.isDarkTheme);
  ctx->renderer.DrawRectFilled(scrollViewPos, resolvedSize, scrollBg, panelStyle.cornerRadius);

  Vec2 availableSize(resolvedSize.x - scrollbarWidth, resolvedSize.y - scrollbarWidth);
  Vec2 contentAreaPos(scrollViewPos.x + panelStyle.padding.x, scrollViewPos.y + panelStyle.padding.y);
  Vec2 contentAreaSize(availableSize.x - panelStyle.padding.x * 2.0f, availableSize.y - panelStyle.padding.y * 2.0f);

  if (IsMouseOver(ctx, contentAreaPos, contentAreaSize) && !ctx->scrollConsumedThisFrame) {
    float wheelY = ctx->input.MouseWheelY();
    if (std::abs(wheelY) > 0.001f) {
      float maxScroll = std::max(0.0f, state.contentSize.y - contentAreaSize.y);
      float oldScroll = state.scrollOffset.y;
      state.scrollOffset.y = std::clamp(state.scrollOffset.y - wheelY * S(SCROLL_SPEED), 0.0f, maxScroll);
      if (std::abs(state.scrollOffset.y - oldScroll) > 0.001f) {
        ctx->scrollConsumedThisFrame = true;
      }
    }
  }

  // Extend the content clip slightly to the left so left-aligned widgets aren't
  // shaved by the clip edge (the gutter lands inside padding.x). The right edge
  // already clears the scrollbar via padding.x + scrollbarWidth.
  float leftGutter = S(CONTENT_LEFT_GUTTER);
  ctx->renderer.PushClipRect(Vec2(contentAreaPos.x - leftGutter, contentAreaPos.y),
                             Vec2(contentAreaSize.x + leftGutter, contentAreaSize.y));
  ScrollViewFrameContext frameCtx{};
  frameCtx.id = scrollViewId;
  frameCtx.position = scrollViewPos;
  frameCtx.size = resolvedSize;
  frameCtx.contentAreaPos = contentAreaPos;
  frameCtx.contentAreaSize = contentAreaSize;
  frameCtx.availableSize = availableSize;
  frameCtx.scrollbarWidth = scrollbarWidth;
  frameCtx.layoutPushed = true;
  frameCtx.useAbsolutePos = state.useAbsolutePos;
  frameCtx.savedCursor = ctx->cursorPos;
  frameCtx.savedLastItemPos = ctx->lastItemPos;
  frameCtx.savedLastItemSize = ctx->lastItemSize;
  ctx->scrollViewStack.push_back(frameCtx);

  ctx->cursorPos = contentAreaPos - state.scrollOffset;
  ctx->offsetStack.push_back(contentAreaPos);
  BeginVertical(ctx->style.spacing, Vec2(contentAreaSize.x, 0.0f), Vec2(0.0f, 0.0f));

  if (scrollOffset) *scrollOffset = state.scrollOffset;
  return true;
}

void EndScrollView() {
  UIContext *ctx = GetContext();
  if (!ctx || ctx->scrollViewStack.empty()) return;

  // Pop from widget tree (Phase 1)
  ctx->widgetTree.PopParent();

  ScrollViewFrameContext frameCtx = ctx->scrollViewStack.back();
  ctx->scrollViewStack.pop_back();
  auto it = ctx->scrollViewStates.find(frameCtx.id);
  if (it == ctx->scrollViewStates.end()) return;
  auto &state = it->second;

  if (!ctx->layoutStack.empty()) {
    auto &stack = ctx->layoutStack.back();
    float spacingTotal =
        (stack.itemCount > 0 && stack.spacing > 0.0f)
            ? stack.spacing * static_cast<float>(stack.itemCount - 1)
            : 0.0f;
    float cursorContentH = stack.cursor.y - stack.contentStart.y;
    if (stack.itemCount > 0 && stack.spacing > 0.0f)
      cursorContentH -= stack.spacing;
    float formulaH = stack.contentSize.y + spacingTotal;
    state.contentSize = Vec2(stack.contentSize.x,
                             std::max(formulaH, cursorContentH));
    EndVertical(false);
  }
  if (!ctx->offsetStack.empty()) ctx->offsetStack.pop_back();
  ctx->renderer.PopClipRect();

  ctx->cursorPos = frameCtx.savedCursor;
  ctx->lastItemPos = frameCtx.savedLastItemPos;
  ctx->lastItemSize = frameCtx.savedLastItemSize;

  float maxScrollY = std::max(0.0f, state.contentSize.y - frameCtx.contentAreaSize.y);
  state.scrollOffset.y = std::clamp(state.scrollOffset.y, 0.0f, maxScrollY);

  if (state.contentSize.y > frameCtx.contentAreaSize.y) {
    Vec2 barPos(frameCtx.position.x + frameCtx.size.x - frameCtx.scrollbarWidth, frameCtx.position.y);
    Vec2 barSize(frameCtx.scrollbarWidth, frameCtx.size.y);
    DrawScrollbar(ctx, barPos, barSize, state.contentSize.y, frameCtx.contentAreaSize.y, state.scrollOffset.y, state.draggingScrollbar, state.dragStartMouse, state.dragStartScroll, state.draggingScrollbar, true);
  }

  if (!frameCtx.useAbsolutePos) {
    ctx->lastItemPos = frameCtx.position;
    AdvanceCursor(ctx, frameCtx.size);
  }
}

// Internal: shared core that supports optional per-tab icon codepoints.
static bool BeginTabViewImpl(const std::string &id, int *activeTab,
                             const std::vector<std::string> &tabLabels,
                             const std::vector<uint32_t> *tabIcons,
                             const Vec2 &size, std::optional<Vec2> pos);

bool BeginTabView(const std::string &id, int *activeTab,
                  const std::vector<std::string> &tabLabels, const Vec2 &size,
                  std::optional<Vec2> pos) {
  return BeginTabViewImpl(id, activeTab, tabLabels, nullptr, size, pos);
}

bool BeginTabView(const std::string &id, int *activeTab,
                  const std::vector<std::pair<std::string, uint32_t>> &tabLabels,
                  const Vec2 &size, std::optional<Vec2> pos) {
  std::vector<std::string> labels;
  std::vector<uint32_t> icons;
  labels.reserve(tabLabels.size());
  icons.reserve(tabLabels.size());
  for (const auto &p : tabLabels) {
    labels.push_back(p.first);
    icons.push_back(p.second);
  }
  return BeginTabViewImpl(id, activeTab, labels, &icons, size, pos);
}

static bool BeginTabViewImpl(const std::string &id, int *activeTab,
                             const std::vector<std::string> &tabLabels,
                             const std::vector<uint32_t> *tabIcons,
                             const Vec2 &size, std::optional<Vec2> pos) {
  UIContext *ctx = GetContext();
  if (!ctx || tabLabels.empty()) return false;

  uint32_t tabViewId = GenerateId("TABVIEW:", id.c_str());

  // Register in widget tree (Phase 1)
  auto* tabNode = static_cast<TabViewNode*>(
      ctx->widgetTree.FindOrCreate(tabViewId, ctx->frame, [&]() {
          auto node = std::make_unique<TabViewNode>(tabViewId);
          node->debugName = id;
          return node;
      })
  );
  ctx->widgetTree.PushParent(tabNode);

  auto &state = ctx->tabViewStates[tabViewId];

  if (!state.initialized) {
    state.activeTab = activeTab ? *activeTab : 0;
    state.initialized = true;
  } else if (activeTab) state.activeTab = *activeTab;

  const PanelStyle &panelStyle = ctx->style.panel;
  const TextStyle &tabTextStyle = ctx->style.GetTextStyle(TypographyStyle::Body);
  float tabHeight = tabTextStyle.fontSize + panelStyle.padding.y * 2.0f;
  
  Vec2 tabViewSize = size;
  if (tabViewSize.x <= 0.0f || tabViewSize.y <= 0.0f) {
    Vec2 avail = GetCurrentAvailableSpace(ctx);
    if (tabViewSize.x <= 0.0f) tabViewSize.x = avail.x > 0.0f ? avail.x : 400.0f;
    if (tabViewSize.y <= 0.0f) tabViewSize.y = avail.y > 0.0f ? avail.y : 300.0f;
  }

  // Auto-grow: if the tab content from the previous frame exceeds the view,
  // grow to fit the full content while there is room in the parent. The
  // requested height (size.y) is treated as a *minimum* and the parent's
  // remaining space as the *maximum*; if the parent can't fit everything, the
  // view stays at maxGrow and the scrollbar (EndTabView) handles the overflow.
  // Previously this only grew on severe overflow (>2x), so a mild overflow
  // clipped the lower content even when the parent had plenty of free space.
  if (state.contentSize.y > 0.0f) {
    float contentAreaHeight = tabViewSize.y - tabHeight - 16.0f; // matches contentSize calc in EndTabView
    if (contentAreaHeight < state.contentSize.y) {
      Vec2 avail = GetCurrentAvailableSpace(ctx);
      float maxGrow = avail.y > 0.0f ? avail.y : tabViewSize.y;
      float desiredTotal = state.contentSize.y + tabHeight + 16.0f;
      tabViewSize.y = std::min(std::max(tabViewSize.y, desiredTotal), maxGrow);
    }
  }

  Vec2 tabViewPos;
  if (pos.has_value()) {
    tabViewPos = ResolveAbsolutePosition(ctx, pos.value(), tabViewSize);
    state.useAbsolutePos = true;
  } else {
    tabViewPos = ctx->cursorPos;
    state.useAbsolutePos = false;
  }

  state.tabBarSize = Vec2(tabViewSize.x, tabHeight);
  Color tabViewBg = AdjustContainerBackground(panelStyle.background, ctx->style.isDarkTheme);
  ctx->renderer.DrawRectFilled(tabViewPos, tabViewSize, tabViewBg, panelStyle.cornerRadius);

  // Calculate total width of all tabs (including optional leading icons).
  float totalTabsWidth = 0.0f;
  float tabIconSize = tabTextStyle.fontSize;
  float tabIconGap = 6.0f;
  std::vector<float> tabWidths(tabLabels.size());
  for (size_t i = 0; i < tabLabels.size(); ++i) {
    Vec2 labelSize = MeasureTextCached(ctx, tabLabels[i], tabTextStyle.fontSize);
    uint32_t cp = (tabIcons && i < tabIcons->size()) ? (*tabIcons)[i] : 0u;
    float iconSlot = (cp != 0u) ? (tabIconSize + tabIconGap) : 0.0f;
    tabWidths[i] = labelSize.x + iconSlot + panelStyle.padding.x * 2.0f;
    totalTabsWidth += tabWidths[i] + (i > 0 ? 4.0f : 0.0f);
  }
  state.totalTabsWidth = totalTabsWidth;

  float availableBarWidth = tabViewSize.x - panelStyle.padding.x * 2.0f;
  bool needsScroll = totalTabsWidth > availableBarWidth;
  constexpr float arrowBtnWidth = 24.0f;

  // Reserve space for arrow buttons when scrolling is needed
  float tabAreaStartX = tabViewPos.x + panelStyle.padding.x;
  float tabAreaWidth = availableBarWidth;
  if (needsScroll) {
    tabAreaStartX += arrowBtnWidth;
    tabAreaWidth -= arrowBtnWidth * 2.0f;
    float maxScroll = std::max(0.0f, totalTabsWidth - tabAreaWidth);
    state.tabBarScrollX = std::clamp(state.tabBarScrollX, 0.0f, maxScroll);

    // Left arrow button
    Vec2 leftBtnPos(tabViewPos.x + panelStyle.padding.x, tabViewPos.y);
    Vec2 leftBtnSize(arrowBtnWidth, tabHeight);
    bool leftHover = IsMouseOver(ctx, leftBtnPos, leftBtnSize);
    bool canScrollLeft = state.tabBarScrollX > 0.0f;
    Color arrowColor = canScrollLeft ? (leftHover ? tabTextStyle.color : Color(0.7f, 0.7f, 0.7f, 1.0f)) : Color(0.4f, 0.4f, 0.4f, 1.0f);
    if (leftHover && ctx->input.IsMousePressed(0) && canScrollLeft)
      state.tabBarScrollX = std::max(0.0f, state.tabBarScrollX - 80.0f);
    // Draw left chevron (<)
    float cx = leftBtnPos.x + arrowBtnWidth * 0.5f;
    float cy = leftBtnPos.y + tabHeight * 0.5f;
    float hs = tabHeight * 0.2f;
    ctx->renderer.DrawLine(Vec2(cx + hs * 0.3f, cy - hs), Vec2(cx - hs * 0.3f, cy), arrowColor, 2.0f);
    ctx->renderer.DrawLine(Vec2(cx - hs * 0.3f, cy), Vec2(cx + hs * 0.3f, cy + hs), arrowColor, 2.0f);

    // Right arrow button
    Vec2 rightBtnPos(tabViewPos.x + panelStyle.padding.x + arrowBtnWidth + tabAreaWidth, tabViewPos.y);
    Vec2 rightBtnSize(arrowBtnWidth, tabHeight);
    bool rightHover = IsMouseOver(ctx, rightBtnPos, rightBtnSize);
    bool canScrollRight = state.tabBarScrollX < maxScroll;
    arrowColor = canScrollRight ? (rightHover ? tabTextStyle.color : Color(0.7f, 0.7f, 0.7f, 1.0f)) : Color(0.4f, 0.4f, 0.4f, 1.0f);
    if (rightHover && ctx->input.IsMousePressed(0) && canScrollRight)
      state.tabBarScrollX = std::min(maxScroll, state.tabBarScrollX + 80.0f);
    // Draw right chevron (>)
    cx = rightBtnPos.x + arrowBtnWidth * 0.5f;
    cy = rightBtnPos.y + tabHeight * 0.5f;
    ctx->renderer.DrawLine(Vec2(cx - hs * 0.3f, cy - hs), Vec2(cx + hs * 0.3f, cy), arrowColor, 2.0f);
    ctx->renderer.DrawLine(Vec2(cx + hs * 0.3f, cy), Vec2(cx - hs * 0.3f, cy + hs), arrowColor, 2.0f);
  } else {
    state.tabBarScrollX = 0.0f;
  }

  // Clip the tab bar area so tabs don't overflow
  if (needsScroll)
    ctx->renderer.PushClipRect(Vec2(tabAreaStartX, tabViewPos.y), Vec2(tabAreaWidth, tabHeight));

  float currentX = tabAreaStartX - state.tabBarScrollX;
  for (size_t i = 0; i < tabLabels.size(); ++i) {
    Vec2 labelSize = MeasureTextCached(ctx, tabLabels[i], tabTextStyle.fontSize);
    Vec2 tSize(tabWidths[i], tabHeight);
    Vec2 tPos(currentX, tabViewPos.y);
    bool visible = (tPos.x + tSize.x > tabAreaStartX) && (tPos.x < tabAreaStartX + tabAreaWidth);
    bool hover = visible && IsMouseOver(ctx, tPos, tSize);
    bool active = (int)i == state.activeTab;
    if (active) {
        ctx->renderer.DrawRectFilled(tPos, tSize, tabViewBg, 0);
        ctx->renderer.DrawLine(tPos + Vec2(0, tSize.y - 2), tPos + Vec2(tSize.x, tSize.y - 2), ctx->style.button.background.normal, 3.0f);
    }
    if (hover && ctx->input.IsMousePressed(0)) {
        state.activeTab = (int)i;
        if (activeTab) *activeTab = (int)i;
    }
    Color tabColor = active ? tabTextStyle.color : Color(0.7f, 0.7f, 0.7f, 1.0f);
    uint32_t tabCp = (tabIcons && i < tabIcons->size()) ? (*tabIcons)[i] : 0u;
    float tabIconSlot = (tabCp != 0u) ? (tabIconSize + tabIconGap) : 0.0f;
    if (tabCp != 0u) {
      DrawWidgetIcon(ctx, tPos, tSize, tabCp, tabColor, tabIconSize,
                     panelStyle.padding.x, tabIconGap);
    }
    ctx->renderer.DrawText(tPos + Vec2(panelStyle.padding.x + tabIconSlot, (tabHeight - labelSize.y) * 0.5f),
                           tabLabels[i], tabColor, tabTextStyle.fontSize);
    currentX += tSize.x + 4.0f;
  }

  if (needsScroll)
    ctx->renderer.PopClipRect();

  Vec2 contentPos = tabViewPos + Vec2(8, tabHeight + 8);
  Vec2 contentSize = tabViewSize - Vec2(16, tabHeight + 16);
  // Extend the content clip slightly to the left so left-aligned widgets aren't
  // shaved by the clip edge (the gutter lands inside the 8px content inset).
  float leftGutter = S(CONTENT_LEFT_GUTTER);
  ctx->renderer.PushClipRect(Vec2(contentPos.x - leftGutter, contentPos.y),
                             Vec2(contentSize.x + leftGutter, contentSize.y));

  // Reserve room on the right so trailing content (slider values, etc.) clears
  // the scrollbar (drawn 6px wide at the content's right edge in EndTabView).
  float sbReserve = S(6.0f) + S(SCROLLBAR_GUTTER);
  Vec2 layoutSize(std::max(0.0f, contentSize.x - sbReserve), contentSize.y);

  Vec2& scroll = state.tabScrollOffsets[state.activeTab];
  ctx->cursorPos = contentPos - Vec2(0, scroll.y);
  ctx->offsetStack.push_back(contentPos);
  BeginVertical(ctx->style.spacing, layoutSize, Vec2(0,0));
  
  ctx->tabFrameStack.push_back({tabViewId, contentPos, contentSize, ctx->cursorPos});
  // brief 21: scope tab content by tabview id AND the active tab index, so the
  // same label used on two different tabs gets distinct ids. Two pushes → two
  // pops in EndTabView (paired with tabFrameStack).
  PushID(id.c_str());
  PushID(state.activeTab);
  return true;
}

void EndTabView() {
  UIContext *ctx = GetContext();
  if (!ctx || ctx->tabFrameStack.empty()) return;

  // brief 21: pop the (id, activeTab) scope pushed in BeginTabViewImpl.
  PopID();
  PopID();

  // Pop from widget tree (Phase 1)
  ctx->widgetTree.PopParent();

  TabContentFrame frame = ctx->tabFrameStack.back();
  ctx->tabFrameStack.pop_back();
  Vec2 endCursor = ctx->cursorPos;
  EndVertical(false);
  if (!ctx->offsetStack.empty()) ctx->offsetStack.pop_back();

  // Measure content BEFORE popping clip rect
  auto it = ctx->tabViewStates.find(frame.tabViewId);
  if (it != ctx->tabViewStates.end()) {
    auto &st = it->second;
    float measuredH = endCursor.y - frame.contentStartCursor.y;
    st.contentSize = Vec2(frame.contentAreaSize.x, std::max(frame.contentAreaSize.y, measuredH));
  }

  ctx->renderer.PopClipRect();

  // Draw scrollbar AFTER popping the content clip rect.
  // Position at the right edge of the parent's visible area (clip rect),
  // not the TabView's content area — this ensures the scrollbar is always
  // visible even when the TabView is wider than its parent container.
  if (it != ctx->tabViewStates.end()) {
    auto &st = it->second;
    if (st.contentSize.y > frame.contentAreaSize.y) {
      float sbWidth = S(6.0f);
      // Use parent clip's right edge if available, otherwise fall back to content area
      float barX = frame.contentAreaPos.x + frame.contentAreaSize.x - sbWidth;
      const auto& clips = ctx->renderer.GetClipStack();
      if (!clips.empty()) {
        float clipRight = static_cast<float>(clips.back().x + clips.back().width);
        barX = clipRight - sbWidth;
      }
      Vec2 barPos(barX, frame.contentAreaPos.y);
      Vec2 barSize(sbWidth, frame.contentAreaSize.y);
      DrawScrollbar(ctx, barPos, barSize, st.contentSize.y, frame.contentAreaSize.y,
                    st.tabScrollOffsets[st.activeTab].y, st.draggingScrollbar,
                    st.dragStartMouse, st.dragStartScroll, st.draggingScrollbar, true);
    }

    if (IsMouseOver(ctx, frame.contentAreaPos, frame.contentAreaSize) && !ctx->scrollConsumedThisFrame) {
      float wy = ctx->input.MouseWheelY();
      if (std::abs(wy) > 0.001f) {
        st.tabScrollOffsets[st.activeTab].y = std::clamp(st.tabScrollOffsets[st.activeTab].y - wy * S(SCROLL_SPEED), 0.0f, std::max(0.0f, st.contentSize.y - frame.contentAreaSize.y));
        ctx->scrollConsumedThisFrame = true;
      }
    }

    if (!st.useAbsolutePos) {
      ctx->lastItemPos = frame.contentAreaPos - Vec2(8, st.tabBarSize.y + 8);
      AdvanceCursor(ctx, Vec2(st.tabBarSize.x, st.tabBarSize.y + frame.contentAreaSize.y + 16));
    }
  }
}

// ============================================================================
// Splitter Widget
// ============================================================================

static constexpr float SPLITTER_DIVIDER_THICKNESS = 6.0f;
static constexpr float SPLITTER_MIN_RATIO = 0.05f;
static constexpr float SPLITTER_MAX_RATIO = 0.95f;
static constexpr float SPLITTER_MIN_PX = 50.0f;

bool BeginSplitter(const std::string& id, bool vertical, float* ratio, const Vec2& size) {
  UIContext* ctx = GetContext();
  if (!ctx) return false;

  uint32_t splitId = GenerateId("SPLITTER:", id.c_str());
  auto& state = ctx->splitterStates[splitId];

  // Initialize ratio from caller pointer, clamp and write back if out of range
  if (ratio) {
    state.ratio = std::clamp(*ratio, SPLITTER_MIN_RATIO, SPLITTER_MAX_RATIO);
    *ratio = state.ratio;
  }

  // Resolve total size — prefer parent splitter region size for nested splitters
  Vec2 totalSize = size;
  if (totalSize.x <= 0.0f || totalSize.y <= 0.0f) {
    Vec2 regionSize(0.0f, 0.0f);
    // If inside a parent splitter region, use that region's exact size
    if (!ctx->splitterStack.empty()) {
      auto& parentFrame = ctx->splitterStack.back();
      if (parentFrame.phase == 0) {
        regionSize = parentFrame.firstRegionSize;
      } else {
        regionSize = parentFrame.secondRegionSize;
      }
    }
    // Fallback to layout available space, then hardcoded defaults
    if (totalSize.x <= 0.0f) {
      if (regionSize.x > 0.0f) {
        totalSize.x = regionSize.x;
      } else {
        Vec2 avail = GetCurrentAvailableSpace(ctx);
        totalSize.x = avail.x > 0.0f ? avail.x : 400.0f;
      }
    }
    if (totalSize.y <= 0.0f) {
      if (regionSize.y > 0.0f) {
        totalSize.y = regionSize.y;
      } else {
        Vec2 avail = GetCurrentAvailableSpace(ctx);
        totalSize.y = avail.y > 0.0f ? avail.y : 300.0f;
      }
    }
  }

  Vec2 splitterPos = ctx->cursorPos;
  float divider = S(SPLITTER_DIVIDER_THICKNESS);

  // Calculate the divider position and two regions
  Vec2 firstPos, firstSize, secondPos, secondSize;
  Vec2 dividerPos, dividerSize;

  if (vertical) {
    // Vertical divider: left | right
    float usable = totalSize.x - divider;
    float firstW = std::max(S(SPLITTER_MIN_PX), usable * state.ratio);
    float secondW = std::max(S(SPLITTER_MIN_PX), usable - firstW);
    // Re-clamp if min constraints ate into space
    if (firstW + secondW > usable) {
      float excess = (firstW + secondW) - usable;
      firstW -= excess * 0.5f;
      secondW -= excess * 0.5f;
    }

    firstPos = splitterPos;
    firstSize = Vec2(firstW, totalSize.y);
    dividerPos = Vec2(splitterPos.x + firstW, splitterPos.y);
    dividerSize = Vec2(divider, totalSize.y);
    secondPos = Vec2(splitterPos.x + firstW + divider, splitterPos.y);
    secondSize = Vec2(secondW, totalSize.y);
  } else {
    // Horizontal divider: top / bottom
    float usable = totalSize.y - divider;
    float firstH = std::max(S(SPLITTER_MIN_PX), usable * state.ratio);
    float secondH = std::max(S(SPLITTER_MIN_PX), usable - firstH);
    if (firstH + secondH > usable) {
      float excess = (firstH + secondH) - usable;
      firstH -= excess * 0.5f;
      secondH -= excess * 0.5f;
    }

    firstPos = splitterPos;
    firstSize = Vec2(totalSize.x, firstH);
    dividerPos = Vec2(splitterPos.x, splitterPos.y + firstH);
    dividerSize = Vec2(totalSize.x, divider);
    secondPos = Vec2(splitterPos.x, splitterPos.y + firstH + divider);
    secondSize = Vec2(totalSize.x, secondH);
  }

  // --- Handle divider interaction ---
  Vec2 mousePos(ctx->input.MouseX(), ctx->input.MouseY());
  bool leftDown = ctx->input.IsMouseDown(0);
  bool leftPressed = ctx->input.IsMousePressed(0);
  bool hoveringDivider = PointInRect(mousePos, dividerPos, dividerSize);

  if (state.isDragging) {
    if (!leftDown) {
      state.isDragging = false;
    } else {
      float usable = vertical ? (totalSize.x - divider) : (totalSize.y - divider);
      float mouseCurrent = vertical ? mousePos.x : mousePos.y;
      float delta = mouseCurrent - state.dragStartMouse;
      float newRatio = state.dragStartRatio + (delta / std::max(usable, 1.0f));
      // Clamp by both ratio and pixel constraints
      float minRatio = S(SPLITTER_MIN_PX) / std::max(usable, 1.0f);
      float maxRatio = 1.0f - minRatio;
      state.ratio = std::clamp(newRatio, std::max(SPLITTER_MIN_RATIO, minRatio),
                                          std::min(SPLITTER_MAX_RATIO, maxRatio));
      if (ratio) *ratio = state.ratio;
    }
  }

  if (!state.isDragging && leftPressed && hoveringDivider) {
    state.isDragging = true;
    state.dragStartMouse = vertical ? mousePos.x : mousePos.y;
    state.dragStartRatio = state.ratio;
  }

  // Set resize cursor
  if (hoveringDivider || state.isDragging) {
    ctx->desiredCursor = vertical ? UIContext::CursorType::ResizeH : UIContext::CursorType::ResizeV;
  }

  // --- Draw divider ---
  const PanelStyle& panelStyle = ctx->style.panel;
  Color divColor = hoveringDivider || state.isDragging
      ? ctx->style.button.background.normal
      : Color(panelStyle.background.r * 1.3f, panelStyle.background.g * 1.3f,
              panelStyle.background.b * 1.3f, 1.0f);
  ctx->renderer.DrawRectFilled(dividerPos, dividerSize, divColor, 0);

  // Draw a small grip indicator in the center of the divider
  {
    Vec2 gripCenter = dividerPos + dividerSize * 0.5f;
    float gripLen = std::min(vertical ? dividerSize.y : dividerSize.x, 30.0f);
    Color gripColor(1.0f, 1.0f, 1.0f, 0.3f);
    if (vertical) {
      for (int i = -1; i <= 1; ++i) {
        float cx = gripCenter.x + i * 1.5f;
        ctx->renderer.DrawRectFilled(Vec2(cx - 0.5f, gripCenter.y - gripLen * 0.5f),
                                     Vec2(1.0f, gripLen), gripColor, 0);
      }
    } else {
      for (int i = -1; i <= 1; ++i) {
        float cy = gripCenter.y + i * 1.5f;
        ctx->renderer.DrawRectFilled(Vec2(gripCenter.x - gripLen * 0.5f, cy - 0.5f),
                                     Vec2(gripLen, 1.0f), gripColor, 0);
      }
    }
  }

  // --- Push frame context and set up first region ---
  UIContext::SplitterFrameContext frameCtx{};
  frameCtx.id = splitId;
  frameCtx.vertical = vertical;
  frameCtx.ratioPtr = ratio;
  frameCtx.position = splitterPos;
  frameCtx.size = totalSize;
  frameCtx.firstRegionPos = firstPos;
  frameCtx.firstRegionSize = firstSize;
  frameCtx.secondRegionPos = secondPos;
  frameCtx.secondRegionSize = secondSize;
  frameCtx.dividerThickness = divider;
  frameCtx.phase = 0;
  frameCtx.savedCursor = ctx->cursorPos;
  frameCtx.savedLastItemPos = ctx->lastItemPos;
  frameCtx.savedLastItemSize = ctx->lastItemSize;
  frameCtx.layoutPushed = true;

  ctx->splitterStack.push_back(frameCtx);

  // Clip and layout for first region
  ctx->renderer.PushClipRect(firstPos, firstSize);
  ctx->cursorPos = firstPos;
  ctx->offsetStack.push_back(firstPos);
  BeginVertical(ctx->style.spacing, firstSize, Vec2(0, 0));

  return true;
}

void SplitterPanel() {
  UIContext* ctx = GetContext();
  if (!ctx || ctx->splitterStack.empty()) return;

  auto& frameCtx = ctx->splitterStack.back();
  if (frameCtx.phase != 0) return; // Already switched

  // End first region
  EndVertical(false);
  if (!ctx->offsetStack.empty()) ctx->offsetStack.pop_back();
  ctx->renderer.PopClipRect();

  // Switch to second region
  frameCtx.phase = 1;
  ctx->renderer.PushClipRect(frameCtx.secondRegionPos, frameCtx.secondRegionSize);
  ctx->cursorPos = frameCtx.secondRegionPos;
  ctx->offsetStack.push_back(frameCtx.secondRegionPos);
  BeginVertical(ctx->style.spacing, frameCtx.secondRegionSize, Vec2(0, 0));
}

void EndSplitter() {
  UIContext* ctx = GetContext();
  if (!ctx || ctx->splitterStack.empty()) return;

  auto frameCtx = ctx->splitterStack.back();
  ctx->splitterStack.pop_back();

  // End whichever region is active
  EndVertical(false);
  if (!ctx->offsetStack.empty()) ctx->offsetStack.pop_back();
  ctx->renderer.PopClipRect();

  // Restore cursor state and advance
  ctx->cursorPos = frameCtx.savedCursor;
  ctx->lastItemPos = frameCtx.savedLastItemPos;
  ctx->lastItemSize = frameCtx.savedLastItemSize;

  ctx->lastItemPos = frameCtx.position;
  AdvanceCursor(ctx, frameCtx.size);
}

} // namespace FluentUI
