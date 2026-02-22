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

bool BeginMenuBar() {
  UIContext *ctx = GetContext();
  if (!ctx)
    return false;

  Vec2 viewport = ctx->renderer.GetViewportSize();

  auto &menuBar = ctx->menuBarState;
  menuBar.position = Vec2(0.0f, 0.0f);
  menuBar.size = Vec2(viewport.x, MENUBAR_HEIGHT);
  menuBar.initialized = true;

  const PanelStyle &panelStyle = ctx->style.panel;

  // Dibujar fondo del MenuBar - con contraste sin borde
  Color menuBarBg = AdjustContainerBackground(panelStyle.headerBackground, ctx->style.isDarkTheme);
  ctx->renderer.DrawRectFilled(menuBar.position, menuBar.size,
                               menuBarBg, 0.0f);

  // Configurar cursor para el MenuBar (sin padding)
  ctx->cursorPos = menuBar.position;
  ctx->lastItemPos = ctx->cursorPos;

  // Iniciar layout horizontal para los menús (sin padding)
  BeginHorizontal(0.0f, menuBar.size, Vec2(0.0f, 0.0f));

  return true;
}

void EndMenuBar() {
  UIContext *ctx = GetContext();
  if (!ctx)
    return;

  // Cerrar layout horizontal
  EndHorizontal(false);

  auto &menuBar = ctx->menuBarState;

  // Avanzar cursor después del MenuBar
  ctx->cursorPos = Vec2(0.0f, menuBar.size.y);
  ctx->lastItemPos = ctx->cursorPos;
  ctx->lastItemSize = menuBar.size;
}

bool BeginMenu(const std::string &label, bool enabled) {
  UIContext *ctx = GetContext();
  if (!ctx)
    return false;

  std::string key = "MENU:" + label;
  uint32_t menuId = GenerateId(key.c_str());
  auto &state = ctx->menuStates[menuId];

  const PanelStyle &panelStyle = ctx->style.panel;
  const TextStyle &textStyle = ctx->style.GetTextStyle(TypographyStyle::Body);
  Vec2 textSize = MeasureTextCached(ctx, label, textStyle.fontSize);

  // Use the same constant height as MenuBar
  float menuHeight = MENUBAR_HEIGHT;
  float menuPadding = 12.0f;
  Vec2 menuSize(textSize.x + menuPadding * 2.0f, menuHeight);
  Vec2 menuPos = ctx->cursorPos;

  bool hover = enabled && IsMouseOver(ctx, menuPos, menuSize);
  bool clicked = hover && enabled && ctx->input.IsMousePressed(0);

  // Toggle del menú si se hace click
  if (clicked) {
    // Cerrar otros menús abiertos
    if (state.open) {
      state.open = false;
      ctx->activeMenuId = 0;
    } else {
      // Cerrar todos los otros menús
      for (auto &[id, otherState] : ctx->menuStates) {
        if (id != menuId) {
          otherState.open = false;
        }
      }
      state.open = true;
      ctx->activeMenuId = menuId;
    }
  } else if (state.open) {
    // If menu is already open, ensure activeMenuId is set
    ctx->activeMenuId = menuId;
  }

  // Dibujar fondo del menú
  Color menuBg = panelStyle.headerBackground;
  if (hover && enabled) {
    menuBg = panelStyle.background;
  }
  if (!enabled) {
    menuBg = Color(menuBg.r * 0.5f, menuBg.g * 0.5f, menuBg.b * 0.5f, menuBg.a);
  }

  // Force exact height before drawing to prevent any size modifications
  menuSize.y = MENUBAR_HEIGHT;
  ctx->renderer.DrawRectFilled(menuPos, menuSize, menuBg, 0.0f);

  // Dibujar texto del menú
  Vec2 textPos(menuPos.x + menuPadding,
               menuPos.y + (menuHeight - textSize.y) * 0.5f);
  Color textColor =
      enabled ? textStyle.color
              : Color(textStyle.color.r * 0.5f, textStyle.color.g * 0.5f,
                      textStyle.color.b * 0.5f, textStyle.color.a);
  ctx->renderer.DrawText(textPos, label, textColor, textStyle.fontSize);

  RegisterOccupiedArea(ctx, menuPos, menuSize);
  AdvanceCursor(ctx, menuSize);

  // Guardar estado
  state.id = label;
  state.position = menuPos;
  state.size = menuSize;
  state.hover = hover;
  state.initialized = true;

  // Si el menú está abierto, preparar el layout vertical para los items
  if (state.open) {
    // Push menu ID to stack so EndMenu knows which menu to close
    ctx->menuIdStack.push_back(menuId);
    ctx->menuItemStartIndexStack.push_back(ctx->currentMenuItems.size());

    Vec2 dropdownPos(state.position.x, state.position.y + state.size.y);
    float dropdownWidth = 200.0f;

    // Asegurar que el dropdown no se salga de la ventana
    Vec2 viewport = ctx->renderer.GetViewportSize();
    if (dropdownPos.x + dropdownWidth > viewport.x) {
      dropdownPos.x = viewport.x - dropdownWidth;
    }

    // Configurar cursor para los items del menú (dentro del dropdown)
    ctx->cursorPos = dropdownPos;

    // Iniciar layout vertical para los items del menú
    BeginVertical(0.0f, Vec2(dropdownWidth, 0.0f), Vec2(0.0f, 0.0f));
  }

  return state.open;
}

