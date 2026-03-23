#include "UI/Widgets.h"
#include "UI/WidgetHelpers.h"
#include "Theme/FluentTheme.h"
#include "core/Animation.h"
#include "core/Context.h"
#include "core/Renderer.h"
#include "core/WidgetNodes.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <optional>

namespace FluentUI {

bool BeginListView(const std::string &id, const Vec2 &size, int *selectedItem,
                   const std::vector<std::string> &items, std::optional<Vec2> pos) {
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
    state.itemSize = Vec2(size.x > 0.0f ? size.x : 200.0f, 32.0f);
    state.scrollOffset = 0.0f;  // Inicializar scroll offset
    state.initialized = true;
  } else {
    if (selectedItem) {
      state.selectedItem = *selectedItem;
    }
    if (size.x > 0.0f) {
      state.itemSize.x = size.x;
    }
  }

  Vec2 listViewSize = size;
  if (listViewSize.x <= 0.0f) {
    listViewSize.x = 200.0f;
  }
  if (listViewSize.y <= 0.0f) {
    listViewSize.y = items.size() * state.itemSize.y;
  }

  // Resolver posición: usar pos si se proporciona, sino usar cursor
  Vec2 listViewPos;
  if (pos.has_value()) {
    listViewPos = pos.value();
    state.useAbsolutePos = true;
    state.absolutePos = pos.value();
  } else {
    listViewPos = ctx->cursorPos;
    listViewPos = ResolveAbsolutePosition(ctx, listViewPos, listViewSize);
    state.useAbsolutePos = false;
  }
  const PanelStyle &panelStyle = ctx->style.panel;
  const TextStyle &textStyle = ctx->style.GetTextStyle(TypographyStyle::Body);

  // Dibujar fondo del ListView con contraste más pronunciado - sin borde
  Color listBg = AdjustContainerBackground(panelStyle.background, ctx->style.isDarkTheme);
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

    // Dibujar texto del item
    Vec2 textPos(itemPos.x + panelStyle.padding.x,
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

bool BeginListView(const std::string &id, const Vec2 &size, std::vector<int> *selectedItems,
                   const std::vector<std::string> &items, std::optional<Vec2> pos) {
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
    state.itemSize = Vec2(size.x > 0.0f ? size.x : 200.0f, 32.0f);
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
    }
  }

  Vec2 listViewSize = size;
  if (listViewSize.x <= 0.0f) {
    listViewSize.x = 200.0f;
  }
  if (listViewSize.y <= 0.0f) {
    listViewSize.y = items.size() * state.itemSize.y;
  }

  // Resolver posición
  Vec2 listViewPos;
  if (pos.has_value()) {
    listViewPos = pos.value();
    state.useAbsolutePos = true;
    state.absolutePos = pos.value();
  } else {
    listViewPos = ctx->cursorPos;
    listViewPos = ResolveAbsolutePosition(ctx, listViewPos, listViewSize);
    state.useAbsolutePos = false;
  }

  const PanelStyle &panelStyle = ctx->style.panel;
  const TextStyle &textStyle = ctx->style.GetTextStyle(TypographyStyle::Body);

  // Dibujar fondo del ListView
  Color listBg = AdjustContainerBackground(panelStyle.background, ctx->style.isDarkTheme);
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

