#pragma once

#include "engine/vector/bezier_path.hpp"
#include <vector>
#include <cstddef>

namespace fap {

class RasterEngine;

class VectorEngine {
public:
    void addStroke(const VectorStroke& stroke);
    void removeStroke(int index);
    VectorStroke& strokeAt(int index);
    const std::vector<VectorStroke>& strokes() const;
    void clear();
    size_t strokeCount() const;
    void renderToRaster(class RasterEngine& target) const;

private:
    std::vector<VectorStroke> strokes_;
};

} // namespace fap
