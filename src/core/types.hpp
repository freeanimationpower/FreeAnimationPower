#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <cmath>
#include <algorithm>

namespace fap {

enum class LayerType {
    Raster,
    Vector,
    Group,
    Audio,
    Camera
};

enum class BlendMode {
    Normal,
    Multiply,
    Screen,
    Overlay,
    Add,
    Subtract,
    Darken,
    Lighten,
    ColorBurn,
    ColorDodge,
    SoftLight,
    HardLight
};

enum class BrushMode {
    Raster,
    Vector,
    Hybrid
};

enum class ToolType {
    Brush,
    Eraser,
    ColorPicker,
    Fill,
    Text,
    Line,
    Rectangle,
    Ellipse,
    Move,
    Select,
    Hand,
    PencilRetouch,
    RulerLine,
    RulerEllipse,
    DeformMesh,
    TweenEdit
};

enum class FillType {
    Solid,
    Fabric,
    Ramp
};

enum class ColorVariationType {
    Monochromatic,
    Analogous,
    Triadic
};

enum class LineStyle {
    Solid,
    Tapered,
    Dashed,
    Dotted
};

struct Color {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 1.0f;

    static Color fromRGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
        return { r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f };
    }

    Color premultiplied() const {
        return { r * a, g * a, b * a, a };
    }

    bool operator==(const Color& other) const {
        return r == other.r && g == other.g && b == other.b && a == other.a;
    }

    bool operator!=(const Color& other) const {
        return !(*this == other);
    }
};

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;

    Vec2() = default;
    Vec2(float x_, float y_) : x(x_), y(y_) {}

    Vec2 operator+(const Vec2& o) const { return { x + o.x, y + o.y }; }
    Vec2 operator-(const Vec2& o) const { return { x - o.x, y - o.y }; }
    Vec2 operator*(float s) const { return { x * s, y * s }; }
    Vec2 operator/(float s) const { return { x / s, y / s }; }
    Vec2& operator+=(const Vec2& o) { x += o.x; y += o.y; return *this; }
    Vec2& operator-=(const Vec2& o) { x -= o.x; y -= o.y; return *this; }

    bool operator==(const Vec2& o) const { return x == o.x && y == o.y; }
    bool operator!=(const Vec2& o) const { return !(*this == o); }

    float length() const { return std::sqrt(x * x + y * y); }
    float lengthSquared() const { return x * x + y * y; }

    Vec2 normalized() const {
        float len = length();
        if (len < 1e-8f) return { 0.0f, 0.0f };
        return { x / len, y / len };
    }

    float dot(const Vec2& o) const { return x * o.x + y * o.y; }
    float cross(const Vec2& o) const { return x * o.y - y * o.x; }

    static float distance(const Vec2& a, const Vec2& b) {
        return (a - b).length();
    }

    static Vec2 lerp(const Vec2& a, const Vec2& b, float t) {
        return { a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t };
    }
};

struct Rect {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;

    float left() const { return x; }
    float right() const { return x + width; }
    float top() const { return y; }
    float bottom() const { return y + height; }

    bool contains(float px, float py) const {
        return px >= x && px <= x + width && py >= y && py <= y + height;
    }

    bool contains(const Vec2& p) const {
        return contains(p.x, p.y);
    }

    bool isEmpty() const {
        return width <= 0.0f || height <= 0.0f;
    }

    Rect intersect(const Rect& other) const {
        float nx = std::max(x, other.x);
        float ny = std::max(y, other.y);
        float nw = std::min(right(), other.right()) - nx;
        float nh = std::min(bottom(), other.bottom()) - ny;
        if (nw < 0) nw = 0;
        if (nh < 0) nh = 0;
        return { nx, ny, nw, nh };
    }

    Rect expanded(float margin) const {
        return { x - margin, y - margin, width + margin * 2, height + margin * 2 };
    }
};

struct StrokePoint {
    Vec2 position;
    float pressure = 1.0f;
    float tilt_x = 0.0f;
    float tilt_y = 0.0f;
    float rotation = 0.0f;
    float timestamp = 0.0f;
};

struct Size {
    int width = 0;
    int height = 0;

    bool operator==(const Size& o) const { return width == o.width && height == o.height; }
    bool operator!=(const Size& o) const { return !(*this == o); }
    bool isValid() const { return width > 0 && height > 0; }
};

class NonCopyable {
public:
    NonCopyable() = default;
    NonCopyable(const NonCopyable&) = delete;
    NonCopyable& operator=(const NonCopyable&) = delete;
};

class NonMovable {
public:
    NonMovable() = default;
    NonMovable(NonMovable&&) = delete;
    NonMovable& operator=(NonMovable&&) = delete;
};

inline float clampf(float v, float lo, float hi) {
    return std::max(lo, std::min(hi, v));
}

inline float lerpf(float a, float b, float t) {
    return a + (b - a) * t;
}

inline float smoothstepf(float edge0, float edge1, float x) {
    if (edge0 == edge1) {
        return (x >= edge1) ? 1.0f : 0.0f;
    }
    float t = clampf((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

} // namespace fap
