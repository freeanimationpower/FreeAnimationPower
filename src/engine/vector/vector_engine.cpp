#include "engine/vector/vector_engine.hpp"
#include "engine/raster/raster_engine.hpp"
#include "engine/common/blend_utils.hpp"
#include <cmath>
#include <algorithm>
#include <vector>
#include <stdexcept>

namespace fap {

namespace {

struct Edge {
    float x;
    float dx;
    int yEnd;
};

void fillPolygonAA(const std::vector<Vec2>& poly, float r, float g, float b, float a,
                   RasterEngine& target, BlendMode blend) {
    if (poly.size() < 3 || a <= 0.0f) return;

    int w = target.width();
    int h = target.height();
    if (w <= 0 || h <= 0) return;

    uint32_t* pixels = target.pixels();

    std::vector<std::vector<Edge>> edgeTable(static_cast<size_t>(h));

    int n = static_cast<int>(poly.size());
    for (int i = 0; i < n; ++i) {
        Vec2 p0 = poly[i];
        Vec2 p1 = poly[(i + 1) % n];

        float dy = p1.y - p0.y;
        if (std::abs(dy) < 1e-9f) continue;

        if (p0.y > p1.y) {
            std::swap(p0, p1);
        }

        float dx = (p1.x - p0.x) / dy;

        int yStart = static_cast<int>(std::ceil(p0.y));
        int yEnd = static_cast<int>(std::ceil(p1.y));

        yStart = std::max(0, yStart);
        yEnd = std::min(h, yEnd);

        if (yStart >= yEnd) continue;

        float xStart = p0.x + dx * (static_cast<float>(yStart) - p0.y);
        edgeTable[static_cast<size_t>(yStart)].push_back({xStart, dx, yEnd});
    }

    std::vector<Edge> active;
    for (int y = 0; y < h; ++y) {
        for (auto& e : edgeTable[static_cast<size_t>(y)]) {
            active.push_back(e);
        }

        active.erase(std::remove_if(active.begin(), active.end(),
            [y](const Edge& e) { return y >= e.yEnd; }), active.end());

        if (active.size() < 2) {
            for (auto& e : active) e.x += e.dx;
            continue;
        }

        std::sort(active.begin(), active.end(),
            [](const Edge& a, const Edge& b) { return a.x < b.x; });

        for (size_t i = 0; i + 1 < active.size(); i += 2) {
            float xLeft = active[i].x;
            float xRight = active[i + 1].x;

            if (xRight <= xLeft) continue;

            int x0 = std::max(0, static_cast<int>(std::floor(xLeft)));
            int x1 = std::min(w, static_cast<int>(std::ceil(xRight)));

            for (int x = x0; x < x1; ++x) {
                float left = std::max(static_cast<float>(x), xLeft);
                float right = std::min(static_cast<float>(x + 1), xRight);
                float coverage = std::max(0.0f, right - left);
                if (coverage > 0.0f) {
                    blendPixel(pixels, w, h, x, y, r, g, b, a * coverage, blend);
                }
            }
        }

        for (auto& e : active) e.x += e.dx;
    }
}

void renderThickPolyline(const std::vector<Vec2>& pts, float width,
                         float r, float g, float b, float a,
                         RasterEngine& target, BlendMode blend) {
    if (pts.size() < 2 || width <= 0.0f) return;

    float hw = width * 0.5f;

    for (size_t i = 1; i < pts.size(); ++i) {
        Vec2 A = pts[i - 1];
        Vec2 B = pts[i];

        float dx = B.x - A.x;
        float dy = B.y - A.y;
        float len = std::sqrt(dx * dx + dy * dy);
        if (len < 1e-9f) continue;

        float invLen = 1.0f / len;
        float ndx = -dy * invLen;
        float ndy = dx * invLen;

        float hwx = ndx * hw;
        float hwy = ndy * hw;

        std::vector<Vec2> quad = {
            {A.x + hwx, A.y + hwy},
            {A.x - hwx, A.y - hwy},
            {B.x - hwx, B.y - hwy},
            {B.x + hwx, B.y + hwy}
        };

        fillPolygonAA(quad, r, g, b, a, target, blend);
    }
}

} // anonymous namespace

void VectorEngine::addStroke(const VectorStroke& stroke) {
    strokes_.push_back(stroke);
}

void VectorEngine::removeStroke(int index) {
    if (index >= 0 && static_cast<size_t>(index) < strokes_.size()) {
        strokes_.erase(strokes_.begin() + index);
    }
}

VectorStroke& VectorEngine::strokeAt(int index) {
    if (index < 0 || static_cast<size_t>(index) >= strokes_.size()) {
        throw std::out_of_range("VectorEngine::strokeAt: index out of range");
    }
    return strokes_[static_cast<size_t>(index)];
}

const std::vector<VectorStroke>& VectorEngine::strokes() const {
    return strokes_;
}

void VectorEngine::clear() {
    strokes_.clear();
}

size_t VectorEngine::strokeCount() const {
    return strokes_.size();
}

void VectorEngine::renderToRaster(RasterEngine& target) const {
    for (const auto& stroke : strokes_) {
        std::vector<Vec2> pts = stroke.path.flattenPoints(0.5f);
        if (pts.size() < 2) continue;

        float finalAlpha = stroke.opacity;
        if (finalAlpha <= 0.0f) continue;

        renderThickPolyline(pts, stroke.width,
                           stroke.color.r, stroke.color.g, stroke.color.b,
                           finalAlpha, target, stroke.blend);
    }
}

} // namespace fap
