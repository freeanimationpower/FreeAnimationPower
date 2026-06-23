#include "engine/vector/bezier_path.hpp"
#include "core/types.hpp"
#include <vector>
#include <cmath>

namespace fap {

BezierPath createPathFromPoints(const std::vector<StrokePoint>& points, float smoothing) {
    BezierPath path;
    size_t n = points.size();

    if (n == 0) return path;

    path.moveTo(points[0].position.x, points[0].position.y);

    if (n == 1) return path;

    if (n == 2) {
        path.lineTo(points[1].position.x, points[1].position.y);
        return path;
    }

    float tau = smoothing / 6.0f;
    float firstTau = smoothing / 3.0f;

    for (size_t i = 0; i < n - 1; ++i) {
        Vec2 p0 = points[i].position;
        Vec2 p3 = points[i + 1].position;

        Vec2 tIn;
        Vec2 tOut;

        if (i == 0) {
            tIn = {
                p0.x + (p3.x - p0.x) * firstTau,
                p0.y + (p3.y - p0.y) * firstTau
            };
        } else {
            Vec2 pm1 = points[i - 1].position;
            tIn = {
                p0.x + (p3.x - pm1.x) * tau,
                p0.y + (p3.y - pm1.y) * tau
            };
        }

        if (i + 2 >= n) {
            tOut = {
                p3.x - (p3.x - p0.x) * firstTau,
                p3.y - (p3.y - p0.y) * firstTau
            };
        } else {
            Vec2 pp2 = points[i + 2].position;
            tOut = {
                p3.x - (pp2.x - p0.x) * tau,
                p3.y - (pp2.y - p0.y) * tau
            };
        }

        path.curveTo(tIn.x, tIn.y, tOut.x, tOut.y, p3.x, p3.y);
    }

    return path;
}

VectorStroke createVectorStroke(const std::vector<StrokePoint>& points,
                                const Color& color, float width, float smoothing) {
    VectorStroke stroke;
    stroke.path = createPathFromPoints(points, smoothing);
    stroke.color = color;
    stroke.width = width;
    stroke.opacity = color.a;
    stroke.blend = BlendMode::Normal;
    return stroke;
}

} // namespace fap
