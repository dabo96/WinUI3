#include "core/Demo.h"
#include "core/Context.h"
#include "core/Renderer.h"
#include "Math/Color.h"
#include "Math/Vec2.h"
#include "UI/Widgets.h"
#include "core/UIKey.h"
#include <cstdio>

namespace FluentUI {
namespace Demo {

namespace {

struct PickerState {
    bool active = false;
    bool frozen = false;
    uint32_t pickedId = 0;
    Vec2 pickedPos{0, 0};
    Vec2 pickedSize{0, 0};
    bool pickedHovered = false;
    bool pickedFocused = false;
};

PickerState* GetState() {
    static PickerState s;
    return &s;
}

} // namespace

void ShowItemPicker(bool* open) {
    UIContext* ctx = GetContext();
    if (!ctx) return;
    PickerState* st = GetState();

    // Toggle on Ctrl+Shift+P
    bool ctrl = ctx->input.IsKeyDown(UIKey::LeftCtrl) || ctx->input.IsKeyDown(UIKey::RightCtrl);
    bool shift = ctx->input.IsKeyDown(UIKey::LeftShift) || ctx->input.IsKeyDown(UIKey::RightShift);
    if (ctrl && shift && ctx->input.IsKeyPressed(UIKey::P)) {
        st->active = !st->active;
        st->frozen = false;
        if (open) *open = st->active;
    }

    if (open && !*open) {
        st->active = false;
        return;
    }
    if (!st->active) return;

    auto prevLayer = ctx->renderer.GetLayer();
    ctx->renderer.SetLayer(RenderLayer::Tooltip);

    // While not frozen, copy lastItem each frame (it updates as the user hovers widgets).
    if (!st->frozen && ctx->lastItem.id != 0) {
        st->pickedId = ctx->lastItem.id;
        st->pickedPos = ctx->lastItem.bboxMin;
        st->pickedSize = Vec2(ctx->lastItem.bboxMax.x - ctx->lastItem.bboxMin.x,
                              ctx->lastItem.bboxMax.y - ctx->lastItem.bboxMin.y);
        st->pickedHovered = ctx->lastItem.hovered;
        st->pickedFocused = ctx->lastItem.focused;
    }

    // Highlight the picked widget with an accent border
    if (st->pickedSize.x > 0.0f && st->pickedSize.y > 0.0f) {
        Color accent = ctx->style.accentColor;
        Color fill = accent; fill.a = 0.18f;
        ctx->renderer.DrawRectFilled(st->pickedPos, st->pickedSize, fill, 0.0f);
        ctx->renderer.DrawRect(st->pickedPos, st->pickedSize, accent, 0.0f);
    }

    // Click freezes / unfreezes the selection
    if (ctx->input.IsMousePressed(0)) st->frozen = !st->frozen;

    ctx->renderer.SetLayer(prevLayer);

    // Inspector panel
    Vec2 viewport = ctx->renderer.GetViewportSize();
    Vec2 panelSize(280.0f, 180.0f);
    Vec2 panelPos(viewport.x - panelSize.x - 20.0f,
                  viewport.y - panelSize.y - 20.0f);

    BeginPanel("FluentUIItemPicker", panelSize, true,
               std::nullopt, std::nullopt, panelPos);
    BeginVertical(4.0f);
    Label(st->frozen ? "Item Picker (frozen)" : "Item Picker (live)",
          std::nullopt, TypographyStyle::Title);
    Separator();

    char buf[128];
    std::snprintf(buf, sizeof(buf), "id: %u", st->pickedId);
    Label(buf);
    std::snprintf(buf, sizeof(buf), "pos: (%.0f, %.0f)", st->pickedPos.x, st->pickedPos.y);
    Label(buf);
    std::snprintf(buf, sizeof(buf), "size: %.0f x %.0f", st->pickedSize.x, st->pickedSize.y);
    Label(buf);
    Label(st->pickedHovered ? "hovered: yes" : "hovered: no");
    Label(st->pickedFocused ? "focused: yes" : "focused: no");
    Label("Ctrl+Shift+P to toggle / click to freeze",
          std::nullopt, TypographyStyle::Caption);

    EndVertical();
    EndPanel();
}

} // namespace Demo
} // namespace FluentUI
