// ─── Brief 15 — Feedback & estado ─────────────────────────────────────────────
// InfoBar, Toast + host, ProgressRing, Badge, Skeleton / SkeletonText.
// Sigue la "anatomía estándar" (ver 12_ui_general_INDEX.md): medir → layout →
// posición → id → estado → input → dibujar → avanzar cursor.
//
// Degradación por falta del brief 10 (motion): los toasts usan fade + slide
// simples conducidos por ctx->time/deltaTime en lugar del motion/FLIP del brief
// 10; el "idle keep-alive" no aplica porque FluentApp renderiza de forma continua.
#include "UI/Widgets.h"
#include "UI/WidgetHelpers.h"
#include "UI/FeedbackWidgets.h"
#include "Theme/FluentTheme.h"
#include "core/Context.h"
#include "core/Renderer.h"
#include "core/Elevation.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <string>

namespace FluentUI {

// ─── Helpers internos ─────────────────────────────────────────────────────────

static Color MixColor(const Color& a, const Color& b, float t) {
  return Color(a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t,
               a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t);
}

// Color de acento por severidad (tomado de tokens del tema).
static Color SeverityColor(InfoSeverity sev, const Style& style) {
  switch (sev) {
    case InfoSeverity::Success: return FluentColors::Success;
    case InfoSeverity::Warning: return FluentColors::Warning;
    case InfoSeverity::Error:   return FluentColors::Error;
    case InfoSeverity::Informational:
    default:                    return style.accentColor;
  }
}

// Glifo Lucide por severidad.
static uint32_t SeverityIcon(InfoSeverity sev) {
  switch (sev) {
    case InfoSeverity::Success: return Icons::CheckCircle;
    case InfoSeverity::Warning: return Icons::AlertTriangle;
    case InfoSeverity::Error:   return Icons::CircleX;
    case InfoSeverity::Informational:
    default:                    return Icons::Info;
  }
}

static const char* SeverityName(InfoSeverity sev) {
  switch (sev) {
    case InfoSeverity::Success: return "Success";
    case InfoSeverity::Warning: return "Warning";
    case InfoSeverity::Error:   return "Error";
    case InfoSeverity::Informational:
    default:                    return "Information";
  }
}

// ─── 1) InfoBar ───────────────────────────────────────────────────────────────

bool InfoBar(const std::string& id, InfoSeverity severity,
             const std::string& title, const std::string& message,
             bool closable, const std::string& actionText) {
  UIContext* ctx = GetContext();
  if (!ctx) return true;

  uint32_t wid = GenerateId("INFOBAR:", id.c_str());

  // Estado "cerrado" persistente por id → no reaparece tras dismiss.
  bool& dismissed = ctx->GetWidgetState(wid).boolVal; // brief 22 (fase 3)
  if (dismissed) return false;

  const Style& style = ctx->style;
  Color sevColor = SeverityColor(severity, style);
  const TextStyle& titleStyle = style.GetTextStyle(TypographyStyle::BodyStrong);
  const TextStyle& bodyStyle  = style.GetTextStyle(TypographyStyle::Body);

  // Métricas (escaladas a DPI).
  const float pad        = S(12.0f);
  const float iconSize   = S(18.0f);
  const float gap        = S(10.0f);
  const float accentBarW = S(4.0f);
  const float btnSize    = S(20.0f);

  // 1+2) Ancho desde las constraints del contenedor (Fill por defecto).
  LayoutConstraints c = ConsumeNextConstraints(SizeConstraint::Fill);
  Vec2 avail = GetCurrentAvailableSpace(ctx);
  float desiredW = avail.x > 1.0f ? avail.x : S(360.0f);
  Vec2 layoutW = ApplyConstraints(ctx, c, Vec2(desiredW, S(48.0f)));
  float barW = std::max(layoutW.x, S(140.0f));

  // Ancho disponible para el bloque de texto (descontando acento, paddings,
  // icono y los botones de acción/cierre a la derecha).
  float closeW = closable ? (btnSize + gap) : 0.0f;
  Vec2 actionTextSize(0.0f, 0.0f);
  float actionW = 0.0f;
  if (!actionText.empty()) {
    actionTextSize = ctx->renderer.MeasureText(actionText, bodyStyle.fontSize);
    actionW = actionTextSize.x + S(16.0f) + gap; // padding horizontal del botón
  }
  float textLeft  = accentBarW + pad + iconSize + gap;
  float textRight = pad + closeW + actionW;
  float textMaxW  = std::max(barW - textLeft - textRight, S(40.0f));

  // Medición con word-wrap (Fase 0): título y mensaje multilínea.
  Vec2 titleSize = title.empty()
      ? Vec2(0.0f, 0.0f)
      : ctx->renderer.MeasureTextWrapped(title, textMaxW, titleStyle.fontSize);
  Vec2 msgSize = message.empty()
      ? Vec2(0.0f, 0.0f)
      : ctx->renderer.MeasureTextWrapped(message, textMaxW, bodyStyle.fontSize);
  float interGap = (titleSize.y > 0.0f && msgSize.y > 0.0f) ? S(2.0f) : 0.0f;
  float textBlockH = titleSize.y + interGap + msgSize.y;
  float contentH = std::max(textBlockH, iconSize);
  float barH = contentH + pad * 2.0f;

  Vec2 barPos = ctx->cursorPos;

  if (IsRectInViewport(ctx, barPos, Vec2(barW, barH))) {
    // Fondo tintado suave por severidad.
    Color bg = MixColor(style.panel.background, sevColor,
                        style.isDarkTheme ? 0.16f : 0.12f);
    bg.a = 1.0f;
    float radius = style.panel.cornerRadius;
    ctx->renderer.DrawRectFilled(barPos, Vec2(barW, barH), bg, radius);
    // Borde sutil tintado.
    Color border = MixColor(bg, sevColor, 0.45f);
    border.a = 0.65f;
    ctx->renderer.DrawRect(barPos, Vec2(barW, barH), border, radius);
    // Acento a la izquierda (barra vertical).
    ctx->renderer.DrawRectFilled(Vec2(barPos.x + S(1.0f), barPos.y + S(3.0f)),
                                 Vec2(accentBarW, barH - S(6.0f)), sevColor,
                                 accentBarW * 0.5f);

    // Icono de severidad (centrado verticalmente respecto al bloque de texto).
    float iconY = barPos.y + pad + (contentH - iconSize) * 0.5f;
    ctx->renderer.DrawIconGlyph(Vec2(barPos.x + accentBarW + pad, iconY),
                                SeverityIcon(severity), sevColor, iconSize);

    // Texto: título en negrita + mensaje envuelto.
    float ty = barPos.y + pad + (contentH - textBlockH) * 0.5f;
    if (!title.empty()) {
      ctx->renderer.DrawTextWrapped(Vec2(barPos.x + textLeft, ty), title,
                                    titleStyle.color, textMaxW, titleStyle.fontSize);
      ty += titleSize.y + interGap;
    }
    if (!message.empty()) {
      ctx->renderer.DrawTextWrapped(Vec2(barPos.x + textLeft, ty), message,
                                    bodyStyle.color, textMaxW, bodyStyle.fontSize);
    }
  }

  // ── Input: botón de acción y botón de cierre (a la derecha) ──
  bool result = true;
  float rightX = barPos.x + barW - pad;

  // Botón de cierre (X).
  if (closable) {
    Vec2 closePos(rightX - btnSize, barPos.y + (barH - btnSize) * 0.5f);
    bool closeHover = IsMouseOver(ctx, closePos, Vec2(btnSize, btnSize));
    if (closeHover) {
      Color h = MixColor(Color(0, 0, 0, 0), titleStyle.color, 0.12f);
      ctx->renderer.DrawRectFilled(closePos, Vec2(btnSize, btnSize), h, S(4.0f));
    }
    ctx->renderer.DrawIconGlyph(
        Vec2(closePos.x + (btnSize - S(14.0f)) * 0.5f,
             closePos.y + (btnSize - S(14.0f)) * 0.5f),
        Icons::X, titleStyle.color, S(14.0f));
    if (closeHover && ctx->input.IsMousePressed(0)) {
      dismissed = true;
      result = false;
    }
    rightX = closePos.x - gap;
  }

  // Botón de acción (texto acento). Al pulsar invoca el valueChanged callback de
  // `id` si el usuario registró uno (la firma del brief no lleva callback aquí).
  if (!actionText.empty()) {
    float aw = actionTextSize.x + S(16.0f);
    Vec2 aPos(rightX - aw, barPos.y + (barH - btnSize) * 0.5f);
    bool aHover = IsMouseOver(ctx, aPos, Vec2(aw, btnSize));
    if (aHover) {
      Color h = MixColor(style.panel.background, sevColor, 0.25f);
      ctx->renderer.DrawRectFilled(aPos, Vec2(aw, btnSize), h, S(4.0f));
    }
    ctx->renderer.DrawText(Vec2(aPos.x + S(8.0f),
                                aPos.y + (btnSize - actionTextSize.y) * 0.5f),
                           actionText, sevColor, bodyStyle.fontSize);
    if (aHover && ctx->input.IsMousePressed(0)) {
      auto it = ctx->valueChangedCallbacks.find(id);
      if (it != ctx->valueChangedCallbacks.end()) it->second(id, nullptr);
    }
  }

  // Accesibilidad: nodo Group con nombre que incluye la severidad.
  ctx->widgetTree.FindOrCreate(wid, ctx->frame, [&]() {
    auto node = std::make_unique<WidgetNode>(wid);
    node->accessibleRole = WidgetNode::AccessibleRole::Group;
    node->accessibleName =
        std::string(SeverityName(severity)) + ": " + title + ". " + message;
    return node;
  });

  // Avanzar el cursor del layout y publicar el item.
  ctx->lastItemPos = barPos;
  AdvanceCursor(ctx, Vec2(barW, barH));
  SetLastItem(wid, barPos, barPos + Vec2(barW, barH), false, false, false, !result);
  return result;
}

// ─── 2) Toast + host ──────────────────────────────────────────────────────────

void ShowToast(const std::string& title, const std::string& message,
               const ToastOptions& opts) {
  UIContext* ctx = GetContext();
  if (!ctx) return;
  ToastInstance t;
  t.id = ctx->nextToastId++;
  t.title = title;
  t.message = message;
  t.severity = opts.severity;
  t.durationSec = opts.durationSec;
  t.actionText = opts.actionText;
  t.onAction = opts.onAction;
  ctx->toasts.push_back(std::move(t));
}

void RenderToasts(UIContext* ctx) {
  if (!ctx || ctx->toasts.empty()) return;

  Renderer& r = ctx->renderer;
  const float dt = ctx->deltaTime;
  const Style& style = ctx->style;

  // Animación (sin brief 10): velocidades de fade/slide por segundo.
  constexpr float ENTER_SPEED = 6.0f;
  constexpr float EXIT_SPEED  = 7.0f;
  const int MAX_VISIBLE = 4;

  Vec2 vp = r.GetViewportSize();
  const float margin   = S(16.0f);
  const float cardW    = S(320.0f);
  const float gap      = S(10.0f);
  const float pad      = S(14.0f);
  const float iconSize = S(18.0f);
  const float btn      = S(18.0f);
  const float tgap     = S(8.0f);

  const TextStyle& titleStyle = style.GetTextStyle(TypographyStyle::BodyStrong);
  const TextStyle& bodyStyle  = style.GetTextStyle(TypographyStyle::Body);

  const float mx = ctx->input.MouseX();
  const float my = ctx->input.MouseY();

  int n = static_cast<int>(ctx->toasts.size());
  int firstVisible = std::max(0, n - MAX_VISIBLE);

  RenderLayer prevLayer = r.GetLayer();
  r.SetLayer(RenderLayer::Overlay);

  // Apilar desde la esquina inferior-derecha hacia arriba; el más nuevo abajo.
  float baseY = vp.y - margin;
  for (int i = n - 1; i >= firstVisible; --i) {
    ToastInstance& t = ctx->toasts[i];

    // Animar entrada/salida.
    t.enterAnim = std::min(1.0f, t.enterAnim + dt * ENTER_SPEED);
    if (t.dismissed) t.exitAnim = std::max(0.0f, t.exitAnim - dt * EXIT_SPEED);
    float alpha = t.enterAnim * t.exitAnim;

    // Geometría: medir alto del contenido.
    float textW = cardW - pad * 2.0f - iconSize - tgap - (btn + tgap);
    Vec2 titleSize = t.title.empty()
        ? Vec2(0, 0)
        : r.MeasureTextWrapped(t.title, textW, titleStyle.fontSize);
    Vec2 msgSize = t.message.empty()
        ? Vec2(0, 0)
        : r.MeasureTextWrapped(t.message, textW, bodyStyle.fontSize);
    Vec2 actionSize(0, 0);
    if (!t.actionText.empty())
      actionSize = r.MeasureText(t.actionText, bodyStyle.fontSize);
    float gapTM = (titleSize.y > 0 && msgSize.y > 0) ? S(2.0f) : 0.0f;
    float gapMA = (actionSize.y > 0) ? S(6.0f) : 0.0f;
    float textBlockH = titleSize.y + gapTM + msgSize.y + gapMA + actionSize.y;
    float cardH = std::max(textBlockH, iconSize) + pad * 2.0f;

    float topY = baseY - cardH;
    // Slide-in desde el borde derecho (degradación de "enter" del brief 10).
    float slide = (1.0f - t.enterAnim) * S(40.0f);
    Vec2 cardPos(vp.x - margin - cardW + slide, topY);

    Color sevColor = SeverityColor(t.severity, style);

    // Hover sobre la card → pausa el temporizador.
    bool hover = mx >= cardPos.x && mx <= cardPos.x + cardW &&
                 my >= cardPos.y && my <= cardPos.y + cardH;

    if (!t.dismissed) {
      if (!hover) t.age += dt;
      if (t.age >= t.durationSec) t.dismissed = true;
    }

    // ── Dibujar la card (z = Flyout) ──
    float radius = style.panel.cornerRadius;
    r.DrawElevationShadow(cardPos, Vec2(cardW, cardH), radius,
                          Elevation::Z::Flyout, alpha);
    Color bg = MixColor(style.panel.background, sevColor,
                        style.isDarkTheme ? 0.08f : 0.06f);
    bg.a = alpha;
    r.DrawRectFilled(cardPos, Vec2(cardW, cardH), bg, radius);
    // Acento a la izquierda.
    Color accent = sevColor; accent.a = alpha;
    r.DrawRectFilled(Vec2(cardPos.x + S(1.0f), cardPos.y + S(3.0f)),
                     Vec2(S(4.0f), cardH - S(6.0f)), accent, S(2.0f));

    float contentTop = cardPos.y + pad;
    Color iconCol = sevColor; iconCol.a = alpha;
    r.DrawIconGlyph(Vec2(cardPos.x + pad, contentTop), SeverityIcon(t.severity),
                    iconCol, iconSize);

    float tx = cardPos.x + pad + iconSize + tgap;
    float ty = contentTop;
    if (!t.title.empty()) {
      Color tc = titleStyle.color; tc.a *= alpha;
      r.DrawTextWrapped(Vec2(tx, ty), t.title, tc, textW, titleStyle.fontSize);
      ty += titleSize.y + gapTM;
    }
    if (!t.message.empty()) {
      Color mc = bodyStyle.color; mc.a *= alpha;
      r.DrawTextWrapped(Vec2(tx, ty), t.message, mc, textW, bodyStyle.fontSize);
      ty += msgSize.y + gapMA;
    }
    // Botón de acción.
    if (!t.actionText.empty()) {
      Color ac = sevColor; ac.a = alpha;
      r.DrawText(Vec2(tx, ty), t.actionText, ac, bodyStyle.fontSize);
      bool aHover = mx >= tx && mx <= tx + actionSize.x &&
                    my >= ty && my <= ty + actionSize.y;
      if (aHover && ctx->input.IsMousePressed(0)) {
        if (t.onAction) t.onAction();
        t.dismissed = true;
      }
    }

    // Botón de cierre (X) arriba-derecha.
    Vec2 closePos(cardPos.x + cardW - pad - btn, cardPos.y + pad - S(2.0f));
    bool cHover = mx >= closePos.x && mx <= closePos.x + btn &&
                  my >= closePos.y && my <= closePos.y + btn;
    Color xCol = titleStyle.color; xCol.a *= alpha * (cHover ? 1.0f : 0.7f);
    r.DrawIconGlyph(Vec2(closePos.x + (btn - S(13.0f)) * 0.5f,
                         closePos.y + (btn - S(13.0f)) * 0.5f),
                    Icons::X, xCol, S(13.0f));
    if (cHover && ctx->input.IsMousePressed(0)) t.dismissed = true;

    baseY = topY - gap;
  }

  r.SetLayer(prevLayer);

  // Retirar toasts que ya completaron su salida.
  ctx->toasts.erase(
      std::remove_if(ctx->toasts.begin(), ctx->toasts.end(),
                     [](const ToastInstance& t) {
                       return t.dismissed && t.exitAnim <= 0.0f;
                     }),
      ctx->toasts.end());
}

// ─── 3) ProgressRing ──────────────────────────────────────────────────────────

void ProgressRing(const std::string& id, float size, float progress) {
  UIContext* ctx = GetContext();
  if (!ctx) return;

  uint32_t wid = GenerateId("RING:", id.c_str());

  float ringSize = S(size);
  LayoutConstraints c = ConsumeNextConstraints(SizeConstraint::Auto);
  Vec2 layoutSize = ApplyConstraints(ctx, c, Vec2(ringSize, ringSize));
  Vec2 pos = ctx->cursorPos;

  float diam = std::min(layoutSize.x, layoutSize.y);
  if (diam < 1.0f) diam = ringSize;
  Vec2 center(pos.x + diam * 0.5f, pos.y + diam * 0.5f);
  float thickness = std::max(S(2.0f), diam * 0.12f);
  float radius = diam * 0.5f - thickness * 0.5f;
  if (radius < 1.0f) radius = 1.0f;

  const Style& style = ctx->style;
  Color accent = style.accentColor;
  // Pista tenue (anillo de fondo).
  Color track = MixColor(style.panel.background, accent, 0.18f);
  track.a = 0.45f;

  constexpr float TWO_PI = 6.28318530718f;

  if (IsRectInViewport(ctx, pos, Vec2(diam, diam))) {
    // Anillo de pista completo.
    ctx->renderer.PathClear();
    ctx->renderer.PathArcTo(center, radius, 0.0f, TWO_PI, 48);
    ctx->renderer.PathStroke(track, false, thickness);

    if (progress >= 0.0f) {
      // Determinado: arco de 0 a progress*360°, empezando arriba (-90°).
      float p = std::clamp(progress, 0.0f, 1.0f);
      float a0 = -1.57079632679f;
      float a1 = a0 + p * TWO_PI;
      if (p > 0.0001f) {
        ctx->renderer.PathClear();
        ctx->renderer.PathArcTo(center, radius, a0, a1, 64);
        ctx->renderer.PathStroke(accent, false, thickness);
      }
    } else {
      // Indeterminado: arco parcial que gira; el barrido "respira" para un look
      // Fluent (acelera/desacelera) — aproximación sin el motion del brief 10.
      float tm = ctx->time;
      float rot = tm * 3.2f; // rad/s
      float sweep = (0.55f + 0.30f * std::sin(tm * 2.2f)) * TWO_PI; // ~90°..270°
      ctx->renderer.PathClear();
      ctx->renderer.PathArcTo(center, radius, rot, rot + sweep, 64);
      ctx->renderer.PathStroke(accent, false, thickness);
    }
  }

  // Accesibilidad: rol ProgressBar con valor (porcentaje o "indeterminate").
  ctx->widgetTree.FindOrCreate(wid, ctx->frame, [&]() {
    auto node = std::make_unique<WidgetNode>(wid);
    node->accessibleRole = WidgetNode::AccessibleRole::ProgressBar;
    node->accessibleName = "Progress ring";
    if (progress >= 0.0f)
      node->accessibleValue =
          std::to_string(static_cast<int>(std::clamp(progress, 0.0f, 1.0f) * 100.0f)) + "%";
    else
      node->accessibleValue = "indeterminate";
    return node;
  });

  ctx->lastItemPos = pos;
  AdvanceCursor(ctx, Vec2(diam, diam));
  SetLastItem(wid, pos, pos + Vec2(diam, diam), false, false, false, false);
}

// ─── 4) Badge ─────────────────────────────────────────────────────────────────

bool Badge(int count, bool dot, std::optional<Vec2> anchorTopRight) {
  UIContext* ctx = GetContext();
  if (!ctx) return false;

  const Style& style = ctx->style;
  Color accent = style.accentColor;
  const TextStyle& cap = style.GetTextStyle(TypographyStyle::Caption);

  // Esquina superior-derecha del item previo (o pos explícita).
  Vec2 corner = anchorTopRight.has_value()
      ? anchorTopRight.value()
      : Vec2(ctx->lastItemPos.x + ctx->lastItemSize.x, ctx->lastItemPos.y);

  // Enhancement (bug #3): el badge es clickable. Hit-test sin estado (posición del
  // ratón + flanco de click); devuelve true si se pulsa. No llama a SetLastItem para
  // no perturbar el anclaje sobre el item previo. Los call sites que lo ignoran siguen
  // compilando (void->bool retrocompatible).
  auto badgeClick = [&](Vec2 bp, Vec2 size) -> bool {
    if (!IsMouseOver(ctx, bp, size)) return false;
    ctx->desiredCursor = UIContext::CursorType::Hand;
    return ctx->input.IsMousePressed(0);
  };

  // Punto (sin número).
  if (dot && count <= 0) {
    float rad = S(4.0f);
    ctx->renderer.DrawCircle(Vec2(corner.x, corner.y), rad, accent, true);
    return badgeClick(Vec2(corner.x - rad, corner.y - rad), Vec2(rad * 2.0f, rad * 2.0f));
  }
  if (count <= 0 && !dot) return false; // nada que mostrar

  std::string txt = count > 99 ? "99+" : std::to_string(count);
  float fs = cap.fontSize * 0.92f;
  Vec2 ts = ctx->renderer.MeasureText(txt, fs);
  float h = std::max(ts.y, S(12.0f)) + S(4.0f);
  float w = std::max(ts.x + S(8.0f), h); // pastilla; mínimo = círculo

  // Centrar la píldora sobre la esquina superior-derecha.
  Vec2 bp(corner.x - w * 0.5f, corner.y - h * 0.5f);
  ctx->renderer.DrawRectFilled(bp, Vec2(w, h), accent, h * 0.5f);
  Color fg(1.0f, 1.0f, 1.0f, 1.0f);
  ctx->renderer.DrawText(Vec2(bp.x + (w - ts.x) * 0.5f, bp.y + (h - ts.y) * 0.5f),
                         txt, fg, fs);
  return badgeClick(bp, Vec2(w, h));
}

// ─── 5) Skeleton / shimmer ────────────────────────────────────────────────────

static void DrawSkeletonBlock(UIContext* ctx, const Vec2& pos, const Vec2& size,
                              float cornerRadius) {
  if (!IsRectInViewport(ctx, pos, size)) return;
  bool dark = ctx->style.isDarkTheme;
  Color base = dark ? Color(0.27f, 0.27f, 0.27f, 1.0f)
                    : Color(0.86f, 0.86f, 0.86f, 1.0f);
  ctx->renderer.DrawRectFilled(pos, size, base, cornerRadius);

  // Shimmer: una banda clara que se desplaza horizontalmente en bucle. Se compone
  // de dos quads con DrawRectGradient (transparente→brillo→transparente) cuyo
  // centro se mueve con ctx->time. Recortado al bloque para que no se desborde.
  const float period = 1.6f;
  float ph = std::fmod(ctx->time, period) / period; // 0..1
  float hx = ph * 1.5f - 0.25f;                      // -0.25..1.25 (entra/sale)
  float cx = pos.x + hx * size.x;
  float bandW = size.x * 0.4f;
  float left = cx - bandW * 0.5f;
  float right = cx + bandW * 0.5f;

  Color clear(1.0f, 1.0f, 1.0f, 0.0f);
  Color bright(1.0f, 1.0f, 1.0f, dark ? 0.16f : 0.5f);

  ctx->renderer.PushClipRect(pos, size);
  // Mitad izquierda: transparente → brillo.
  ctx->renderer.DrawRectGradient(Vec2(left, pos.y), Vec2(cx - left, size.y),
                                 clear, bright, clear, bright);
  // Mitad derecha: brillo → transparente.
  ctx->renderer.DrawRectGradient(Vec2(cx, pos.y), Vec2(right - cx, size.y),
                                 bright, clear, bright, clear);
  ctx->renderer.PopClipRect();
}

void Skeleton(const Vec2& size, float cornerRadius) {
  UIContext* ctx = GetContext();
  if (!ctx) return;

  Vec2 desired(size.x > 0 ? size.x : S(120.0f), size.y > 0 ? size.y : S(16.0f));
  LayoutConstraints c = ConsumeNextConstraints(SizeConstraint::Auto);
  Vec2 layoutSize = ApplyConstraints(ctx, c, desired);
  Vec2 pos = ctx->cursorPos;

  DrawSkeletonBlock(ctx, pos, layoutSize, S(cornerRadius));

  ctx->lastItemPos = pos;
  AdvanceCursor(ctx, layoutSize);
  ctx->lastItemSize = layoutSize;
}

void SkeletonText(int lines, float lineHeight, float lastLineFraction) {
  UIContext* ctx = GetContext();
  if (!ctx) return;
  if (lines < 1) lines = 1;
  lastLineFraction = std::clamp(lastLineFraction, 0.1f, 1.0f);

  float lh = S(lineHeight);
  float barH = lh * 0.62f;              // alto del bloque por línea
  float lineGap = lh - barH;            // espacio entre líneas

  LayoutConstraints c = ConsumeNextConstraints(SizeConstraint::Fill);
  Vec2 avail = GetCurrentAvailableSpace(ctx);
  float fullW = avail.x > 1.0f ? avail.x : S(240.0f);
  Vec2 layoutSize = ApplyConstraints(ctx, c, Vec2(fullW, lh * lines));
  float w = std::max(layoutSize.x, S(40.0f));

  Vec2 pos = ctx->cursorPos;
  float radius = S(4.0f);
  for (int i = 0; i < lines; ++i) {
    float lineW = (i == lines - 1) ? w * lastLineFraction : w;
    DrawSkeletonBlock(ctx, Vec2(pos.x, pos.y + i * lh), Vec2(lineW, barH), radius);
  }

  float totalH = lh * lines - lineGap;
  ctx->lastItemPos = pos;
  AdvanceCursor(ctx, Vec2(w, totalH));
  ctx->lastItemSize = Vec2(w, totalH);
}

} // namespace FluentUI
