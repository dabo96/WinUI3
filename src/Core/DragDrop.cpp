#include "core/DragDrop.h"
#include "core/Context.h"
#include "UI/WidgetHelpers.h"
#include <cmath>
#include <cstring>

namespace FluentUI {

namespace {
constexpr float kDragThreshold = 5.0f;
}

// ─── DragDropSource ───────────────────────────────────────────────────────

DragDropSource::DragDropSource(const std::string &payloadType) : ctx(GetContext()) {
    if (!ctx) return;

    auto &dd = ctx->dragDrop;

    // Determine activation: a drag begins from the last drawn item if the user
    // pressed the mouse on that item this frame and now exceeds the threshold.
    if (!dd.active) {
        // The widget that wraps this source must have published lastItemPos / lastItemSize.
        Vec2 itemPos = ctx->lastItemPos;
        Vec2 itemSize = ctx->lastItemSize;
        if (itemSize.x > 0.0f && itemSize.y > 0.0f) {
            bool hover = IsMouseOver(ctx, itemPos, itemSize);
            // Press start: store start pos in dragDrop until threshold crossed.
            if (hover && ctx->input.IsMousePressed(0)) {
                dd.startPos = Vec2(ctx->input.MouseX(), ctx->input.MouseY());
                dd.currentPos = dd.startPos;
                dd.sourceWidgetId = 1u;     // Mark as "armed"
                dd.payloadType = payloadType;
                dd.payloadBytes.clear();
                dd.active = false;
            }
            if (dd.sourceWidgetId == 1u && ctx->input.IsMouseDown(0)) {
                Vec2 cur(ctx->input.MouseX(), ctx->input.MouseY());
                Vec2 d = cur - dd.startPos;
                float dist2 = d.x * d.x + d.y * d.y;
                if (dist2 >= kDragThreshold * kDragThreshold) {
                    dd.active = true;
                    dd.currentPos = cur;
                    dd.payloadType = payloadType;
                    ownedActivation = true;
                }
            }
        }
    } else if (dd.payloadType == payloadType) {
        // Source is already active — keep current pos updated
        dd.currentPos = Vec2(ctx->input.MouseX(), ctx->input.MouseY());
        ownedActivation = true;
    }
}

DragDropSource::~DragDropSource() {
    if (!ctx) return;
    auto &dd = ctx->dragDrop;
    // If the user released without a target accepting, clear state next frame.
    if (dd.active && !ctx->input.IsMouseDown(0)) {
        if (!dd.acceptedThisFrame) {
            // Cancelled — reset everything
            dd.active = false;
            dd.payloadType.clear();
            dd.payloadBytes.clear();
            dd.sourceWidgetId = 0;
            if (dd.previewDrawCtx) {
                auto *fn = static_cast<std::function<void()> *>(dd.previewDrawCtx);
                delete fn;
                dd.previewDrawCtx = nullptr;
            }
        }
    }
}

bool DragDropSource::IsActive() const {
    if (!ctx) return false;
    return ownedActivation && ctx->dragDrop.active;
}

void DragDropSource::SetPayloadBytes(const uint8_t *data, size_t bytes) {
    if (!ctx) return;
    auto &dd = ctx->dragDrop;
    dd.payloadBytes.assign(data, data + bytes);
}

void DragDropSource::SetPayload(const std::string &s) {
    SetPayloadBytes(reinterpret_cast<const uint8_t *>(s.data()), s.size());
}

void DragDropSource::DragPreview(std::function<void()> draw) {
    if (!ctx) return;
    auto &dd = ctx->dragDrop;
    // Stash the draw callback. The Context drives invocation in the overlay layer.
    if (dd.previewDrawCtx) {
        auto *old = static_cast<std::function<void()> *>(dd.previewDrawCtx);
        delete old;
    }
    dd.previewDrawCtx = new std::function<void()>(std::move(draw));
}

// ─── DragDropTarget ───────────────────────────────────────────────────────

DragDropTarget::DragDropTarget() : ctx(GetContext()) {
    if (!ctx) return;
    // Default region = last drawn widget rect
    regionPos = ctx->lastItemPos;
    regionSize = ctx->lastItemSize;
}

DragDropTarget::~DragDropTarget() {}

void DragDropTarget::SetRegion(const Vec2 &pos, const Vec2 &size) {
    regionPos = pos;
    regionSize = size;
}

bool DragDropTarget::IsHovering() const {
    if (!ctx) return false;
    auto &dd = ctx->dragDrop;
    if (!dd.active) return false;
    if (regionSize.x <= 0.0f || regionSize.y <= 0.0f) return false;
    return IsMouseOver(ctx, regionPos, regionSize);
}

bool DragDropTarget::AcceptRaw(const std::string &payloadType,
                               const uint8_t **out, size_t *outSize) {
    if (!ctx || !out || !outSize) return false;
    auto &dd = ctx->dragDrop;
    if (!dd.active || dd.payloadType != payloadType) return false;
    if (!IsHovering()) return false;
    // Drop happens on mouse-up over the target this frame
    if (!ctx->input.IsMouseReleased(0)) return false;

    *out = dd.payloadBytes.data();
    *outSize = dd.payloadBytes.size();
    dd.acceptedThisFrame = true;
    dd.delivered = true;
    return true;
}

bool DragDropTarget::AcceptPayload(const std::string &payloadType, std::string *out) {
    const uint8_t *data = nullptr; size_t size = 0;
    if (!AcceptRaw(payloadType, &data, &size)) return false;
    if (out) out->assign(reinterpret_cast<const char *>(data), size);
    return true;
}

} // namespace FluentUI
