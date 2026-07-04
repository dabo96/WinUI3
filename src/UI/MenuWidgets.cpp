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

  // Top offset lets a host stack the menu bar below a custom TitleBar() (0 = top).
  float offsetY = ctx->menuBarOffsetY;

  auto &menuBar = ctx->menuBarState;
  menuBar.position = Vec2(0.0f, offsetY);
  menuBar.size = Vec2(viewport.x, MENUBAR_HEIGHT);
  menuBar.initialized = true;

  const PanelStyle &panelStyle = ctx->style.panel;

  // Full-width background — darkest bar (use app background, not panel header)
  Color menuBarBg = ctx->style.backgroundColor;
  ctx->renderer.DrawRectFilled(menuBar.position, menuBar.size, menuBarBg, 0.0f);

  // Subtle bottom border (--border-soft)
  ctx->renderer.DrawRectFilled(
      Vec2(0.0f, offsetY + MENUBAR_HEIGHT - 1.0f),
      Vec2(viewport.x, 1.0f),
      panelStyle.borderColor, 0.0f);

  // Cursor starts with left padding; horizontal layout uses auto width (not full viewport)
  float leftPad = 12.0f;
  ctx->cursorPos = Vec2(leftPad, offsetY);
  ctx->lastItemPos = ctx->cursorPos;

  BeginHorizontal(4.0f, Vec2(0.0f, MENUBAR_HEIGHT), Vec2(0.0f, 0.0f));

  return true;
}

void EndMenuBar() {
  UIContext *ctx = GetContext();
  if (!ctx)
    return;

  EndHorizontal(false);

  auto &menuBar = ctx->menuBarState;

  // Advance cursor below the menu bar (respecting any top offset)
  ctx->cursorPos = Vec2(0.0f, menuBar.position.y + menuBar.size.y);
  ctx->lastItemPos = ctx->cursorPos;
  ctx->lastItemSize = menuBar.size;
}

bool BeginMenu(const std::string &label, bool enabled) {
  return BeginMenu(label, 0u, enabled);
}

bool BeginMenu(const std::string &label, uint32_t iconCodepoint, bool enabled) {
  UIContext *ctx = GetContext();
  if (!ctx)
    return false;

  uint32_t menuId = GenerateId("MENU:", label.c_str());
  auto &state = ctx->GetMenuState(menuId);

  const PanelStyle &panelStyle = ctx->style.panel;
  const TextStyle &textStyle = ctx->style.GetTextStyle(TypographyStyle::Body);
  Vec2 textSize = MeasureTextCached(ctx, label, textStyle.fontSize);

  // Use the same constant height as MenuBar
  float menuHeight = MENUBAR_HEIGHT;
  float menuPadding = 10.0f;  // compact padding matching HTML reference
  // Reserve an icon slot on the left when an icon is requested.
  // Icon is rendered at label's font size + small gap.
  float iconSize = textStyle.fontSize + 2.0f;
  float iconGap = 6.0f;
  float iconSlot = (iconCodepoint != 0u) ? (iconSize + iconGap) : 0.0f;
  Vec2 menuSize(textSize.x + menuPadding * 2.0f + iconSlot, menuHeight);
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
      // brief 22 (fase 6): los menús viven en widgetStates; recorremos el mapa
      // unificado filtrando las entradas con sub-estado menu presente.
      for (auto &[id, ws] : ctx->widgetStates) {
        if (ws.menu && id != menuId) {
          ws.menu->open = false;
        }
      }
      state.open = true;
      ctx->activeMenuId = menuId;
    }
  } else if (state.open) {
    // If menu is already open, ensure activeMenuId is set
    ctx->activeMenuId = menuId;
  }

  // Menu item background: transparent by default, subtle highlight on hover/open.
  // Overlay tinted by theme — blanco aclara sobre barra oscura, negro oscurece
  // sobre barra clara. Un overlay blanco fijo era invisible en tema claro.
  menuSize.y = MENUBAR_HEIGHT;
  bool darkBar = ctx->style.isDarkTheme;
  if (state.open) {
    Color openBg = darkBar ? Color(1.0f, 1.0f, 1.0f, 0.12f)
                           : Color(0.0f, 0.0f, 0.0f, 0.10f);
    ctx->renderer.DrawRectFilled(menuPos, menuSize, openBg, 4.0f);
  } else if (hover && enabled) {
    Color hoverBg = darkBar ? Color(1.0f, 1.0f, 1.0f, 0.08f)
                            : Color(0.0f, 0.0f, 0.0f, 0.07f);
    ctx->renderer.DrawRectFilled(menuPos, menuSize, hoverBg, 4.0f);
  }

  // Compute text / icon colors (icon shares the text color curve)
  Color textColor;
  if (!enabled) {
    textColor = Color(textStyle.color.r * 0.4f, textStyle.color.g * 0.4f,
                      textStyle.color.b * 0.4f, textStyle.color.a);
  } else if (hover || state.open) {
    textColor = textStyle.color;  // full brightness on hover/open
  } else {
    textColor = Color(textStyle.color.r * 0.75f, textStyle.color.g * 0.75f,
                      textStyle.color.b * 0.75f, textStyle.color.a);  // dimmed default
  }

  // Dibujar icono (si hay)
  if (iconCodepoint != 0u) {
    Vec2 iconPos(menuPos.x + menuPadding,
                 menuPos.y + (menuHeight - iconSize) * 0.5f);
    ctx->renderer.DrawIconGlyph(iconPos, iconCodepoint, textColor, iconSize);
  }

  // Dibujar texto del menú
  Vec2 textPos(menuPos.x + menuPadding + iconSlot,
               menuPos.y + (menuHeight - textSize.y) * 0.5f);
  ctx->renderer.DrawText(textPos, label, textColor, textStyle.fontSize);

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

    // Configurar cursor para los items del menú (dentro del dropdown)
    ctx->cursorPos = dropdownPos;

    // Los MenuItem siguientes caen DENTRO del dropdown abierto; eximir su
    // detección de input del bloqueo por overlay (si no, se auto-bloquearían).
    ctx->insideOverlayRender = true;

    // Iniciar layout vertical — width will be determined by content in EndMenu
    BeginVertical(0.0f, std::nullopt, Vec2(0.0f, 0.0f));
  }

  return state.open;
}

