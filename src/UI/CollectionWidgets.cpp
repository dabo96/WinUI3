// CollectionWidgets.cpp — Brief 16 (collections at app scale).
// Pagination and FlipView/Carousel. GridView/DataGrid/ExpanderList live next to
// the Table in ListWidgets.cpp; these two are standalone collection controls.
#include "UI/Widgets.h"
#include "UI/WidgetHelpers.h"
#include "Theme/FluentTheme.h"
#include "core/Animation.h"
#include "core/Context.h"
#include "core/Renderer.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace FluentUI {

// ─── Pagination ──────────────────────────────────────────────────────────────
// ‹ 1 2 … 8 9 … 42 › — first/last always shown, current ±1, ellipsis for gaps.
int Pagination(const std::string &id, int pageCount, int *currentPage) {
  UIContext *ctx = GetContext();
  if (!ctx || pageCount <= 0)
    return 0;

  uint32_t pgId = GenerateId("PAGINATION:", id.c_str());
  ctx->focusableWidgets.push_back(pgId);

  int cur = currentPage ? *currentPage
                        : ctx->intStates.try_emplace(pgId, 0).first->second;
  cur = std::clamp(cur, 0, pageCount - 1);

  bool focused = (ctx->focusedWidgetId == pgId);
  if (focused) {
    if (ctx->input.IsKeyPressed(SDL_SCANCODE_LEFT))  cur = std::max(0, cur - 1);
    if (ctx->input.IsKeyPressed(SDL_SCANCODE_RIGHT)) cur = std::min(pageCount - 1, cur + 1);
  }

  // Build the visible page set: 0, last, cur-1..cur+1.
  std::vector<int> pages;
  auto want = [&](int p) {
    if (p >= 0 && p < pageCount) pages.push_back(p);
  };
  want(0); want(pageCount - 1); want(cur - 1); want(cur); want(cur + 1);
  std::sort(pages.begin(), pages.end());
  pages.erase(std::unique(pages.begin(), pages.end()), pages.end());

  // Token stream: -2 = prev, -3 = next, -1 = ellipsis, >=0 = page index.
  std::vector<int> tokens;
  tokens.push_back(-2);
  int prevPage = -1;
  for (int p : pages) {
    if (prevPage >= 0 && p > prevPage + 1) tokens.push_back(-1);
    tokens.push_back(p);
    prevPage = p;
  }
  tokens.push_back(-3);

  // Layout + draw.
  const TextStyle &ts = ctx->style.GetTextStyle(TypographyStyle::Body);
  const float ascender = ctx->renderer.GetFontAscender();
  const float voff = (ascender - 0.7f * 0.5f) * ts.fontSize;
  const float h = 32.0f;
  const float minW = 32.0f;
  const float gap = 4.0f;
  const float padX = 10.0f;

  Vec2 origin = ctx->cursorPos;
  float x = origin.x;
  const float y = origin.y;

  for (int token : tokens) {
    std::string label;
    bool isEllipsis = (token == -1);
    bool enabled = true;
    bool isCur = false;
    if (token == -2) { label = "<"; enabled = cur > 0; }
    else if (token == -3) { label = ">"; enabled = cur < pageCount - 1; }
    else if (token == -1) { label = "..."; }
    else { label = std::to_string(token + 1); isCur = (token == cur); }

    Vec2 ms = MeasureTextCached(ctx, label, ts.fontSize);
    float w = std::max(minW, ms.x + padX * 2.0f);
    Vec2 bp(x, y), bs(w, h);

    if (isEllipsis) {
      Color c = ts.color; c.a *= 0.6f;
      ctx->renderer.DrawText(Vec2(bp.x + (w - ms.x) * 0.5f, bp.y + h * 0.5f - voff),
                             label, c, ts.fontSize);
    } else {
      bool hover = enabled && IsMouseOver(ctx, bp, bs);
      bool down = hover && ctx->input.IsMouseDown(0);

      Color fill;
      Color textCol = ts.color;
      if (isCur) {
        fill = ctx->style.button.background.normal; // accent
        textCol = Color(1, 1, 1, 1);
      } else if (down) {
        fill = ctx->style.button.background.pressed; fill.a *= 0.45f;
      } else if (hover) {
        fill = ctx->style.button.background.hover; fill.a *= 0.22f;
      } else {
        fill = InputFieldBackground(ctx, false);
      }
      ctx->renderer.DrawRectFilled(bp, bs, fill, 4.0f);
      if (!isCur) {
        Color border = InputFieldBorder(ctx, hover); border.a *= 0.6f;
        ctx->renderer.DrawRect(bp, bs, border, 4.0f);
      }
      if (!enabled) {
        Color cc = textCol; cc.a *= 0.4f; textCol = cc;
      }
      ctx->renderer.DrawText(Vec2(bp.x + (w - ms.x) * 0.5f, bp.y + h * 0.5f - voff),
                             label, textCol, ts.fontSize);

      if (hover) ctx->desiredCursor = UIContext::CursorType::Hand;
      if (hover && ctx->input.IsMousePressed(0)) {
        if (token == -2) cur = std::max(0, cur - 1);
        else if (token == -3) cur = std::min(pageCount - 1, cur + 1);
        else cur = token;
        ctx->focusedWidgetId = pgId;
      }
    }
    x += w + gap;
  }

  float totalW = std::max(0.0f, x - origin.x - gap);
  cur = std::clamp(cur, 0, pageCount - 1);
  if (currentPage) *currentPage = cur;
  ctx->intStates[pgId] = cur;

  if (focused)
    DrawFocusRing(ctx, origin, Vec2(totalW, h), 4.0f);

  ctx->lastItemPos = origin;
  ctx->lastItemSize = Vec2(totalW, h);
  SetLastItem(pgId, origin, origin + Vec2(totalW, h), false, false, focused, false);
  AdvanceCursor(ctx, Vec2(totalW, h));
  return cur;
}

