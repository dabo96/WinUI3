#pragma once

namespace FluentUI {
    struct Vec2 {
        float x, y;
        Vec2() : x(0), y(0) {}
        Vec2(float X, float Y) : x(X), y(Y) {}
        Vec2 operator+(const Vec2& v) const { return { x + v.x, y + v.y }; }
        Vec2 operator-(const Vec2& v) const { return { x - v.x, y - v.y }; }
        Vec2 operator*(float s) const { return { x * s, y * s }; }
        
        bool operator==(const Vec2& v) const { return x == v.x && y == v.y; }
        bool operator!=(const Vec2& v) const { return !(*this == v); }
    };
}