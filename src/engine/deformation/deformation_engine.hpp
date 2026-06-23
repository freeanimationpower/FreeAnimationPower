#pragma once

#include "deformation_mesh.hpp"
#include "core/layer.hpp"
#include <memory>
#include <vector>
#include <cstdint>
#include <functional>

namespace fap {

class DeformationEngine {
public:
    DeformationEngine();

    void setMesh(std::unique_ptr<DeformationMesh> mesh);
    DeformationMesh* mesh() { return mesh_.get(); }
    const DeformationMesh* mesh() const { return mesh_.get(); }

    void createDefaultMesh(const Rect& bounds, int cols = 4, int rows = 4);

    void deformRasterLayer(RasterLayer& layer) const;
    void deformVectorLayer(VectorLayer& layer) const;

private:
    std::unique_ptr<DeformationMesh> mesh_;
    void deformPixels(const uint32_t* src, int srcW, int srcH,
                      uint32_t* dst, int dstW, int dstH,
                      int originX, int originY) const;
};

} // namespace fap
