#include "core/stroke.hpp"

namespace fap {

Stroke::Stroke(BrushMode mode) : mode_(mode) {}

void Stroke::addPoint(const StrokePoint& point) {
    points_.push_back(point);
}

void Stroke::clear() {
    points_.clear();
}

RasterStroke::RasterStroke() : Stroke(BrushMode::Raster) {}

} // namespace fap
