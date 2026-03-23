#pragma once
#include "Math/Vec2.h"
#include <algorithm>

namespace FluentUI {

struct Rect {
    Vec2 pos;
    Vec2 size;

    Rect() : pos(0, 0), size(0, 0) {}
    Rect(Vec2 p, Vec2 s) : pos(p), size(s) {}
    Rect(float x, float y, float w, float h) : pos(x, y), size(w, h) {}

    float Right() const { return pos.x + size.x; }
    float Bottom() const { return pos.y + size.y; }
    float Left() const { return pos.x; }
    float Top() const { return pos.y; }
    float Width() const { return size.x; }
    float Height() const { return size.y; }
    Vec2 Center() const { return pos + size * 0.5f; }
    Vec2 TopLeft() const { return pos; }
    Vec2 BottomRight() const { return { Right(), Bottom() }; }

    bool Contains(const Vec2& point) const {
        return point.x >= pos.x && point.x <= pos.x + size.x &&
               point.y >= pos.y && point.y <= pos.y + size.y;
    }

    bool Overlaps(const Rect& other) const {
        return pos.x < other.Right() && Right() > other.pos.x &&
               pos.y < other.Bottom() && Bottom() > other.pos.y;
    }

    Rect Intersection(const Rect& other) const {
        float l = std::max(pos.x, other.pos.x);
        float t = std::max(pos.y, other.pos.y);
        float r = std::min(Right(), other.Right());
        float b = std::min(Bottom(), other.Bottom());
        if (r <= l || b <= t) return Rect(0, 0, 0, 0);
        return Rect(l, t, r - l, b - t);
    }

    Rect Union(const Rect& other) const {
        float l = std::min(pos.x, other.pos.x);
        float t = std::min(pos.y, other.pos.y);
        float r = std::max(Right(), other.Right());
        float b = std::max(Bottom(), other.Bottom());
        return Rect(l, t, r - l, b - t);
    }

    Rect Expanded(float amount) const {
        return Rect(pos.x - amount, pos.y - amount,
                    size.x + amount * 2, size.y + amount * 2);
    }

    Rect Shrunk(float amount) const {
        return Expanded(-amount);
    }

    bool IsEmpty() const { return size.x <= 0 || size.y <= 0; }

    bool operator==(const Rect& r) const { return pos == r.pos && size == r.size; }
    bool operator!=(const Rect& r) const { return !(*this == r); }
};

} // namespace FluentUI
