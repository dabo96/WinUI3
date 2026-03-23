#pragma once
#include <cmath>
#include <algorithm>

namespace FluentUI {
    struct Vec2 {
        float x, y;
        Vec2() : x(0), y(0) {}
        Vec2(float X, float Y) : x(X), y(Y) {}

        // Arithmetic operators
        Vec2 operator+(const Vec2& v) const { return { x + v.x, y + v.y }; }
        Vec2 operator-(const Vec2& v) const { return { x - v.x, y - v.y }; }
        Vec2 operator*(float s) const { return { x * s, y * s }; }
        Vec2 operator/(float s) const { return { x / s, y / s }; }
        Vec2 operator*(const Vec2& v) const { return { x * v.x, y * v.y }; }
        Vec2 operator/(const Vec2& v) const { return { x / v.x, y / v.y }; }
        Vec2 operator-() const { return { -x, -y }; }

        Vec2& operator+=(const Vec2& v) { x += v.x; y += v.y; return *this; }
        Vec2& operator-=(const Vec2& v) { x -= v.x; y -= v.y; return *this; }
        Vec2& operator*=(float s) { x *= s; y *= s; return *this; }
        Vec2& operator/=(float s) { x /= s; y /= s; return *this; }

        bool operator==(const Vec2& v) const { return x == v.x && y == v.y; }
        bool operator!=(const Vec2& v) const { return !(*this == v); }

        // Vector operations
        float dot(const Vec2& v) const { return x * v.x + y * v.y; }
        float cross(const Vec2& v) const { return x * v.y - y * v.x; }
        float length() const { return std::sqrt(x * x + y * y); }
        float lengthSquared() const { return x * x + y * y; }

        Vec2 normalized() const {
            float len = length();
            return len > 0.0f ? Vec2(x / len, y / len) : Vec2(0, 0);
        }

        Vec2 perp() const { return { -y, x }; }

        float distance(const Vec2& v) const { return (*this - v).length(); }

        Vec2 clamp(const Vec2& minVal, const Vec2& maxVal) const {
            return { std::clamp(x, minVal.x, maxVal.x), std::clamp(y, minVal.y, maxVal.y) };
        }

        static Vec2 Lerp(const Vec2& a, const Vec2& b, float t) {
            return a + (b - a) * t;
        }

        static Vec2 Min(const Vec2& a, const Vec2& b) {
            return { std::min(a.x, b.x), std::min(a.y, b.y) };
        }

        static Vec2 Max(const Vec2& a, const Vec2& b) {
            return { std::max(a.x, b.x), std::max(a.y, b.y) };
        }

        // Friend operators for scalar * Vec2
        friend Vec2 operator*(float s, const Vec2& v) { return { s * v.x, s * v.y }; }
    };
}
