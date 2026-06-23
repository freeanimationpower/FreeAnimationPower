#pragma once

#include "core/layer.hpp"

namespace fap {

class RasterEngine;

class Compositor {
public:
    void composite(const GroupLayer& root, RasterEngine& target);
};

} // namespace fap
