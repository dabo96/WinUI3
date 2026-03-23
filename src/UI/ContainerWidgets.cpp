#include "UI/Widgets.h"
#include "UI/WidgetHelpers.h"
#include "Theme/FluentTheme.h"
#include "core/Animation.h"
#include "core/Context.h"
#include "core/Renderer.h"
#include "core/WidgetNodes.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <optional>

namespace FluentUI {

bool BeginPanel(const std::string &id, const Vec2 &desiredSize,
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
    if (state.size.x <= 0.0f) state.size.x = 300.0f;
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
    // Permitir actualización de ancho desde código si no se está redimensionando manualmente
    if (!state.resizing && desiredSize.x > 0.0f) {
        state.size.x = desiredSize.x;
    }
    // No forzar altura aquí - EndPanel la ajusta automáticamente al contenido
  }

  // SINCRONIZACIÓN DE POSICIÓN (Crucial para scroll)
  if (!state.useAbsolutePos && !state.dragging && !state.resizing) {
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
    if (!leftDown) state.resizing = false;
    else {
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

  // CULLING: Para contenedores como Panel, somos muy permisivos.
  // Solo descartamos si está completamente fuera del viewport global para ahorrar rendimiento,
  // pero dejamos que el Clipping Stack maneje el recorte fino en scroll.
  Vec2 viewportSize = ctx->renderer.GetViewportSize();
  Vec2 drawSize = state.minimized ? Vec2(state.size.x, titleHeight) : state.size;
  bool isVisible = (state.position.x + state.size.x > 0 && state.position.x < viewportSize.x &&
                    state.position.y + drawSize.y > 0 && state.position.y < viewportSize.y);
  
  if (isVisible) {
    // Dibujo del panel
    if (!state.minimized && panelStyle.shadowOpacity > 0.0f) {
      ctx->renderer.DrawRectFilled(state.position + Vec2(0, panelStyle.shadowOffsetY), state.size, Color(0,0,0, panelStyle.shadowOpacity), panelStyle.cornerRadius);
    }
    Color bg = panelStyle.background;
    if (state.useAcrylic) ctx->renderer.DrawRectAcrylic(state.position, drawSize, bg, panelStyle.cornerRadius, state.acrylicOpacity);
    else ctx->renderer.DrawRectFilled(state.position, drawSize, bg, panelStyle.cornerRadius);
    
    ctx->renderer.DrawRectFilled(state.position, titleSize, panelStyle.headerBackground, panelStyle.cornerRadius);
    ctx->renderer.DrawText(state.position + panelStyle.padding * 0.5f, id, panelStyle.headerText.color, panelStyle.headerText.fontSize);
    
    // Botón e Icono + / -
    bool bh = PointInRect(mousePos, minimizeButtonPos, minimizeButtonSize);
    Color bc = (bh && leftDown) ? panelStyle.titleButton.pressed : (bh ? panelStyle.titleButton.hover : panelStyle.titleButton.normal);
    ctx->renderer.DrawRectFilled(minimizeButtonPos, minimizeButtonSize, bc, panelStyle.cornerRadius * 0.4f);
    Vec2 center = minimizeButtonPos + minimizeButtonSize * 0.5f;
    float hl = std::round(minimizeButtonSize.x * 0.2f);
    ctx->renderer.DrawRectFilled(Vec2(std::round(center.x - hl), std::round(center.y - 0.6f)), Vec2(hl * 2, 1.2f), Color(1,1,1,0.9f), 0);
    if (state.minimized) ctx->renderer.DrawRectFilled(Vec2(std::round(center.x - 0.6f), std::round(center.y - hl)), Vec2(1.2f, hl * 2), Color(1,1,1,0.9f), 0);
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

  bool shouldRenderContent = !state.minimized && isVisible;
  if (shouldRenderContent) {
    Vec2 contentOrigin = state.position + Vec2(panelStyle.padding.x, titleHeight + panelStyle.padding.y);
    float cw = std::max(0.0f, state.size.x - panelStyle.padding.x * 2.0f);
    float ch = std::max(0.0f, state.size.y - titleHeight - panelStyle.padding.y * 2.0f);
    
    // Manejar scroll interno si el contenido es mayor que el panel
    if (state.contentSize.y > ch && IsMouseOver(ctx, contentOrigin, Vec2(cw, ch)) && !ctx->scrollConsumedThisFrame) {
        float wy = ctx->input.MouseWheelY();
        if (std::abs(wy) > 0.001f) {
            state.scrollOffset.y = std::clamp(state.scrollOffset.y - wy * S(SCROLL_SPEED), 0.0f, std::max(0.0f, state.contentSize.y - ch));
            ctx->scrollConsumedThisFrame = true;
        }
    }

    ctx->renderer.PushClipRect(contentOrigin, Vec2(cw, ch));
    frameCtx.clipPushed = true;
    
    // Aplicar el scroll offset al cursor del contenido
    ctx->cursorPos = contentOrigin - Vec2(0, state.scrollOffset.y);
    ctx->offsetStack.push_back(contentOrigin);
    BeginVertical(ctx->style.spacing, Vec2(cw, 0.0f), Vec2(0,0));
    frameCtx.layoutPushed = true;
    ctx->panelStack.push_back(frameCtx);
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
  auto it = ctx->panelStates.find(frameCtx.id);
  if (it == ctx->panelStates.end()) return;
  auto &state = it->second;

  if (!ctx->offsetStack.empty()) ctx->offsetStack.pop_back();
  if (frameCtx.layoutPushed) {
    EndVertical(false);
    state.contentSize = ctx->lastItemSize;
  }
  if (frameCtx.clipPushed) ctx->renderer.PopClipRect();

  // AJUSTE DINÁMICO DE TAMAÑO O SCROLLBAR
  float verticalPadding = ctx->style.panel.padding.y * 2.0f;
  float requiredHeight = frameCtx.titleHeight + state.contentSize.y + verticalPadding;

  if (!state.resizing) {
      // Auto-ajustar al contenido, usando userHeight como mínimo
      float newHeight = requiredHeight;
      if (frameCtx.userHeight > 0.0f) {
          newHeight = std::max(newHeight, frameCtx.userHeight);
      }
      if (frameCtx.maxHeight > 0.0f) {
          newHeight = std::min(newHeight, frameCtx.maxHeight);
      }
      state.size.y = newHeight;
  }

  // Dibujar scrollbar si el contenido es más alto que el panel actual
  float availableHeight = state.size.y - (frameCtx.titleHeight + verticalPadding);
  if (frameCtx.isVisible && !state.minimized && state.contentSize.y > availableHeight + 1.0f) {
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
      state.scrollOffset.y -= wheelY * S(SCROLL_SPEED);
      state.scrollOffset.y = std::max(0.0f, state.scrollOffset.y);
      ctx->scrollConsumedThisFrame = true;
    }
  }

  ctx->renderer.PushClipRect(contentAreaPos, contentAreaSize);
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
    state.contentSize = ctx->layoutStack.back().contentSize;
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

bool BeginTabView(const std::string &id, int *activeTab,
                  const std::vector<std::string> &tabLabels, const Vec2 &size,
                  std::optional<Vec2> pos) {
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
  if (tabViewSize.x <= 0.0f) tabViewSize.x = 400.0f;
  if (tabViewSize.y <= 0.0f) tabViewSize.y = 300.0f;

  Vec2 tabViewPos = pos.has_value() ? pos.value() : ctx->cursorPos;
  if (pos.has_value()) state.useAbsolutePos = true;
  else {
    tabViewPos = ResolveAbsolutePosition(ctx, tabViewPos, tabViewSize);
    state.useAbsolutePos = false;
  }

  state.tabBarSize = Vec2(tabViewSize.x, tabHeight);
  Color tabViewBg = AdjustContainerBackground(panelStyle.background, ctx->style.isDarkTheme);
  ctx->renderer.DrawRectFilled(tabViewPos, tabViewSize, tabViewBg, panelStyle.cornerRadius);

  float currentX = tabViewPos.x + panelStyle.padding.x;
  for (size_t i = 0; i < tabLabels.size(); ++i) {
    Vec2 labelSize = MeasureTextCached(ctx, tabLabels[i], tabTextStyle.fontSize);
    Vec2 tSize(labelSize.x + panelStyle.padding.x * 2.0f, tabHeight);
    Vec2 tPos(currentX, tabViewPos.y);
    bool hover = IsMouseOver(ctx, tPos, tSize);
    bool active = (int)i == state.activeTab;
    if (active) {
        ctx->renderer.DrawRectFilled(tPos, tSize, tabViewBg, 0);
        ctx->renderer.DrawLine(tPos + Vec2(0, tSize.y - 2), tPos + Vec2(tSize.x, tSize.y - 2), ctx->style.button.background.normal, 3.0f);
    }
    if (hover && ctx->input.IsMousePressed(0)) {
        state.activeTab = (int)i;
        if (activeTab) *activeTab = (int)i;
    }
    ctx->renderer.DrawText(tPos + Vec2(panelStyle.padding.x, (tabHeight - labelSize.y) * 0.5f), tabLabels[i], active ? tabTextStyle.color : Color(0.7f, 0.7f, 0.7f, 1.0f), tabTextStyle.fontSize);
    currentX += tSize.x + 4.0f;
  }

  Vec2 contentPos = tabViewPos + Vec2(8, tabHeight + 8);
  Vec2 contentSize = tabViewSize - Vec2(16, tabHeight + 16);
  ctx->renderer.PushClipRect(contentPos, contentSize);
  
  Vec2& scroll = state.tabScrollOffsets[state.activeTab];
  ctx->cursorPos = contentPos - Vec2(0, scroll.y);
  ctx->offsetStack.push_back(contentPos);
  BeginVertical(ctx->style.spacing, contentSize, Vec2(0,0));
  
  ctx->tabFrameStack.push_back({tabViewId, contentPos, contentSize, ctx->cursorPos});
  return true;
}

void EndTabView() {
  UIContext *ctx = GetContext();
  if (!ctx || ctx->tabFrameStack.empty()) return;

  // Pop from widget tree (Phase 1)
  ctx->widgetTree.PopParent();

  TabContentFrame frame = ctx->tabFrameStack.back();
  ctx->tabFrameStack.pop_back();
  Vec2 endCursor = ctx->cursorPos;
  EndVertical(false);
  if (!ctx->offsetStack.empty()) ctx->offsetStack.pop_back();
  ctx->renderer.PopClipRect();

  auto it = ctx->tabViewStates.find(frame.tabViewId);
  if (it != ctx->tabViewStates.end()) {
    auto &st = it->second;
    st.contentSize = Vec2(frame.contentAreaSize.x, std::max(frame.contentAreaSize.y, endCursor.y - frame.contentStartCursor.y));
    
    if (IsMouseOver(ctx, frame.contentAreaPos, frame.contentAreaSize) && !ctx->scrollConsumedThisFrame) {
      float wy = ctx->input.MouseWheelY();
      if (std::abs(wy) > 0.001f) {
        st.tabScrollOffsets[st.activeTab].y = std::clamp(st.tabScrollOffsets[st.activeTab].y - wy * S(SCROLL_SPEED), 0.0f, std::max(0.0f, st.contentSize.y - frame.contentAreaSize.y));
        ctx->scrollConsumedThisFrame = true;
      }
    }

    if (st.contentSize.y > frame.contentAreaSize.y) {
      DrawScrollbar(ctx, frame.contentAreaPos + Vec2(frame.contentAreaSize.x - 8, 0), Vec2(8, frame.contentAreaSize.y), st.contentSize.y, frame.contentAreaSize.y, st.tabScrollOffsets[st.activeTab].y, st.draggingScrollbar, st.dragStartMouse, st.dragStartScroll, st.draggingScrollbar, true);
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

  // Initialize ratio from caller pointer
  if (ratio) {
    state.ratio = std::clamp(*ratio, SPLITTER_MIN_RATIO, SPLITTER_MAX_RATIO);
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
