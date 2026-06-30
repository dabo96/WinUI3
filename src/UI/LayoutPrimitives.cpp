// brief 19 — Primitivas de layout: WrapPanel, UniformGrid, breakpoints y Canvas.
//
// All of these are pure CPU-side layout helpers built on the existing layout
// machinery (ConsumeNextConstraints / ApplyConstraints / AdvanceCursor and the
// container stacks in UIContext). They behave identically under the GL and
// Vulkan backends. New container state lives in the dedicated stacks added to
// UIContext (wrapStack / uniformGridStack / canvasStack); the only change to the
// shared core is the orthogonal `isWrap` flag on LayoutStack handled inside
// AdvanceCursor (see WidgetHelpers.cpp).
#include "UI/Widgets.h"
#include "UI/WidgetHelpers.h"
#include "core/Context.h"
#include "core/Renderer.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <string>

namespace FluentUI {

// ─── WrapPanel ────────────────────────────────────────────────────────────────
//
// Children flow left→right and wrap to a new line when the row fills up. The
// wrap itself is performed inside AdvanceCursor's isWrap branch; here we only set
// up / tear down the layout + the parallel WrapFrameContext bookkeeping. Children
// must be Auto-sized: a Fill-width child would otherwise consume the whole row.

void BeginWrapPanel(const std::string &id, float hGap, float vGap) {
  UIContext *ctx = GetContext();
  if (!ctx)
    return;

  Vec2 padding(0.0f, 0.0f);
  Vec2 available = ComputeAvailableSpace(ctx, std::nullopt, padding);

  LayoutStack stack;
  stack.origin = ctx->cursorPos;
  stack.spacing = 0.0f; // gaps are tracked per-row in the WrapFrameContext
  stack.isVertical = false;
  stack.isWrap = true;
  stack.padding = padding;
  stack.contentSize = Vec2(0.0f, 0.0f);
  stack.availableSpace = available;
  stack.lineHeight = 0.0f;
  stack.itemCount = 0;
  stack.contentStart = stack.origin + stack.padding;
  stack.cursor = stack.contentStart;
  ctx->layoutStack.push_back(stack);
  ctx->cursorPos = stack.contentStart;
  ctx->offsetStack.push_back(stack.contentStart);

  WrapFrameContext wf;
  wf.origin = stack.origin;
  wf.left = stack.contentStart.x;
  wf.availWidth = available.x;
  wf.hGap = hGap;
  wf.vGap = vGap;
  wf.rowHeight = 0.0f;
  wf.maxWidth = 0.0f;
  wf.totalHeight = 0.0f;
  wf.savedCursor = stack.origin;
  wf.savedLastItemPos = ctx->lastItemPos;
  wf.savedLastItemSize = ctx->lastItemSize;
  ctx->wrapStack.push_back(wf);

  PushID(id.c_str());
}

void EndWrapPanel() {
  UIContext *ctx = GetContext();
  if (!ctx || ctx->layoutStack.empty() || ctx->wrapStack.empty())
    return;
  if (!ctx->layoutStack.back().isWrap)
    return; // mismatched End*/Begin* — leave the stack untouched

  WrapFrameContext wf = ctx->wrapStack.back();
  ctx->wrapStack.pop_back();
  ctx->layoutStack.pop_back();
  if (!ctx->offsetStack.empty())
    ctx->offsetStack.pop_back();
  PopID();

  // Report the full available width × the accumulated row height to the parent.
  float width = wf.availWidth > 0.0f ? wf.availWidth : wf.maxWidth;
  Vec2 finalSize(width, wf.totalHeight);

  ctx->cursorPos = wf.origin;
  ctx->lastItemPos = wf.origin;

  if (!ctx->layoutStack.empty()) {
    AdvanceCursor(ctx, finalSize);
  } else {
    ctx->lastItemSize = finalSize;
    ctx->cursorPos.x = wf.origin.x;
    ctx->cursorPos.y = wf.origin.y + finalSize.y + ctx->style.spacing;
  }
}

// ─── UniformGrid ──────────────────────────────────────────────────────────────
//
// N equal-width columns. A throwaway vertical LayoutStack is pushed so that each
// child's AdvanceCursor mutates it (and not the parent layout); the actual cell
// positions are driven manually here, mirroring BeginGrid/GridNextCell/EndGrid.
// Each cell is given a Fixed-width constraint of cellWidth; height is auto.

void BeginUniformGrid(const std::string &id, int columns, float gap) {
  UIContext *ctx = GetContext();
  if (!ctx)
    return;
  if (columns < 1)
    columns = 1;
  if (gap < 0.0f)
    gap = 0.0f;

  Vec2 available = GetCurrentAvailableSpace(ctx);
  float availWidth = available.x > 0.0f
                         ? available.x
                         : ctx->renderer.GetViewportSize().x - ctx->cursorPos.x;
  if (availWidth < 0.0f)
    availWidth = 0.0f;

  float cellWidth =
      (availWidth - static_cast<float>(columns - 1) * gap) /
      static_cast<float>(columns);
  if (cellWidth < 0.0f)
    cellWidth = 0.0f;

  Vec2 origin = ctx->cursorPos;

  UniformGridFrameContext g;
  g.origin = origin;
  g.columns = columns;
  g.cellWidth = cellWidth;
  g.gap = gap;
  g.currentCell = 0;
  g.rowHeight = 0.0f;
  g.totalHeight = 0.0f;
  g.savedCursor = ctx->cursorPos;
  g.savedLastItemPos = ctx->lastItemPos;
  g.savedLastItemSize = ctx->lastItemSize;
  ctx->uniformGridStack.push_back(g);
  PushID(id.c_str());

  // Throwaway vertical layout to absorb children's AdvanceCursor calls. Width is
  // the full grid width so any Fill-height child resolves sensibly; cell width is
  // pinned per-cell via SetNextConstraints below.
  float gridWidth = cellWidth * static_cast<float>(columns) +
                    static_cast<float>(columns - 1) * gap;
  float gridHeight = available.y > 0.0f ? available.y : 0.0f;
  BeginVertical(0.0f, Vec2(gridWidth, gridHeight), Vec2(0.0f, 0.0f));

  // Place the cursor at the first cell and constrain it.
  ctx->cursorPos = origin;
  LayoutConstraints c{};
  c.width = SizeConstraint::Fixed;
  c.fixedWidth = cellWidth;
  SetNextConstraints(c);
}

void BeginUniformGrid(const std::string &id, float minCellWidth, float gap) {
  UIContext *ctx = GetContext();
  if (!ctx)
    return;
  if (gap < 0.0f)
    gap = 0.0f;

  Vec2 available = GetCurrentAvailableSpace(ctx);
  float availWidth = available.x > 0.0f
                         ? available.x
                         : ctx->renderer.GetViewportSize().x - ctx->cursorPos.x;
  if (availWidth < 0.0f)
    availWidth = 0.0f;

  int columns = 1;
  if (minCellWidth > 0.0f) {
    columns = static_cast<int>(
        std::floor((availWidth + gap) / (minCellWidth + gap)));
    if (columns < 1)
      columns = 1;
  }
  BeginUniformGrid(id, columns, gap);
}

void UniformGridNextCell() {
  UIContext *ctx = GetContext();
  if (!ctx || ctx->uniformGridStack.empty())
    return;

  auto &g = ctx->uniformGridStack.back();

  // Track the height of the item just placed in the current cell.
  float itemBottom = ctx->lastItemPos.y + ctx->lastItemSize.y;
  float cellTopY = g.origin.y + g.totalHeight;
  float itemHeight = itemBottom - cellTopY;
  if (itemHeight > g.rowHeight)
    g.rowHeight = itemHeight;

  g.currentCell++;
  int col = g.currentCell % g.columns;

  // Wrapped to a new row: commit the previous row's height (+ vertical gap).
  if (col == 0) {
    g.totalHeight += g.rowHeight + g.gap;
    g.rowHeight = 0.0f;
  }

  ctx->cursorPos =
      Vec2(g.origin.x + static_cast<float>(col) * (g.cellWidth + g.gap),
           g.origin.y + g.totalHeight);

  LayoutConstraints c{};
  c.width = SizeConstraint::Fixed;
  c.fixedWidth = g.cellWidth;
  SetNextConstraints(c);
}

void EndUniformGrid() {
  UIContext *ctx = GetContext();
  if (!ctx || ctx->uniformGridStack.empty())
    return;

  UniformGridFrameContext g = ctx->uniformGridStack.back();

  // Commit the final (open) row.
  float itemBottom = ctx->lastItemPos.y + ctx->lastItemSize.y;
  float cellTopY = g.origin.y + g.totalHeight;
  float itemHeight = itemBottom - cellTopY;
  if (itemHeight > g.rowHeight)
    g.rowHeight = itemHeight;
  g.totalHeight += g.rowHeight;

  ctx->uniformGridStack.pop_back();
  PopID();

  // Pop the throwaway vertical layout without advancing the parent.
  EndVertical(false);

  float gridWidth = g.cellWidth * static_cast<float>(g.columns) +
                    static_cast<float>(g.columns - 1) * g.gap;

  ctx->lastItemPos = g.savedLastItemPos;
  ctx->lastItemSize = g.savedLastItemSize;
  ctx->cursorPos = g.origin;
  ctx->lastItemPos = g.origin;
  AdvanceCursor(ctx, Vec2(gridWidth, g.totalHeight));
}

// ─── Breakpoints / adaptive layout ────────────────────────────────────────────
//
// Pure functions. Thresholds are WinUI/Fluent logical-px values; physical widths
// are converted to logical px by dividing by the DPI scale.

Breakpoint CurrentBreakpoint(float availWidth) {
  UIContext *ctx = GetContext();

  float physical = availWidth;
  if (physical <= 0.0f) {
    if (ctx && !ctx->layoutStack.empty())
      physical = ctx->layoutStack.back().availableSpace.x;
    else if (ctx)
      physical = ctx->renderer.GetViewportSize().x;
  }

  float logical = physical;
  if (ctx && ctx->dpiScale > 0.0f)
    logical = physical / ctx->dpiScale;

  if (logical < 640.0f)
    return Breakpoint::Small;
  if (logical < 1008.0f)
    return Breakpoint::Medium;
  if (logical < 1366.0f)
    return Breakpoint::Large;
  return Breakpoint::XLarge;
}

void VisibleFrom(Breakpoint min, const std::function<void()> &build) {
  if (!build)
    return;
  if (static_cast<int>(CurrentBreakpoint()) >= static_cast<int>(min))
    build();
}

void AdaptiveLayout(const std::function<void(Breakpoint)> &build) {
  if (build)
    build(CurrentBreakpoint());
}

// ─── Canvas / absolute positioning ────────────────────────────────────────────
//
// Children are positioned by explicit coordinates relative to the canvas origin
// (via their pos param or CanvasChild) and clipped to the canvas rect. A
// throwaway vertical layout stack absorbs any AdvanceCursor calls so the parent
// flow is untouched; the canvas itself advances the parent by its full rect on
// EndCanvas. ResolveAbsolutePosition clamps negative coordinates to 0.

void BeginCanvas(const std::string &id, Vec2 size) {
  UIContext *ctx = GetContext();
  if (!ctx)
    return;

  uint32_t cid = GenerateId("CANVAS:", id.c_str());

  LayoutConstraints c = ConsumeNextConstraints();
  Vec2 canvasSize = ApplyConstraints(ctx, c, size);
  Vec2 origin = ctx->cursorPos;

  CanvasFrameContext cf;
  cf.id = cid;
  cf.origin = origin;
  cf.size = canvasSize;
  cf.savedCursor = ctx->cursorPos;
  cf.savedLastItemPos = ctx->lastItemPos;
  cf.savedLastItemSize = ctx->lastItemSize;
  ctx->canvasStack.push_back(cf);

  ctx->renderer.PushClipRect(origin, canvasSize);
  PushID(id.c_str());

  // Throwaway vertical layout + offset. The offset (= origin) makes a child's pos
  // param resolve relative to the canvas top-left via ResolveAbsolutePosition.
  LayoutStack stack;
  stack.origin = origin;
  stack.contentStart = origin;
  stack.cursor = origin;
  stack.padding = Vec2(0.0f, 0.0f);
  stack.contentSize = Vec2(0.0f, 0.0f);
  stack.availableSpace = canvasSize;
  stack.spacing = 0.0f;
  stack.isVertical = true;
  stack.itemCount = 0;
  ctx->layoutStack.push_back(stack);
  ctx->offsetStack.push_back(origin);
  ctx->cursorPos = origin;
}

void CanvasChild(Vec2 pos, const std::function<void()> &build) {
  UIContext *ctx = GetContext();
  if (!ctx || ctx->canvasStack.empty()) {
    if (build)
      build();
    return;
  }

  Vec2 origin = ctx->canvasStack.back().origin;
  Vec2 childOrigin = origin + pos;
  if (childOrigin.x < origin.x)
    childOrigin.x = origin.x;
  if (childOrigin.y < origin.y)
    childOrigin.y = origin.y;

  ctx->cursorPos = childOrigin;
  // Group children of this child at childOrigin; don't advance the canvas flow.
  BeginVertical(-1.0f, std::nullopt, Vec2(0.0f, 0.0f));
  if (build)
    build();
  EndVertical(false);
  ctx->cursorPos = origin;
}

void EndCanvas() {
  UIContext *ctx = GetContext();
  if (!ctx || ctx->canvasStack.empty())
    return;

  // Pop the throwaway layout + offset pushed in BeginCanvas.
  if (!ctx->layoutStack.empty())
    ctx->layoutStack.pop_back();
  if (!ctx->offsetStack.empty())
    ctx->offsetStack.pop_back();
  PopID();
  ctx->renderer.PopClipRect();

  CanvasFrameContext cf = ctx->canvasStack.back();
  ctx->canvasStack.pop_back();

  ctx->cursorPos = cf.origin;
  ctx->lastItemPos = cf.origin;
  AdvanceCursor(ctx, cf.size);
}

} // namespace FluentUI
