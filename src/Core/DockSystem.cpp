#include "core/DockSystem.h"
#include <algorithm>
#include <cmath>

namespace FluentUI {

void DockSpace::HandleInteraction(float mouseX, float mouseY, bool mouseDown, bool mousePressed) {
    if (!root_ || root_->type != DockNode::Type::Split) return;

    // Recursively handle split resize
    std::function<void(DockNode*)> processNode = [&](DockNode* node) {
        if (!node || node->type != DockNode::Type::Split) return;

        const auto& b = node->bounds;
        float divider = DIVIDER_THICKNESS;

        // Compute divider rect
        Rect dividerRect;
        if (node->splitVertical) {
            float firstW = (b.size.x - divider) * node->splitRatio;
            dividerRect = {{b.pos.x + firstW, b.pos.y}, {divider, b.size.y}};
        } else {
            float firstH = (b.size.y - divider) * node->splitRatio;
            dividerRect = {{b.pos.x, b.pos.y + firstH}, {b.size.x, divider}};
        }

        // Check interaction
        if (node->resizing) {
            if (mouseDown) {
                float delta;
                if (node->splitVertical) {
                    delta = mouseX - node->resizeStartMouse;
                    float totalSpace = b.size.x - divider;
                    if (totalSpace > 0) {
                        node->splitRatio = node->resizeStartRatio + delta / totalSpace;
                    }
                } else {
                    delta = mouseY - node->resizeStartMouse;
                    float totalSpace = b.size.y - divider;
                    if (totalSpace > 0) {
                        node->splitRatio = node->resizeStartRatio + delta / totalSpace;
                    }
                }
                // Clamp ratio (min 50px per side)
                float minRatio = 50.0f / (node->splitVertical ? b.size.x : b.size.y);
                float maxRatio = 1.0f - minRatio;
                node->splitRatio = std::clamp(node->splitRatio, minRatio, maxRatio);
            } else {
                node->resizing = false;
            }
        } else if (mousePressed && dividerRect.Contains({mouseX, mouseY})) {
            node->resizing = true;
            node->resizeStartMouse = node->splitVertical ? mouseX : mouseY;
            node->resizeStartRatio = node->splitRatio;
        }

        // Recurse into children
        if (node->first) processNode(node->first.get());
        if (node->second) processNode(node->second.get());
    };

    processNode(root_.get());
}

DockPosition HitTestDockZones(const Rect& bounds, float mouseX, float mouseY) {
    if (bounds.IsEmpty()) return DockPosition::Float;
    if (!bounds.Contains({mouseX, mouseY})) return DockPosition::Float;

    float w = bounds.size.x;
    float h = bounds.size.y;
    float zoneW = w * 0.25f;  // 25% edge zones
    float zoneH = h * 0.25f;

    float localX = mouseX - bounds.pos.x;
    float localY = mouseY - bounds.pos.y;

    // Center zone (middle 50%)
    bool inCenterX = localX > zoneW && localX < w - zoneW;
    bool inCenterY = localY > zoneH && localY < h - zoneH;
    if (inCenterX && inCenterY) return DockPosition::Center;

    // Edge zones — pick the closest edge
    float distL = localX;
    float distR = w - localX;
    float distT = localY;
    float distB = h - localY;
    float minDist = std::min({distL, distR, distT, distB});

    if (minDist == distL) return DockPosition::Left;
    if (minDist == distR) return DockPosition::Right;
    if (minDist == distT) return DockPosition::Top;
    return DockPosition::Bottom;
}

Rect GetDockZonePreviewRect(const Rect& bounds, DockPosition zone) {
    float w = bounds.size.x;
    float h = bounds.size.y;
    float half = 0.5f;

    switch (zone) {
    case DockPosition::Left:
        return {bounds.pos, {w * half, h}};
    case DockPosition::Right:
        return {{bounds.pos.x + w * half, bounds.pos.y}, {w * half, h}};
    case DockPosition::Top:
        return {bounds.pos, {w, h * half}};
    case DockPosition::Bottom:
        return {{bounds.pos.x, bounds.pos.y + h * half}, {w, h * half}};
    case DockPosition::Center:
        return bounds;
    default:
        return {};
    }
}

} // namespace FluentUI
