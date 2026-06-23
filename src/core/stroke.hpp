#pragma once

#include "types.hpp"
#include <string>
#include <vector>
#include <memory>
#include <cstdint>

namespace fap {

class Stroke : public NonCopyable {
public:
    Stroke(BrushMode mode);
    virtual ~Stroke() = default;

    BrushMode mode() const { return mode_; }
    const std::vector<StrokePoint>& points() const { return points_; }

    void addPoint(const StrokePoint& point);
    void clear();

    size_t pointCount() const { return points_.size(); }
    bool empty() const { return points_.empty(); }

protected:
    BrushMode mode_;
    std::vector<StrokePoint> points_;
};

class RasterStroke : public Stroke {
public:
    RasterStroke();
    // TODO: rasterized pixel data
};

// VectorStroke is now defined in engine/vector/bezier_path.hpp
struct VectorStroke;

} // namespace fap
