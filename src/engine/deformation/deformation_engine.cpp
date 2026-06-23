#include "deformation_engine.hpp"
#include "engine/vector/bezier_path.hpp"
#include <algorithm>
#include <cmath>
#include <vector>

namespace fap {

DeformationEngine::DeformationEngine() = default;

void DeformationEngine::setMesh(std::unique_ptr<DeformationMesh> mesh)
{
    mesh_ = std::move(mesh);
}

void DeformationEngine::createDefaultMesh(const Rect& bounds, int cols, int rows)
{
    auto m = std::make_unique<DeformationMesh>(cols, rows);
    m->setBounds(bounds);
    mesh_ = std::move(m);
}

void DeformationEngine::deformRasterLayer(RasterLayer& layer) const
{
    if (!mesh_) return;

    layer.ensureUnique();

    const int srcW = layer.width();
    const int srcH = layer.height();
    const int ox = layer.originX();
    const int oy = layer.originY();

    std::vector<uint32_t> dstPixels(srcW * srcH, 0);
    const uint32_t* src = layer.pixelData();

    deformPixels(src, srcW, srcH, dstPixels.data(), srcW, srcH, ox, oy);

    std::copy(dstPixels.begin(), dstPixels.end(), layer.pixelData());
    layer.bufferEpochTick();
}

void DeformationEngine::deformVectorLayer(VectorLayer& layer) const
{
    if (!mesh_) return;

    for (size_t i = 0; i < layer.strokeCount(); ++i) {
        auto& stroke = layer.strokeAt(i);
        for (auto& seg : stroke.path.segments()) {
            seg.p0 = mesh_->mapPoint(seg.p0);
            seg.p1 = mesh_->mapPoint(seg.p1);
            seg.p2 = mesh_->mapPoint(seg.p2);
            seg.p3 = mesh_->mapPoint(seg.p3);
        }
    }
}

void DeformationEngine::deformPixels(const uint32_t* src, int srcW, int srcH,
                                     uint32_t* dst, int dstW, int dstH,
                                     int originX, int originY) const
{
    for (int dy = 0; dy < dstH; ++dy) {
        for (int dx = 0; dx < dstW; ++dx) {
            float worldX = static_cast<float>(dx + originX);
            float worldY = static_cast<float>(dy + originY);

            Vec2 srcPos = mesh_->mapPoint({ worldX, worldY });

            int sx = static_cast<int>(std::round(srcPos.x - originX));
            int sy = static_cast<int>(std::round(srcPos.y - originY));

            if (sx >= 0 && sx < srcW && sy >= 0 && sy < srcH) {
                dst[dy * dstW + dx] = src[sy * srcW + sx];
            }
        }
    }
}

} // namespace fap
