#include "engine/compositor/compositor.hpp"
#include "engine/raster/raster_engine.hpp"
#include "engine/common/blend_utils.hpp"
#include <algorithm>

namespace fap {

namespace {

void compositeRasterLayer(const RasterLayer& raster, RasterEngine& target) {
    const uint32_t* src = raster.pixelData();
    size_t srcStride = static_cast<size_t>(raster.width());

    int ox = raster.originX();
    int oy = raster.originY();
    int srcW = raster.width();
    int srcH = raster.height();
    int tgtW = target.width();
    int tgtH = target.height();

    int startX = std::max(0, ox);
    int startY = std::max(0, oy);
    int endX = std::min(tgtW, ox + srcW);
    int endY = std::min(tgtH, oy + srcH);

    if (startX >= endX || startY >= endY) return;

    float layerOpacity = raster.opacity();
    BlendMode mode = raster.blendMode();

    uint32_t* dst = target.pixels();

    for (int ty = startY; ty < endY; ++ty) {
        int sy = ty - oy;
        size_t srcRowBase = static_cast<size_t>(sy) * srcStride;

        for (int tx = startX; tx < endX; ++tx) {
            int sx = tx - ox;
            uint32_t sp = src[srcRowBase + static_cast<size_t>(sx)];

            uint8_t sa = static_cast<uint8_t>((sp >> 24) & 0xFF);
            if (sa == 0) continue;

            float sb = static_cast<float>(sp & 0xFF) / 255.0f;
            float sg = static_cast<float>((sp >> 8) & 0xFF) / 255.0f;
            float sr = static_cast<float>((sp >> 16) & 0xFF) / 255.0f;
            float sAlpha = (static_cast<float>(sa) / 255.0f) * layerOpacity;

            blendPixel(dst, tgtW, tgtH, tx, ty, sr, sg, sb, sAlpha, mode);
        }
    }
}

void compositeLayer(const Layer* layer, RasterEngine& target) {
    if (!layer || !layer->visible()) return;

    if (layer->type() == LayerType::Raster) {
        compositeRasterLayer(*static_cast<const RasterLayer*>(layer), target);
    } else if (layer->type() == LayerType::Group) {
        const auto* group = static_cast<const GroupLayer*>(layer);
        for (auto it = group->layers().rbegin(); it != group->layers().rend(); ++it) {
            compositeLayer(it->get(), target);
        }
    }
}

} // anonymous namespace

void Compositor::composite(const GroupLayer& root, RasterEngine& target) {
    compositeLayer(&root, target);
}

} // namespace fap
