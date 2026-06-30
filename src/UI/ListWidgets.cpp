#include "UI/Widgets.h"
#include "UI/WidgetHelpers.h"
#include "UI/Icons.h"
#include "Theme/FluentTheme.h"
#include "core/Animation.h"
#include "core/Context.h"
#include "core/Renderer.h"
#include "core/WidgetNodes.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <optional>

namespace FluentUI {

static bool BeginListViewSingleImpl(const std::string &id, const Vec2 &size,
                                    int *selectedItem,
                                    const std::vector<std::string> &items,
                                    const std::vector<uint32_t> *icons,
                                    std::optional<Vec2> pos);

bool BeginListView(const std::string &id, const Vec2 &size, int *selectedItem,
                   const std::vector<std::string> &items, std::optional<Vec2> pos) {
  return BeginListViewSingleImpl(id, size, selectedItem, items, nullptr, pos);
}

bool BeginListView(const std::string &id, const Vec2 &size, int *selectedItem,
                   const std::vector<std::pair<std::string, uint32_t>> &items,
                   std::optional<Vec2> pos) {
  std::vector<std::string> labels;
  std::vector<uint32_t> icons;
  labels.reserve(items.size());
  icons.reserve(items.size());
  for (const auto &p : items) { labels.push_back(p.first); icons.push_back(p.second); }
  return BeginListViewSingleImpl(id, size, selectedItem, labels, &icons, pos);
}

static bool BeginListViewSingleImpl(const std::string &id, const Vec2 &size,
                                    int *selectedItem,
                                    const std::vector<std::string> &items,
                                    const std::vector<uint32_t> *icons,
                                    std::optional<Vec2> pos) {
  UIContext *ctx = GetContext();
  if (!ctx || items.empty())
    return false;

  uint32_t listViewId = GenerateId("LISTVIEW:", id.c_str());

  // Register in widget tree (Phase 1)
  ctx->widgetTree.FindOrCreate(listViewId, ctx->frame, [&]() {
      auto node = std::make_unique<ListViewNode>(listViewId);
      node->debugName = id;
      return node;
  });

  auto &state = ctx->listViewStates[listViewId];

  if (!state.initialized) {
    state.selectedItem = selectedItem ? *selectedItem : -1;
    Vec2 avail = GetCurrentAvailableSpace(ctx);
    state.itemSize = Vec2(size.x > 0.0f ? size.x : (avail.x > 0.0f ? avail.x : 200.0f), 32.0f);
    state.scrollOffset = 0.0f;  // Inicializar scroll offset
    state.initialized = true;
  } else {
    if (selectedItem) {
      state.selectedItem = *selectedItem;
    }
    if (size.x > 0.0f) {
      state.itemSize.x = size.x;
    } else {
      Vec2 avail = GetCurrentAvailableSpace(ctx);
      if (avail.x > 0.0f) state.itemSize.x = avail.x;
    }
  }

  Vec2 listViewSize = size;
  if (listViewSize.x <= 0.0f) {
    Vec2 avail = GetCurrentAvailableSpace(ctx);
    listViewSize.x = avail.x > 0.0f ? avail.x : 200.0f;
  }
  if (listViewSize.y <= 0.0f) {
    listViewSize.y = items.size() * state.itemSize.y;
  }

  // Resolver posición: usar pos si se proporciona, sino usar cursor
  Vec2 listViewPos;
  if (pos.has_value()) {
    listViewPos = ResolveAbsolutePosition(ctx, pos.value(), listViewSize);
    state.useAbsolutePos = true;
    state.absolutePos = listViewPos;
  } else {
    listViewPos = ctx->cursorPos;
    state.useAbsolutePos = false;
  }
  const PanelStyle &panelStyle = ctx->style.panel;
  const TextStyle &textStyle = ctx->style.GetTextStyle(TypographyStyle::Body);

  // Dibujar fondo del ListView con contraste más pronunciado - sin borde
  Color listBg = AdjustListSurfaceBackground(panelStyle.background, ctx->style.isDarkTheme);
  ctx->renderer.DrawRectFilled(listViewPos, listViewSize, listBg,
                               panelStyle.cornerRadius);

  // Calcular tamaños
  float scrollbarWidth = SCROLLBAR_WIDTH;
  float visibleHeight = listViewSize.y;
  float totalHeight = items.size() * state.itemSize.y;
  bool needsScrollbar = totalHeight > visibleHeight;
  float contentWidth = needsScrollbar ? listViewSize.x - scrollbarWidth : listViewSize.x;

  // Manejar mouse wheel
  bool hoverListView = IsMouseOver(ctx, listViewPos, listViewSize);

  if (hoverListView && needsScrollbar && !ctx->scrollConsumedThisFrame) {
    float wheelY = ctx->input.MouseWheelY();
    if (std::abs(wheelY) > 0.001f) {
      float scrollSpeed = SCROLL_SPEED;
      state.scrollOffset -= wheelY * scrollSpeed;
      state.scrollOffset = std::clamp(state.scrollOffset, 0.0f,
                                      std::max(0.0f, totalHeight - visibleHeight));
      ctx->scrollConsumedThisFrame = true; // Marcar como consumido
    }
  }

  bool leftPressed = ctx->input.IsMousePressed(0);

  // Aplicar clipping
  Vec2 clipSize(contentWidth, visibleHeight);
  ctx->renderer.PushClipRect(listViewPos, clipSize);

  // Calculate visible range to avoid iterating all items
  int totalItems = static_cast<int>(items.size());
  int startIndex = std::max(0, static_cast<int>(state.scrollOffset / state.itemSize.y));
  int endIndex = std::min(totalItems,
      startIndex + static_cast<int>(visibleHeight / state.itemSize.y) + 2);

  for (int i = startIndex; i < endIndex; ++i) {
    Vec2 itemPos(listViewPos.x, listViewPos.y + i * state.itemSize.y - state.scrollOffset);
    Vec2 itemSize(contentWidth, state.itemSize.y);

    bool hover = IsMouseOver(ctx, itemPos, itemSize);
    bool isSelected = (i == state.selectedItem);

    // Dibujar fondo del item
    Color itemBg = panelStyle.background;
    if (isSelected) {
      itemBg = ctx->style.button.background.normal;
    } else if (hover) {
      itemBg = panelStyle.headerBackground;
    }
    ctx->renderer.DrawRectFilled(itemPos, itemSize, itemBg, 0.0f);

    // Dibujar indicador de selección
    if (isSelected) {
      Vec2 indicatorPos(itemPos.x, itemPos.y);
      Vec2 indicatorSize(3.0f, itemSize.y);
      ctx->renderer.DrawRectFilled(indicatorPos, indicatorSize,
                                   ctx->style.button.background.hover, 0.0f);
    }

    // Dibujar icono opcional + texto del item
    float listIconSize = textStyle.fontSize;
    float listIconGap = 6.0f;
    uint32_t cp = (icons && static_cast<size_t>(i) < icons->size()) ? (*icons)[i] : 0u;
    float listIconSlot = (cp != 0u) ? (listIconSize + listIconGap) : 0.0f;
    if (cp != 0u) {
      DrawWidgetIcon(ctx, itemPos, itemSize, cp, textStyle.color, listIconSize,
                     panelStyle.padding.x, listIconGap);
    }
    Vec2 textPos(itemPos.x + panelStyle.padding.x + listIconSlot,
                 itemPos.y + (state.itemSize.y - textStyle.fontSize) * 0.5f);
    ctx->renderer.DrawText(textPos, items[i], textStyle.color,
                           textStyle.fontSize);

    // Manejar click en el item
    if (hover && leftPressed) {
      state.selectedItem = i;
      if (selectedItem) {
        *selectedItem = i;
      }
    }
  }

  // Remover clipping
  ctx->renderer.PopClipRect();

  // Dibujar scrollbar (interaction + rendering handled by helper)
  if (needsScrollbar) {
    Vec2 barPos(listViewPos.x + contentWidth, listViewPos.y);
    Vec2 barSize(scrollbarWidth, visibleHeight);
    DrawScrollbar(ctx, barPos, barSize, totalHeight, visibleHeight,
                  state.scrollOffset, state.draggingScrollbar,
                  state.dragStartMouse, state.dragStartScroll,
                  state.draggingScrollbar, true);
  }

  // Actualizar selectedItem externo
  if (selectedItem) {
    *selectedItem = state.selectedItem;
  }

  // Avanzar cursor solo si NO se usa posición absoluta
  if (!state.useAbsolutePos) {
    ctx->lastItemPos = listViewPos;
    AdvanceCursor(ctx, listViewSize);
  } else {
    ctx->lastItemPos = listViewPos;
  }

  return true;
}

static bool BeginListViewMultiImpl(const std::string &id, const Vec2 &size,
                                   std::vector<int> *selectedItems,
                                   const std::vector<std::string> &items,
                                   const std::vector<uint32_t> *icons,
                                   std::optional<Vec2> pos);

bool BeginListView(const std::string &id, const Vec2 &size, std::vector<int> *selectedItems,
                   const std::vector<std::string> &items, std::optional<Vec2> pos) {
  return BeginListViewMultiImpl(id, size, selectedItems, items, nullptr, pos);
}

bool BeginListView(const std::string &id, const Vec2 &size, std::vector<int> *selectedItems,
                   const std::vector<std::pair<std::string, uint32_t>> &items,
                   std::optional<Vec2> pos) {
  std::vector<std::string> labels;
  std::vector<uint32_t> icons;
  labels.reserve(items.size());
  icons.reserve(items.size());
  for (const auto &p : items) { labels.push_back(p.first); icons.push_back(p.second); }
  return BeginListViewMultiImpl(id, size, selectedItems, labels, &icons, pos);
}

static bool BeginListViewMultiImpl(const std::string &id, const Vec2 &size,
                                   std::vector<int> *selectedItems,
                                   const std::vector<std::string> &items,
                                   const std::vector<uint32_t> *icons,
                                   std::optional<Vec2> pos) {
  UIContext *ctx = GetContext();
  if (!ctx || items.empty())
    return false;

  uint32_t listViewId = GenerateId("LISTVIEW_MS:", id.c_str());

  // Register in widget tree (Phase 1)
  ctx->widgetTree.FindOrCreate(listViewId, ctx->frame, [&]() {
      auto node = std::make_unique<ListViewNode>(listViewId);
      node->debugName = id;
      return node;
  });

  auto &state = ctx->listViewStates[listViewId];

  if (!state.initialized) {
    state.selectedItem = -1;
    state.lastClickedItem = -1;
    Vec2 avail = GetCurrentAvailableSpace(ctx);
    state.itemSize = Vec2(size.x > 0.0f ? size.x : (avail.x > 0.0f ? avail.x : 200.0f), 32.0f);
    state.scrollOffset = 0.0f;
    state.initialized = true;
    // Sync from external selection
    if (selectedItems) {
      state.selectedItems = *selectedItems;
    }
  } else {
    if (selectedItems) {
      state.selectedItems = *selectedItems;
    }
    if (size.x > 0.0f) {
      state.itemSize.x = size.x;
    } else {
      Vec2 avail = GetCurrentAvailableSpace(ctx);
      if (avail.x > 0.0f) state.itemSize.x = avail.x;
    }
  }

  Vec2 listViewSize = size;
  if (listViewSize.x <= 0.0f) {
    Vec2 avail = GetCurrentAvailableSpace(ctx);
    listViewSize.x = avail.x > 0.0f ? avail.x : 200.0f;
  }
  if (listViewSize.y <= 0.0f) {
    listViewSize.y = items.size() * state.itemSize.y;
  }

  // Resolver posición
  Vec2 listViewPos;
  if (pos.has_value()) {
    listViewPos = ResolveAbsolutePosition(ctx, pos.value(), listViewSize);
    state.useAbsolutePos = true;
    state.absolutePos = listViewPos;
  } else {
    listViewPos = ctx->cursorPos;
    state.useAbsolutePos = false;
  }

  const PanelStyle &panelStyle = ctx->style.panel;
  const TextStyle &textStyle = ctx->style.GetTextStyle(TypographyStyle::Body);

  // Dibujar fondo del ListView
  Color listBg = AdjustListSurfaceBackground(panelStyle.background, ctx->style.isDarkTheme);
  ctx->renderer.DrawRectFilled(listViewPos, listViewSize, listBg,
                               panelStyle.cornerRadius);

  // Calcular tamaños
  float scrollbarWidth = SCROLLBAR_WIDTH;
  float visibleHeight = listViewSize.y;
  float totalHeight = items.size() * state.itemSize.y;
  bool needsScrollbar = totalHeight > visibleHeight;
  float contentWidth = needsScrollbar ? listViewSize.x - scrollbarWidth : listViewSize.x;

  // Manejar mouse wheel
  bool hoverListView = IsMouseOver(ctx, listViewPos, listViewSize);

  if (hoverListView && needsScrollbar && !ctx->scrollConsumedThisFrame) {
    float wheelY = ctx->input.MouseWheelY();
    if (std::abs(wheelY) > 0.001f) {
      float scrollSpeed = SCROLL_SPEED;
      state.scrollOffset -= wheelY * scrollSpeed;
      state.scrollOffset = std::clamp(state.scrollOffset, 0.0f,
                                      std::max(0.0f, totalHeight - visibleHeight));
      ctx->scrollConsumedThisFrame = true;
    }
  }

  bool leftPressed = ctx->input.IsMousePressed(0);

  // Get keyboard modifiers for multi-select
  SDL_Keymod modState = SDL_GetModState();
  bool ctrlHeld = (modState & SDL_KMOD_CTRL) != 0;
  bool shiftHeld = (modState & SDL_KMOD_SHIFT) != 0;

  // Aplicar clipping
  Vec2 clipSize(contentWidth, visibleHeight);
  ctx->renderer.PushClipRect(listViewPos, clipSize);

  // Calculate visible range
  int totalItems = static_cast<int>(items.size());
  int startIndex = std::max(0, static_cast<int>(state.scrollOffset / state.itemSize.y));
  int endIndex = std::min(totalItems,
      startIndex + static_cast<int>(visibleHeight / state.itemSize.y) + 2);

  // Helper lambda: check if item index is in the selection
  auto isItemSelected = [&](int idx) -> bool {
    const auto &sel = state.selectedItems;
    return std::find(sel.begin(), sel.end(), idx) != sel.end();
  };

  for (int i = startIndex; i < endIndex; ++i) {
    Vec2 itemPos(listViewPos.x, listViewPos.y + i * state.itemSize.y - state.scrollOffset);
    Vec2 itemSize(contentWidth, state.itemSize.y);

    bool hover = IsMouseOver(ctx, itemPos, itemSize);
    bool isSelected = isItemSelected(i);

    // Dibujar fondo del item
    Color itemBg = panelStyle.background;
    if (isSelected) {
      itemBg = ctx->style.button.background.normal;
    } else if (hover) {
      itemBg = panelStyle.headerBackground;
    }
    ctx->renderer.DrawRectFilled(itemPos, itemSize, itemBg, 0.0f);

    // Dibujar indicador de selección
    if (isSelected) {
      Vec2 indicatorPos(itemPos.x, itemPos.y);
      Vec2 indicatorSize(3.0f, itemSize.y);
      ctx->renderer.DrawRectFilled(indicatorPos, indicatorSize,
                                   ctx->style.button.background.hover, 0.0f);
    }

    // Dibujar icono opcional + texto del item
    float listIconSize = textStyle.fontSize;
    float listIconGap = 6.0f;
    uint32_t cp = (icons && static_cast<size_t>(i) < icons->size()) ? (*icons)[i] : 0u;
    float listIconSlot = (cp != 0u) ? (listIconSize + listIconGap) : 0.0f;
    if (cp != 0u) {
      DrawWidgetIcon(ctx, itemPos, itemSize, cp, textStyle.color, listIconSize,
                     panelStyle.padding.x, listIconGap);
    }
    Vec2 textPos(itemPos.x + panelStyle.padding.x + listIconSlot,
                 itemPos.y + (state.itemSize.y - textStyle.fontSize) * 0.5f);
    ctx->renderer.DrawText(textPos, items[i], textStyle.color,
                           textStyle.fontSize);

    // Manejar click en el item
    if (hover && leftPressed) {
      if (ctrlHeld) {
        // Ctrl+Click: toggle individual item
        auto it = std::find(state.selectedItems.begin(), state.selectedItems.end(), i);
        if (it != state.selectedItems.end()) {
          state.selectedItems.erase(it);
        } else {
          state.selectedItems.push_back(i);
        }
        state.lastClickedItem = i;
      } else if (shiftHeld && state.lastClickedItem >= 0) {
        // Shift+Click: range select from lastClickedItem to current
        int rangeStart = std::min(state.lastClickedItem, i);
        int rangeEnd = std::max(state.lastClickedItem, i);
        // Clear and re-select the range
        state.selectedItems.clear();
        for (int j = rangeStart; j <= rangeEnd; ++j) {
          state.selectedItems.push_back(j);
        }
        // Don't update lastClickedItem on shift-click (anchor stays)
      } else {
        // Plain click: single select
        state.selectedItems.clear();
        state.selectedItems.push_back(i);
        state.lastClickedItem = i;
      }
    }
  }

  // Remover clipping
  ctx->renderer.PopClipRect();

  // Dibujar scrollbar
  if (needsScrollbar) {
    Vec2 barPos(listViewPos.x + contentWidth, listViewPos.y);
    Vec2 barSize(scrollbarWidth, visibleHeight);
    DrawScrollbar(ctx, barPos, barSize, totalHeight, visibleHeight,
                  state.scrollOffset, state.draggingScrollbar,
                  state.dragStartMouse, state.dragStartScroll,
                  state.draggingScrollbar, true);
  }

  // Sync back to external vector
  if (selectedItems) {
    *selectedItems = state.selectedItems;
  }

  // Avanzar cursor
  if (!state.useAbsolutePos) {
    ctx->lastItemPos = listViewPos;
    AdvanceCursor(ctx, listViewSize);
  } else {
    ctx->lastItemPos = listViewPos;
  }

  return true;
}

void EndListView() {
  // EndListView no necesita hacer nada especial por ahora
  // El contenido ya fue renderizado en BeginListView
}

// ctx->treeViewDepth and ctx->currentTreeViewId moved to UIContext

bool BeginTreeView(const std::string &id, const Vec2 &size, std::optional<Vec2> pos) {
  UIContext *ctx = GetContext();
  if (!ctx)
    return false;

  uint32_t treeViewId = GenerateId("TREEVIEW:", id.c_str());

  // Register in widget tree (Phase 1)
  auto* treeNode = static_cast<TreeViewNode*>(
      ctx->widgetTree.FindOrCreate(treeViewId, ctx->frame, [&]() {
          auto node = std::make_unique<TreeViewNode>(treeViewId);
          node->debugName = id;
          return node;
      })
  );
  ctx->widgetTree.PushParent(treeNode);

  auto &state = ctx->treeViewStates[treeViewId];
  ctx->currentTreeViewId = treeViewId;
  // brief 21: scope TreeNodes by the tree id (paired with widgetTree push/pop).
  PushID(id.c_str());

  if (!state.initialized) {
    Vec2 avail = GetCurrentAvailableSpace(ctx);
    state.itemSize = Vec2(size.x > 0.0f ? size.x : (avail.x > 0.0f ? avail.x : 200.0f), 28.0f);
    state.indentSize = 20.0f;
    state.expandButtonSize = 14.0f;
    state.initialized = true;
  } else {
    if (size.x > 0.0f) {
      state.itemSize.x = size.x;
    } else {
      Vec2 avail = GetCurrentAvailableSpace(ctx);
      if (avail.x > 0.0f) state.itemSize.x = avail.x;
    }
  }

  // Reserve space for scrollbar when content overflows
  bool needsScroll = state.contentHeight > (size.y > 0.0f ? size.y : state.viewSize.y);
  if (needsScroll) {
    state.itemSize.x -= (SCROLLBAR_WIDTH + 4.0f);
  }

  Vec2 treeViewSize = size;
  if (treeViewSize.x <= 0.0f || treeViewSize.y <= 0.0f) {
    Vec2 avail = GetCurrentAvailableSpace(ctx);
    if (treeViewSize.x <= 0.0f) treeViewSize.x = avail.x > 0.0f ? avail.x : 200.0f;
    if (treeViewSize.y <= 0.0f) treeViewSize.y = avail.y > 0.0f ? avail.y : 300.0f;
  }

  // Resolver posición: usar pos si se proporciona, sino usar cursor
  Vec2 treeViewPos;
  if (pos.has_value()) {
    treeViewPos = ResolveAbsolutePosition(ctx, pos.value(), treeViewSize);
    state.useAbsolutePos = true;
    state.absolutePos = treeViewPos;
  } else {
    treeViewPos = ctx->cursorPos;
    state.useAbsolutePos = false;
  }
  const PanelStyle &panelStyle = ctx->style.panel;

  // AJUSTE DINÁMICO: Usar altura del contenido del frame anterior si está disponible
  float treeViewHeight = treeViewSize.y;
  if (state.initialized) {
      // Usar la altura real del contenido + padding
      float contentHeight = (ctx->cursorPos.y - treeViewPos.y); // Estimación burda para el primer frame
      // En realidad, guardaremos el contentHeight en el estado en EndTreeView
      // Por ahora, usemos el tamaño guardado del frame anterior.
  }

  // Guardar posición y tamaño visual para scroll
  state.viewPos = treeViewPos;
  state.viewSize = treeViewSize;

  // Manejar scroll con rueda del mouse
  float viewH = treeViewSize.y;
  if (state.contentHeight > viewH) {
    float maxScroll = state.contentHeight - viewH;
    bool mouseOver = IsMouseOver(ctx, treeViewPos, treeViewSize);
    if (mouseOver && !ctx->scrollConsumedThisFrame) {
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

  // Dibujar fondo del TreeView con contraste más pronunciado - sin borde
  // (igual que el ListView, acorde al tema Fluent de Microsoft).
  Color treeBg = AdjustListSurfaceBackground(panelStyle.background, ctx->style.isDarkTheme);
  ctx->renderer.DrawRectFilled(treeViewPos, treeViewSize, treeBg,
                               panelStyle.cornerRadius);

  // Aplicar clipping
  ctx->renderer.PushClipRect(treeViewPos, treeViewSize);

  // Configurar cursor para el contenido del TreeView
  // Añadir un pequeño padding interno para que no toque los bordes del fondo
  Vec2 innerPadding = panelStyle.padding * 0.5f;
  ctx->cursorPos = Vec2(treeViewPos.x + innerPadding.x, treeViewPos.y + innerPadding.y - state.scrollOffset);
  ctx->lastItemPos = ctx->cursorPos;
  ctx->treeViewDepth = 0;

  // Phase C5: reset DFS visit order for this frame
  ctx->treeVisitOrder[treeViewId].clear();

  // Iniciar layout vertical para apilar los nodos correctamente
  Vec2 contentSize(treeViewSize.x - innerPadding.x * 2.0f,
                   treeViewSize.y - innerPadding.y * 2.0f);
  BeginVertical(ctx->style.spacing, Vec2(contentSize.x, 0.0f), Vec2(0.0f, 0.0f));

  return true;
}

void EndTreeView() {
  UIContext *ctx = GetContext();
  if (!ctx)
    return;

  // brief 21: pop the scope pushed in BeginTreeView.
  PopID();

  // Pop from widget tree (Phase 1)
  ctx->widgetTree.PopParent();

  // Capture content end cursor before closing layout
  Vec2 contentEndCursor = ctx->cursorPos;

  // Cerrar el layout vertical
  if (!ctx->layoutStack.empty()) {
    EndVertical(false); // No avanzar el cursor del padre aquí
  }

  // Medir y guardar la altura real del contenido (para scroll del próximo frame)
  if (ctx->currentTreeViewId != 0) {
    auto it = ctx->treeViewStates.find(ctx->currentTreeViewId);
    if (it != ctx->treeViewStates.end()) {
      auto &state = it->second;
      Vec2 innerPadding = ctx->style.panel.padding * 0.5f;
      // contentEndCursor.y está offset por -scrollOffset, sumar de vuelta para obtener la altura real
      float realContentHeight = (contentEndCursor.y + state.scrollOffset) - (state.viewPos.y) + innerPadding.y;
      state.contentHeight = realContentHeight;

      float viewH = state.viewSize.y;
      bool needsScroll = state.contentHeight > viewH;

      // Dibujar scrollbar si necesario (dentro del clip rect del TreeView)
      if (needsScroll && viewH > 20.0f) {
        float scrollbarW = SCROLLBAR_WIDTH;
        Vec2 barPos(state.viewPos.x + state.viewSize.x - scrollbarW - 2.0f, state.viewPos.y);
        Vec2 barSize(scrollbarW, viewH);
        DrawScrollbar(ctx, barPos, barSize, state.contentHeight, viewH,
                      state.scrollOffset, state.draggingScrollbar,
                      state.dragStartMouse, state.dragStartScroll,
                      state.draggingScrollbar, true);
      }

      // Remover clipping
      ctx->renderer.PopClipRect();

      // Avanzar cursor solo si NO se usa posición absoluta
      if (!state.useAbsolutePos) {
        ctx->lastItemPos = state.viewPos;
        AdvanceCursor(ctx, state.viewSize);
      }
    } else {
      ctx->renderer.PopClipRect();
    }
  } else {
    ctx->renderer.PopClipRect();
  }

  ctx->currentTreeViewId = 0;
  ctx->treeViewDepth = 0;
}

bool TreeNode(const std::string &id, const std::string &label, bool *isOpen,
              bool *isSelected) {
  return TreeNode(id, label, 0u, isOpen, isSelected);
}

bool TreeNode(const std::string &id, const std::string &label, uint32_t iconCodepoint,
              bool *isOpen, bool *isSelected) {
  UIContext *ctx = GetContext();
  if (!ctx || ctx->currentTreeViewId == 0)
    return false;

  auto it = ctx->treeViewStates.find(ctx->currentTreeViewId);
  if (it == ctx->treeViewStates.end())
    return false;

  const auto &state = it->second;
  const PanelStyle &panelStyle = ctx->style.panel;
  const TextStyle &textStyle = ctx->style.GetTextStyle(TypographyStyle::Body);

  // Use internal state as source of truth; initialize from caller on first encounter
  std::string nodeKey =
      "TREENODE:" + std::to_string(ctx->currentTreeViewId) + ":" + id;
  auto stateIt = ctx->treeNodeStates.find(nodeKey);
  if (stateIt == ctx->treeNodeStates.end()) {
    // First time seeing this node — initialize from caller's value
    bool initial = isOpen ? *isOpen : false;
    ctx->treeNodeStates[nodeKey] = initial;
    stateIt = ctx->treeNodeStates.find(nodeKey);
  }
  bool nodeIsOpen = stateIt->second;
  bool nodeIsSelected = isSelected ? *isSelected : false;

  Vec2 itemSize(state.itemSize.x, state.itemSize.y);
  Vec2 itemPos = ctx->cursorPos;

  // Calcular indentación
  float indentX = ctx->treeViewDepth * state.indentSize;
  itemPos.x += indentX;

  bool hover = IsMouseOver(ctx, itemPos, itemSize);
  bool clicked = hover && ctx->input.IsMousePressed(0);

  // Determinar si tiene hijos (simplificado: si isOpen != nullptr, asumimos que
  // puede tener hijos)
  bool hasChildren = (isOpen != nullptr);

  // Dibujar fondo del item
  Color itemBg = Color(0,0,0,0); // Transparente por defecto
  if (nodeIsSelected) {
    itemBg = ctx->style.button.background.normal;
  } else if (hover) {
    itemBg = panelStyle.headerBackground;
  }
  
  if (itemBg.a > 0.0f) {
    ctx->renderer.DrawRectFilled(itemPos, itemSize, itemBg, 4.0f); // Usar un poco de corner radius
  }

  // Dibujar indicador de selección
  if (nodeIsSelected) {
    Vec2 indicatorPos(itemPos.x, itemPos.y);
    Vec2 indicatorSize(3.0f, itemSize.y);
    ctx->renderer.DrawRectFilled(indicatorPos, indicatorSize,
                                 ctx->style.button.background.hover, 0.0f);
  }

  // Dibujar botón de expand/collapse si tiene hijos
  float buttonX = itemPos.x;
  float buttonY = itemPos.y + (itemSize.y - state.expandButtonSize) * 0.5f;
  Vec2 buttonPos(buttonX, buttonY);
  Vec2 buttonSize(state.expandButtonSize, state.expandButtonSize);

  if (hasChildren) {
    bool hoverButton = IsMouseOver(ctx, buttonPos, buttonSize);

    // Disclosure triangle drawn with primitives (NOT an icon-font glyph): points
    // right (▶) when collapsed and down (▼) when expanded. Using geometry instead
    // of a Lucide glyph guarantees it always renders regardless of the icon atlas
    // state, and it changes unambiguously between states. Hover raises its opacity.
    Color triColor = textStyle.color;
    triColor.a *= hoverButton ? 1.0f : 0.7f;
    float cx = buttonPos.x + buttonSize.x * 0.5f;
    float cy = buttonPos.y + buttonSize.y * 0.5f;
    float r = std::round(buttonSize.x * 0.30f); // half-extent of the triangle
    if (nodeIsOpen) {
      // ▼ pointing down
      ctx->renderer.DrawTriangleFilled(Vec2(cx - r, cy - r * 0.5f),
                                       Vec2(cx + r, cy - r * 0.5f),
                                       Vec2(cx, cy + r * 0.6f), triColor);
    } else {
      // ▶ pointing right
      ctx->renderer.DrawTriangleFilled(Vec2(cx - r * 0.5f, cy - r),
                                       Vec2(cx - r * 0.5f, cy + r),
                                       Vec2(cx + r * 0.6f, cy), triColor);
    }

    // Manejar click en el botón
    if (hoverButton && ctx->input.IsMousePressed(0)) {
      nodeIsOpen = !nodeIsOpen;
      ctx->treeNodeStates[nodeKey] = nodeIsOpen;
      if (isOpen) {
        *isOpen = nodeIsOpen;
      }
    }
  }

  // Slot for an optional leading icon glyph (after the expand button)
  float iconSlot = 20.0f;
  float chevronAdvance = (hasChildren ? state.expandButtonSize + 4.0f : 0.0f);
  if (iconCodepoint != 0u) {
    float iconSize = textStyle.fontSize;
    DrawWidgetIcon(ctx, itemPos, itemSize, iconCodepoint, textStyle.color,
                   iconSize, chevronAdvance + (iconSlot - iconSize) * 0.5f, 0.0f);
  }

  float textX = itemPos.x + chevronAdvance + iconSlot + 4.0f;
  Vec2 textPos(textX, itemPos.y + (itemSize.y - textStyle.fontSize) * 0.5f);
  ctx->renderer.DrawText(textPos, label, textStyle.color, textStyle.fontSize);

  // Manejar click en el item (selección)
  if (hover && clicked && !hasChildren) {
    // Si no tiene hijos, el click selecciona
    nodeIsSelected = true;
    if (isSelected) {
      *isSelected = true;
    }
  } else if (hover && clicked && hasChildren) {
    // Si tiene hijos y click fuera del botón, también selecciona
    if (ctx->input.MouseX() > buttonPos.x + buttonSize.x + 4.0f) {
      nodeIsSelected = true;
      if (isSelected) {
        *isSelected = true;
      }
    }
  }

  AdvanceCursor(ctx, itemSize);

  // Sync caller's isOpen pointer from internal state (not the other way around)
  if (isOpen) {
    *isOpen = ctx->treeNodeStates[nodeKey];
  }

  // Devolver true si el nodo está abierto (útil para mostrar hijos)
  return nodeIsOpen && hasChildren;
}

// Phase C5: Multi-select TreeNode with Ctrl/Shift modifiers and range selection.
bool TreeNodeMulti(const std::string &id, const std::string &label, int nodeId,
                   bool *isOpen, std::vector<int> *selectedIds) {
  return TreeNodeMulti(id, label, 0u, nodeId, isOpen, selectedIds);
}

bool TreeNodeMulti(const std::string &id, const std::string &label, uint32_t iconCodepoint,
                   int nodeId, bool *isOpen, std::vector<int> *selectedIds) {
  UIContext *ctx = GetContext();
  if (!ctx || ctx->currentTreeViewId == 0 || !selectedIds) {
    return TreeNode(id, label, iconCodepoint, isOpen, nullptr);
  }

  // Record visit order for range select
  uint32_t treeId = ctx->currentTreeViewId;
  ctx->treeVisitOrder[treeId].push_back(nodeId);

  bool isInSelection = std::find(selectedIds->begin(), selectedIds->end(), nodeId) != selectedIds->end();
  bool tempSelected = isInSelection;

  bool wasClicked = TreeNode(id, label, iconCodepoint, isOpen, &tempSelected);
  if (!wasClicked) return false;

  // Click happened — apply modifier semantics
  SDL_Keymod km = SDL_GetModState();
  bool ctrlHeld = (km & SDL_KMOD_CTRL) != 0;
  bool shiftHeld = (km & SDL_KMOD_SHIFT) != 0;

  if (shiftHeld) {
    // Range select from anchor in DFS order
    auto anchorIt = ctx->treeLastSelectedId.find(treeId);
    int anchor = (anchorIt != ctx->treeLastSelectedId.end()) ? anchorIt->second : nodeId;
    const auto& order = ctx->treeVisitOrder[treeId];
    int aIdx = -1, bIdx = -1;
    for (int i = 0; i < static_cast<int>(order.size()); ++i) {
      if (order[i] == anchor) aIdx = i;
      if (order[i] == nodeId) bIdx = i;
    }
    if (aIdx < 0 || bIdx < 0) {
      selectedIds->assign({nodeId});
    } else {
      if (aIdx > bIdx) std::swap(aIdx, bIdx);
      selectedIds->clear();
      for (int i = aIdx; i <= bIdx; ++i) selectedIds->push_back(order[i]);
    }
  } else if (ctrlHeld) {
    // Toggle membership
    auto it2 = std::find(selectedIds->begin(), selectedIds->end(), nodeId);
    if (it2 != selectedIds->end()) {
      selectedIds->erase(it2);
    } else {
      selectedIds->push_back(nodeId);
    }
    ctx->treeLastSelectedId[treeId] = nodeId;
  } else {
    // Replace selection
    selectedIds->assign({nodeId});
    ctx->treeLastSelectedId[treeId] = nodeId;
  }
  return true;
}

void TreeNodePush() {
  UIContext *ctx = GetContext();
  if (ctx) ctx->treeViewDepth++;
}

void TreeNodePop() {
  UIContext *ctx = GetContext();
  if (ctx && ctx->treeViewDepth > 0) {
    ctx->treeViewDepth--;
  }
}

// ============================================================
// Table / DataGrid
// ============================================================

bool BeginTable(const std::string& id, std::vector<TableColumn>& columns,
                int rowCount, const Vec2& size, TableState* externalState) {
  UIContext *ctx = GetContext();
  if (!ctx || columns.empty())
    return false;

  uint32_t tableId = GenerateId("TABLE:", id.c_str());

  // Register in widget tree (Phase 1)
  ctx->widgetTree.FindOrCreate(tableId, ctx->frame, [&]() {
      auto node = std::make_unique<TableNode>(tableId);
      node->debugName = id;
      return node;
  });

  auto &state = ctx->tableStates[tableId];
  if (!state.initialized) {
    state.initialized = true;
    if (externalState) {
      state.sortColumn = externalState->sortColumn;
      state.sortAscending = externalState->sortAscending;
      state.scrollOffset = externalState->scrollOffset;
      state.scrollOffsetX = externalState->scrollOffsetX; // Phase C7
    }
  } else if (externalState) {
    // Sync sort state from external if provided
    state.sortColumn = externalState->sortColumn;
    state.sortAscending = externalState->sortAscending;
  }

  // Calculate total content width from columns
  float totalColWidth = 0.0f;
  for (const auto& col : columns) {
    totalColWidth += col.width;
  }

  // Resolve table size
  Vec2 tableSize = size;
  if (tableSize.x <= 0.0f || tableSize.y <= 0.0f) {
    Vec2 avail = GetCurrentAvailableSpace(ctx);
    if (tableSize.x <= 0.0f) tableSize.x = avail.x > 0.0f ? avail.x : (totalColWidth + SCROLLBAR_WIDTH);
    if (tableSize.y <= 0.0f) tableSize.y = avail.y > 0.0f ? avail.y : 300.0f;
  }

  // Position
  Vec2 tablePos = ctx->cursorPos;

  const PanelStyle &panelStyle = ctx->style.panel;
  const TextStyle &captionStyle = ctx->style.GetTextStyle(TypographyStyle::Caption);

  // Calculate dimensions
  float headerHeight = 28.0f;
  float rowHeight = 28.0f;
  float scrollbarWidth = SCROLLBAR_WIDTH;
  // Phase C7: horizontal scroll computation. Reserve space for h-scrollbar
  // when total columns width exceeds the available scrollable region.
  int frozenColCount = 0;
  if (externalState) {
    frozenColCount = std::clamp(externalState->frozenColumns, 0,
                                static_cast<int>(columns.size()));
  }
  float frozenWidth = 0.0f;
  for (int i = 0; i < frozenColCount; ++i) frozenWidth += columns[i].width;
  // First pass: assume no h-scrollbar
  float dataAreaHeight = tableSize.y - headerHeight;
  float totalDataHeight = static_cast<float>(rowCount) * rowHeight;
  bool needsScrollbar = totalDataHeight > dataAreaHeight;
  float availableContentWidth = needsScrollbar ? tableSize.x - scrollbarWidth : tableSize.x;
  bool needsHScrollbar = (totalColWidth > availableContentWidth);
  if (needsHScrollbar) {
    dataAreaHeight = tableSize.y - headerHeight - scrollbarWidth;
    totalDataHeight = static_cast<float>(rowCount) * rowHeight;
    needsScrollbar = totalDataHeight > dataAreaHeight;
    availableContentWidth = needsScrollbar ? tableSize.x - scrollbarWidth : tableSize.x;
  }

  // Draw table background
  Color tableBg = AdjustContainerBackground(panelStyle.background, ctx->style.isDarkTheme);
  ctx->renderer.DrawRectFilled(tablePos, tableSize, tableBg, panelStyle.cornerRadius);

  // --- Handle column resize dragging (must process before header drawing) ---
  float mouseX = ctx->input.MouseX();
  float mouseY = ctx->input.MouseY();
  bool leftPressed = ctx->input.IsMousePressed(0);
  bool leftDown = ctx->input.IsMouseDown(0);

  if (state.draggingColumnResize) {
    if (leftDown) {
      // Apply resize
      float deltaX = mouseX - state.resizeStartX;
      float newWidth = state.resizeStartWidth + deltaX;
      int colIdx = state.resizeColumnIndex;
      if (colIdx >= 0 && colIdx < static_cast<int>(columns.size())) {
        columns[colIdx].width = std::max(columns[colIdx].minWidth, newWidth);
      }
      ctx->desiredCursor = UIContext::CursorType::ResizeH;
    } else {
      // Finished dragging
      state.draggingColumnResize = false;
      state.resizeColumnIndex = -1;
    }
  }

  // --- Draw header row ---
  Vec2 headerPos = tablePos;
  Vec2 headerSize(tableSize.x, headerHeight);
  Color headerBg = panelStyle.headerBackground;
  ctx->renderer.DrawRectFilled(headerPos, headerSize, headerBg, 0.0f);

  // Phase C7: clamp horizontal scroll offset to valid range now that we know widths.
  {
    float scrollableContent = std::max(0.0f, totalColWidth - frozenWidth);
    float scrollableArea = std::max(0.0f, availableContentWidth - frozenWidth);
    float maxScrollX = std::max(0.0f, scrollableContent - scrollableArea);
    state.scrollOffsetX = std::clamp(state.scrollOffsetX, 0.0f, maxScrollX);
  }
  float scrollOffsetX = state.scrollOffsetX;

  // Draw header cells and handle interactions
  // Phase C7: frozen columns drawn at logical X; non-frozen shifted by -scrollOffsetX.
  // Clipping splits the header into frozen and scrollable sub-regions so that
  // non-frozen content doesn't paint over the frozen area.
  float frozenColX = tablePos.x;
  float scrollColX = tablePos.x + frozenWidth - scrollOffsetX;
  constexpr float resizeHitWidth = 4.0f; // +-4px from column border for resize hit area
  constexpr float sortArrowSize = 5.0f;

  for (int c = 0; c < static_cast<int>(columns.size()); ++c) {
    bool frozen = (c < frozenColCount);
    float colW = columns[c].width;
    float colX = frozen ? frozenColX : scrollColX;
    // Push appropriate sub-region clip for this header cell
    if (frozen) {
      ctx->renderer.PushClipRect(Vec2(tablePos.x, tablePos.y),
                                 Vec2(frozenWidth, headerHeight));
    } else {
      ctx->renderer.PushClipRect(Vec2(tablePos.x + frozenWidth, tablePos.y),
                                 Vec2(std::max(0.0f, availableContentWidth - frozenWidth),
                                      headerHeight));
    }
    Vec2 cellPos(colX, tablePos.y);
    Vec2 cellSize(colW, headerHeight);

    // Draw header text (with optional leading icon)
    float textPad = 6.0f;
    float headerIconSize = captionStyle.fontSize;
    float headerIconGap = 4.0f;
    float headerIconSlot = (columns[c].iconCodepoint != 0u) ? (headerIconSize + headerIconGap) : 0.0f;

    // Determine if this column is sorted
    bool isSorted = (state.sortColumn == c);
    Color headerTextColor = captionStyle.color;
    if (isSorted) {
      headerTextColor = ctx->style.button.background.hover; // accent color for sorted column
    }

    if (columns[c].iconCodepoint != 0u) {
      DrawWidgetIcon(ctx, Vec2(colX, tablePos.y), Vec2(columns[c].width, headerHeight),
                     columns[c].iconCodepoint, headerTextColor, headerIconSize,
                     textPad, headerIconGap);
    }

    Vec2 textPos(colX + textPad + headerIconSlot,
                 tablePos.y + (headerHeight - captionStyle.fontSize) * 0.5f);

    ctx->renderer.DrawText(textPos, columns[c].header, headerTextColor, captionStyle.fontSize);

    // Draw sort indicator if this column is sorted
    if (isSorted) {
      Vec2 headerTextSize = MeasureTextCached(ctx, columns[c].header, captionStyle.fontSize);
      float arrowX = colX + textPad + headerIconSlot + headerTextSize.x + 4.0f;
      float arrowCenterY = tablePos.y + headerHeight * 0.5f;

      if (state.sortAscending) {
        // Up arrow (triangle pointing up)
        Vec2 p1(arrowX + sortArrowSize * 0.5f, arrowCenterY - sortArrowSize * 0.4f);
        Vec2 p2(arrowX, arrowCenterY + sortArrowSize * 0.4f);
        Vec2 p3(arrowX + sortArrowSize, arrowCenterY + sortArrowSize * 0.4f);
        ctx->renderer.DrawLine(p1, p2, headerTextColor, 1.2f);
        ctx->renderer.DrawLine(p1, p3, headerTextColor, 1.2f);
        ctx->renderer.DrawLine(p2, p3, headerTextColor, 1.2f);
      } else {
        // Down arrow (triangle pointing down)
        Vec2 p1(arrowX + sortArrowSize * 0.5f, arrowCenterY + sortArrowSize * 0.4f);
        Vec2 p2(arrowX, arrowCenterY - sortArrowSize * 0.4f);
        Vec2 p3(arrowX + sortArrowSize, arrowCenterY - sortArrowSize * 0.4f);
        ctx->renderer.DrawLine(p1, p2, headerTextColor, 1.2f);
        ctx->renderer.DrawLine(p1, p3, headerTextColor, 1.2f);
        ctx->renderer.DrawLine(p2, p3, headerTextColor, 1.2f);
      }
    }

    // Draw column separator line
    if (c < static_cast<int>(columns.size()) - 1) {
      float sepX = colX + colW;
      Color sepColor = panelStyle.borderColor;
      sepColor.a *= 0.5f;
      ctx->renderer.DrawLine(Vec2(sepX, tablePos.y + 2.0f),
                             Vec2(sepX, tablePos.y + headerHeight - 2.0f),
                             sepColor, 1.0f);
    }

    // Handle header click for sorting (only if not dragging resize)
    if (!state.draggingColumnResize && columns[c].sortable) {
      bool hoverHeader = IsMouseOver(ctx, cellPos, cellSize);
      if (hoverHeader && leftPressed) {
        // Check we are not in the resize zone
        float rightEdge = colX + colW;
        bool inResizeZone = std::abs(mouseX - rightEdge) < resizeHitWidth;
        if (!inResizeZone) {
          if (state.sortColumn == c) {
            state.sortAscending = !state.sortAscending;
          } else {
            state.sortColumn = c;
            state.sortAscending = true;
          }
        }
      }
    }

    // Handle resize zone detection (right edge of each column header)
    if (!state.draggingColumnResize) {
      float rightEdge = colX + colW;
      bool inResizeZone = (mouseY >= tablePos.y && mouseY <= tablePos.y + headerHeight &&
                           std::abs(mouseX - rightEdge) < resizeHitWidth);
      if (inResizeZone) {
        ctx->desiredCursor = UIContext::CursorType::ResizeH;
        if (leftPressed) {
          state.draggingColumnResize = true;
          state.resizeColumnIndex = c;
          state.resizeStartX = mouseX;
          state.resizeStartWidth = colW;
        }
      }
    }

    // Pop the per-cell header clip
    ctx->renderer.PopClipRect();

    if (frozen) frozenColX += colW;
    else        scrollColX += colW;
  }

  // Draw header bottom border
  Color borderColor = panelStyle.borderColor;
  ctx->renderer.DrawLine(Vec2(tablePos.x, tablePos.y + headerHeight),
                         Vec2(tablePos.x + tableSize.x, tablePos.y + headerHeight),
                         borderColor, 1.0f);

  // --- Virtual scrolling calculations ---
  // Handle mouse wheel for scroll
  bool hoverTable = IsMouseOver(ctx, tablePos, tableSize);
  if (hoverTable && needsScrollbar && !ctx->scrollConsumedThisFrame) {
    float wheelY = ctx->input.MouseWheelY();
    if (std::abs(wheelY) > 0.001f) {
      state.scrollOffset -= wheelY * SCROLL_SPEED;
      state.scrollOffset = std::clamp(state.scrollOffset, 0.0f,
                                      std::max(0.0f, totalDataHeight - dataAreaHeight));
      ctx->scrollConsumedThisFrame = true;
    }
  }

  // Clamp scroll offset
  state.scrollOffset = std::clamp(state.scrollOffset, 0.0f,
                                  std::max(0.0f, totalDataHeight - dataAreaHeight));

  int startVisibleRow = std::max(0, static_cast<int>(std::floor(state.scrollOffset / rowHeight)));
  int endVisibleRow = std::min(rowCount,
      startVisibleRow + static_cast<int>(std::ceil(dataAreaHeight / rowHeight)) + 1);

  // Push frame context onto table stack
  TableFrameContext frame;
  frame.id = tableId;
  frame.position = tablePos;
  frame.size = tableSize;
  frame.columnsPtr = columns.data();
  frame.columnCount = static_cast<int>(columns.size());
  frame.rowCount = rowCount;
  frame.currentRow = startVisibleRow - 1; // First TableNextRow() will advance to startVisibleRow
  frame.currentCol = 0;
  frame.headerHeight = headerHeight;
  frame.rowHeight = rowHeight;
  frame.scrollOffset = state.scrollOffset;
  frame.startVisibleRow = startVisibleRow;
  frame.endVisibleRow = endVisibleRow;
  frame.sortColumn = state.sortColumn;
  frame.sortAscending = state.sortAscending;
  frame.savedCursor = ctx->cursorPos;
  frame.savedLastItemPos = ctx->lastItemPos;
  frame.savedLastItemSize = ctx->lastItemSize;
  frame.contentWidth = availableContentWidth;
  frame.scrollbarWidth = needsScrollbar ? scrollbarWidth : 0.0f;
  // Phase C7: forward freeze columns + selectedRows pointer
  if (externalState) {
    frame.frozenColumns = std::clamp(externalState->frozenColumns, 0,
                                     static_cast<int>(columns.size()));
    frame.selectedRowsPtr = &externalState->selectedRows;
  } else {
    frame.frozenColumns = 0;
    frame.selectedRowsPtr = nullptr;
  }
  frame.lastSelectedRow = state.lastSelectedRow;
  // Cap anchor to valid range if rowCount changed
  if (frame.lastSelectedRow >= rowCount) frame.lastSelectedRow = -1;
  // Phase C7: forward horizontal scroll fields
  frame.scrollOffsetX = state.scrollOffsetX;
  frame.frozenContentWidth = frozenWidth;
  frame.totalColumnsWidth = totalColWidth;
  frame.dataAreaHeight = dataAreaHeight;
  frame.needsHScrollbar = needsHScrollbar;
  frame.cellClipPushed = false;

  // Push clip rect for data area
  Vec2 dataAreaPos(tablePos.x, tablePos.y + headerHeight);
  Vec2 dataAreaSize(availableContentWidth, dataAreaHeight);
  ctx->renderer.PushClipRect(dataAreaPos, dataAreaSize);
  frame.clipPushed = true;

  ctx->tableStack.push_back(frame);
  // brief 21: scope cell content by the table id (paired with tableStack).
  PushID(id.c_str());

  // Set cursor to the first visible row position
  ctx->cursorPos = Vec2(tablePos.x,
      tablePos.y + headerHeight + startVisibleRow * rowHeight - state.scrollOffset);

  // Sync sort state back to external
  if (externalState) {
    externalState->sortColumn = state.sortColumn;
    externalState->sortAscending = state.sortAscending;
    externalState->scrollOffset = state.scrollOffset;
  }

  return true;
}

void TableNextRow() {
  UIContext *ctx = GetContext();
  if (!ctx || ctx->tableStack.empty())
    return;

  auto &frame = ctx->tableStack.back();
  // Phase C7: pop any pending per-cell clip from the previous row's last cell.
  if (frame.cellClipPushed) {
    ctx->renderer.PopClipRect();
    frame.cellClipPushed = false;
  }
  frame.currentRow++;
  frame.currentCol = 0;

  // Position cursor at the start of this row
  float rowY = frame.position.y + frame.headerHeight +
               frame.currentRow * frame.rowHeight - frame.scrollOffset;
  ctx->cursorPos = Vec2(frame.position.x, rowY);

  // Draw alternating row background for even rows
  if (frame.currentRow >= frame.startVisibleRow && frame.currentRow < frame.endVisibleRow) {
    if (frame.currentRow % 2 == 1) {
      const PanelStyle &panelStyle = ctx->style.panel;
      Color rowBg = panelStyle.background;
      // Subtle alternating: adjust brightness slightly
      if (ctx->style.isDarkTheme) {
        rowBg.r = std::min(1.0f, rowBg.r + 0.015f);
        rowBg.g = std::min(1.0f, rowBg.g + 0.015f);
        rowBg.b = std::min(1.0f, rowBg.b + 0.015f);
      } else {
        rowBg.r = std::max(0.0f, rowBg.r - 0.02f);
        rowBg.g = std::max(0.0f, rowBg.g - 0.02f);
        rowBg.b = std::max(0.0f, rowBg.b - 0.02f);
      }
      ctx->renderer.DrawRectFilled(
          Vec2(frame.position.x, rowY),
          Vec2(frame.contentWidth, frame.rowHeight),
          rowBg, 0.0f);
    }
  }
}

void TableSetCell(int column) {
  UIContext *ctx = GetContext();
  if (!ctx || ctx->tableStack.empty())
    return;

  auto &frame = ctx->tableStack.back();
  if (!frame.columnsPtr || column < 0 || column >= frame.columnCount)
    return;

  // Phase C7: pop any clip from the previous cell so we can push the new one.
  if (frame.cellClipPushed) {
    ctx->renderer.PopClipRect();
    frame.cellClipPushed = false;
  }

  frame.currentCol = column;

  // Phase C7: compute cell X with frozen vs scrollable handling.
  bool frozen = (column < frame.frozenColumns);
  float colX;
  if (frozen) {
    colX = frame.position.x;
    for (int c = 0; c < column; ++c) colX += frame.columnsPtr[c].width;
  } else {
    colX = frame.position.x + frame.frozenContentWidth - frame.scrollOffsetX;
    for (int c = frame.frozenColumns; c < column; ++c) colX += frame.columnsPtr[c].width;
  }

  float rowY = frame.position.y + frame.headerHeight +
               frame.currentRow * frame.rowHeight - frame.scrollOffset;

  // Push a sub-region clip so non-frozen content cannot paint over the frozen
  // region (and vice versa) when scrolled horizontally.
  if (frozen) {
    ctx->renderer.PushClipRect(
        Vec2(frame.position.x, frame.position.y + frame.headerHeight),
        Vec2(frame.frozenContentWidth, frame.dataAreaHeight));
  } else {
    float scrollableW = std::max(0.0f, frame.contentWidth - frame.frozenContentWidth);
    ctx->renderer.PushClipRect(
        Vec2(frame.position.x + frame.frozenContentWidth,
             frame.position.y + frame.headerHeight),
        Vec2(scrollableW, frame.dataAreaHeight));
  }
  frame.cellClipPushed = true;

  // Set cursor to cell position with some padding
  float cellPad = 6.0f;
  ctx->cursorPos = Vec2(colX + cellPad,
      rowY + (frame.rowHeight - ctx->style.GetTextStyle(TypographyStyle::Body).fontSize) * 0.5f);
}

// Phase C7: Row selection helper. Call inside a row to make the entire row
// click-selectable using the table's selectedRows set with Ctrl/Shift modifiers.
bool TableRowSelectable(int rowIndex) {
  UIContext *ctx = GetContext();
  if (!ctx || ctx->tableStack.empty()) return false;
  auto &frame = ctx->tableStack.back();
  if (rowIndex < 0 || rowIndex >= frame.rowCount) return false;
  if (!frame.selectedRowsPtr) return false;

  // Compute row rect in screen space
  float rowY = frame.position.y + frame.headerHeight +
               rowIndex * frame.rowHeight - frame.scrollOffset;
  Vec2 rowPos(frame.position.x, rowY);
  Vec2 rowSize(frame.contentWidth, frame.rowHeight);

  // Skip if outside the visible area
  float visTop = frame.position.y + frame.headerHeight;
  float visBot = frame.position.y + frame.size.y;
  if (rowY + frame.rowHeight < visTop || rowY > visBot)
    return false;

  auto &sel = *frame.selectedRowsPtr;
  bool isSelected = std::find(sel.begin(), sel.end(), rowIndex) != sel.end();

  bool hover = IsMouseOver(ctx, rowPos, rowSize);
  bool clicked = false;

  // Hover highlight (subtle)
  if (hover && !isSelected) {
    Color hoverBg = ctx->style.button.background.hover;
    hoverBg.a *= 0.18f;
    ctx->renderer.DrawRectFilled(rowPos, rowSize, hoverBg, 0.0f);
  }

  // Selection highlight
  if (isSelected) {
    Color selBg = ctx->style.button.background.hover;
    selBg.a *= 0.32f;
    ctx->renderer.DrawRectFilled(rowPos, rowSize, selBg, 0.0f);
    Color selBorder = ctx->style.button.background.hover;
    selBorder.a *= 0.6f;
    ctx->renderer.DrawLine(Vec2(rowPos.x, rowPos.y),
                           Vec2(rowPos.x + rowSize.x, rowPos.y),
                           selBorder, 1.0f);
    ctx->renderer.DrawLine(Vec2(rowPos.x, rowPos.y + rowSize.y),
                           Vec2(rowPos.x + rowSize.x, rowPos.y + rowSize.y),
                           selBorder, 1.0f);
  }

  if (hover && ctx->input.IsMousePressed(0)) {
    bool ctrl = ctx->input.IsKeyDown(SDL_SCANCODE_LCTRL) ||
                ctx->input.IsKeyDown(SDL_SCANCODE_RCTRL);
    bool shift = ctx->input.IsKeyDown(SDL_SCANCODE_LSHIFT) ||
                 ctx->input.IsKeyDown(SDL_SCANCODE_RSHIFT);

    if (shift && frame.lastSelectedRow >= 0) {
      // Range select: replace with [anchor..rowIndex]
      sel.clear();
      int lo = std::min(frame.lastSelectedRow, rowIndex);
      int hi = std::max(frame.lastSelectedRow, rowIndex);
      for (int r = lo; r <= hi; ++r) sel.push_back(r);
    } else if (ctrl) {
      // Toggle membership; update anchor only on add
      auto it = std::find(sel.begin(), sel.end(), rowIndex);
      if (it != sel.end()) {
        sel.erase(it);
      } else {
        sel.push_back(rowIndex);
        frame.lastSelectedRow = rowIndex;
      }
    } else {
      // Replace selection with single row
      sel.clear();
      sel.push_back(rowIndex);
      frame.lastSelectedRow = rowIndex;
    }
    clicked = true;
  }

  return clicked;
}

void EndTable() {
  UIContext *ctx = GetContext();
  if (!ctx || ctx->tableStack.empty())
    return;

  auto frame = ctx->tableStack.back();
  ctx->tableStack.pop_back();
  // brief 21: pop the scope pushed in BeginTable.
  PopID();

  // Phase C7: pop any pending per-cell clip from the last cell of the last row.
  if (frame.cellClipPushed) {
    ctx->renderer.PopClipRect();
    frame.cellClipPushed = false;
  }

  // Pop clip rect for data area
  if (frame.clipPushed) {
    ctx->renderer.PopClipRect();
  }

  // Phase C7: draw visual freeze separator after the last frozen column
  if (frame.frozenColumns > 0 && frame.frozenColumns < frame.columnCount &&
      frame.columnsPtr) {
    float fx = frame.position.x;
    int fc = std::min(frame.frozenColumns, frame.columnCount);
    for (int c = 0; c < fc; ++c) fx += frame.columnsPtr[c].width;
    if (fx < frame.position.x + frame.contentWidth) {
      Color sepCol = ctx->style.panel.borderColor;
      sepCol.a *= 0.85f;
      ctx->renderer.DrawLine(
          Vec2(fx, frame.position.y + frame.headerHeight),
          Vec2(fx, frame.position.y + frame.size.y),
          sepCol, 1.5f);
    }
  }

  // Draw vertical scrollbar if needed
  auto &state = ctx->tableStates[frame.id];
  // Phase C7: persist anchor row id back to per-table state
  state.lastSelectedRow = frame.lastSelectedRow;
  // Phase C7: use the dataAreaHeight that already excludes h-scrollbar space.
  float dataAreaHeight = frame.dataAreaHeight;
  float totalDataHeight = static_cast<float>(frame.rowCount) * frame.rowHeight;
  bool needsScrollbar = totalDataHeight > dataAreaHeight;

  if (needsScrollbar) {
    Vec2 barPos(frame.position.x + frame.contentWidth,
                frame.position.y + frame.headerHeight);
    Vec2 barSize(frame.scrollbarWidth, dataAreaHeight);
    DrawScrollbar(ctx, barPos, barSize, totalDataHeight, dataAreaHeight,
                  state.scrollOffset, state.draggingScrollbar,
                  state.dragStartMouse, state.dragStartScroll,
                  state.draggingScrollbar, true);
  }

  // Phase C7: horizontal scrollbar for non-frozen columns.
  if (frame.needsHScrollbar) {
    float scrollableContent = std::max(0.0f, frame.totalColumnsWidth - frame.frozenContentWidth);
    float scrollableArea = std::max(0.0f, frame.contentWidth - frame.frozenContentWidth);
    Vec2 hbarPos(frame.position.x + frame.frozenContentWidth,
                 frame.position.y + frame.headerHeight + dataAreaHeight);
    Vec2 hbarSize(scrollableArea, SCROLLBAR_WIDTH);
    DrawScrollbar(ctx, hbarPos, hbarSize, scrollableContent, scrollableArea,
                  state.scrollOffsetX, state.draggingHScrollbar,
                  state.hDragStartMouse, state.hDragStartScroll,
                  state.draggingHScrollbar, false);
  }

  // Draw table outer border
  const PanelStyle &panelStyle = ctx->style.panel;
  ctx->renderer.DrawRect(frame.position, frame.size, panelStyle.borderColor,
                         panelStyle.cornerRadius);

  // Phase C7: sync horizontal scroll offset back to external state.
  if (auto it = ctx->tableStates.find(frame.id); it != ctx->tableStates.end()) {
    // (state was retrieved above; nothing extra needed here — external sync
    //  happens implicitly the next frame because BeginTable copies from state
    //  if not initialized; for already-initialized tables we leave external
    //  scrollOffsetX read-only from user perspective.)
  }

  // Restore cursor state and advance
  ctx->cursorPos = frame.savedCursor;
  ctx->lastItemPos = frame.position;
  ctx->lastItemSize = frame.size;
  AdvanceCursor(ctx, frame.size);
}

// ═════════════════════════════════════════════════════════════════════════════
// BRIEF 16 — GridView, DataGrid, ExpanderList
// ═════════════════════════════════════════════════════════════════════════════

namespace {

// Scrollbar drag bookkeeping for widgets that don't own a state struct (GridView).
struct ScrollbarDrag {
  bool dragging = false;
  Vec2 startMouse{0.0f, 0.0f};
  float startScroll = 0.0f;
};
std::unordered_map<uint32_t, ScrollbarDrag> g_gridScroll;

// Per-DataGrid persistent state (column order/widths + edit/reorder transients).
struct DataGridLocal {
  std::vector<int> order;    // display position -> logical column index
  std::vector<float> widths; // logical column index -> current width
  TableState table;          // underlying Table state (sort/scroll/frozen/resize)
  // Inline editing.
  int editRow = -1;
  int editLogicalCol = -1;
  std::string editBuf;
  bool editStarted = false;  // request focus on the editor next render
  // Double-click detection.
  int lastClickRow = -1, lastClickCol = -1;
  float lastClickTime = -1.0f;
  // Header drag-to-reorder.
  int dragFromPos = -1;      // display pos where the press started (-1 = none pending)
  float dragStartX = 0.0f;
  bool headerDragging = false;
  int sortSnapCol = -1;
  bool sortSnapAsc = true;
};
std::unordered_map<uint32_t, DataGridLocal> g_dataGrids;

bool DG_IsTruthy(const std::string &s) {
  if (s.empty()) return false;
  char c = static_cast<char>(std::tolower(static_cast<unsigned char>(s[0])));
  return c == '1' || c == 't' || c == 'y' || c == 'v'; // 1/true/yes/verdadero
}

} // anonymous namespace

void GridView(const std::string &id, int itemCount, Vec2 itemSize,
              const std::function<void(int)> &itemBuilder, float gap,
              float minItemWidth) {
  UIContext *ctx = GetContext();
  if (!ctx || itemCount <= 0)
    return;
  if (gap < 0.0f) gap = 0.0f;

  uint32_t gridId = GenerateId("GRIDVIEW:", id.c_str());
  ctx->focusableWidgets.push_back(gridId);

  Vec2 avail = GetCurrentAvailableSpace(ctx);
  float viewW = avail.x > 0.0f ? avail.x
                               : ctx->renderer.GetViewportSize().x - ctx->cursorPos.x;
  float viewH = avail.y > 0.0f ? avail.y : 300.0f;
  if (viewW < 1.0f) viewW = 1.0f;
  if (viewH < 1.0f) viewH = 1.0f;
  Vec2 origin = ctx->cursorPos;

  float cellH = itemSize.y > 0.0f ? itemSize.y : 100.0f;
  // Same column-count formula as BeginUniformGrid (brief 19).
  float refW = (minItemWidth > 0.0f) ? minItemWidth : itemSize.x;
  if (refW < 1.0f) refW = 1.0f;

  auto computeLayout = [&](float widthAvail, int &cols, float &cellW) {
    cols = static_cast<int>(std::floor((widthAvail + gap) / (refW + gap)));
    if (cols < 1) cols = 1;
    if (minItemWidth > 0.0f)
      cellW = (widthAvail - static_cast<float>(cols - 1) * gap) / static_cast<float>(cols);
    else
      cellW = itemSize.x;
    if (cellW < 1.0f) cellW = 1.0f;
  };

  int cols = 1;
  float cellW = refW;
  computeLayout(viewW, cols, cellW);
  float rowStride = cellH + gap;
  int rows = (itemCount + cols - 1) / cols;
  float totalH = static_cast<float>(rows) * cellH + static_cast<float>(rows - 1) * gap;
  if (totalH < 0.0f) totalH = 0.0f;

  bool needsBar = totalH > viewH;
  float contentW = needsBar ? viewW - SCROLLBAR_WIDTH : viewW;
  if (needsBar) {
    // Re-flow against the narrower content area so tiles don't slip under the bar.
    computeLayout(contentW, cols, cellW);
    rows = (itemCount + cols - 1) / cols;
    totalH = static_cast<float>(rows) * cellH + static_cast<float>(rows - 1) * gap;
  }

  float &scroll = ctx->floatStates[gridId];
  uint32_t selId = GenerateId("GRIDSEL:", id.c_str());
  int &sel = ctx->intStates.try_emplace(selId, -1).first->second;
  if (sel >= itemCount) sel = itemCount - 1;

  bool focused = (ctx->focusedWidgetId == gridId);
  if (focused) {
    int cur = sel < 0 ? 0 : sel;
    bool moved = false;
    if (ctx->input.IsKeyPressed(SDL_SCANCODE_RIGHT)) { cur = std::min(itemCount - 1, cur + 1); moved = true; }
    if (ctx->input.IsKeyPressed(SDL_SCANCODE_LEFT))  { cur = std::max(0, cur - 1); moved = true; }
    if (ctx->input.IsKeyPressed(SDL_SCANCODE_DOWN))  { if (cur + cols < itemCount) cur += cols; moved = true; }
    if (ctx->input.IsKeyPressed(SDL_SCANCODE_UP))    { if (cur - cols >= 0) cur -= cols; moved = true; }
    if (moved) {
      sel = cur;
      int r = cur / cols;
      float top = static_cast<float>(r) * rowStride;
      float bot = top + cellH;
      if (top < scroll) scroll = top;
      else if (bot > scroll + viewH) scroll = bot - viewH;
    }
  }

  bool hoverGrid = IsMouseOver(ctx, origin, Vec2(viewW, viewH));
  if (hoverGrid && needsBar && !ctx->scrollConsumedThisFrame) {
    float wy = ctx->input.MouseWheelY();
    if (std::abs(wy) > 0.001f) {
      scroll -= wy * SCROLL_SPEED;
      ctx->scrollConsumedThisFrame = true;
    }
  }
  scroll = std::clamp(scroll, 0.0f, std::max(0.0f, totalH - viewH));

  // Background well.
  Color bg = AdjustListSurfaceBackground(ctx->style.panel.background, ctx->style.isDarkTheme);
  ctx->renderer.DrawRectFilled(origin, Vec2(viewW, viewH), bg, ctx->style.panel.cornerRadius);

  ctx->renderer.PushClipRect(origin, Vec2(contentW, viewH));
  int startRow = std::max(0, static_cast<int>(std::floor(scroll / rowStride)));
  int endRow = std::min(rows, static_cast<int>(std::ceil((scroll + viewH) / rowStride)) + 1);

  Vec2 savedCursor = ctx->cursorPos;
  PushID(id.c_str());
  for (int r = startRow; r < endRow; ++r) {
    for (int c = 0; c < cols; ++c) {
      int idx = r * cols + c;
      if (idx >= itemCount) break;
      float cx = origin.x + static_cast<float>(c) * (cellW + gap);
      float cy = origin.y + static_cast<float>(r) * rowStride - scroll;
      Vec2 cellPos(cx, cy);
      Vec2 cellSize(cellW, cellH);

      bool cellHover = IsMouseOver(ctx, cellPos, cellSize);
      if (sel == idx) {
        Color s = ctx->style.button.background.hover; s.a *= 0.30f;
        ctx->renderer.DrawRectFilled(cellPos, cellSize, s, 4.0f);
      } else if (cellHover) {
        Color h = ctx->style.button.background.hover; h.a *= 0.14f;
        ctx->renderer.DrawRectFilled(cellPos, cellSize, h, 4.0f);
      }
      if (cellHover && ctx->input.IsMousePressed(0)) {
        sel = idx;
        ctx->focusedWidgetId = gridId;
      }

      PushID(idx);
      ctx->cursorPos = cellPos;
      LayoutConstraints cc{};
      cc.width = SizeConstraint::Fixed;
      cc.fixedWidth = cellW;
      SetNextConstraints(cc);
      if (itemBuilder) itemBuilder(idx);
      PopID();
    }
  }
  PopID();
  ctx->renderer.PopClipRect();

  if (focused)
    DrawFocusRing(ctx, origin, Vec2(viewW, viewH), ctx->style.panel.cornerRadius);

  if (needsBar) {
    auto &gd = g_gridScroll[gridId];
    Vec2 barPos(origin.x + contentW, origin.y);
    Vec2 barSize(SCROLLBAR_WIDTH, viewH);
    DrawScrollbar(ctx, barPos, barSize, totalH, viewH, scroll, gd.dragging,
                  gd.startMouse, gd.startScroll, gd.dragging, true);
  }

  ctx->cursorPos = savedCursor;
  ctx->lastItemPos = origin;
  ctx->lastItemSize = Vec2(viewW, viewH);
  bool active = hoverGrid && ctx->input.IsMouseDown(0);
  SetLastItem(gridId, origin, origin + Vec2(viewW, viewH), hoverGrid, active, focused, false);
  AdvanceCursor(ctx, Vec2(viewW, viewH));
}

DataGridResult DataGrid(const std::string &id, const std::vector<DataColumn> &cols,
                        int rowCount,
                        const std::function<std::string(int, int)> &getCell,
                        const std::function<void(int, int, const std::string &)> &setCell) {
  DataGridResult result;
  UIContext *ctx = GetContext();
  if (!ctx || cols.empty())
    return result;

  uint32_t dgId = GenerateId("DATAGRID:", id.c_str());
  auto &st = g_dataGrids[dgId];
  int nCols = static_cast<int>(cols.size());

  if (static_cast<int>(st.order.size()) != nCols) {
    st.order.resize(nCols);
    for (int i = 0; i < nCols; ++i) st.order[i] = i;
  }
  if (static_cast<int>(st.widths.size()) != nCols) {
    st.widths.resize(nCols);
    for (int i = 0; i < nCols; ++i) st.widths[i] = cols[i].width;
  }
  // Defensive: keep order indices valid even if a stale permutation survives a
  // column-count change of equal size.
  for (int o : st.order) {
    if (o < 0 || o >= nCols) {
      for (int i = 0; i < nCols; ++i) st.order[i] = i;
      break;
    }
  }

  // Build Table columns in current display order.
  std::vector<TableColumn> tcols(nCols);
  for (int p = 0; p < nCols; ++p) {
    int lc = st.order[p];
    tcols[p].header = cols[lc].header;
    tcols[p].width = st.widths[lc];
    tcols[p].minWidth = 40.0f;
    tcols[p].sortable = cols[lc].sortable;
  }

  Vec2 gridPos = ctx->cursorPos;
  // Snapshot sort so a confirmed reorder can neutralise the sort that Table
  // toggles on header press (press == sort in Table; drag == reorder for us).
  st.sortSnapCol = st.table.sortColumn;
  st.sortSnapAsc = st.table.sortAscending;

  const TextStyle &bodyStyle = ctx->style.GetTextStyle(TypographyStyle::Body);
  const float ascender = ctx->renderer.GetFontAscender();
  const float visualCenterOffset = (ascender - 0.7f * 0.5f) * bodyStyle.fontSize;

  if (BeginTable(id, tcols, rowCount, Vec2(0, 0), &st.table)) {
    // Persist any width change from interactive resize back to logical store.
    for (int p = 0; p < nCols; ++p) st.widths[st.order[p]] = tcols[p].width;

    if (!ctx->tableStack.empty()) {
      int startRow = ctx->tableStack.back().startVisibleRow;
      int endRow = ctx->tableStack.back().endVisibleRow;

      for (int row = startRow; row < endRow && row < rowCount; ++row) {
        TableNextRow();
        TableRowSelectable(row); // row selection via Table's selectedRows
        for (int p = 0; p < nCols; ++p) {
          TableSetCell(p);
          int lc = st.order[p];
          const DataColumn &dc = cols[lc];

          // Exact cell geometry (mirrors TableSetCell's frozen/scroll math).
          const TableFrameContext &fr = ctx->tableStack.back();
          bool frozen = (p < fr.frozenColumns);
          float colX;
          if (frozen) {
            colX = fr.position.x;
            for (int c = 0; c < p; ++c) colX += fr.columnsPtr[c].width;
          } else {
            colX = fr.position.x + fr.frozenContentWidth - fr.scrollOffsetX;
            for (int c = fr.frozenColumns; c < p; ++c) colX += fr.columnsPtr[c].width;
          }
          float colW = fr.columnsPtr[p].width;
          float rowY = fr.position.y + fr.headerHeight +
                       static_cast<float>(row) * fr.rowHeight - fr.scrollOffset;
          Vec2 cellPos(colX, rowY);
          Vec2 cellSz(colW, fr.rowHeight);
          std::string val = getCell ? getCell(row, lc) : std::string();

          bool isEditingCell = (st.editRow == row && st.editLogicalCol == lc);

          if (dc.type == DataColumn::Type::Bool) {
            // Bool: interactive checkbox if editable, static glyph otherwise.
            float box = 16.0f;
            Vec2 bp(colX + 6.0f, rowY + (fr.rowHeight - box) * 0.5f);
            bool checked = DG_IsTruthy(val);
            Color border = InputFieldBorder(ctx, false);
            if (checked) {
              Color accent = ctx->style.button.background.normal;
              ctx->renderer.DrawRectFilled(bp, Vec2(box, box), accent, 3.0f);
              ctx->renderer.DrawLine(Vec2(bp.x + 3.5f, bp.y + box * 0.55f),
                                     Vec2(bp.x + box * 0.42f, bp.y + box - 3.5f),
                                     Color(1, 1, 1, 1), 1.6f);
              ctx->renderer.DrawLine(Vec2(bp.x + box * 0.42f, bp.y + box - 3.5f),
                                     Vec2(bp.x + box - 3.0f, bp.y + 3.5f),
                                     Color(1, 1, 1, 1), 1.6f);
            } else {
              ctx->renderer.DrawRect(bp, Vec2(box, box), border, 3.0f);
            }
            if (dc.editable) {
              bool h = IsMouseOver(ctx, bp, Vec2(box, box));
              if (h && ctx->input.IsMousePressed(0) && setCell) {
                setCell(row, lc, checked ? "false" : "true");
                result.editedRow = row;
                result.editedCol = lc;
              }
            }
          } else if (isEditingCell) {
            // Inline text editor (fallback for Text/Number/Choice — brief 14
            // NumberBox/ComboBox not available yet).
            float pad = 3.0f;
            Vec2 edPos(colX + pad, rowY + (fr.rowHeight - 26.0f) * 0.5f);
            TextInput(std::string("##DGEDIT"), &st.editBuf,
                      std::max(20.0f, colW - 2.0f * pad), false,
                      std::optional<Vec2>(edPos), nullptr, 0);
            uint32_t editorId = ctx->lastItem.id;
            if (st.editStarted) {
              ctx->focusedWidgetId = editorId;
              st.editStarted = false;
            }
            bool commit = false, cancel = false;
            if (ctx->input.IsKeyPressed(SDL_SCANCODE_RETURN) ||
                ctx->input.IsKeyPressed(SDL_SCANCODE_KP_ENTER))
              commit = true;
            if (ctx->input.IsKeyPressed(SDL_SCANCODE_ESCAPE))
              cancel = true;
            if (ctx->lastItem.deactivated) // blur
              commit = true;
            if (commit) {
              if (setCell) setCell(row, lc, st.editBuf);
              result.editedRow = row;
              result.editedCol = lc;
              st.editRow = -1; st.editLogicalCol = -1;
            } else if (cancel) {
              st.editRow = -1; st.editLogicalCol = -1;
              ctx->focusedWidgetId = 0;
            }
          } else {
            // Static text + double-click to begin editing.
            Vec2 textPos(colX + 6.0f,
                         rowY + fr.rowHeight * 0.5f - visualCenterOffset);
            ctx->renderer.DrawText(textPos, val, bodyStyle.color, bodyStyle.fontSize);
            if (dc.editable) {
              bool h = IsMouseOver(ctx, cellPos, cellSz);
              if (h && ctx->input.IsMousePressed(0)) {
                bool dbl = (st.lastClickRow == row && st.lastClickCol == lc &&
                            (ctx->time - st.lastClickTime) < 0.40f);
                st.lastClickRow = row; st.lastClickCol = lc;
                st.lastClickTime = ctx->time;
                if (dbl) {
                  // Commit any edit pending on another cell before switching.
                  if (st.editRow >= 0 &&
                      (st.editRow != row || st.editLogicalCol != lc)) {
                    if (setCell) setCell(st.editRow, st.editLogicalCol, st.editBuf);
                  }
                  st.editRow = row; st.editLogicalCol = lc;
                  st.editBuf = val; st.editStarted = true;
                }
              }
            }
          }
        }
      }
      // Commit a pending edit whose row scrolled out of view (implicit blur).
      if (st.editRow >= 0 && (st.editRow < startRow || st.editRow >= endRow)) {
        if (setCell) setCell(st.editRow, st.editLogicalCol, st.editBuf);
        st.editRow = -1; st.editLogicalCol = -1;
      }
    }
  }
  EndTable();

  // ── Header drag-to-reorder (post-EndTable; Table doesn't handle reorder). ──
  {
    const float headerH = 28.0f; // matches Table's headerHeight
    int frozen = std::clamp(st.table.frozenColumns, 0, nCols);
    float frozenW = 0.0f;
    for (int c = 0; c < frozen; ++c) frozenW += tcols[c].width;
    auto colDispX = [&](int p) -> float {
      if (p < frozen) {
        float x = gridPos.x;
        for (int c = 0; c < p; ++c) x += tcols[c].width;
        return x;
      }
      float x = gridPos.x + frozenW - st.table.scrollOffsetX;
      for (int c = frozen; c < p; ++c) x += tcols[c].width;
      return x;
    };
    float mx = ctx->input.MouseX();
    float my = ctx->input.MouseY();
    bool inHeader = (my >= gridPos.y && my <= gridPos.y + headerH);

    if (st.dragFromPos < 0 && inHeader && ctx->input.IsMousePressed(0)) {
      for (int p = 0; p < nCols; ++p) {
        float x0 = colDispX(p), x1 = x0 + tcols[p].width;
        if (mx >= x0 && mx < x1) {
          // Skip the resize hit zone near the right border (owned by Table).
          if (std::abs(mx - x1) >= 4.0f) {
            st.dragFromPos = p;
            st.dragStartX = mx;
          }
          break;
        }
      }
    }

    if (st.dragFromPos >= 0 && ctx->input.IsMouseDown(0)) {
      if (!st.headerDragging && std::abs(mx - st.dragStartX) > 6.0f)
        st.headerDragging = true;
      if (st.headerDragging) {
        // Insertion indicator: nearest column boundary to the cursor.
        int target = nCols;
        for (int p = 0; p < nCols; ++p) {
          float mid = colDispX(p) + tcols[p].width * 0.5f;
          if (mx < mid) { target = p; break; }
        }
        float lineX = (target < nCols)
                          ? colDispX(target)
                          : colDispX(nCols - 1) + tcols[nCols - 1].width;
        Color accent = ctx->style.button.background.hover;
        ctx->renderer.DrawLine(Vec2(lineX, gridPos.y),
                               Vec2(lineX, gridPos.y + headerH), accent, 2.0f);
        float sx = colDispX(st.dragFromPos);
        Color tint = accent; tint.a *= 0.18f;
        ctx->renderer.DrawRectFilled(Vec2(sx, gridPos.y),
                                     Vec2(tcols[st.dragFromPos].width, headerH), tint, 0.0f);
      }
    }

    if (st.dragFromPos >= 0 && !ctx->input.IsMouseDown(0)) {
      if (st.headerDragging) {
        int target = nCols;
        for (int p = 0; p < nCols; ++p) {
          float mid = colDispX(p) + tcols[p].width * 0.5f;
          if (mx < mid) { target = p; break; }
        }
        int from = st.dragFromPos;
        if (target > from) target -= 1; // account for removal shift
        target = std::clamp(target, 0, nCols - 1);
        if (target != from) {
          int moved = st.order[from];
          st.order.erase(st.order.begin() + from);
          st.order.insert(st.order.begin() + target, moved);
          // Neutralise the accidental sort toggle from the initial press.
          st.table.sortColumn = st.sortSnapCol;
          st.table.sortAscending = st.sortSnapAsc;
        }
      }
      st.headerDragging = false;
      st.dragFromPos = -1;
    }
  }

  // Report sort in the caller's ORIGINAL column space.
  result.ascending = st.table.sortAscending;
  if (st.table.sortColumn >= 0 && st.table.sortColumn < nCols)
    result.sortedColumn = st.order[st.table.sortColumn];
  else
    result.sortedColumn = -1;
  return result;
}

void ExpanderList(const std::string &id, int itemCount,
                  const std::function<std::string(int)> &headerFn,
                  const std::function<void(int)> &bodyFn, bool accordion) {
  UIContext *ctx = GetContext();
  if (!ctx || itemCount <= 0)
    return;

  uint32_t listId = GenerateId("EXPLIST:", id.c_str());
  int *openIdx = nullptr;
  if (accordion)
    openIdx = &ctx->intStates.try_emplace(listId, -1).first->second;

  PushID(id.c_str());
  for (int i = 0; i < itemCount; ++i) {
    PushID(i);
    std::string header = headerFn ? headerFn(i) : std::string();

    uint32_t subId = GenerateId("EXPITEM:", id.c_str(), std::to_string(i).c_str());
    bool open;
    if (accordion)
      open = (*openIdx == i);
    else
      open = ctx->boolStates.try_emplace(subId, false).first->second;

    bool before = open;
    // CollapsingHeader toggles `open` on click/keyboard and applies an auto-indent
    // to following siblings (which our body inherits, then the next header resets).
    CollapsingHeader(header, &open);

    if (accordion) {
      if (open && !before) *openIdx = i;
      else if (!open && before && *openIdx == i) *openIdx = -1;
    } else {
      ctx->boolStates[subId] = open;
    }

    // Body height animation (brief 10) degrades to snap: show/hide instantly.
    if (open && bodyFn)
      bodyFn(i);

    PopID();
  }
  PopID();
}

} // namespace FluentUI
