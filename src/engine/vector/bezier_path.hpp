#pragma once

#include "core/types.hpp"
#include <vector>

namespace fap {

struct BezierPoint {
    Vec2 position;
    Vec2 tangentIn;
    Vec2 tangentOut;
    bool smooth = false;
};

struct CubicBezier {
    Vec2 p0, p1, p2, p3;
};

class BezierPath {
public:
    void moveTo(float x, float y);
    void lineTo(float x, float y);
    void curveTo(float cx1, float cy1, float cx2, float cy2, float x, float y);
    void closePath();

    Vec2 evaluateAt(float t) const;
    Vec2 tangentAt(float t) const;
    std::vector<Vec2> flattenPoints(float tolerance = 0.5f) const;
    float totalLength() const;

    bool empty() const;
    void clear();
    int pointCount() const;

    const std::vector<CubicBezier>& segments() const { return segments_; }
    std::vector<CubicBezier>& segments() { return segments_; }
    std::vector<BezierPoint> points() const;

private:
    std::vector<CubicBezier> segments_;
    Vec2 currentPos_{0.0f, 0.0f};
    bool hasOpenSubpath_ = false;
    Vec2 subpathStart_{0.0f, 0.0f};

    void finalizeSubpath();

    static Vec2 evalBezier(const CubicBezier& b, float t);
    static Vec2 tanBezier(const CubicBezier& b, float t);
    static float flatness(const CubicBezier& b);
    static void subdivide(const CubicBezier& b, float t, CubicBezier& left, CubicBezier& right);
    void flattenSegment(const CubicBezier& seg, float tolerance, std::vector<Vec2>& out) const;
};

struct VectorStroke {
    BezierPath path;
    Color color;
    float width = 2.0f;
    float opacity = 1.0f;
    BlendMode blend = BlendMode::Normal;
};

BezierPath createPathFromPoints(const std::vector<StrokePoint>& points, float smoothing = 0.5f);
VectorStroke createVectorStroke(const std::vector<StrokePoint>& points, const Color& color,
                                float width, float smoothing = 0.5f);

} // namespace fap
