#include "UI/Widgets.h"
#include "UI/WidgetHelpers.h"
#include "core/Context.h"
#include "core/Renderer.h"
#include "Theme/FluentTheme.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <string>

namespace FluentUI {

namespace {

// Zeller-style: returns weekday for given Y/M/D (0 = Sunday).
int DayOfWeek(int year, int month, int day) {
    if (month < 3) { month += 12; year -= 1; }
    int K = year % 100;
    int J = year / 100;
    int h = (day + (13 * (month + 1)) / 5 + K + K / 4 + J / 4 + 5 * J) % 7;
    return (h + 6) % 7; // shift so 0 = Sunday
}

bool IsLeapYear(int y) {
    return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}

int DaysInMonth(int year, int month) {
    static const int dim[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 1 || month > 12) return 30;
    if (month == 2 && IsLeapYear(year)) return 29;
    return dim[month - 1];
}

const char* MonthName(int m) {
    static const char* names[] = {"Jan","Feb","Mar","Apr","May","Jun",
                                  "Jul","Aug","Sep","Oct","Nov","Dec"};
    return (m >= 1 && m <= 12) ? names[m - 1] : "?";
}

DateTimeValue Today() {
    DateTimeValue v;
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm lt{};
#if defined(_WIN32)
    localtime_s(&lt, &t);
#else
    localtime_r(&t, &lt);
#endif
    v.year = lt.tm_year + 1900;
    v.month = lt.tm_mon + 1;
    v.day = lt.tm_mday;
    v.hour = lt.tm_hour;
    v.minute = lt.tm_min;
    v.second = lt.tm_sec;
    return v;
}

bool SmallButton(UIContext* ctx, const std::string& text, const Vec2& pos, const Vec2& size, uint32_t id) {
    Vec2 mp(ctx->input.MouseX(), ctx->input.MouseY());
    bool hover = PointInRect(mp, pos, size);
    bool clicked = hover && ctx->input.IsMousePressed(0);
    Color bg = ctx->style.button.background.normal;
    if (hover) bg = ctx->style.button.background.hover;
    ctx->renderer.DrawRectFilled(pos, size, bg, 4.0f);
    const TextStyle& ts = ctx->style.GetTextStyle(TypographyStyle::Caption);
    Vec2 tsz = MeasureTextCached(ctx, text, ts.fontSize);
    Vec2 tpos(pos.x + (size.x - tsz.x) * 0.5f, pos.y + (size.y - tsz.y) * 0.5f);
    ctx->renderer.DrawText(tpos, text, ts.color, ts.fontSize);
    (void)id;
    return clicked;
}

} // namespace

bool DatePicker(const std::string& label, DateTimeValue* value, std::optional<Vec2> pos) {
    UIContext* ctx = GetContext();
    if (!ctx || !value) return false;

    const TextStyle& titleStyle = ctx->style.GetTextStyle(TypographyStyle::Subtitle);
    const TextStyle& cellStyle = ctx->style.GetTextStyle(TypographyStyle::Caption);
    Color accent = ctx->style.accentColor;

    // Layout: 240×260 box.
    const float W = 240.0f;
    const float HEADER_H = 28.0f;
    const float DOW_H = 18.0f;
    const float CELL_W = W / 7.0f;
    const float CELL_H = 26.0f;
    const float ROWS = 6.0f;
    const float TOTAL_H = HEADER_H + DOW_H + ROWS * CELL_H + 6.0f;

    Vec2 totalSize(W, TOTAL_H);
    LayoutConstraints constraints = ConsumeNextConstraints();
    Vec2 finalSize = ApplyConstraints(ctx, constraints, totalSize);
    Vec2 widgetPos = pos.has_value()
        ? ResolveAbsolutePosition(ctx, pos.value(), finalSize)
        : ctx->cursorPos;

    uint32_t id = GenerateId("DATE:", label.c_str());
    bool changed = false;

    // Background panel
    ctx->renderer.DrawRectFilled(widgetPos, finalSize, ctx->style.panel.background, 6.0f);
    ctx->renderer.DrawRect(widgetPos, finalSize, ctx->style.panel.borderColor, 6.0f);

    // Header: < [Month Year] >
    Vec2 hpos = widgetPos;
    Vec2 prevSz(28.0f, HEADER_H);
    Vec2 nextSz(28.0f, HEADER_H);
    if (SmallButton(ctx, "<", hpos, prevSz, id ^ 0x1)) {
        value->month -= 1;
        if (value->month < 1) { value->month = 12; value->year -= 1; }
        changed = true;
    }
    if (SmallButton(ctx, ">", Vec2(widgetPos.x + W - nextSz.x, hpos.y), nextSz, id ^ 0x2)) {
        value->month += 1;
        if (value->month > 12) { value->month = 1; value->year += 1; }
        changed = true;
    }
    char headerBuf[32];
    std::snprintf(headerBuf, sizeof(headerBuf), "%s %d", MonthName(value->month), value->year);
    Vec2 hSize = MeasureTextCached(ctx, headerBuf, titleStyle.fontSize);
    ctx->renderer.DrawText(Vec2(widgetPos.x + (W - hSize.x) * 0.5f,
                                widgetPos.y + (HEADER_H - hSize.y) * 0.5f),
                           headerBuf, titleStyle.color, titleStyle.fontSize);

    // Day-of-week row
    static const char* dow[] = {"S","M","T","W","T","F","S"};
    float dowY = widgetPos.y + HEADER_H;
    for (int i = 0; i < 7; ++i) {
        Vec2 sz = MeasureTextCached(ctx, dow[i], cellStyle.fontSize);
        Vec2 cp(widgetPos.x + i * CELL_W + (CELL_W - sz.x) * 0.5f,
                dowY + (DOW_H - sz.y) * 0.5f);
        Color c = cellStyle.color; c.a = 0.7f;
        ctx->renderer.DrawText(cp, dow[i], c, cellStyle.fontSize);
    }

    // Day grid
    int firstWeekday = DayOfWeek(value->year, value->month, 1);
    int daysInM = DaysInMonth(value->year, value->month);
    DateTimeValue today = Today();
    float gridY = dowY + DOW_H;
    for (int d = 1; d <= daysInM; ++d) {
        int slot = firstWeekday + d - 1;
        int row = slot / 7;
        int col = slot % 7;
        Vec2 cp(widgetPos.x + col * CELL_W,
                gridY + row * CELL_H);
        Vec2 cs(CELL_W, CELL_H);

        bool isSelected = (d == value->day);
        bool isToday = (today.year == value->year &&
                        today.month == value->month && today.day == d);

        Vec2 mp(ctx->input.MouseX(), ctx->input.MouseY());
        bool hover = PointInRect(mp, cp, cs);
        if (hover && ctx->input.IsMousePressed(0)) {
            value->day = d;
            changed = true;
            isSelected = true;
        }

        if (isSelected) {
            ctx->renderer.DrawRectFilled(cp + Vec2(2,2), cs - Vec2(4,4), accent, 4.0f);
        } else if (hover) {
            Color hb = ctx->style.button.background.hover;
            ctx->renderer.DrawRectFilled(cp + Vec2(2,2), cs - Vec2(4,4), hb, 4.0f);
        } else if (isToday) {
            Color tb = accent; tb.a = 0.4f;
            ctx->renderer.DrawRect(cp + Vec2(2,2), cs - Vec2(4,4), tb, 4.0f);
        }

        char db[8];
        std::snprintf(db, sizeof(db), "%d", d);
        Vec2 ds = MeasureTextCached(ctx, db, cellStyle.fontSize);
        Color dc = isSelected ? Color(1.0f, 1.0f, 1.0f, 1.0f) : cellStyle.color;
        ctx->renderer.DrawText(Vec2(cp.x + (cs.x - ds.x) * 0.5f,
                                    cp.y + (cs.y - ds.y) * 0.5f),
                               db, dc, cellStyle.fontSize);
    }

    // Clamp day to valid range when month changes
    if (value->day > daysInM) value->day = daysInM;
    if (value->day < 1) value->day = 1;

    ctx->lastItemPos = widgetPos;
    AdvanceCursor(ctx, finalSize);
    SetLastItem(id, widgetPos, widgetPos + finalSize, false, false, false, changed);
    return changed;
}

bool TimePicker(const std::string& label, DateTimeValue* value, std::optional<Vec2> pos) {
    UIContext* ctx = GetContext();
    if (!ctx || !value) return false;

    const TextStyle& valStyle = ctx->style.GetTextStyle(TypographyStyle::Body);

    Vec2 totalSize(160.0f, 32.0f);
    LayoutConstraints constraints = ConsumeNextConstraints();
    Vec2 finalSize = ApplyConstraints(ctx, constraints, totalSize);
    Vec2 widgetPos = pos.has_value()
        ? ResolveAbsolutePosition(ctx, pos.value(), finalSize)
        : ctx->cursorPos;

    uint32_t id = GenerateId("TIME:", label.c_str());
    bool changed = false;

    auto Spinner = [&](int* v, int lo, int hi, const Vec2& cp, uint32_t spId) {
        Vec2 cs(40.0f, 32.0f);
        ctx->renderer.DrawRectFilled(cp, cs, ctx->style.panel.background, 3.0f);
        ctx->renderer.DrawRect(cp, cs, ctx->style.panel.borderColor, 3.0f);

        char buf[8];
        std::snprintf(buf, sizeof(buf), "%02d", *v);
        Vec2 ts = MeasureTextCached(ctx, buf, valStyle.fontSize);
        ctx->renderer.DrawText(Vec2(cp.x + (cs.x - ts.x) * 0.5f,
                                    cp.y + (cs.y - ts.y) * 0.5f),
                               buf, valStyle.color, valStyle.fontSize);

        // Up/Down zones: top half / bottom half on hover+click
        Vec2 mp(ctx->input.MouseX(), ctx->input.MouseY());
        if (PointInRect(mp, cp, cs) && ctx->input.IsMousePressed(0)) {
            if (mp.y < cp.y + cs.y * 0.5f) {
                *v = (*v >= hi) ? lo : *v + 1;
            } else {
                *v = (*v <= lo) ? hi : *v - 1;
            }
            changed = true;
        }
        // Wheel scroll on hover
        if (PointInRect(mp, cp, cs) && ctx->input.MouseWheelY() != 0.0f) {
            int delta = (ctx->input.MouseWheelY() > 0.0f) ? 1 : -1;
            int nv = *v + delta;
            if (nv < lo) nv = hi;
            if (nv > hi) nv = lo;
            *v = nv;
            changed = true;
        }
        (void)spId;
    };

    Vec2 cp = widgetPos;
    Spinner(&value->hour,   0, 23, cp, id ^ 0x10); cp.x += 44.0f;
    ctx->renderer.DrawText(Vec2(cp.x - 4.0f, cp.y + 8.0f), ":", valStyle.color, valStyle.fontSize);
    Spinner(&value->minute, 0, 59, cp, id ^ 0x20); cp.x += 44.0f;
    ctx->renderer.DrawText(Vec2(cp.x - 4.0f, cp.y + 8.0f), ":", valStyle.color, valStyle.fontSize);
    Spinner(&value->second, 0, 59, cp, id ^ 0x30);

    ctx->lastItemPos = widgetPos;
    AdvanceCursor(ctx, finalSize);
    SetLastItem(id, widgetPos, widgetPos + finalSize, false, false, false, changed);
    (void)label;
    return changed;
}

bool DateTimePicker(const std::string& label, DateTimeValue* value, std::optional<Vec2> pos) {
    UIContext* ctx = GetContext();
    if (!ctx || !value) return false;
    bool a = DatePicker(label, value, pos);
    bool b = TimePicker(label, value);
    return a || b;
}

} // namespace FluentUI