// ─── FlipView / Carousel ─────────────────────────────────────────────────────
// One item visible with ‹ › arrows + dot indicators. Slide-in transition (brief
// 10); degrades to a cut when motion is off (progress jumps to 1).
int FlipView(const std::string &id, int itemCount,
             const std::function<void(int)> &itemBuilder, int *currentIndex) {
  UIContext *ctx = GetContext();
  if (!ctx || itemCount <= 0)
    return 0;

  uint32_t fvId = GenerateId("FLIPVIEW:", id.c_str());
  ctx->focusableWidgets.push_back(fvId);

  int idx = currentIndex ? *currentIndex
                         : ctx->intStates.try_emplace(fvId, 0).first->second;
  idx = std::clamp(idx, 0, itemCount - 1);

  uint32_t animId = GenerateId("FLIPANIM:", id.c_str());
  uint32_t dirId = GenerateId("FLIPDIR:", id.c_str());
  float &progress = ctx->floatStates.try_emplace(animId, 1.0f).first->second;
  int &slideDir = ctx->intStates.try_emplace(dirId, 1).first->second;

  bool focused = (ctx->focusedWidgetId == fvId);

  Vec2 avail = GetCurrentAvailableSpace(ctx);
  float w = avail.x > 0.0f ? avail.x
                           : ctx->renderer.GetViewportSize().x - ctx->cursorPos.x;
  float hgt = avail.y > 0.0f ? avail.y : 300.0f;
  if (w < 1.0f) w = 1.0f;
  if (hgt < 1.0f) hgt = 1.0f;
  Vec2 origin = ctx->cursorPos;

  const float arrowW = 36.0f;
  const float dotsH = 24.0f;
  Vec2 contentPos(origin.x + arrowW, origin.y);
  Vec2 contentSize(std::max(1.0f, w - 2.0f * arrowW), std::max(1.0f, hgt - dotsH));

  int newIdx = idx;
  if (focused) {
    if (ctx->input.IsKeyPressed(SDL_SCANCODE_LEFT))  newIdx = (idx - 1 + itemCount) % itemCount;
    if (ctx->input.IsKeyPressed(SDL_SCANCODE_RIGHT)) newIdx = (idx + 1) % itemCount;
  }

  // Background panel.
  Color bg = AdjustContainerBackground(ctx->style.panel.background, ctx->style.isDarkTheme);
  ctx->renderer.DrawRectFilled(origin, Vec2(w, hgt), bg, ctx->style.panel.cornerRadius);

  const TextStyle &ts = ctx->style.GetTextStyle(TypographyStyle::Title);
  const float ascender = ctx->renderer.GetFontAscender();

  auto arrowButton = [&](const std::string &glyph, Vec2 ap, Vec2 as, bool en) -> bool {
    bool hover = en && IsMouseOver(ctx, ap, as);
    if (hover) {
      Color hc = ctx->style.button.background.hover; hc.a *= 0.22f;
      ctx->renderer.DrawRectFilled(ap, as, hc, 4.0f);
      ctx->desiredCursor = UIContext::CursorType::Hand;
    }
    Vec2 ms = MeasureTextCached(ctx, glyph, ts.fontSize);
    Color c = ts.color; if (!en) c.a *= 0.35f;
    float voff = (ascender - 0.7f * 0.5f) * ts.fontSize;
    ctx->renderer.DrawText(Vec2(ap.x + (as.x - ms.x) * 0.5f, ap.y + as.y * 0.5f - voff),
                           glyph, c, ts.fontSize);
    return hover && ctx->input.IsMousePressed(0);
  };

  // Arrows (carousel wraps).
  float arrowY = origin.y + (hgt - dotsH) * 0.5f - 18.0f;
  if (arrowButton("<", Vec2(origin.x, arrowY), Vec2(arrowW, 36.0f), true))
    newIdx = (idx - 1 + itemCount) % itemCount;
  if (arrowButton(">", Vec2(origin.x + w - arrowW, arrowY), Vec2(arrowW, 36.0f), true))
    newIdx = (idx + 1) % itemCount;

  // Dot indicators.
  float dotGap = 14.0f;
  float dx = origin.x + w * 0.5f - static_cast<float>(itemCount - 1) * dotGap * 0.5f;
  float dy = origin.y + hgt - dotsH * 0.5f;
  for (int i = 0; i < itemCount; ++i) {
    Vec2 center(dx + static_cast<float>(i) * dotGap, dy);
    bool active = (i == idx);
    Color dc = active ? ctx->style.button.background.normal : ctx->style.panel.borderColor;
    if (!active) dc.a *= 0.7f;
    ctx->renderer.DrawCircle(center, active ? 4.5f : 3.5f, dc, true);
    Vec2 dotBox(center.x - 7.0f, center.y - 7.0f);
    if (IsMouseOver(ctx, dotBox, Vec2(14.0f, 14.0f))) {
      ctx->desiredCursor = UIContext::CursorType::Hand;
      if (ctx->input.IsMousePressed(0)) newIdx = i;
    }
  }

  // Apply navigation change and (re)start the slide-in.
  if (newIdx != idx) {
    slideDir = (newIdx > idx) ? 1 : -1;
    // Treat wrap edges as the natural direction.
    if (idx == 0 && newIdx == itemCount - 1) slideDir = -1;
    if (idx == itemCount - 1 && newIdx == 0) slideDir = 1;
    progress = 0.0f;
    idx = newIdx;
    ctx->focusedWidgetId = fvId;
  }

  // Advance the transition (0.22s); cut == progress already 1.
  if (progress < 1.0f)
    progress = std::min(1.0f, progress + ctx->deltaTime / 0.22f);
  float eased = Easing::EaseOutCubic(std::clamp(progress, 0.0f, 1.0f));
  float off = (1.0f - eased) * contentSize.x * static_cast<float>(slideDir);

  // Content (clipped, slides in).
  ctx->renderer.PushClipRect(contentPos, contentSize);
  Vec2 savedCursor = ctx->cursorPos;
  ctx->cursorPos = Vec2(contentPos.x + off, contentPos.y);
  LayoutConstraints cc{};
  cc.width = SizeConstraint::Fixed;
  cc.fixedWidth = contentSize.x;
  SetNextConstraints(cc);
  PushID(id.c_str());
  if (itemBuilder) itemBuilder(idx);
  PopID();
  ctx->renderer.PopClipRect();

  if (focused)
    DrawFocusRing(ctx, origin, Vec2(w, hgt), ctx->style.panel.cornerRadius);

  idx = std::clamp(idx, 0, itemCount - 1);
  if (currentIndex) *currentIndex = idx;
  ctx->intStates[fvId] = idx;

  ctx->cursorPos = savedCursor;
  ctx->lastItemPos = origin;
  ctx->lastItemSize = Vec2(w, hgt);
  SetLastItem(fvId, origin, origin + Vec2(w, hgt), false, false, focused, false);
  AdvanceCursor(ctx, Vec2(w, hgt));
  return idx;
}

} // namespace FluentUI