void EndMenu() {
  UIContext *ctx = GetContext();
  if (!ctx)
    return;

  // Fin de los items del menú: vuelve a aplicar el bloqueo de input por overlay
  // a los widgets de fondo.
  ctx->insideOverlayRender = false;

  // Pop the menu ID from the stack to know which menu we're closing
  if (ctx->menuIdStack.empty())
    return;

  uint32_t currentMenuId = ctx->menuIdStack.back();
  ctx->menuIdStack.pop_back();

  // Find the menu state for this specific menu (currentMenuId proviene del
  // menuIdStack, sólo empujado por BeginMenu con el menú abierto → la entrada
  // existe; GetMenuState no crea nada espurio aquí).
  auto &state = ctx->GetMenuState(currentMenuId);

  // ALWAYS close the BeginVertical that BeginMenu opened, even if a MenuItem
  // click set state.open = false during this frame. Otherwise the layout stack
  // is permanently corrupted.

  // Capture cursor position BEFORE EndVertical restores it to the parent
  Vec2 contentEndCursor = ctx->cursorPos;

  // Cerrar el layout vertical
  if (!ctx->layoutStack.empty()) {
    EndVertical(false);

    // Explicitly restore cursor to parent horizontal layout position
    if (!ctx->layoutStack.empty()) {
       LayoutStack &parentStack = ctx->layoutStack.back();
       if (!parentStack.isVertical) {
           ctx->cursorPos = Vec2(parentStack.cursor.x, parentStack.contentStart.y);
       }
    }
  }

  if (state.open) {
    // Calcular posición y tamaño del dropdown basado en el contenido real
    Vec2 dropdownStartPos(state.position.x, state.position.y + state.size.y);

    float dropdownWidth = 120.0f; // minimum width
    float dropdownHeight = contentEndCursor.y - dropdownStartPos.y;
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

    // Save dropdown rect in MenuState so NewFrame can check clicks against it
    state.dropdownPos = dropdown.dropdownPos;
    state.dropdownSize = dropdown.dropdownSize;

    // Publicar el rect para el bloqueo de input por overlay de los widgets de
    // fondo. Persiste hasta que RenderDeferredDropdowns lo limpie al cerrarse,
    // así el click sobre un item no se cuela al widget de debajo.
    ctx->openMenuId = currentMenuId;
    ctx->openMenuDropdownPos = dropdown.dropdownPos;
    ctx->openMenuDropdownSize = dropdown.dropdownSize;

    ctx->deferredMenuDropdowns.push_back(dropdown);
  } else {
    // Menu was closed by a MenuItem click this frame — discard pending items
    if (!ctx->menuItemStartIndexStack.empty()) {
        size_t startIndex = ctx->menuItemStartIndexStack.back();
        ctx->menuItemStartIndexStack.pop_back();
        if (startIndex < ctx->currentMenuItems.size()) {
            ctx->currentMenuItems.resize(startIndex);
        }
    }
  }
}

