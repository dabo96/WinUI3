#pragma once
// ─── Brief 15 — Feedback & estado ─────────────────────────────────────────────
// InfoBar (mensaje inline con severidad/dismiss), Toast + host (notificaciones
// transitorias en una esquina), ProgressRing (spinner circular determinado/
// indeterminado), Badge (contador/punto anclado al item previo) y Skeleton /
// SkeletonText (placeholders con shimmer). Implementación en
// src/UI/FeedbackWidgets.cpp. Incluido desde include/UI/Widgets.h.
//
// Nota de degradación: el brief 10 (motion: enter/exit + idle keep-alive) NO
// existe todavía. Los toasts degradan a fade + slide simples conducidos por
// ctx->time/deltaTime; las animaciones funcionan porque el loop de FluentApp
// renderiza de forma continua (no on-demand), así que no hace falta marcar ids
// como "animados".
#include "Math/Vec2.h"
#include "Math/Color.h"
#include <string>
#include <optional>
#include <functional>

namespace FluentUI {

struct UIContext;

// Severidad compartida por InfoBar y Toast. Declarada como enum opaco en
// core/Context.h (ToastInstance la usa); aquí se completa con los enumeradores.
enum class InfoSeverity : int { Informational, Success, Warning, Error };

// ─── 1) InfoBar (mensaje inline) ──────────────────────────────────────────────
// Barra de estado inline con icono de severidad, título en negrita, mensaje
// multilínea (word-wrap), acción opcional y botón de cierre. El estado "cerrado"
// persiste por `id` (ctx->boolStates) para no reaparecer. Devuelve false cuando
// el usuario lo cierra (el caller debe dejar de mostrarlo). La acción opcional,
// si se pulsa, invoca el valueChanged callback registrado para `id` (si lo hay).
// Rol de accesibilidad: Group, con accessibleName que incluye la severidad.
bool InfoBar(const std::string& id, InfoSeverity severity,
             const std::string& title, const std::string& message,
             bool closable = true, const std::string& actionText = "");

// ─── 2) Toast / Notification host ─────────────────────────────────────────────
struct ToastOptions {
    InfoSeverity severity = InfoSeverity::Informational;
    float durationSec = 4.0f;
    std::string actionText;
    std::function<void()> onAction;
};

// Encola un toast (puede llamarse desde cualquier punto del frame).
void ShowToast(const std::string& title, const std::string& message,
               const ToastOptions& opts = {});

// Renderiza y actualiza la cola de toasts. Llamar UNA vez por frame, al final del
// frame (capa Overlay). Apila los toasts en la esquina inferior-derecha, máx. 4
// visibles + cola; auto-descarte por `durationSec` (pausado mientras el cursor
// está encima); entrada (slide+fade) y salida (fade) degradadas sin el brief 10.
void RenderToasts(UIContext* ctx);

// ─── 3) ProgressRing (spinner circular) ───────────────────────────────────────
// progress en [0,1] = determinado (arco proporcional). progress < 0 =
// indeterminado (arco que gira). Usa el Path API (PathArcTo + PathStroke).
void ProgressRing(const std::string& id, float size = 32.0f, float progress = -1.0f);

// ─── 4) Badge (contador / punto) ──────────────────────────────────────────────
// Píldora de acento con el número ("99+" si excede) o, con dot=true y count<=0,
// solo un punto. Por defecto se ancla a la esquina superior-derecha del último
// item dibujado (ctx->lastItemPos + lastItemSize); o en `anchorTopRight` si se da.
void Badge(int count, bool dot = false, std::optional<Vec2> anchorTopRight = {});

// ─── 5) Skeleton / shimmer (placeholder de carga) ─────────────────────────────
// Bloque redondeado neutro con un brillo (shimmer) que se desplaza en bucle.
void Skeleton(const Vec2& size, float cornerRadius = 4.0f);
// N líneas de skeleton (la última más corta) para simular un párrafo cargando.
void SkeletonText(int lines, float lineHeight = 16.0f, float lastLineFraction = 0.6f);

} // namespace FluentUI