void EndMenu() {
  UIContext *ctx = GetContext();
  if (!ctx)
    return;

  // Pop the menu ID from the stack to know which menu we're closing
  if (ctx->menuIdStack.empty())
    return;

  uint32_t currentMenuId = ctx->menuIdStack.back();
  ctx->menuIdStack.pop_back();

  // Find the menu state for this specific menu
  auto it = ctx->menuStates.find(currentMenuId);
  if (it != ctx->menuStates.end() && it->second.open) {
    auto &state = it->second;

    // Cerrar el layout vertical primero para obtener el tamaño real del contenido
    if (!ctx->layoutStack.empty()) {
      EndVertical(false);

      // Explicitly restore cursor to parent horizontal layout position
      // This ensures we don't lose track of where the next menu item should be
      if (!ctx->layoutStack.empty()) {
         LayoutStack &parentStack = ctx->layoutStack.back();
         if (!parentStack.isVertical) {
             ctx->cursorPos = Vec2(parentStack.cursor.x, parentStack.contentStart.y);
         }
      }
    }

    // Calcular posición y tamaño del dropdown basado en el contenido real
    Vec2 dropdownStartPos(state.position.x, state.position.y + state.size.y);
    Vec2 currentCursor = ctx->cursorPos;

    float dropdownWidth = 200.0f;
    float dropdownHeight = currentCursor.y - dropdownStartPos.y;
    if (dropdownHeight < 10.0f) {
      dropdownHeight = 10.0f;
    }

    Vec2 dropdownPos = dropdownStartPos;

    // Asegurar que el dropdown no se salga de la ventana
    Vec2 viewport = ctx->renderer.GetViewportSize();
    if (dropdownPos.x + dropdownWidth > viewport.x) {
      dropdownPos.x = viewport.x - dropdownWidth;
    }
    if (dropdownPos.y + dropdownHeight > viewport.y) {
      dropdownPos.y = state.position.y - dropdownHeight;
      if (dropdownPos.y < 0.0f) {
        dropdownPos.y = 0.0f;
        dropdownHeight = viewport.y - dropdownPos.y;
      }
    }

    // Encolar el dropdown para renderizado diferido
    UIContext::DeferredMenuDropdown dropdown;
    dropdown.dropdownPos = dropdownPos;
    dropdown.dropdownSize = Vec2(dropdownWidth, dropdownHeight);
    dropdown.menuId = currentMenuId;

    // Move items from currentMenuItems to dropdown
    if (!ctx->menuItemStartIndexStack.empty()) {
        size_t startIndex = ctx->menuItemStartIndexStack.back();
        ctx->menuItemStartIndexStack.pop_back();

        if (startIndex < ctx->currentMenuItems.size()) {
            // Calculate max width from items
            float maxItemWidth = dropdownWidth;
            for (size_t i = startIndex; i < ctx->currentMenuItems.size(); ++i) {
                maxItemWidth = std::max(maxItemWidth, ctx->currentMenuItems[i].size.x);
            }

            // Update dropdown width
            dropdown.dropdownSize.x = maxItemWidth;

            // Update all items to match max width
            for (size_t i = startIndex; i < ctx->currentMenuItems.size(); ++i) {
                ctx->currentMenuItems[i].size.x = maxItemWidth;
            }

            // Copy items to dropdown and adjust their positions to be relative to dropdownPos
            // El primer item debe comenzar exactamente en dropdownPos.y, sin espacio extra
            float currentY = dropdownPos.y;
            for (size_t i = startIndex; i < ctx->currentMenuItems.size(); ++i) {
                UIContext::DeferredMenuItem item = ctx->currentMenuItems[i];
                // Recalcular posición para que el primer item comience exactamente en dropdownPos
                item.pos.y = currentY;
                item.pos.x = dropdownPos.x;
                dropdown.items.push_back(item);
                // Avanzar Y para el siguiente item usando la altura del item actual
                currentY += item.size.y;
            }

            // Remove items from currentMenuItems
            ctx->currentMenuItems.resize(startIndex);
        }
    }

    ctx->deferredMenuDropdowns.push_back(dropdown);
  }
}

