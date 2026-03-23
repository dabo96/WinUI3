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
LayoutConstraints ConsumeNextConstraints();
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

// Common utility
bool PointInRect(const Vec2 &p, const Vec2 &pos, const Vec2 &size);

// Layout constants (base values at 1x DPI)
constexpr float MENUBAR_HEIGHT = 32.0f;
constexpr float SCROLLBAR_WIDTH = 10.0f;
constexpr float SCROLLBAR_WIDTH_LARGE = 12.0f;
constexpr float SCROLLBAR_THUMB_MIN = 20.0f;
constexpr float TOOLBAR_HEIGHT = 36.0f;
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

// Check if mouse cursor is over a rectangle
inline bool IsMouseOver(UIContext *ctx, const Vec2 &pos, const Vec2 &size) {
  float mx = ctx->input.MouseX();
  float my = ctx->input.MouseY();
  return mx >= pos.x && mx <= pos.x + size.x &&
         my >= pos.y && my <= pos.y + size.y;
}

// Perf 1.2: Register animation slots for widgets that use color/float animations
// Call this only from widgets that actually use ctx->colorAnimations[AnimSlot(...)]
void RegisterAnimSlots(uint32_t widgetId);

} // namespace FluentUI
