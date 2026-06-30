#pragma once
#include "Math/Vec2.h"
#include "UI/Layout.h"
#include "core/Context.h"
#include "core/Renderer.h"
#include "Theme/Style.h"
#include "Theme/FluentTheme.h"
#include <string>
#include <optional>
#include <algorithm>
#include <cmath>

namespace FluentUI {

// DPI scaling helper — multiplies a pixel value by the current display scale factor
inline float DPIScale(UIContext* ctx, float px) {
  return ctx ? px * ctx->dpiScale : px;
}

// Shared layout and helper functions used across widget modules

bool IsRectInViewport(UIContext *ctx, const Vec2 &pos, const Vec2 &size);
float ResolveSpacing(UIContext *ctx, float spacing);
Vec2 ResolvePadding(UIContext *ctx, const std::optional<Vec2> &paddingOpt);
Vec2 CurrentOffset(UIContext *ctx);
Vec2 GetParentAvailableSpace(UIContext *ctx);
Vec2 ComputeAvailableSpace(UIContext *ctx, const std::optional<Vec2> &explicitSize, const Vec2 &padding);
LayoutConstraints ConsumeNextConstraints(SizeConstraint defaultWidth = SizeConstraint::Auto);
Vec2 GetCurrentAvailableSpace(UIContext *ctx);
Vec2 ApplyConstraints(UIContext *ctx, const LayoutConstraints &constraints, const Vec2 &desiredSize);
float CurrentLayoutSpacing(UIContext *ctx);
float GetCurrentSpacing(UIContext *ctx);
bool RectanglesOverlap(const Vec2 &pos1, const Vec2 &size1, const Vec2 &pos2, const Vec2 &size2);
Vec2 ResolveAbsolutePosition(UIContext *ctx, const Vec2 &desiredPos, const Vec2 &widgetSize);
void AdvanceCursor(UIContext *ctx, const Vec2 &size);
Vec2 MeasureTextCached(UIContext *ctx, const std::string &text, float fontSize);
Vec2 MeasureTextCached(UIContext *ctx, const std::string &text, float fontSize, const std::string &fontName);

// Style-aware text drawing/measuring: uses fontName from TextStyle if set
void DrawStyledText(UIContext *ctx, const Vec2 &pos, const std::string &text, const TextStyle &style);
Vec2 MeasureStyledText(UIContext *ctx, const std::string &text, const TextStyle &style);
uint32_t GenerateId(const char *str);
// Issue 14: Overloads that avoid string concatenation
uint32_t GenerateId(const char *prefix, const char *str);
uint32_t GenerateId(const char *a, const char *b, const char *c);

// brief 21: ID scope stack (ImGui-style PushID/PopID). Push a new scope seed
// derived from the current seed + a discriminant; subsequent GenerateId() calls
// (i.e. all widgets) mix that seed in, so identical labels under different scopes
// get distinct IDs without "##" suffixes. Containers push/pop automatically; user
// code calls PushID(index) when iterating items with repeated labels. Must be
// balanced (every PushID paired with a PopID); a debug assert at end-of-frame
// catches leaks.
void PushID(const char *str);
void PushID(int i);
void PushID(const void *ptr);
void PopID();

// Issue 8: Shared UTF-8 decoder (defined in Renderer.cpp)
std::uint32_t DecodeUTF8(const char*& ptr, const char* end);

// Calculate animation slot key for a widget ID to avoid hash collisions.
// Use slot 0, 1, 2 for background, foreground, border color animations.
uint32_t AnimSlot(uint32_t widgetId, uint32_t slot);

// Scrollbar helper shared by ScrollView, TabView, ListView
void DrawScrollbar(UIContext *ctx, const Vec2 &barPos, const Vec2 &barSize,
                   float contentSize, float viewSize, float &scrollOffset,
                   bool isDragging, Vec2 &dragStartMouse, float &dragStartScroll,
                   bool &draggingOut, bool isVertical = true);

// Adjust a background color for better contrast depending on the theme.
// Dark theme: lighten by 15%; Light theme: darken by 8%.
Color AdjustContainerBackground(const Color &bg, bool isDarkTheme);

// Background for list-like "well" surfaces (ListView / TreeView). These widgets
// often sit inside a panel whose colour is already AdjustContainerBackground,
// so a single adjustment is nearly invisible. This applies a stronger contrast
// so the surface reads clearly against the surrounding panel (Fluent style).
Color AdjustListSurfaceBackground(const Color &bg, bool isDarkTheme);

// Draw a Unicode icon glyph (from the secondary icon font) vertically centered
// inside the rect [rectPos, rectPos+rectSize], placed at rectPos.x+leftPadding.
// Returns the horizontal slot consumed (iconSize + gap) when codepoint != 0,
// regardless of whether the icon font is currently loaded — this keeps widget
// layout stable across font hot-reloads. Returns 0 when codepoint == 0 so
// callers can use the result unconditionally as a horizontal advance.
float DrawWidgetIcon(UIContext *ctx, const Vec2 &rectPos, const Vec2 &rectSize,
                     uint32_t codepoint, const Color &color,
                     float iconSize, float leftPadding = 0.0f, float gap = 6.0f);

// Background for text / number / combo input fields. Recessed (darker than the
// surrounding surface) on dark themes and lifted toward white on light themes, so
// the field reads as a distinct control in both. `hover` returns the subtler
// hover variant. Avoids the near-invisible fields that result from deriving the
// field colour from panel.background with a tiny multiplier.
Color InputFieldBackground(UIContext *ctx, bool hover);

// 1px border colour for input fields. Slightly brighter than the surface on dark
// themes and slightly darker on light themes, so the field edge is always visible
// against the panel even when the recessed fill is close to the background.
// `hover` returns a stronger variant for hover feedback.
Color InputFieldBorder(UIContext *ctx, bool hover);

// Common utility
bool PointInRect(const Vec2 &p, const Vec2 &pos, const Vec2 &size);

// Layout constants (base values at 1x DPI) — aligned to Fluent 2 / Win11 4px grid
constexpr float MENUBAR_HEIGHT = 32.0f;
constexpr float SCROLLBAR_WIDTH = 10.0f;
constexpr float SCROLLBAR_WIDTH_LARGE = 12.0f;
constexpr float SCROLLBAR_THUMB_MIN = 20.0f;
// Inner gutters for scrollable containers (TabView / Panel / ScrollView):
//   - CONTENT_LEFT_GUTTER: the content clip is extended this many px to the left
//     so left-aligned widgets (checkbox boxes, focus rings, rounded borders) are
//     not shaved by the clip boundary. The gutter lands inside the container's
//     own edge padding, so it never bleeds outside the container.
//   - SCROLLBAR_GUTTER: gap reserved between trailing content (e.g. slider value
//     text) and the scrollbar so content never sits underneath it.
constexpr float CONTENT_LEFT_GUTTER = 3.0f;
constexpr float SCROLLBAR_GUTTER = 4.0f;
constexpr float TOOLBAR_HEIGHT = 40.0f;
constexpr float STATUSBAR_HEIGHT = 24.0f;
constexpr float DROPDOWN_MAX_HEIGHT = 200.0f;
constexpr float SCROLL_SPEED = 30.0f;
constexpr float FOCUS_RING_OFFSET = 2.0f;
constexpr float FOCUS_RING_OPACITY = 0.6f;

// Shorthand: scale a layout constant by DPI. Usage: S(MENUBAR_HEIGHT)
#define S(val) DPIScale(ctx, val)

// Draw a Fluent Design focus ring around a widget
inline void DrawFocusRing(UIContext *ctx, const Vec2 &pos, const Vec2 &size,
                          float cornerRadius = 4.0f) {
  Color focusColor = FluentColors::Accent;
  focusColor.a = FOCUS_RING_OPACITY;
  Vec2 offset(FOCUS_RING_OFFSET, FOCUS_RING_OFFSET);
  ctx->renderer.DrawRectFilled(pos - offset, size + offset * 2.0f, focusColor,
                               cornerRadius + FOCUS_RING_OFFSET);
}

// Mecanismo unificado de "input clickthrough": ¿debe este widget de fondo
// ignorar el input del ratón porque hay un overlay capturándolo encima?
//
//  - ContextMenu y Modal son "modales": mientras están activos bloquean TODO
//    el fondo (su propio contenido queda exento vía insideContextMenu/insideModal).
//  - ComboBox y MenuBar bloquean solo los widgets que caen DEBAJO de su dropdown
//    (su contenido queda exento vía insideOverlayRender), de modo que el resto
//    de la UI sigue activo (p.ej. para cerrar el dropdown al hacer click fuera).
//
// Los overlays se dibujan diferidos/encima pero el input se procesa en modo
// inmediato widget por widget; sin esta comprobación, el widget de debajo
// procesaría el click destinado al overlay.
inline bool IsMouseInputBlocked(UIContext *ctx) {
  // El propio contenido interactivo de un overlay (items de combo o de menú)
  // nunca se bloquea, esté donde esté (p.ej. un combo dentro de un modal cuyos
  // items se renderizan diferidos tras EndModal).
  if (ctx->insideOverlayRender) return false;
  // Context menu activo: bloqueo global de todo el fondo.
  if (ctx->activeContextMenuId != 0 && !ctx->insideContextMenu) return true;
  // Modal activo: bloqueo global de todo lo que no sea el modal.
  if (ctx->activeModalId != 0 && !ctx->insideModal) return true;

  // Combo/menú abierto: bloqueo solo bajo el rect de su dropdown.
  float mx = ctx->input.MouseX();
  float my = ctx->input.MouseY();
  if (ctx->openComboId != 0 &&
      PointInRect(Vec2(mx, my), ctx->openComboDropdownPos,
                  ctx->openComboDropdownSize)) {
    return true;
  }
  // Menú: rect persistente (no activeMenuId, que MenuItem limpia a mitad de
  // frame y dejaría pasar el click al widget de debajo).
  if (ctx->openMenuId != 0 &&
      PointInRect(Vec2(mx, my), ctx->openMenuDropdownPos,
                  ctx->openMenuDropdownSize)) {
    return true;
  }
  return false;
}

// Check if mouse cursor is over a rectangle.
// Returns false when the mouse input is captured by an active overlay
// (context menu / modal / combo / menu) so clicks don't leak to widgets below.
inline bool IsMouseOver(UIContext *ctx, const Vec2 &pos, const Vec2 &size) {
  if (IsMouseInputBlocked(ctx)) {
    return false;
  }
  float mx = ctx->input.MouseX();
  float my = ctx->input.MouseY();
  bool over = mx >= pos.x && mx <= pos.x + size.x &&
              my >= pos.y && my <= pos.y + size.y;
  if (over) ctx->mouseOverAnyWidget = true;
  return over;
}

// Perf 1.2: Register animation slots for widgets that use color/float animations
// Call this only from widgets that actually use ctx->colorAnimations[AnimSlot(...)]
void RegisterAnimSlots(uint32_t widgetId);

} // namespace FluentUI