    // Dibujar texto del item
    Vec2 textPos(itemPos.x + panelStyle.padding.x,
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

  if (!state.initialized) {
    state.itemSize = Vec2(size.x > 0.0f ? size.x : 200.0f, 24.0f);
    state.indentSize = 20.0f;
    state.expandButtonSize = 14.0f;
    state.initialized = true;
  } else {
    if (size.x > 0.0f) {
      state.itemSize.x = size.x;
    }
  }

  Vec2 treeViewSize = size;
  if (treeViewSize.x <= 0.0f) {
    treeViewSize.x = 200.0f;
  }
  if (treeViewSize.y <= 0.0f) {
    treeViewSize.y = 300.0f;
  }

  // Resolver posición: usar pos si se proporciona, sino usar cursor
  Vec2 treeViewPos;
  if (pos.has_value()) {
    treeViewPos = pos.value();
    state.useAbsolutePos = true;
    state.absolutePos = pos.value();
  } else {
    treeViewPos = ctx->cursorPos;
    treeViewPos = ResolveAbsolutePosition(ctx, treeViewPos, treeViewSize);
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

  // Dibujar fondo del TreeView
  Color treeBg = AdjustContainerBackground(panelStyle.background, ctx->style.isDarkTheme);
  ctx->renderer.DrawRectFilled(treeViewPos, treeViewSize, treeBg,
                               panelStyle.cornerRadius);

  // Aplicar clipping
  ctx->renderer.PushClipRect(treeViewPos, treeViewSize);

  // Configurar cursor para el contenido del TreeView
  // Añadir un pequeño padding interno para que no toque los bordes del fondo
  Vec2 innerPadding = panelStyle.padding * 0.5f;
  ctx->cursorPos = treeViewPos + innerPadding;
  ctx->lastItemPos = ctx->cursorPos;
  ctx->treeViewDepth = 0;

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

  // Pop from widget tree (Phase 1)
  ctx->widgetTree.PopParent();

  // Capture content end cursor before closing layout
  Vec2 contentEndCursor = ctx->cursorPos;

  // Cerrar el layout vertical
  if (!ctx->layoutStack.empty()) {
    EndVertical(false); // No avanzar el cursor del padre aquí
  }

  // Remover clipping
  ctx->renderer.PopClipRect();

  // Obtener el tamaño final del contenido
  if (ctx->currentTreeViewId != 0) {
    auto it = ctx->treeViewStates.find(ctx->currentTreeViewId);
    if (it != ctx->treeViewStates.end()) {
      const auto &state = it->second;

      // Avanzar cursor solo si NO se usa posición absoluta
      if (!state.useAbsolutePos) {
        Vec2 treeViewPos = ctx->lastItemPos - ctx->style.panel.padding;
        // Calculate real content height from cursor delta
        float contentHeight = contentEndCursor.y - (treeViewPos.y + ctx->style.panel.padding.y);
        float treeViewHeight = std::max(contentHeight + ctx->style.panel.padding.y * 2.0f, state.itemSize.y);
        ctx->lastItemPos = treeViewPos;
        AdvanceCursor(ctx, Vec2(state.itemSize.x, treeViewHeight));
      }
    }
  }

  ctx->currentTreeViewId = 0;
  ctx->treeViewDepth = 0;
}

bool TreeNode(const std::string &id, const std::string &label, bool *isOpen,
              bool *isSelected) {
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

    // Dibujar botón de expand/collapse
    Color buttonBg =
        hoverButton ? panelStyle.headerBackground : Color(0,0,0,0);
    if (buttonBg.a > 0.0f) {
      ctx->renderer.DrawRectFilled(buttonPos, buttonSize, buttonBg, 2.0f);
    }
    ctx->renderer.DrawRect(buttonPos, buttonSize, panelStyle.borderColor, 2.0f);

    // Dibujar símbolo + o - usando rectángulos rellenos para máxima nitidez y alineación
    Vec2 center(buttonPos.x + buttonSize.x * 0.5f,
                buttonPos.y + buttonSize.y * 0.5f);
    float lineLength = buttonSize.x * 0.4f;
    
    float thickness = 1.2f;
    float halfLen = std::round(lineLength * 0.5f);
    Color symbolColor = textStyle.color;
    symbolColor.a *= 0.8f; // Un poco más suave que el texto principal

    // Línea horizontal (siempre presente)
    ctx->renderer.DrawRectFilled(
        Vec2(std::round(center.x - halfLen), std::round(center.y - thickness * 0.5f)),
        Vec2(halfLen * 2.0f, thickness),
        symbolColor, 0.0f);

    // Línea vertical (solo si está cerrado para formar el '+')
    if (!nodeIsOpen) {
        ctx->renderer.DrawRectFilled(
            Vec2(std::round(center.x - thickness * 0.5f), std::round(center.y - halfLen)),
            Vec2(thickness, halfLen * 2.0f),
            symbolColor, 0.0f);
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

  // Dibujar texto del nodo
  float textX =
      itemPos.x + (hasChildren ? state.expandButtonSize + 4.0f : 0.0f) + 4.0f;
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
  if (tableSize.x <= 0.0f) {
    tableSize.x = totalColWidth + SCROLLBAR_WIDTH;
  }
  if (tableSize.y <= 0.0f) {
    tableSize.y = 300.0f;
  }

  // Position
  Vec2 tablePos = ctx->cursorPos;
  tablePos = ResolveAbsolutePosition(ctx, tablePos, tableSize);

  const PanelStyle &panelStyle = ctx->style.panel;
  const TextStyle &captionStyle = ctx->style.GetTextStyle(TypographyStyle::Caption);

  // Calculate dimensions
  float headerHeight = 28.0f;
  float rowHeight = 28.0f;
  float scrollbarWidth = SCROLLBAR_WIDTH;
  float dataAreaHeight = tableSize.y - headerHeight;
  float totalDataHeight = static_cast<float>(rowCount) * rowHeight;
  bool needsScrollbar = totalDataHeight > dataAreaHeight;
  float availableContentWidth = needsScrollbar ? tableSize.x - scrollbarWidth : tableSize.x;

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

  // Draw header cells and handle interactions
  float colX = tablePos.x;
  constexpr float resizeHitWidth = 4.0f; // +-4px from column border for resize hit area
  constexpr float sortArrowSize = 5.0f;

  for (int c = 0; c < static_cast<int>(columns.size()); ++c) {
    float colW = columns[c].width;
    Vec2 cellPos(colX, tablePos.y);
    Vec2 cellSize(colW, headerHeight);

    // Draw header text
    float textPad = 6.0f;
    Vec2 textPos(colX + textPad,
                 tablePos.y + (headerHeight - captionStyle.fontSize) * 0.5f);

    // Determine if this column is sorted
    bool isSorted = (state.sortColumn == c);
    Color headerTextColor = captionStyle.color;
    if (isSorted) {
      headerTextColor = ctx->style.button.background.hover; // accent color for sorted column
    }

    ctx->renderer.DrawText(textPos, columns[c].header, headerTextColor, captionStyle.fontSize);

    // Draw sort indicator if this column is sorted
    if (isSorted) {
      Vec2 headerTextSize = MeasureTextCached(ctx, columns[c].header, captionStyle.fontSize);
      float arrowX = colX + textPad + headerTextSize.x + 4.0f;
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

    colX += colW;
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

  // Push clip rect for data area
  Vec2 dataAreaPos(tablePos.x, tablePos.y + headerHeight);
  Vec2 dataAreaSize(availableContentWidth, dataAreaHeight);
  ctx->renderer.PushClipRect(dataAreaPos, dataAreaSize);
  frame.clipPushed = true;

  ctx->tableStack.push_back(frame);

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

  frame.currentCol = column;

  // Calculate X position for this column
  float colX = frame.position.x;
  for (int c = 0; c < column; ++c) {
    colX += frame.columnsPtr[c].width;
  }

  float rowY = frame.position.y + frame.headerHeight +
               frame.currentRow * frame.rowHeight - frame.scrollOffset;

  // Set cursor to cell position with some padding
  float cellPad = 6.0f;
  ctx->cursorPos = Vec2(colX + cellPad,
      rowY + (frame.rowHeight - ctx->style.GetTextStyle(TypographyStyle::Body).fontSize) * 0.5f);
}

void EndTable() {
  UIContext *ctx = GetContext();
  if (!ctx || ctx->tableStack.empty())
    return;

  auto frame = ctx->tableStack.back();
  ctx->tableStack.pop_back();

  // Pop clip rect for data area
  if (frame.clipPushed) {
    ctx->renderer.PopClipRect();
  }

  // Draw vertical scrollbar if needed
  auto &state = ctx->tableStates[frame.id];
  float dataAreaHeight = frame.size.y - frame.headerHeight;
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

  // Draw table outer border
  const PanelStyle &panelStyle = ctx->style.panel;
  ctx->renderer.DrawRect(frame.position, frame.size, panelStyle.borderColor,
                         panelStyle.cornerRadius);

  // Restore cursor state and advance
  ctx->cursorPos = frame.savedCursor;
  ctx->lastItemPos = frame.position;
  ctx->lastItemSize = frame.size;
  AdvanceCursor(ctx, frame.size);
}

} // namespace FluentUI