bool MenuItem(const std::string &label, bool enabled) {
  return MenuItem(label, 0u, enabled);
}

bool MenuItem(const std::string &label, uint32_t iconCodepoint, bool enabled) {
  UIContext *ctx = GetContext();
  if (!ctx)
    return false;

  // Solo renderizar si hay un menú activo en el stack
  if (ctx->menuIdStack.empty())
    return false;

  uint32_t currentMenuId = ctx->menuIdStack.back();
  auto &state = ctx->GetMenuState(currentMenuId);
  if (!state.open)
    return false;

  const PanelStyle &panelStyle = ctx->style.panel;
  const TextStyle &textStyle = ctx->style.GetTextStyle(TypographyStyle::Body);
  float itemHeight = textStyle.fontSize + panelStyle.padding.y * 2.0f;

  Vec2 textSize = MeasureTextCached(ctx, label, textStyle.fontSize);
  float iconSize = textStyle.fontSize;
  float iconGap = 6.0f;
  float iconSlot = (iconCodepoint != 0u) ? (iconSize + iconGap) : 0.0f;
  float itemWidth = textSize.x + iconSlot + panelStyle.padding.x * 2.0f + 32.0f; // text + icon + padding + margin for shortcuts

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
  item.iconCodepoint = iconCodepoint;

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

  AdvanceCursor(ctx, itemSize);

  // Si se hace click, cerrar el menú
  if (clicked) {
    state.open = false;
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
  auto &state = ctx->GetMenuState(currentMenuId);
  if (!state.open)
    return;

  const PanelStyle &panelStyle = ctx->style.panel;
  float separatorHeight = 1.0f;
  float separatorPadding = 1.0f; // Reduced padding
  // Width will be adjusted to match other items in EndMenu
  float separatorWidth = 120.0f; // minimum, will be expanded by EndMenu

  Vec2 separatorSize(separatorWidth, separatorHeight + separatorPadding * 2.0f);
  Vec2 separatorPos = ctx->cursorPos;

  // Encolar separador para renderizado diferido
  UIContext::DeferredMenuItem item;
  item.type = UIContext::DeferredMenuItem::Type::Separator;
  item.pos = separatorPos;
  item.size = separatorSize;
  item.bgColor = panelStyle.borderColor; // Usar color de borde para la línea

  ctx->currentMenuItems.push_back(item);

  AdvanceCursor(ctx, separatorSize);
}

// ---------------------------------------------------------------------------
// Toolbar
// ---------------------------------------------------------------------------

void BeginToolbar() {
  UIContext *ctx = GetContext();
  if (!ctx)
    return;

  Vec2 viewport = ctx->renderer.GetViewportSize();
  Vec2 toolbarPos = ctx->cursorPos;
  Vec2 toolbarSize = Vec2(viewport.x, TOOLBAR_HEIGHT);

  const PanelStyle &panelStyle = ctx->style.panel;

  // Draw toolbar background — slightly lighter than menubar for visual separation
  Color toolbarBg = panelStyle.background;
  ctx->renderer.DrawRectFilled(toolbarPos, toolbarSize, toolbarBg, 0.0f);

  // Draw a subtle bottom border
  Color borderColor = panelStyle.borderColor;
  borderColor.a *= 0.5f;
  ctx->renderer.DrawRectFilled(Vec2(toolbarPos.x, toolbarPos.y + TOOLBAR_HEIGHT - 1.0f),
                               Vec2(viewport.x, 1.0f), borderColor, 0.0f);

  // Set cursor to toolbar origin; let BeginHorizontal apply the padding once
  float hPad = 10.0f;
  float vPad = (TOOLBAR_HEIGHT - 26.0f) * 0.5f;  // center 26px buttons
  ctx->cursorPos = toolbarPos;
  ctx->lastItemPos = ctx->cursorPos;

  // Horizontal layout applies padding internally (no double-apply)
  BeginHorizontal(4.0f, Vec2(0.0f, TOOLBAR_HEIGHT), Vec2(hPad, vPad));
}

void EndToolbar() {
  UIContext *ctx = GetContext();
  if (!ctx)
    return;

  EndHorizontal(false);

  Vec2 viewport = ctx->renderer.GetViewportSize();

  // Advance cursor past the toolbar, mirroring EndMenuBar pattern.
  // The toolbar was drawn starting at lastItemPos.y (or earlier);
  // ensure cursor lands exactly one TOOLBAR_HEIGHT below where it started.
  // Since BeginToolbar set cursorPos to toolbarPos + padding, we compute
  // the bottom edge from the layout origin.
  float toolbarTop = ctx->lastItemPos.y;
  // If the horizontal layout hasn't moved cursor past the bar, force it.
  if (ctx->cursorPos.y < toolbarTop + TOOLBAR_HEIGHT) {
    ctx->cursorPos.y = toolbarTop + TOOLBAR_HEIGHT;
  }
  ctx->cursorPos.x = 0.0f;
  ctx->lastItemPos = ctx->cursorPos;
  ctx->lastItemSize = Vec2(viewport.x, TOOLBAR_HEIGHT);
}

// ---------------------------------------------------------------------------
// StatusBar
// ---------------------------------------------------------------------------

void BeginStatusBar(const std::string &text) {
  UIContext *ctx = GetContext();
  if (!ctx)
    return;

  Vec2 viewport = ctx->renderer.GetViewportSize();
  Vec2 statusPos = Vec2(0.0f, viewport.y - STATUSBAR_HEIGHT);
  Vec2 statusSize = Vec2(viewport.x, STATUSBAR_HEIGHT);

  const PanelStyle &panelStyle = ctx->style.panel;

  // Draw statusbar background
  Color statusBg = AdjustContainerBackground(panelStyle.headerBackground, ctx->style.isDarkTheme);
  ctx->renderer.DrawRectFilled(statusPos, statusSize, statusBg, 0.0f);

  // Draw a subtle top border
  Color borderColor = panelStyle.borderColor;
  borderColor.a *= 0.5f;
  ctx->renderer.DrawRectFilled(statusPos, Vec2(viewport.x, 1.0f), borderColor, 0.0f);

  // If only text is provided (no content lambda will follow), draw the label now
  if (!text.empty()) {
    const TextStyle &textStyle = ctx->style.GetTextStyle(TypographyStyle::Caption);
    Vec2 textSize = MeasureTextCached(ctx, text, textStyle.fontSize);
    float textY = statusPos.y + (STATUSBAR_HEIGHT - textSize.y) * 0.5f;
    ctx->renderer.DrawText(Vec2(statusPos.x + 8.0f, textY), text,
                           textStyle.color, textStyle.fontSize);
  }

  // Save current cursor and set up for statusbar content
  float vPad = (STATUSBAR_HEIGHT - 16.0f) * 0.5f; // center 16px content in 24px bar
  ctx->cursorPos = Vec2(statusPos.x + 4.0f, statusPos.y + vPad);
  ctx->lastItemPos = ctx->cursorPos;

  // Begin horizontal layout for statusbar contents
  BeginHorizontal(4.0f, statusSize, Vec2(4.0f, vPad));
}

void EndStatusBar() {
  UIContext *ctx = GetContext();
  if (!ctx)
    return;

  EndHorizontal(false);

  // StatusBar is at the bottom, no need to advance cursor for subsequent widgets
  // Restore cursor to where it was before the statusbar
  ctx->lastItemSize = Vec2(ctx->renderer.GetViewportSize().x, STATUSBAR_HEIGHT);
}

} // namespace FluentUI
