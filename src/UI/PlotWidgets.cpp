#include "UI/Widgets.h"
#include "UI/WidgetHelpers.h"
#include "core/Context.h"
#include "core/Renderer.h"
#include "core/Elevation.h"
#include "Theme/FluentTheme.h"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <string>

namespace FluentUI {

namespace {

struct PlotMetrics {
    float vMin;
    float vMax;
    int hoveredIdx;     // -1 if not hovering
    Vec2 plotPos;
    Vec2 plotSize;
};

PlotMetrics ComputePlotMetrics(UIContext* ctx, const Vec2& widgetPos, const Vec2& widgetSize,
                               const float* values, int count, int offset,
                               float minScale, float maxScale) {
    PlotMetrics m;
    m.plotPos = widgetPos;
    m.plotSize = widgetSize;

    // Auto-compute scale if not provided
    if (minScale == FLT_MAX || maxScale == FLT_MAX) {
        float lo = FLT_MAX, hi = -FLT_MAX;
        for (int i = 0; i < count; ++i) {
            float v = values[(i + offset) % count];
            if (v < lo) lo = v;
            if (v > hi) hi = v;
        }
        if (lo == FLT_MAX) { lo = 0.0f; hi = 1.0f; }
        if (minScale == FLT_MAX) minScale = lo;
        if (maxScale == FLT_MAX) maxScale = hi;
    }
    if (maxScale - minScale < 1e-6f) maxScale = minScale + 1.0f;

    m.vMin = minScale;
    m.vMax = maxScale;

    // Hover detection
    Vec2 mp(ctx->input.MouseX(), ctx->input.MouseY());
    bool hover = PointInRect(mp, widgetPos, widgetSize);
    if (hover && count > 0) {
        float t = (mp.x - widgetPos.x) / std::max(1.0f, widgetSize.x);
        int idx = static_cast<int>(t * count);
        m.hoveredIdx = std::clamp(idx, 0, count - 1);
    } else {
        m.hoveredIdx = -1;
    }
    return m;
}

void DrawPlotFrame(UIContext* ctx, const Vec2& pos, const Vec2& size) {
    const PanelStyle& panel = ctx->style.panel;
    Color bg = panel.background;
    bg.a = 0.5f;
    ctx->renderer.DrawRectFilled(pos, size, bg, 2.0f);
    Color border = panel.borderColor;
    border.a = 0.6f;
    ctx->renderer.DrawRect(pos, size, border, 2.0f);
}

} // namespace

void PlotLines(const std::string& label, const float* values, int count,
               int offset, const std::string& overlay,
               float minScale, float maxScale, const Vec2& size) {
    UIContext* ctx = GetContext();
    if (!ctx || !values || count <= 0) return;

    const TextStyle& labelStyle = ctx->style.GetTextStyle(TypographyStyle::Caption);
    Vec2 labelSize = label.empty() ? Vec2(0, 0)
                                   : MeasureTextCached(ctx, label, labelStyle.fontSize);

    Vec2 desired = size;
    if (desired.x <= 0.0f) desired.x = 200.0f;
    if (desired.y <= 0.0f) desired.y = 60.0f;
    Vec2 totalSize(desired.x + (label.empty() ? 0.0f : labelSize.x + 8.0f), desired.y);

    LayoutConstraints constraints = ConsumeNextConstraints(SizeConstraint::Fill);
    Vec2 finalSize = ApplyConstraints(ctx, constraints, totalSize);
    Vec2 widgetPos = ctx->cursorPos;

    Vec2 plotPos = widgetPos;
    Vec2 plotSize(finalSize.x - (label.empty() ? 0.0f : labelSize.x + 8.0f), finalSize.y);

    DrawPlotFrame(ctx, plotPos, plotSize);

    PlotMetrics m = ComputePlotMetrics(ctx, plotPos, plotSize, values, count, offset,
                                       minScale, maxScale);
    float range = m.vMax - m.vMin;

    // Build path
    ctx->renderer.PathClear();
    for (int i = 0; i < count; ++i) {
        float v = values[(i + offset) % count];
        float t = (count > 1) ? static_cast<float>(i) / static_cast<float>(count - 1) : 0.5f;
        float x = plotPos.x + t * plotSize.x;
        float ny = (v - m.vMin) / range;
        float y = plotPos.y + plotSize.y - ny * plotSize.y;
        ctx->renderer.PathLineTo(Vec2(x, y));
    }
    Color line = ctx->style.accentColor;
    ctx->renderer.PathStroke(line, false, 1.5f);

    // Hover marker + tooltip
    if (m.hoveredIdx >= 0) {
        float v = values[(m.hoveredIdx + offset) % count];
        float t = (count > 1) ? static_cast<float>(m.hoveredIdx) / static_cast<float>(count - 1) : 0.5f;
        float x = plotPos.x + t * plotSize.x;
        float ny = (v - m.vMin) / range;
        float y = plotPos.y + plotSize.y - ny * plotSize.y;
        ctx->renderer.DrawCircle(Vec2(x, y), 3.0f, line, true);
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%d: %.3f", m.hoveredIdx, v);
        Vec2 tipSize = MeasureTextCached(ctx, buf, labelStyle.fontSize);
        Vec2 tipPos(x + 6.0f, y - tipSize.y - 4.0f);
        Color tipBg = ctx->style.panel.background;
        tipBg.a = 0.9f;
        ctx->renderer.DrawElevationShadow(tipPos - Vec2(4, 2),
                                          tipSize + Vec2(8, 4), 2.0f,
                                          Elevation::Z::Flyout);
        ctx->renderer.DrawRectFilled(tipPos - Vec2(4, 2),
                                     tipSize + Vec2(8, 4), tipBg, 2.0f);
        ctx->renderer.DrawText(tipPos, buf, labelStyle.color, labelStyle.fontSize);
    }

    // Overlay
    if (!overlay.empty()) {
        Vec2 ovSize = MeasureTextCached(ctx, overlay, labelStyle.fontSize);
        Vec2 ovPos(plotPos.x + (plotSize.x - ovSize.x) * 0.5f,
                   plotPos.y + 2.0f);
        ctx->renderer.DrawText(ovPos, overlay, labelStyle.color, labelStyle.fontSize);
    }

    // Side label
    if (!label.empty()) {
        Vec2 lp(plotPos.x + plotSize.x + 8.0f,
                plotPos.y + (plotSize.y - labelSize.y) * 0.5f);
        ctx->renderer.DrawText(lp, label, labelStyle.color, labelStyle.fontSize);
    }

    ctx->lastItemPos = widgetPos;
    AdvanceCursor(ctx, finalSize);
    SetLastItem(GenerateId("PLOT_L:", label.c_str()),
                widgetPos, widgetPos + finalSize,
                m.hoveredIdx >= 0, false, false, false);
}

void PlotHistogram(const std::string& label, const float* values, int count,
                   int offset, const std::string& overlay,
                   float minScale, float maxScale, const Vec2& size) {
    UIContext* ctx = GetContext();
    if (!ctx || !values || count <= 0) return;

    const TextStyle& labelStyle = ctx->style.GetTextStyle(TypographyStyle::Caption);
    Vec2 labelSize = label.empty() ? Vec2(0, 0)
                                   : MeasureTextCached(ctx, label, labelStyle.fontSize);

    Vec2 desired = size;
    if (desired.x <= 0.0f) desired.x = 200.0f;
    if (desired.y <= 0.0f) desired.y = 60.0f;
    Vec2 totalSize(desired.x + (label.empty() ? 0.0f : labelSize.x + 8.0f), desired.y);

    LayoutConstraints constraints = ConsumeNextConstraints(SizeConstraint::Fill);
    Vec2 finalSize = ApplyConstraints(ctx, constraints, totalSize);
    Vec2 widgetPos = ctx->cursorPos;

    Vec2 plotPos = widgetPos;
    Vec2 plotSize(finalSize.x - (label.empty() ? 0.0f : labelSize.x + 8.0f), finalSize.y);

    DrawPlotFrame(ctx, plotPos, plotSize);

    PlotMetrics m = ComputePlotMetrics(ctx, plotPos, plotSize, values, count, offset,
                                       minScale, maxScale);
    float range = m.vMax - m.vMin;
    float barW = plotSize.x / static_cast<float>(count);
    float gap = std::min(2.0f, barW * 0.15f);
    Color bar = ctx->style.accentColor;
    Color barHover(std::min(1.0f, bar.r * 1.2f),
                   std::min(1.0f, bar.g * 1.2f),
                   std::min(1.0f, bar.b * 1.2f), bar.a);

    for (int i = 0; i < count; ++i) {
        float v = values[(i + offset) % count];
        float ny = (v - m.vMin) / range;
        float h = ny * plotSize.y;
        Vec2 bp(plotPos.x + i * barW + gap * 0.5f,
                plotPos.y + plotSize.y - h);
        Vec2 bs(barW - gap, h);
        ctx->renderer.DrawRectFilled(bp, bs, (i == m.hoveredIdx) ? barHover : bar, 1.0f);
    }

    if (m.hoveredIdx >= 0) {
        float v = values[(m.hoveredIdx + offset) % count];
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%d: %.3f", m.hoveredIdx, v);
        Vec2 tipSize = MeasureTextCached(ctx, buf, labelStyle.fontSize);
        Vec2 tipPos(plotPos.x + m.hoveredIdx * barW,
                    plotPos.y - tipSize.y - 6.0f);
        Color tipBg = ctx->style.panel.background;
        tipBg.a = 0.9f;
        ctx->renderer.DrawElevationShadow(tipPos - Vec2(4, 2),
                                          tipSize + Vec2(8, 4), 2.0f,
                                          Elevation::Z::Flyout);
        ctx->renderer.DrawRectFilled(tipPos - Vec2(4, 2),
                                     tipSize + Vec2(8, 4), tipBg, 2.0f);
        ctx->renderer.DrawText(tipPos, buf, labelStyle.color, labelStyle.fontSize);
    }

    if (!overlay.empty()) {
        Vec2 ovSize = MeasureTextCached(ctx, overlay, labelStyle.fontSize);
        Vec2 ovPos(plotPos.x + (plotSize.x - ovSize.x) * 0.5f, plotPos.y + 2.0f);
        ctx->renderer.DrawText(ovPos, overlay, labelStyle.color, labelStyle.fontSize);
    }

    if (!label.empty()) {
        Vec2 lp(plotPos.x + plotSize.x + 8.0f,
                plotPos.y + (plotSize.y - labelSize.y) * 0.5f);
        ctx->renderer.DrawText(lp, label, labelStyle.color, labelStyle.fontSize);
    }

    ctx->lastItemPos = widgetPos;
    AdvanceCursor(ctx, finalSize);
    SetLastItem(GenerateId("PLOT_H:", label.c_str()),
                widgetPos, widgetPos + finalSize,
                m.hoveredIdx >= 0, false, false, false);
}

void Sparkline(const float* values, int count, const Vec2& size) {
    UIContext* ctx = GetContext();
    if (!ctx || !values || count <= 0) return;

    Vec2 desired = size;
    if (desired.x <= 0.0f) desired.x = 80.0f;
    if (desired.y <= 0.0f) desired.y = 16.0f;

    Vec2 widgetPos = ctx->cursorPos;

    float lo = FLT_MAX, hi = -FLT_MAX;
    for (int i = 0; i < count; ++i) {
        if (values[i] < lo) lo = values[i];
        if (values[i] > hi) hi = values[i];
    }
    float range = hi - lo;
    if (range < 1e-6f) range = 1.0f;

    ctx->renderer.PathClear();
    for (int i = 0; i < count; ++i) {
        float t = (count > 1) ? static_cast<float>(i) / static_cast<float>(count - 1) : 0.5f;
        float x = widgetPos.x + t * desired.x;
        float ny = (values[i] - lo) / range;
        float y = widgetPos.y + desired.y - ny * desired.y;
        ctx->renderer.PathLineTo(Vec2(x, y));
    }
    ctx->renderer.PathStroke(ctx->style.accentColor, false, 1.0f);

    ctx->lastItemPos = widgetPos;
    AdvanceCursor(ctx, desired);
}

} // namespace FluentUI
