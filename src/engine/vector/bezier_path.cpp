#include "engine/vector/bezier_path.hpp"
#include <cmath>
#include <algorithm>

namespace fap {

static Vec2 lerp(const Vec2& a, const Vec2& b, float t) {
    return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
}

static float dot(const Vec2& a, const Vec2& b) {
    return a.x * b.x + a.y * b.y;
}

static float length(const Vec2& v) {
    return std::sqrt(v.x * v.x + v.y * v.y);
}

static Vec2 normalize(const Vec2& v) {
    float len = length(v);
    if (len < 1e-9f) return {0.0f, 0.0f};
    return {v.x / len, v.y / len};
}

void BezierPath::finalizeSubpath() {
    hasOpenSubpath_ = false;
}

void BezierPath::moveTo(float x, float y) {
    finalizeSubpath();
    currentPos_ = {x, y};
    subpathStart_ = {x, y};
    hasOpenSubpath_ = true;
}

void BezierPath::lineTo(float x, float y) {
    Vec2 p0 = currentPos_;
    Vec2 p3 = {x, y};
    Vec2 delta = p3 - p0;
    Vec2 p1 = p0 + delta * (1.0f / 3.0f);
    Vec2 p2 = p0 + delta * (2.0f / 3.0f);
    segments_.push_back({p0, p1, p2, p3});
    currentPos_ = p3;
}

void BezierPath::curveTo(float cx1, float cy1, float cx2, float cy2, float x, float y) {
    Vec2 p0 = currentPos_;
    Vec2 p1 = {cx1, cy1};
    Vec2 p2 = {cx2, cy2};
    Vec2 p3 = {x, y};
    segments_.push_back({p0, p1, p2, p3});
    currentPos_ = p3;
}

void BezierPath::closePath() {
    if (!hasOpenSubpath_) return;
    if (currentPos_.x != subpathStart_.x || currentPos_.y != subpathStart_.y) {
        lineTo(subpathStart_.x, subpathStart_.y);
    }
    finalizeSubpath();
}

Vec2 BezierPath::evalBezier(const CubicBezier& b, float t) {
    float u = 1.0f - t;
    float uu = u * u;
    float uuu = uu * u;
    float tt = t * t;
    float ttt = tt * t;
    return {
        uuu * b.p0.x + 3.0f * uu * t * b.p1.x + 3.0f * u * tt * b.p2.x + ttt * b.p3.x,
        uuu * b.p0.y + 3.0f * uu * t * b.p1.y + 3.0f * u * tt * b.p2.y + ttt * b.p3.y
    };
}

Vec2 BezierPath::tanBezier(const CubicBezier& b, float t) {
    float u = 1.0f - t;
    float uu = u * u;
    float tt = t * t;
    Vec2 d0 = b.p1 - b.p0;
    Vec2 d1 = b.p2 - b.p1;
    Vec2 d2 = b.p3 - b.p2;
    return {
        3.0f * uu * d0.x + 6.0f * u * t * d1.x + 3.0f * tt * d2.x,
        3.0f * uu * d0.y + 6.0f * u * t * d1.y + 3.0f * tt * d2.y
    };
}

Vec2 BezierPath::evaluateAt(float t) const {
    if (segments_.empty()) return {0.0f, 0.0f};
    if (segments_.size() == 1) return evalBezier(segments_[0], t);

    size_t segCount = segments_.size();
    float globalT = t * static_cast<float>(segCount);
    size_t idx = static_cast<size_t>(globalT);
    if (idx >= segCount) {
        idx = segCount - 1;
        return evalBezier(segments_[idx], 1.0f);
    }
    float localT = globalT - static_cast<float>(idx);
    return evalBezier(segments_[idx], localT);
}

Vec2 BezierPath::tangentAt(float t) const {
    if (segments_.empty()) return {1.0f, 0.0f};
    if (segments_.size() == 1) return normalize(tanBezier(segments_[0], t));

    size_t segCount = segments_.size();
    float globalT = t * static_cast<float>(segCount);
    size_t idx = static_cast<size_t>(globalT);
    if (idx >= segCount) {
        idx = segCount - 1;
        return normalize(tanBezier(segments_[idx], 1.0f));
    }
    float localT = globalT - static_cast<float>(idx);
    return normalize(tanBezier(segments_[idx], localT));
}

float BezierPath::flatness(const CubicBezier& b) {
    Vec2 chord = b.p3 - b.p0;
    float chordLen2 = dot(chord, chord);
    if (chordLen2 < 1e-12f) {
        float d1 = dot(b.p1 - b.p0, b.p1 - b.p0);
        float d2 = dot(b.p2 - b.p0, b.p2 - b.p0);
        return std::sqrt(std::max(d1, d2));
    }
    float cross1 = std::abs((b.p1.x - b.p0.x) * chord.y - (b.p1.y - b.p0.y) * chord.x);
    float cross2 = std::abs((b.p2.x - b.p0.x) * chord.y - (b.p2.y - b.p0.y) * chord.x);
    float invLen = 1.0f / std::sqrt(chordLen2);
    return std::max(cross1, cross2) * invLen;
}

void BezierPath::subdivide(const CubicBezier& b, float t, CubicBezier& left, CubicBezier& right) {
    Vec2 p01 = lerp(b.p0, b.p1, t);
    Vec2 p12 = lerp(b.p1, b.p2, t);
    Vec2 p23 = lerp(b.p2, b.p3, t);
    Vec2 p012 = lerp(p01, p12, t);
    Vec2 p123 = lerp(p12, p23, t);
    Vec2 p0123 = lerp(p012, p123, t);
    left = {b.p0, p01, p012, p0123};
    right = {p0123, p123, p23, b.p3};
}

void BezierPath::flattenSegment(const CubicBezier& seg, float tolerance, std::vector<Vec2>& out) const {
    if (flatness(seg) <= tolerance) {
        out.push_back(seg.p3);
        return;
    }
    CubicBezier left, right;
    subdivide(seg, 0.5f, left, right);
    flattenSegment(left, tolerance, out);
    flattenSegment(right, tolerance, out);
}

std::vector<Vec2> BezierPath::flattenPoints(float tolerance) const {
    std::vector<Vec2> result;
    if (segments_.empty()) return result;

    result.push_back(segments_[0].p0);
    for (const auto& seg : segments_) {
        flattenSegment(seg, tolerance, result);
    }
    return result;
}

float BezierPath::totalLength() const {
    auto pts = flattenPoints(0.5f);
    float len = 0.0f;
    for (size_t i = 1; i < pts.size(); ++i) {
        Vec2 d = pts[i] - pts[i - 1];
        len += length(d);
    }
    return len;
}

bool BezierPath::empty() const {
    return segments_.empty() && !hasOpenSubpath_;
}

void BezierPath::clear() {
    segments_.clear();
    currentPos_ = {0.0f, 0.0f};
    subpathStart_ = {0.0f, 0.0f};
    hasOpenSubpath_ = false;
}

int BezierPath::pointCount() const {
    return static_cast<int>(segments_.size() * 4);
}

std::vector<BezierPoint> BezierPath::points() const {
    std::vector<BezierPoint> result;
    if (segments_.empty()) return result;

    BezierPoint first;
    first.position = segments_[0].p0;
    first.tangentIn = segments_[0].p0;
    first.tangentOut = segments_[0].p1;
    result.push_back(first);

    for (size_t i = 0; i < segments_.size(); ++i) {
        BezierPoint pt;
        pt.position = segments_[i].p3;
        pt.tangentIn = segments_[i].p2;
        if (i + 1 < segments_.size()) {
            pt.tangentOut = segments_[i + 1].p1;
        } else {
            pt.tangentOut = segments_[i].p3;
        }
        result.push_back(pt);
    }
    return result;
}

} // namespace fap
