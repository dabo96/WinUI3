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

bool BeginListView(const std::string &id, const Vec2 &size, int *selectedItem,
                   const std::vector<std::string> &items, std::optional<Vec2> pos) {
  UIContext *ctx = GetContext();
  if (!ctx || items.empty())
    return false;

  std::string key = "LISTVIEW:" + id;
  uint32_t listViewId = GenerateId(key.c_str());
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
  // Registrar área ocupada solo si NO se usa posición absoluta
  if (!state.useAbsolutePos) {
    RegisterOccupiedArea(ctx, listViewPos, listViewSize);
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

void EndListView() {
  // EndListView no necesita hacer nada especial por ahora
  // El contenido ya fue renderizado en BeginListView
}

// ctx->treeViewDepth and ctx->currentTreeViewId moved to UIContext

bool BeginTreeView(const std::string &id, const Vec2 &size, std::optional<Vec2> pos) {
  UIContext *ctx = GetContext();
  if (!ctx)
    return false;

  std::string key = "TREEVIEW:" + id;
  uint32_t treeViewId = GenerateId(key.c_str());
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
  // Registrar área ocupada solo si NO se usa posición absoluta
  if (!state.useAbsolutePos) {
    RegisterOccupiedArea(ctx, treeViewPos, treeViewSize);
  }

  const PanelStyle &panelStyle = ctx->style.panel;

  // Dibujar fondo del TreeView con contraste más pronunciado - sin borde
  Color treeBg = AdjustContainerBackground(panelStyle.background, ctx->style.isDarkTheme);
  ctx->renderer.DrawRectFilled(treeViewPos, treeViewSize, treeBg,
                               panelStyle.cornerRadius);

  // Aplicar clipping
  ctx->renderer.PushClipRect(treeViewPos, treeViewSize);

  // Configurar cursor para el contenido del TreeView
  ctx->cursorPos = treeViewPos + panelStyle.padding;
  ctx->lastItemPos = ctx->cursorPos;
  ctx->treeViewDepth = 0;

  // Iniciar layout vertical para apilar los nodos correctamente
  Vec2 contentSize(treeViewSize.x - panelStyle.padding.x * 2.0f,
                   treeViewSize.y - panelStyle.padding.y * 2.0f);
  BeginVertical(ctx->style.spacing, Vec2(contentSize.x, 0.0f), Vec2(0.0f, 0.0f));

  return true;
}

void EndTreeView() {
  UIContext *ctx = GetContext();
  if (!ctx)
    return;

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

  // Usar el estado guardado o el valor por defecto
  std::string nodeKey =
      "TREENODE:" + std::to_string(ctx->currentTreeViewId) + ":" + id;
  bool nodeIsOpen = isOpen ? *isOpen : ctx->treeNodeStates[nodeKey];
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
  Color itemBg = panelStyle.background;
  if (nodeIsSelected) {
    itemBg = ctx->style.button.background.normal;
  } else if (hover) {
    itemBg = panelStyle.headerBackground;
  }
  ctx->renderer.DrawRectFilled(itemPos, itemSize, itemBg, 0.0f);

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
        hoverButton ? panelStyle.headerBackground : panelStyle.background;
    ctx->renderer.DrawRectFilled(buttonPos, buttonSize, buttonBg, 2.0f);
    ctx->renderer.DrawRect(buttonPos, buttonSize, panelStyle.borderColor, 2.0f);

    // Dibujar símbolo + o -
    Vec2 center(buttonPos.x + buttonSize.x * 0.5f,
                buttonPos.y + buttonSize.y * 0.5f);
    float lineLength = buttonSize.x * 0.4f;

    // Línea horizontal
    Vec2 hStart(center.x - lineLength * 0.5f, center.y);
    Vec2 hEnd(center.x + lineLength * 0.5f, center.y);
    ctx->renderer.DrawLine(hStart, hEnd, textStyle.color, 2.0f);

    // Línea vertical solo si está cerrado
    if (!nodeIsOpen) {
      Vec2 vStart(center.x, center.y - lineLength * 0.5f);
      Vec2 vEnd(center.x, center.y + lineLength * 0.5f);
      ctx->renderer.DrawLine(vStart, vEnd, textStyle.color, 2.0f);
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

  RegisterOccupiedArea(ctx, itemPos, itemSize);
  AdvanceCursor(ctx, itemSize);

  // Actualizar el estado guardado
  if (isOpen) {
    ctx->treeNodeStates[nodeKey] = *isOpen;
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

} // namespace FluentUI
