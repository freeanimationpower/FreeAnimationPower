#pragma once

#include "core/types.hpp"
#include <vector>
#include <cstdint>
#include <algorithm>

namespace fap {

class RasterEngine {
public:
    RasterEngine(int width, int height);

    int width() const { return width_; }
    int height() const { return height_; }

    uint32_t* pixels() { return pixels_.data(); }
    const uint32_t* pixels() const { return pixels_.data(); }

    void fill(const Color& color);
    void clear();

private:
    int width_;
    int height_;
    std::vector<uint32_t> pixels_;
};

} // namespace fap