bool MenuItem(const std::string &label, bool enabled) {
  UIContext *ctx = GetContext();
  if (!ctx)
    return false;

  // Solo renderizar si hay un menú activo en el stack
  if (ctx->menuIdStack.empty())
    return false;

  uint32_t currentMenuId = ctx->menuIdStack.back();
  auto it = ctx->menuStates.find(currentMenuId);
  if (it == ctx->menuStates.end() || !it->second.open)
    return false;

  const PanelStyle &panelStyle = ctx->style.panel;
  const TextStyle &textStyle = ctx->style.GetTextStyle(TypographyStyle::Body);
  float itemHeight = textStyle.fontSize + panelStyle.padding.y * 2.0f;
  float itemWidth = 200.0f;

  Vec2 textSize = MeasureTextCached(ctx, label, textStyle.fontSize);
  itemWidth = std::max(itemWidth, textSize.x + panelStyle.padding.x * 2.0f);

  Vec2 itemSize(itemWidth, itemHeight);
  Vec2 itemPos = ctx->cursorPos;

  bool hover = enabled && IsMouseOver(ctx, itemPos, itemSize);
  bool clicked = hover && ctx->input.IsMousePressed(0);

  // En lugar de dibujar inmediatamente, encolar para renderizado diferido
  UIContext::DeferredMenuItem item;
  item.label = label;
  item.enabled = enabled;
  item.pos = itemPos;
  item.size = itemSize;

  // Calcular colores
  Color itemBg = hover ? panelStyle.headerBackground : panelStyle.background;
  if (!enabled) {
    itemBg = Color(itemBg.r * 0.5f, itemBg.g * 0.5f, itemBg.b * 0.5f, itemBg.a);
  }
  item.bgColor = itemBg;

  Color textColor =
      enabled ? textStyle.color
              : Color(textStyle.color.r * 0.5f, textStyle.color.g * 0.5f,
                      textStyle.color.b * 0.5f, textStyle.color.a);
  item.textColor = textColor;

  ctx->currentMenuItems.push_back(item);

  // No dibujamos nada aquí, se dibujará en RenderDeferredDropdowns

  RegisterOccupiedArea(ctx, itemPos, itemSize);
  AdvanceCursor(ctx, itemSize);

  // Si se hace click, cerrar el menú
  if (clicked) {
    it->second.open = false;
    ctx->activeMenuId = 0;
  }

  return clicked;
}

void MenuSeparator() {
  UIContext *ctx = GetContext();
  if (!ctx)
    return;

  // Solo renderizar si hay un menú activo en el stack
  if (ctx->menuIdStack.empty())
    return;

  uint32_t currentMenuId = ctx->menuIdStack.back();
  auto it = ctx->menuStates.find(currentMenuId);
  if (it == ctx->menuStates.end() || !it->second.open)
    return;

  const PanelStyle &panelStyle = ctx->style.panel;
  float separatorHeight = 1.0f;
  float separatorPadding = 1.0f; // Reduced padding
  float separatorWidth = 200.0f;

  Vec2 separatorSize(separatorWidth, separatorHeight + separatorPadding * 2.0f);
  Vec2 separatorPos = ctx->cursorPos;

  // Encolar separador para renderizado diferido
  UIContext::DeferredMenuItem item;
  item.type = UIContext::DeferredMenuItem::Type::Separator;
  item.pos = separatorPos;
  item.size = separatorSize;
  item.bgColor = panelStyle.borderColor; // Usar color de borde para la línea

  ctx->currentMenuItems.push_back(item);

  RegisterOccupiedArea(ctx, separatorPos, separatorSize);
  AdvanceCursor(ctx, separatorSize);
}

} // namespace FluentUI
