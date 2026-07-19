#include "raster_effect.hpp"
#include <algorithm>
#include <cstdlib>
#include <vector>

namespace fap {

static inline uint32_t hash31(int32_t x, int32_t y, int32_t f, int32_t s) {
    uint32_t h = static_cast<uint32_t>(x) * 374761393U
               + static_cast<uint32_t>(y) * 668265263U
               + static_cast<uint32_t>(f) * 1274126177U
               + static_cast<uint32_t>(s) * 3443311159U;
    h = (h ^ (h >> 13)) * 1274126177U;
    h = h ^ (h >> 16);
    return h;
}

void applyLineBoil(QImage& image, int frame, float strength, int seed)
{
    if (strength <= 0.0f || image.isNull()) return;

    const int w = image.width();
    const int h = image.height();
    const int cellSize = 8;
    const float halfCell = static_cast<float>(cellSize) * 0.5f;

    QImage src = image.copy();
    const uint8_t* srcBits = src.constBits();
    uint8_t* dstBits = image.bits();
    const int stride = static_cast<int>(sizeof(uint32_t)) * w;

    for (int y = 0; y < h; ++y) {
        uint32_t* dstRow = reinterpret_cast<uint32_t*>(dstBits + y * stride);
        int cy0 = y / cellSize;
        int cy1 = cy0 + 1;
        float fy = (static_cast<float>(y % cellSize) - halfCell) / static_cast<float>(cellSize);

        uint32_t h00 = hash31(cy0, cy0, frame, seed);
        uint32_t h10 = hash31(cy1, cy0, frame, seed);
        uint32_t h01 = hash31(cy0, cy1, frame, seed);
        uint32_t h11 = hash31(cy1, cy1, frame, seed);

        for (int x = 0; x < w; ++x) {
            int cx0 = x / cellSize;
            int cx1 = cx0 + 1;
            float fx = (static_cast<float>(x % cellSize) - halfCell) / static_cast<float>(cellSize);

            uint32_t hn00 = hash31(cx0, cy0, frame, seed);
            float dx00 = (static_cast<float>(hn00 & 0xFF) / 255.0f - 0.5f) * strength * 2.0f;
            float dy00 = (static_cast<float>((hn00 >> 8) & 0xFF) / 255.0f - 0.5f) * strength * 2.0f;

            uint32_t hn10 = hash31(cx1, cy0, frame, seed);
            float dx10 = (static_cast<float>(hn10 & 0xFF) / 255.0f - 0.5f) * strength * 2.0f;
            float dy10 = (static_cast<float>((hn10 >> 8) & 0xFF) / 255.0f - 0.5f) * strength * 2.0f;

            uint32_t hn01 = hash31(cx0, cy1, frame, seed);
            float dx01 = (static_cast<float>(hn01 & 0xFF) / 255.0f - 0.5f) * strength * 2.0f;
            float dy01 = (static_cast<float>((hn01 >> 8) & 0xFF) / 255.0f - 0.5f) * strength * 2.0f;

            uint32_t hn11 = hash31(cx1, cy1, frame, seed);
            float dx11 = (static_cast<float>(hn11 & 0xFF) / 255.0f - 0.5f) * strength * 2.0f;
            float dy11 = (static_cast<float>((hn11 >> 8) & 0xFF) / 255.0f - 0.5f) * strength * 2.0f;

            float tdx = dx00 * (1.0f - fx) * (1.0f - fy)
                      + dx10 * fx * (1.0f - fy)
                      + dx01 * (1.0f - fx) * fy
                      + dx11 * fx * fy;
            float tdy = dy00 * (1.0f - fx) * (1.0f - fy)
                      + dy10 * fx * (1.0f - fy)
                      + dy01 * (1.0f - fx) * fy
                      + dy11 * fx * fy;

            int sx = std::clamp(static_cast<int>(static_cast<float>(x) + tdx), 0, w - 1);
            int sy = std::clamp(static_cast<int>(static_cast<float>(y) + tdy), 0, h - 1);

            const uint32_t* srcRow = reinterpret_cast<const uint32_t*>(srcBits + sy * stride);
            dstRow[x] = srcRow[sx];
        }
    }
}

} // namespace fap
