// ─── brief 13: App shell & navegación ────────────────────────────────────────
// Implementación de NavigationView, NavFrame, CommandBar, BreadcrumbBar y la
// TitleBar custom. Sigue la "anatomía estándar" (12_ui_general_INDEX.md) y reusa
// los overlays del brief 14 (BeginFlyout/MenuFlyout) y los breakpoints del 19.
#include "UI/Widgets.h"
#include "UI/WidgetHelpers.h"
#include "Theme/FluentTheme.h"
#include "core/Context.h"
#include "core/Renderer.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <functional>
#include <string>

namespace FluentUI {

// ─── 2) NavFrame (sin estado de UI: opera sobre el POD del usuario) ───────────

void NavigateTo(NavFrame& f, const std::string& pageKey) {
  if (pageKey == f.current)
    return;
  if (!f.current.empty())
    f.backStack.push_back(f.current);
  f.current = pageKey;
  f.forwardStack.clear();
}

bool NavigateBack(NavFrame& f) {
  if (f.backStack.empty())
    return false;
  if (!f.current.empty())
    f.forwardStack.push_back(f.current);
  f.current = f.backStack.back();
  f.backStack.pop_back();
  return true;
}

bool NavigateForward(NavFrame& f) {
  if (f.forwardStack.empty())
    return false;
  if (!f.current.empty())
    f.backStack.push_back(f.current);
  f.current = f.forwardStack.back();
  f.forwardStack.pop_back();
  return true;
}

// ─── 1) NavigationView ───────────────────────────────────────────────────────

std::string NavigationView(const std::string& id,
                           const std::vector<NavItem>& items,
                           std::string* selectedKey, NavDisplayMode mode,
                           const std::vector<NavItem>& footerItems) {
  UIContext* ctx = GetContext();
  if (!ctx)
    return std::string();

  // Estado por id.
  uint32_t widthKey = GenerateId("NAVW:", id.c_str());
  uint32_t baseModeKey = GenerateId("NAVM:", id.c_str());
  uint32_t paneOpenKey = GenerateId("NAVP:", id.c_str());
  uint32_t selKey = GenerateId("NAVS:", id.c_str());

  std::string& sel = selectedKey ? *selectedKey : ctx->stringStates[selKey];
  // El modo base (alternado por la hamburguesa) se inicializa una vez con `mode`.
  int& baseMode =
      ctx->intStates.try_emplace(baseModeKey, static_cast<int>(mode)).first->second;
  bool& paneOpen = ctx->boolStates.try_emplace(paneOpenKey, false).first->second;

  // Responsive (brief 19): decidir según el ancho de la VENTANA (no el de la
  // barra, que es estrecho). Small → Minimal, Medium → Compact.
  Breakpoint bp = CurrentBreakpoint(ctx->renderer.GetViewportSize().x);
  int responsiveForce = 0;
  if (bp == Breakpoint::Small)
    responsiveForce = static_cast<int>(NavDisplayMode::Minimal);
  else if (bp == Breakpoint::Medium)
    responsiveForce = static_cast<int>(NavDisplayMode::Compact);
  int effMode = std::max(baseMode, responsiveForce);
  bool minimal = (effMode == static_cast<int>(NavDisplayMode::Minimal));

  // Anchos objetivo (animados).
  float expandedW = S(280.0f);
  float compactW = S(48.0f);
  float targetW;
  if (minimal)
    targetW = paneOpen ? expandedW : compactW;
  else if (effMode == static_cast<int>(NavDisplayMode::Compact))
    targetW = compactW;
  else
    targetW = expandedW;

  float& curW = ctx->floatStates.try_emplace(widthKey, targetW).first->second;
  // Lerp exponencial hacia el ancho objetivo (degradación suave sin spring).
  float t = std::min(1.0f, ctx->deltaTime * 14.0f);
  curW += (targetW - curW) * t;
  if (std::fabs(targetW - curW) < 0.5f)
    curW = targetW;

  bool showLabels = curW > (compactW + S(60.0f));

  // Layout: ancho fijo (animado), alto = espacio disponible (fill).
  Vec2 avail = GetCurrentAvailableSpace(ctx);
  float barH = avail.y > 1.0f ? avail.y : ctx->renderer.GetViewportSize().y;
  Vec2 barPos = ctx->cursorPos;
  Vec2 barSize(curW, barH);

  // Métricas de fila.
  const TextStyle& body = ctx->style.GetTextStyle(TypographyStyle::Body);
  float fontSize = body.fontSize;
  float rowH = S(40.0f);
  float pad = S(12.0f);
  float iconSize = S(18.0f);

  // Fondo de la barra + borde derecho.
  Color bg = AdjustContainerBackground(ctx->style.panel.background,
                                       ctx->style.isDarkTheme);
  ctx->renderer.DrawRectFilled(barPos, barSize, bg, 0.0f);
  ctx->renderer.DrawLine(Vec2(barPos.x + barSize.x, barPos.y),
                         Vec2(barPos.x + barSize.x, barPos.y + barSize.y),
                         ctx->style.panel.borderColor, 1.0f);

  ctx->renderer.PushClipRect(barPos, barSize);

  std::string result = sel;

  // Hamburguesa (zona fija de ancho `compactW` a la izquierda).
  {
    float hy = barPos.y + S(8.0f);
    Vec2 hp(barPos.x, hy);
    Vec2 hs(compactW, rowH);
    bool hover = IsMouseOver(ctx, hp, hs);
    if (hover) {
      Color hv = ctx->style.panel.headerBackground;
      hv.a *= 0.5f;
      ctx->renderer.DrawRectFilled(hp, hs, hv, S(4.0f));
    }
    float cx = hp.x + compactW * 0.5f;
    float cy = hp.y + rowH * 0.5f;
    float w = S(8.0f);
    Color lc = body.color;
    for (int k = -1; k <= 1; ++k)
      ctx->renderer.DrawLine(Vec2(cx - w, cy + k * S(5.0f)),
                             Vec2(cx + w, cy + k * S(5.0f)), lc, 1.5f);
    if (hover && ctx->input.IsMousePressed(0)) {
      if (minimal)
        paneOpen = !paneOpen;
      else
        baseMode = (baseMode == static_cast<int>(NavDisplayMode::Expanded))
                       ? static_cast<int>(NavDisplayMode::Compact)
                       : static_cast<int>(NavDisplayMode::Expanded);
    }
  }

  // Helper recursivo para dibujar una lista de NavItems desde `yy`.
  std::function<void(const std::vector<NavItem>&, int, float&)> drawList;
  drawList = [&](const std::vector<NavItem>& list, int depth, float& yy) {
    for (const NavItem& it : list) {
      Vec2 rp(barPos.x, yy);
      Vec2 rs(curW, rowH);
      bool hasChildren = !it.children.empty();
      bool selected = !it.key.empty() && it.key == sel;
      bool hover = IsMouseOver(ctx, rp, rs);

      uint32_t expKey = GenerateId("NAVE:", (id + ":" + it.key).c_str());
      bool& expanded =
          ctx->boolStates.try_emplace(expKey, false).first->second;

      // Fondo de selección / hover + barra de acento.
      if (selected) {
        ctx->renderer.DrawRectFilled(rp, rs, ctx->style.panel.headerBackground,
                                     S(4.0f));
        ctx->renderer.DrawRectFilled(Vec2(rp.x + S(3.0f), rp.y + rs.y * 0.25f),
                                     Vec2(S(3.0f), rs.y * 0.5f),
                                     ctx->style.accentColor, S(2.0f));
      } else if (hover) {
        Color hv = ctx->style.panel.headerBackground;
        hv.a *= 0.5f;
        ctx->renderer.DrawRectFilled(rp, rs, hv, S(4.0f));
      }

      float indent = pad + depth * S(16.0f);
      Color txtCol = selected ? ctx->style.accentColor : body.color;

      if (it.icon)
        DrawWidgetIcon(ctx, rp, rs, it.icon, txtCol, iconSize, indent, S(8.0f));

      if (showLabels)
        ctx->renderer.DrawText(
            Vec2(rp.x + indent + iconSize + S(10.0f),
                 rp.y + (rs.y - fontSize) * 0.5f),
            it.label, txtCol, fontSize);

      float rightEdge = rp.x + curW - pad;

      // Chevron de expansión (sólo con label visible).
      if (hasChildren && showLabels) {
        float ax = rightEdge - S(6.0f);
        float ay = rp.y + rs.y * 0.5f;
        float s = S(4.0f);
        if (expanded) {
          ctx->renderer.DrawLine(Vec2(ax - s, ay - s * 0.5f), Vec2(ax, ay + s * 0.5f),
                                 txtCol, 1.5f);
          ctx->renderer.DrawLine(Vec2(ax, ay + s * 0.5f), Vec2(ax + s, ay - s * 0.5f),
                                 txtCol, 1.5f);
        } else {
          ctx->renderer.DrawLine(Vec2(ax - s * 0.5f, ay - s), Vec2(ax + s * 0.5f, ay),
                                 txtCol, 1.5f);
          ctx->renderer.DrawLine(Vec2(ax + s * 0.5f, ay), Vec2(ax - s * 0.5f, ay + s),
                                 txtCol, 1.5f);
        }
        rightEdge -= S(18.0f);
      }

      // Badge (contador). Con label: pill numérica; compacto: punto sobre el icono.
      if (it.badge > 0) {
        if (showLabels) {
          std::string bt = it.badge > 99 ? "99+" : std::to_string(it.badge);
          Vec2 ts = MeasureTextCached(ctx, bt, fontSize * 0.82f);
          float bw = ts.x + S(10.0f);
          float bh = fontSize + S(2.0f);
          Vec2 bpos(rightEdge - bw, rp.y + (rs.y - bh) * 0.5f);
          ctx->renderer.DrawRectFilled(bpos, Vec2(bw, bh), ctx->style.accentColor,
                                       bh * 0.5f);
          ctx->renderer.DrawText(
              Vec2(bpos.x + S(5.0f), bpos.y + (bh - fontSize * 0.82f) * 0.5f), bt,
              Color(1, 1, 1, 1), fontSize * 0.82f);
        } else {
          float dx = rp.x + indent + iconSize - S(1.0f);
          float dy = rp.y + rs.y * 0.5f - iconSize * 0.5f + S(1.0f);
          ctx->renderer.DrawRectFilled(Vec2(dx, dy), Vec2(S(8.0f), S(8.0f)),
                                       ctx->style.accentColor, S(4.0f));
        }
      }

      if (hover && ctx->input.IsMousePressed(0)) {
        if (hasChildren) {
          expanded = !expanded;
        } else if (!it.key.empty()) {
          sel = it.key;
          result = it.key;
        }
      }

      yy += rowH + S(2.0f);
      if (hasChildren && expanded && showLabels)
        drawList(it.children, depth + 1, yy);
    }
  };

  // Filas principales (bajo la hamburguesa).
  float y = barPos.y + S(8.0f) + rowH + S(8.0f);
  drawList(items, 0, y);

  // Footer anclado abajo.
  if (!footerItems.empty()) {
    float footerH = (float)footerItems.size() * (rowH + S(2.0f));
    float footerY = barPos.y + barH - footerH - S(8.0f);
    ctx->renderer.DrawLine(Vec2(barPos.x + pad, footerY - S(8.0f)),
                           Vec2(barPos.x + curW - pad, footerY - S(8.0f)),
                           ctx->style.panel.borderColor, 1.0f);
    drawList(footerItems, 0, footerY);
  }

  ctx->renderer.PopClipRect();

  ctx->lastItemPos = barPos;
  AdvanceCursor(ctx, barSize);
  SetLastItem(GenerateId("NAV:", id.c_str()), barPos, barPos + barSize, false,
              false, false, result != sel);
  return result;
}

// ─── 3) CommandBar ───────────────────────────────────────────────────────────

namespace {
// Dibuja un botón de comando (icono + label) y devuelve true si se hizo click.
bool DrawCommandButton(UIContext* ctx, const Vec2& pos, const Vec2& size,
                       const CommandItem& c, float fontSize, float iconSize,
                       float pad) {
  bool enabled = c.enabled;
  bool hover = enabled && IsMouseOver(ctx, pos, size);
  if (hover) {
    Color hv = ctx->style.panel.headerBackground;
    ctx->renderer.DrawRectFilled(pos, size, hv, S(4.0f));
  }
  Color col = enabled ? ctx->style.GetTextStyle(TypographyStyle::Body).color
                      : Color(0.5f, 0.5f, 0.5f, 1.0f);
  float x = pos.x + pad;
  if (c.icon) {
    DrawWidgetIcon(ctx, pos, size, c.icon, col, iconSize, pad,
                   c.label.empty() ? 0.0f : S(6.0f));
    x += iconSize + (c.label.empty() ? 0.0f : S(6.0f));
  }
  if (!c.label.empty())
    ctx->renderer.DrawText(Vec2(x, pos.y + (size.y - fontSize) * 0.5f), c.label,
                           col, fontSize);
  return hover && ctx->input.IsMousePressed(0);
}
} // namespace

void CommandBar(const std::string& id, const std::vector<CommandItem>& primary,
                const std::vector<CommandItem>& secondary) {
  UIContext* ctx = GetContext();
  if (!ctx)
    return;

  const TextStyle& body = ctx->style.GetTextStyle(TypographyStyle::Body);
  float fontSize = body.fontSize;
  float rowH = S(40.0f);
  float pad = S(10.0f);
  float gap = S(4.0f);
  float iconSize = S(16.0f);

  Vec2 avail = GetCurrentAvailableSpace(ctx);
  float availW = avail.x > 1.0f ? avail.x : ctx->renderer.GetViewportSize().x;
  Vec2 barPos = ctx->cursorPos;

  auto btnW = [&](const CommandItem& c) -> float {
    float w = pad * 2.0f;
    if (c.icon)
      w += iconSize + (c.label.empty() ? 0.0f : S(6.0f));
    if (!c.label.empty())
      w += MeasureTextCached(ctx, c.label, fontSize).x;
    return std::max(w, rowH);
  };

  float overflowW = rowH; // botón "···" cuadrado
  bool hasSecondary = !secondary.empty();

  // Reparto primario: colocar mientras quepa; el resto va a overflow. Dos pasadas
  // para reservar el ancho del botón "···" sólo cuando hace falta.
  std::vector<int> visible, overflow;
  auto place = [&](float reserve) {
    visible.clear();
    overflow.clear();
    float x = 0.0f;
    for (int i = 0; i < (int)primary.size(); ++i) {
      float w = btnW(primary[i]);
      if (x + w <= availW - reserve) {
        visible.push_back(i);
        x += w + gap;
      } else {
        overflow.push_back(i);
      }
    }
  };
  place(hasSecondary ? overflowW + gap : 0.0f);
  bool needOverflow = hasSecondary || !overflow.empty();
  if (needOverflow && !hasSecondary)
    place(overflowW + gap); // re-colocar reservando el botón overflow

  // Dibujar los visibles.
  float x = barPos.x;
  for (int idx : visible) {
    float w = btnW(primary[idx]);
    if (DrawCommandButton(ctx, Vec2(x, barPos.y), Vec2(w, rowH), primary[idx],
                          fontSize, iconSize, pad)) {
      if (primary[idx].enabled && primary[idx].onInvoke)
        primary[idx].onInvoke();
    }
    x += w + gap;
  }

  // Botón overflow + menú.
  if (needOverflow) {
    Vec2 ob(barPos.x + availW - overflowW, barPos.y);
    Vec2 os(overflowW, rowH);
    bool hover = IsMouseOver(ctx, ob, os);
    if (hover)
      ctx->renderer.DrawRectFilled(ob, os, ctx->style.panel.headerBackground,
                                   S(4.0f));
    float cx = ob.x + overflowW * 0.5f;
    float cy = ob.y + rowH * 0.5f;
    for (int k = -1; k <= 1; ++k)
      ctx->renderer.DrawCircle(Vec2(cx + k * S(5.0f), cy), S(1.6f), body.color,
                               true);
    std::string ofId = id + "_of";
    if (hover && ctx->input.IsMousePressed(0))
      OpenFlyout(ofId);

    std::vector<MenuEntry> entries;
    auto pushCmd = [&](const CommandItem& c) {
      MenuEntry e;
      e.label = c.label;
      e.icon = c.icon;
      e.enabled = c.enabled;
      e.onInvoke = c.onInvoke;
      entries.push_back(e);
    };
    for (int idx : overflow)
      pushCmd(primary[idx]);
    if (!overflow.empty() && hasSecondary) {
      MenuEntry sep;
      sep.separator = true;
      entries.push_back(sep);
    }
    for (const CommandItem& c : secondary)
      pushCmd(c);
    MenuFlyout(ofId, Rect(ob, os), entries);
  }

  ctx->lastItemPos = barPos;
  AdvanceCursor(ctx, Vec2(availW, rowH));
}

// ─── 4) BreadcrumbBar ────────────────────────────────────────────────────────

int BreadcrumbBar(const std::string& id, const std::vector<std::string>& crumbs) {
  UIContext* ctx = GetContext();
  if (!ctx || crumbs.empty())
    return -1;

  const TextStyle& body = ctx->style.GetTextStyle(TypographyStyle::Body);
  float fontSize = body.fontSize;
  float rowH = S(28.0f);
  float gap = S(6.0f);
  int n = (int)crumbs.size();

  Vec2 barPos = ctx->cursorPos;
  Vec2 avail = GetCurrentAvailableSpace(ctx);
  float availW = avail.x > 1.0f ? avail.x : ctx->renderer.GetViewportSize().x;

  Color dim(body.color.r, body.color.g, body.color.b, body.color.a * 0.6f);
  std::string chevron = ">";
  float sepW = MeasureTextCached(ctx, chevron, fontSize).x + gap * 2.0f;

  float total = 0.0f;
  for (int i = 0; i < n; ++i) {
    total += MeasureTextCached(ctx, crumbs[i], fontSize).x;
    if (i)
      total += sepW;
  }

  int result = -1;
  float x = barPos.x;

  auto drawCrumb = [&](int idx, bool clickable) {
    Vec2 ts = MeasureTextCached(ctx, crumbs[idx], fontSize);
    Vec2 hitPos(x, barPos.y);
    Vec2 hitSize(ts.x, rowH);
    bool last = (idx == n - 1);
    bool hover = clickable && IsMouseOver(ctx, hitPos, hitSize);
    Color col = last ? body.color : (hover ? ctx->style.accentColor : dim);
    ctx->renderer.DrawText(Vec2(x, barPos.y + (rowH - fontSize) * 0.5f),
                           crumbs[idx], col, fontSize);
    if (hover && ctx->input.IsMousePressed(0))
      result = idx;
    x += ts.x;
  };
  auto drawSep = [&]() {
    ctx->renderer.DrawText(Vec2(x + gap, barPos.y + (rowH - fontSize) * 0.5f),
                           chevron, dim, fontSize);
    x += sepW;
  };

  if (total <= availW || n <= 2) {
    for (int i = 0; i < n; ++i) {
      if (i)
        drawSep();
      drawCrumb(i, i != n - 1);
    }
  } else {
    // Colapsar las intermedias en "…".
    drawCrumb(0, true);
    drawSep();
    Vec2 es = MeasureTextCached(ctx, "...", fontSize);
    Vec2 ep(x, barPos.y);
    Vec2 esz(es.x + gap, rowH);
    bool ehover = IsMouseOver(ctx, ep, esz);
    ctx->renderer.DrawText(Vec2(x, barPos.y + (rowH - fontSize) * 0.5f), "...",
                           ehover ? ctx->style.accentColor : dim, fontSize);
    Rect anchor(ep, Vec2(esz.x, rowH));
    std::string fId = id + "_bc";
    if (ehover && ctx->input.IsMousePressed(0))
      OpenFlyout(fId);
    x += esz.x;
    drawSep();
    drawCrumb(n - 1, false);

    // Flyout con las migas ocultas [1 .. n-2].
    if (BeginFlyout(fId, anchor)) {
      for (int k = 1; k <= n - 2; ++k) {
        Vec2 rp = ctx->cursorPos;
        Vec2 ts = MeasureTextCached(ctx, crumbs[k], fontSize);
        Vec2 rs(std::max(S(140.0f), ts.x + S(16.0f)), rowH);
        bool hover = IsMouseOver(ctx, rp, rs);
        if (hover)
          ctx->renderer.DrawRectFilled(rp, rs, ctx->style.panel.headerBackground,
                                       S(4.0f));
        ctx->renderer.DrawText(Vec2(rp.x + S(8.0f), rp.y + (rs.y - fontSize) * 0.5f),
                               crumbs[k], body.color, fontSize);
        if (hover && ctx->input.IsMousePressed(0)) {
          result = k;
          CloseFlyout(fId);
        }
        AdvanceCursor(ctx, rs);
      }
      EndFlyout();
    }
  }

  ctx->lastItemPos = barPos;
  AdvanceCursor(ctx, Vec2(availW, rowH));
  return result;
}

// ─── 5) TitleBar custom ──────────────────────────────────────────────────────

TitleBarResult TitleBar(const std::string& id, const std::string& title,
                        uint32_t icon, std::function<void()> centerContent) {
  TitleBarResult res;
  UIContext* ctx = GetContext();
  if (!ctx)
    return res;

  const TextStyle& body = ctx->style.GetTextStyle(TypographyStyle::Body);
  float fontSize = body.fontSize;
  float barH = S(40.0f);
  Vec2 barPos = ctx->cursorPos;
  float fullW = ctx->renderer.GetViewportSize().x;
  Vec2 barSize(std::max(1.0f, fullW - barPos.x), barH);

  SDL_Window* win = ctx->window;
  bool maximized =
      win && (SDL_GetWindowFlags(win) & SDL_WINDOW_MAXIMIZED) != 0;

  // Fondo (cabecera ligeramente distinta del cuerpo).
  Color bg = AdjustContainerBackground(ctx->style.backgroundColor,
                                       ctx->style.isDarkTheme);
  ctx->renderer.DrawRectFilled(barPos, barSize, bg, 0.0f);

  // Izquierda: icono + título.
  float pad = S(12.0f);
  float lx = barPos.x + pad;
  float iconSize = S(18.0f);
  if (icon) {
    ctx->renderer.DrawIconGlyph(
        Vec2(lx, barPos.y + (barH - iconSize) * 0.5f), icon, body.color, iconSize);
    lx += iconSize + S(8.0f);
  }
  if (!title.empty())
    ctx->renderer.DrawText(Vec2(lx, barPos.y + (barH - fontSize) * 0.5f), title,
                           body.color, fontSize);

  // Caption buttons (min/max/close) a la derecha.
  float capBtnW = S(46.0f);
  float capX = barPos.x + barSize.x - capBtnW * 3.0f;
  Rect captionButtonsRect(Vec2(capX, barPos.y), Vec2(capBtnW * 3.0f, barH));

  auto capBtn = [&](float bx, int kind) -> bool {
    Vec2 bp(bx, barPos.y);
    Vec2 bs(capBtnW, barH);
    bool hover = IsMouseOver(ctx, bp, bs);
    bool clicked = hover && ctx->input.IsMousePressed(0);
    if (hover) {
      Color hv = (kind == 2) ? Color(0.86f, 0.15f, 0.18f, 1.0f)
                             : ctx->style.panel.headerBackground;
      ctx->renderer.DrawRectFilled(bp, bs, hv, 0.0f);
    }
    Color ic = (hover && kind == 2) ? Color(1, 1, 1, 1) : body.color;
    float cx = bx + capBtnW * 0.5f;
    float cy = barPos.y + barH * 0.5f;
    float s = S(5.0f);
    if (kind == 0) { // minimizar
      ctx->renderer.DrawLine(Vec2(cx - s, cy), Vec2(cx + s, cy), ic, 1.0f);
    } else if (kind == 1) { // maximizar / restaurar
      ctx->renderer.DrawRect(Vec2(cx - s, cy - s), Vec2(2.0f * s, 2.0f * s), ic,
                             0.0f);
    } else { // cerrar
      ctx->renderer.DrawLine(Vec2(cx - s, cy - s), Vec2(cx + s, cy + s), ic, 1.0f);
      ctx->renderer.DrawLine(Vec2(cx - s, cy + s), Vec2(cx + s, cy - s), ic, 1.0f);
    }
    return clicked;
  };

  if (capBtn(capX, 0)) {
    res.minimizePressed = true;
    if (win)
      SDL_MinimizeWindow(win);
  }
  if (capBtn(capX + capBtnW, 1)) {
    res.maximizePressed = true;
    if (win) {
      if (maximized)
        SDL_RestoreWindow(win);
      else
        SDL_MaximizeWindow(win);
    }
  }
  if (capBtn(capX + capBtnW * 2.0f, 2)) {
    res.closePressed = true;
    if (win) {
      SDL_Event ev;
      SDL_zero(ev);
      ev.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
      ev.window.windowID = SDL_GetWindowID(win);
      SDL_PushEvent(&ev);
    }
  }

  // Contenido central opcional (búsqueda / CommandBar). Región fija centrada y
  // recortada; se marca completa como exclusión de arrastre.
  bool hasCenter = (bool)centerContent;
  Rect centerRect;
  if (hasCenter) {
    float cW = std::min(S(360.0f), barSize.x * 0.5f);
    float cX = barPos.x + (barSize.x - cW) * 0.5f;
    centerRect = Rect(Vec2(cX, barPos.y), Vec2(cW, barH));
    ctx->renderer.PushClipRect(centerRect.pos, centerRect.size);
    Vec2 savedCursor = ctx->cursorPos;
    ctx->cursorPos = Vec2(cX, barPos.y + (barH - S(30.0f)) * 0.5f);
    centerContent();
    ctx->cursorPos = savedCursor;
    ctx->renderer.PopClipRect();
  }

  // Publicar las zonas de hit-test para el callback de SDL.
  {
    std::lock_guard<std::mutex> lk(ctx->titleBarHit.mutex);
    TitleBarHitRegions& tb = ctx->titleBarHit;
    tb.active = true;
    tb.caption = Rect(barPos, barSize);
    tb.exclusions.clear();
    tb.exclusions.push_back(captionButtonsRect);
    if (hasCenter)
      tb.exclusions.push_back(centerRect);
    // Sin margen de redimensión cuando está maximizada (no hay borde que tirar).
    tb.resizeBorder = maximized ? 0.0f : S(6.0f);
    tb.resizable = true;
  }

  ctx->lastItemPos = barPos;
  AdvanceCursor(ctx, barSize);
  return res;
}

} // namespace FluentUI
