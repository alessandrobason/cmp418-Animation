#pragma once

#include <maths/vector2.h>
#include <maths/vector4.h>

namespace gef {
    struct Rect {
        union {
            struct { float x, y, w, h; };
            struct { gef::Vector2 pos, size; };
        };
        Rect() : x(0), y(0), w(0), h(0) {}
        Rect(float v) : x(v), y(v), w(v), h(v) {}
        Rect(float x, float y, float w, float h) : x(x), y(y), w(w), h(h) {}
        Rect(const gef::Vector2 &p, const gef::Vector2 &s) : pos(p), size(s) {}

        gef::Vector2 centre() const { return pos + size / (float)2; }

        Rect operator+(const Rect &v) const { return { x + v.x, y + v.y, w + v.w, h + v.h }; }
        Rect operator-(const Rect &v) const { return { x - v.x, y - v.y, w - v.w, h - v.h }; }
        Rect operator*(const Rect &v) const { return { x * v.x, y * v.y, w * v.w, h * v.h }; }
        Rect operator/(const Rect &v) const { return { x / v.x, y / v.y, w / v.w, h / v.h }; }

        Rect &operator+=(const Rect &v) { x += v.x; y += v.y; w += v.w; h += v.h; return *this; }
        Rect &operator-=(const Rect &v) { x -= v.x; y -= v.y; w -= v.w; h -= v.h; return *this; }
        Rect &operator*=(const Rect &v) { x *= v.x; y *= v.y; w *= v.w; h *= v.h; return *this; }
        Rect &operator/=(const Rect &v) { x /= v.x; y /= v.y; w /= v.w; h /= v.h; return *this; }

        Rect operator+(const Vector2 &v) const { return { x + v.x, y + v.y, w + v.x, h + v.y }; }
        Rect operator-(const Vector2 &v) const { return { x - v.x, y - v.y, w - v.x, h - v.y }; }
        Rect operator*(const Vector2 &v) const { return { x * v.x, y * v.y, w * v.x, h * v.y }; }
        Rect operator/(const Vector2 &v) const { return { x / v.x, y / v.y, w / v.x, h / v.y }; }

        Rect &operator+=(const Vector2 &v) { x += v.x; y += v.y; w += v.x; h += v.y; return *this; }
        Rect &operator-=(const Vector2 &v) { x -= v.x; y -= v.y; w -= v.x; h -= v.y; return *this; }
        Rect &operator*=(const Vector2 &v) { x *= v.x; y *= v.y; w *= v.x; h *= v.y; return *this; }
        Rect &operator/=(const Vector2 &v) { x /= v.x; y /= v.y; w /= v.x; h /= v.y; return *this; }

        Rect operator+(float v) const { return { x + v, y + v, w + v, h + v }; }
        Rect operator-(float v) const { return { x - v, y - v, w - v, h - v }; }
        Rect operator*(float v) const { return { x * v, y * v, w * v, h * v }; }
        Rect operator/(float v) const { return { x / v, y / v, w / v, h / v }; }

        Rect &operator+=(float v) { x += v; y += v; w += v; h += v; return *this; }
        Rect &operator-=(float v) { x -= v; y -= v; w -= v; h -= v; return *this; }
        Rect &operator*=(float v) { x *= v; y *= v; w *= v; h *= v; return *this; }
        Rect &operator/=(float v) { x /= v; y /= v; w /= v; h /= v; return *this; }

        bool operator==(const Rect &v) const { return x == v.x && y == v.y && w == v.w && h == v.h; }
        bool operator!=(const Rect &v) const { return !(*this == v); }
    };

} // namespace gef 