#include "engine/raster/raster_engine.hpp"

namespace fap {

RasterEngine::RasterEngine(int width, int height)
    : width_(width), height_(height)
{
    pixels_.resize(static_cast<size_t>(width) * static_cast<size_t>(height), 0x00000000);
}

void RasterEngine::fill(const Color& color) {
    uint8_t r = static_cast<uint8_t>(std::clamp(color.r, 0.0f, 1.0f) * 255.0f);
    uint8_t g = static_cast<uint8_t>(std::clamp(color.g, 0.0f, 1.0f) * 255.0f);
    uint8_t b = static_cast<uint8_t>(std::clamp(color.b, 0.0f, 1.0f) * 255.0f);
    uint8_t a = static_cast<uint8_t>(std::clamp(color.a, 0.0f, 1.0f) * 255.0f);
    uint32_t pixel = (static_cast<uint32_t>(r) << 0) |
                     (static_cast<uint32_t>(g) << 8) |
                     (static_cast<uint32_t>(b) << 16) |
                     (static_cast<uint32_t>(a) << 24);
    std::fill(pixels_.begin(), pixels_.end(), pixel);
}

void RasterEngine::clear() {
    std::fill(pixels_.begin(), pixels_.end(), 0x00000000);
}

} // namespace fap
