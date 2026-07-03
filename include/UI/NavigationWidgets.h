#pragma once
// ─── brief 13: App shell & navegación ────────────────────────────────────────
// NavigationView (panel lateral Fluent), navegación por páginas (NavFrame),
// CommandBar con overflow, BreadcrumbBar y TitleBar custom (window chrome).
//
// Funciones libres en `namespace FluentUI`, declaradas aquí e implementadas en
// src/UI/NavigationWidgets.cpp. Reusan CommandItem / FlyoutPlacement / MenuEntry
// (brief 14) y CurrentBreakpoint / Breakpoint (brief 19). Incluido desde
// UI/Widgets.h con una sola línea (la TitleBar se entrelaza además con FluentApp,
// que registra el hit-test cuando AppConfig::useCustomTitleBar = true).
#include "Math/Vec2.h"
#include <string>
#include <vector>
#include <functional>
#include <cstdint>

namespace FluentUI {

// Definido en UI/Widgets.h (brief 14). Este header se incluye desde Widgets.h
// justo después, por lo que el tipo ya está completo; la declaración adelantada
// sólo mantiene este header coherente para las firmas de CommandBar.
struct CommandItem;

// ─── 1) NavigationView ───────────────────────────────────────────────────────

/// Un ítem del NavigationView. Los `children` se renderizan como sub-ítems
/// expandibles (chevron). `badge` > 0 dibuja un contador junto a la fila.
struct NavItem {
    std::string key;               ///< Identificador estable de la página.
    std::string label;             ///< Texto mostrado (oculto en Compact/Minimal).
    uint32_t    icon = 0;          ///< Codepoint Lucide (0 = sin icono).
    int         badge = 0;         ///< Contador opcional (0 = sin badge).
    std::vector<NavItem> children; ///< Sub-ítems expandibles.
};

/// Modo de presentación del panel lateral.
///   Expanded → ancho completo (icono + label)
///   Compact  → solo iconos (ancho reducido)
///   Minimal  → oculto, con hamburguesa que despliega el panel
enum class NavDisplayMode { Expanded, Compact, Minimal };

/// Panel lateral de navegación Fluent. Barra vertical de ancho fijo animado:
/// filas con icono + label, selección con barra de acento a la izquierda + fondo
/// sutil, sub-ítems con chevron, hamburguesa que alterna el modo y `footerItems`
/// anclados abajo. Responsive (brief 19): por debajo del breakpoint Small el modo
/// se fuerza a Minimal, en Medium a Compact.
/// @param selectedKey  Estado del usuario (o interno por id si es null).
/// @return La key seleccionada actualmente.
std::string NavigationView(const std::string& id,
                           const std::vector<NavItem>& items,
                           std::string* selectedKey,
                           NavDisplayMode mode = NavDisplayMode::Expanded,
                           const std::vector<NavItem>& footerItems = {});

// ─── 2) NavFrame (navegación por páginas con historial) ──────────────────────

/// Frame ligero con back/forward. POD retenido por el usuario.
struct NavFrame {
    std::vector<std::string> backStack;    ///< Keys visitadas (más reciente al final).
    std::vector<std::string> forwardStack; ///< Keys para "adelante".
    std::string current;                   ///< Página actual.
};

/// Navega a `pageKey`: empuja la actual al back-stack, fija la nueva y limpia el
/// forward-stack. No-op si `pageKey` ya es la página actual.
void NavigateTo(NavFrame& f, const std::string& pageKey);
/// Vuelve a la página anterior. @return false si el back-stack está vacío.
bool NavigateBack(NavFrame& f);
/// Avanza a la siguiente página. @return false si el forward-stack está vacío.
bool NavigateForward(NavFrame& f);

// ─── 3) CommandBar (toolbar con overflow) ────────────────────────────────────

/// Barra de comandos que coloca `primary` de izquierda a derecha mientras quepan
/// en el ancho disponible; los que no caben + `secondary` van a un MenuFlyout
/// abierto desde un botón "···" a la derecha. Se recalcula en cada frame (resize).
void CommandBar(const std::string& id,
                const std::vector<CommandItem>& primary,
                const std::vector<CommandItem>& secondary = {});

// ─── 4) BreadcrumbBar ────────────────────────────────────────────────────────

/// Migas de pan separadas por un chevron ›. Todas clicables salvo la última
/// (ubicación actual). Si no caben, las intermedias se colapsan en un "…" que
/// abre un Flyout con las ocultas.
/// @return Índice de la miga clicada este frame (-1 si ninguna).
int BreadcrumbBar(const std::string& id, const std::vector<std::string>& crumbs);

// ─── 5) TitleBar custom / window chrome ──────────────────────────────────────

/// Resultado de los caption buttons de la TitleBar este frame.
struct TitleBarResult {
    bool minimizePressed = false;
    bool maximizePressed = false;
    bool closePressed = false;
};

/// Barra de título custom: icono + título a la izquierda, `centerContent` opcional
/// al centro y caption buttons (minimizar/maximizar/cerrar) a la derecha. Publica
/// las zonas draggable / no-draggable en el UIContext para que el callback de
/// hit-test (registrado por FluentApp con AppConfig::useCustomTitleBar) mueva/
/// redimensione la ventana sin que los controles inicien arrastre.
/// brief 26: Min/Max/Restore y Close actúan por el puerto de plataforma
/// (GetPlatform(ctx)->MinimizeWindow/... / RequestWindowClose), no vía SDL directo.
/// Reutilizable en cualquier ventana (incl. flotantes del brief 09).
TitleBarResult TitleBar(const std::string& id, const std::string& title,
                        uint32_t icon = 0,
                        std::function<void()> centerContent = nullptr);

} // namespace FluentUI
