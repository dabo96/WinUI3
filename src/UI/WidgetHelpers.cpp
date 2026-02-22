#include "UI/WidgetHelpers.h"
#include "core/Context.h"
#include "core/Renderer.h"
#include <cmath>
#include <algorithm>

namespace FluentUI {

// Static variable to store layout constraints for the next widget
static std::optional<LayoutConstraints> nextConstraints;

bool IsRectInViewport(UIContext *ctx, const Vec2 &pos, const Vec2 &size) {
  if (!ctx)
    return true;

  Vec2 viewport = ctx->renderer.GetViewportSize();
  Vec2 clipPos(0.0f, 0.0f);
  Vec2 clipSize = viewport;

  if (!ctx->renderer.GetClipStack().empty()) {
    const auto &clip = ctx->renderer.GetClipStack().back();
    clipPos = Vec2(static_cast<float>(clip.x), static_cast<float>(clip.y));
    clipSize =
        Vec2(static_cast<float>(clip.width), static_cast<float>(clip.height));
  }

  return !(pos.x + size.x <= clipPos.x || pos.x >= clipPos.x + clipSize.x ||
           pos.y + size.y <= clipPos.y || pos.y >= clipPos.y + clipSize.y);
}

float ResolveSpacing(UIContext *ctx, float spacing) {
  if (spacing >= 0.0f) {
    return spacing;
  }
  return ctx ? ctx->style.spacing : 4.0f;
}

Vec2 ResolvePadding(UIContext *ctx, const std::optional<Vec2> &paddingOpt) {
  if (paddingOpt.has_value()) {
    return paddingOpt.value();
  }
  float p = ctx ? ctx->style.padding : 6.0f;
  return Vec2(p, p);
}

Vec2 CurrentOffset(UIContext *ctx) {
  if (!ctx || ctx->offsetStack.empty()) {
    return Vec2(0.0f, 0.0f);
  }
  return ctx->offsetStack.back();
}

Vec2 GetParentAvailableSpace(UIContext *ctx) {
  if (!ctx)
    return Vec2(0.0f, 0.0f);

  if (ctx->layoutStack.empty()) {
    Vec2 viewport = ctx->renderer.GetViewportSize();
    float availableX =
        std::max(0.0f, viewport.x - ctx->cursorPos.x - ctx->style.padding);
    float availableY =
        std::max(0.0f, viewport.y - ctx->cursorPos.y - ctx->style.padding);
    return Vec2(availableX, availableY);
  }
  return ctx->layoutStack.back().availableSpace;
}

Vec2 ComputeAvailableSpace(UIContext *ctx,
                            const std::optional<Vec2> &explicitSize,
                            const Vec2 &padding) {
  Vec2 parentAvailable = GetParentAvailableSpace(ctx);
  if (explicitSize.has_value()) {
    parentAvailable = explicitSize.value();
  }

  Vec2 contentAvailable(std::max(0.0f, parentAvailable.x - padding.x * 2.0f),
                        std::max(0.0f, parentAvailable.y - padding.y * 2.0f));
  return contentAvailable;
}

LayoutConstraints ConsumeNextConstraints() {
  if (nextConstraints.has_value()) {
    LayoutConstraints c = nextConstraints.value();
    nextConstraints.reset();
    return c;
  }
  return LayoutConstraints{};
}

Vec2 GetCurrentAvailableSpace(UIContext *ctx) {
  Vec2 space = GetParentAvailableSpace(ctx);
  if (ctx && !ctx->layoutStack.empty()) {
    space = ctx->layoutStack.back().availableSpace;
  }
  return Vec2(std::max(0.0f, space.x), std::max(0.0f, space.y));
}

Vec2 ApplyConstraints(UIContext *ctx, const LayoutConstraints &constraints,
                       const Vec2 &desiredSize) {
  Vec2 result = desiredSize;
  Vec2 available = GetCurrentAvailableSpace(ctx);

  switch (constraints.width) {
  case SizeConstraint::Fixed:
    result.x = constraints.fixedWidth;
    break;
  case SizeConstraint::Fill:
    result.x = available.x > 0.0f ? available.x : desiredSize.x;
    break;
  case SizeConstraint::Auto:
    if (constraints.fixedWidth > 0.0f) {
      result.x = constraints.fixedWidth;
    }
    break;
  }

  switch (constraints.height) {
  case SizeConstraint::Fixed:
    result.y = constraints.fixedHeight;
    break;
  case SizeConstraint::Fill:
    result.y = available.y > 0.0f ? available.y : desiredSize.y;
    break;
  case SizeConstraint::Auto:
    if (constraints.fixedHeight > 0.0f) {
      result.y = constraints.fixedHeight;
    }
    break;
  }

  if (constraints.minWidth > 0.0f)
    result.x = std::max(result.x, constraints.minWidth);
  if (constraints.maxWidth > 0.0f)
    result.x = std::min(result.x, constraints.maxWidth);
  if (constraints.minHeight > 0.0f)
    result.y = std::max(result.y, constraints.minHeight);
  if (constraints.maxHeight > 0.0f)
    result.y = std::min(result.y, constraints.maxHeight);

  result.x = std::max(result.x, 0.0f);
  result.y = std::max(result.y, 0.0f);
  return result;
}

float CurrentLayoutSpacing(UIContext *ctx) {
  if (!ctx)
    return 0.0f;
  if (!ctx->layoutStack.empty())
    return ctx->layoutStack.back().spacing;
  return ctx->style.spacing;
}

float GetCurrentSpacing(UIContext *ctx) { return CurrentLayoutSpacing(ctx); }

bool RectanglesOverlap(const Vec2 &pos1, const Vec2 &size1, const Vec2 &pos2,
                        const Vec2 &size2) {
  return !(pos1.x + size1.x <= pos2.x || pos2.x + size2.x <= pos1.x ||
           pos1.y + size1.y <= pos2.y || pos2.y + size2.y <= pos1.y);
}

Vec2 ResolveAbsolutePosition(UIContext *ctx, const Vec2 &desiredPos,
                              const Vec2 &widgetSize) {
  if (!ctx)
    return desiredPos;

  Vec2 resolvedPos = desiredPos;
  Vec2 viewport = ctx->renderer.GetViewportSize();

  if (resolvedPos.x < 0.0f) {
    resolvedPos.x = 0.0f;
  }
  if (resolvedPos.y < 0.0f) {
    resolvedPos.y = 0.0f;
  }

  return resolvedPos;
}

void RegisterOccupiedArea(UIContext *ctx, const Vec2 &pos, const Vec2 &size) {
  if (!ctx)
    return;

  OccupiedArea area;
  area.pos = pos;
  area.size = size;

  if (!ctx->layoutStack.empty()) {
    ctx->layoutStack.back().occupiedAreas.push_back(area);
  }
}

void ClearOccupiedAreas(UIContext *ctx) {
  if (!ctx)
    return;
  ctx->globalOccupiedAreas.clear();
  for (auto &stack : ctx->layoutStack) {
    stack.occupiedAreas.clear();
  }
}

void AdvanceCursor(UIContext *ctx, const Vec2 &size) {
  if (!ctx)
    return;

  ctx->lastItemSize = size;

  if (ctx->layoutStack.empty()) {
    ctx->cursorPos.y += size.y + ctx->style.spacing;
    return;
  }

  LayoutStack &stack = ctx->layoutStack.back();
  if (stack.isVertical) {
    stack.contentSize.x = std::max(stack.contentSize.x, size.x);
    stack.contentSize.y += size.y;
    stack.cursor.y += size.y;
    stack.availableSpace.y = std::max(0.0f, stack.availableSpace.y - size.y);
    stack.itemCount++;
    if (stack.spacing > 0.0f) {
      stack.cursor.y += stack.spacing;
      stack.availableSpace.y =
          std::max(0.0f, stack.availableSpace.y - stack.spacing);
    }
    ctx->cursorPos = Vec2(stack.cursor.x, stack.cursor.y);
  } else {
    stack.contentSize.x += size.x;
    stack.contentSize.y = std::max(stack.contentSize.y, size.y);
    stack.cursor.x += size.x;
    stack.availableSpace.x = std::max(0.0f, stack.availableSpace.x - size.x);
    stack.itemCount++;
    if (stack.spacing > 0.0f) {
      stack.cursor.x += stack.spacing;
      stack.availableSpace.x =
          std::max(0.0f, stack.availableSpace.x - stack.spacing);
    }
    ctx->cursorPos = Vec2(stack.cursor.x, stack.contentStart.y);
  }
}

Vec2 MeasureTextCached(UIContext *ctx, const std::string &text,
                        float fontSize) {
  if (!ctx || text.empty())
    return Vec2(0.0f, 0.0f);

  std::string cacheKey = text + "|" + std::to_string(fontSize);

  auto it = ctx->textMeasurementCache.find(cacheKey);
  if (it != ctx->textMeasurementCache.end()) {
    return it->second;
  }

  Vec2 size = ctx->renderer.MeasureText(text, fontSize);

  // Evict cache if it exceeds max size
  if (ctx->textMeasurementCache.size() >= UIContext::TEXT_CACHE_MAX_SIZE) {
    ctx->textMeasurementCache.clear();
  }

  ctx->textMeasurementCache[cacheKey] = size;
  return size;
}

// Animation slot offsets - use large prime-based offsets to avoid collisions
// with other widget IDs. Widgets that use color animations store them at
// AnimSlot(id, 0), AnimSlot(id, 1), AnimSlot(id, 2).
uint32_t AnimSlot(uint32_t widgetId, uint32_t slot) {
  // Use a large offset with different primes per slot to minimize collision risk
  constexpr uint32_t SLOT_OFFSET_BASE = 0x9E3779B9u; // golden ratio fractional
  return widgetId + SLOT_OFFSET_BASE * (slot + 1);
}

uint32_t GenerateId(const char *str) {
  uint32_t hash = 5381;
  int c;
  while ((c = *str++)) {
    hash = ((hash << 5) + hash) + c;
  }
  UIContext *ctx = GetContext();
  if (ctx) {
    uint32_t frame = ctx->frame;
    ctx->lastSeenFrame[hash] = frame;
    // Register animation slots so GC doesn't collect them
    ctx->lastSeenFrame[AnimSlot(hash, 0)] = frame;
    ctx->lastSeenFrame[AnimSlot(hash, 1)] = frame;
    ctx->lastSeenFrame[AnimSlot(hash, 2)] = frame;
  }
  return hash;
}

void DrawScrollbar(UIContext *ctx, const Vec2 &barPos, const Vec2 &barSize,
                   float contentSize, float viewSize, float &scrollOffset,
                   bool isDragging, Vec2 &dragStartMouse, float &dragStartScroll,
                   bool &draggingOut, bool isVertical) {
  if (!ctx || contentSize <= viewSize)
    return;

  float mouseX = ctx->input.MouseX();
  float mouseY = ctx->input.MouseY();
  bool leftDown = ctx->input.IsMouseDown(0);
  bool leftPressed = ctx->input.IsMousePressed(0);

  float ratio = viewSize / contentSize;
  float thumbLength = std::max(20.0f, (isVertical ? barSize.y : barSize.x) * ratio);
  float trackLength = isVertical ? barSize.y : barSize.x;
  float maxThumbTravel = trackLength - thumbLength;
  float maxScroll = std::max(0.0f, contentSize - viewSize);

  float thumbOffset = maxScroll > 0.0f ? (scrollOffset / maxScroll) * maxThumbTravel : 0.0f;

  Vec2 thumbPos = isVertical ? Vec2(barPos.x, barPos.y + thumbOffset)
                             : Vec2(barPos.x + thumbOffset, barPos.y);
  Vec2 thumbSize = isVertical ? Vec2(barSize.x, thumbLength)
                              : Vec2(thumbLength, barSize.y);

  bool hoverThumb = (mouseX >= thumbPos.x && mouseX <= thumbPos.x + thumbSize.x &&
                     mouseY >= thumbPos.y && mouseY <= thumbPos.y + thumbSize.y);
  bool hoverTrack = (mouseX >= barPos.x && mouseX <= barPos.x + barSize.x &&
                     mouseY >= barPos.y && mouseY <= barPos.y + barSize.y);

  if (isDragging) {
    if (!leftDown) {
      draggingOut = false;
    } else {
      float mouseDelta = isVertical ? (mouseY - dragStartMouse.y) : (mouseX - dragStartMouse.x);
      float scrollDelta = maxThumbTravel > 0.0f ? (mouseDelta / maxThumbTravel) * maxScroll : 0.0f;
      scrollOffset = std::clamp(dragStartScroll + scrollDelta, 0.0f, maxScroll);
    }
  } else if (leftPressed && hoverThumb) {
    draggingOut = true;
    dragStartMouse = Vec2(mouseX, mouseY);
    dragStartScroll = scrollOffset;
  } else if (leftPressed && hoverTrack) {
    float clickPos = isVertical ? (mouseY - barPos.y) : (mouseX - barPos.x);
    float scrollRatio = clickPos / trackLength;
    scrollOffset = std::clamp(scrollRatio * maxScroll, 0.0f, maxScroll);
  }

  Color trackColor = hoverTrack ? ctx->style.panel.headerBackground : ctx->style.panel.background;
  ctx->renderer.DrawRectFilled(barPos, barSize, trackColor, 0.0f);

  Color thumbColor = (hoverThumb || isDragging)
                         ? ctx->style.button.background.hover
                         : ctx->style.button.background.normal;
  ctx->renderer.DrawRectFilled(thumbPos, thumbSize, thumbColor, 4.0f);
}

Color AdjustContainerBackground(const Color &bg, bool isDarkTheme) {
  if (isDarkTheme) {
    return Color(bg.r * 1.15f, bg.g * 1.15f, bg.b * 1.15f, 1.0f);
  }
  return Color(bg.r * 0.92f, bg.g * 0.92f, bg.b * 0.92f, 1.0f);
}

bool PointInRect(const Vec2 &p, const Vec2 &pos, const Vec2 &size) {
  return p.x >= pos.x && p.x <= pos.x + size.x && p.y >= pos.y &&
         p.y <= pos.y + size.y;
}

void SetNextConstraints(const LayoutConstraints &constraints) {
  nextConstraints = constraints;
}

} // namespace FluentUI
