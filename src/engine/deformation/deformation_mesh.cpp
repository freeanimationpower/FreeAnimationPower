#include "deformation_mesh.hpp"
#include <algorithm>
#include <cmath>

namespace fap {

DeformationMesh::DeformationMesh(int cols, int rows)
    : cols_(std::max(2, cols))
    , rows_(std::max(2, rows))
{
    points_.resize(cols_ * rows_);
    recalcOriginalPositions();
}

void DeformationMesh::setBounds(const Rect& bounds)
{
    bounds_ = bounds;
    recalcOriginalPositions();
}

DeformationPoint& DeformationMesh::pointAt(int col, int row)
{
    return points_[pointIndex(col, row)];
}

const DeformationPoint& DeformationMesh::pointAt(int col, int row) const
{
    return points_[pointIndex(col, row)];
}

int DeformationMesh::pointIndex(int col, int row) const
{
    return row * cols_ + col;
}

void DeformationMesh::setPointOffset(int col, int row, const Vec2& offset)
{
    auto& pt = pointAt(col, row);
    if (!pt.locked) {
        pt.offset = offset;
    }
}

void DeformationMesh::lockPoint(int col, int row, bool locked)
{
    pointAt(col, row).locked = locked;
}

Vec2 DeformationMesh::originalPosition(int col, int row) const
{
    return pointAt(col, row).position;
}

Vec2 DeformationMesh::deformedPosition(int col, int row) const
{
    const auto& pt = pointAt(col, row);
    return { pt.position.x + pt.offset.x, pt.position.y + pt.offset.y };
}

Vec2 DeformationMesh::mapPoint(const Vec2& p) const
{
    return mapPointBilinear(p);
}

Vec2 DeformationMesh::mapPointBilinear(const Vec2& p) const
{
    float u = (p.x - bounds_.x) / bounds_.width;
    float v = (p.y - bounds_.y) / bounds_.height;
    u = std::clamp(u, 0.0f, 1.0f);
    v = std::clamp(v, 0.0f, 1.0f);

    float colF = u * (cols_ - 1);
    float rowF = v * (rows_ - 1);
    int col0 = std::clamp(static_cast<int>(colF), 0, cols_ - 1);
    int row0 = std::clamp(static_cast<int>(rowF), 0, rows_ - 1);
    int col1 = std::min(col0 + 1, cols_ - 1);
    int row1 = std::min(row0 + 1, rows_ - 1);
    float fx = colF - col0;
    float fy = rowF - row0;

    auto p00 = deformedPosition(col0, row0);
    auto p10 = deformedPosition(col1, row0);
    auto p01 = deformedPosition(col0, row1);
    auto p11 = deformedPosition(col1, row1);

    float x = p00.x * (1 - fx) * (1 - fy) + p10.x * fx * (1 - fy)
            + p01.x * (1 - fx) * fy + p11.x * fx * fy;
    float y = p00.y * (1 - fx) * (1 - fy) + p10.y * fx * (1 - fy)
            + p01.y * (1 - fx) * fy + p11.y * fx * fy;

    return { x, y };
}

void DeformationMesh::reset()
{
    for (auto& pt : points_) {
        pt.offset = { 0.0f, 0.0f };
    }
    recalcOriginalPositions();
}

void DeformationMesh::recalcOriginalPositions()
{
    for (int row = 0; row < rows_; ++row) {
        for (int col = 0; col < cols_; ++col) {
            float u = (cols_ > 1) ? static_cast<float>(col) / (cols_ - 1) : 0.5f;
            float v = (rows_ > 1) ? static_cast<float>(row) / (rows_ - 1) : 0.5f;
            auto& pt = pointAt(col, row);
            pt.position = {
                bounds_.x + u * bounds_.width,
                bounds_.y + v * bounds_.height
            };
        }
    }
}

} // namespace fap
