#pragma once

#include "core/types.hpp"
#include <vector>
#include <cstdint>

namespace fap {

enum class DeformationMode {
    Bilinear,
    Perspective,
    Rigid
};

struct DeformationPoint {
    Vec2 position;
    Vec2 offset;
    bool locked = false;
};

class DeformationMesh {
public:
    DeformationMesh(int cols, int rows);

    int cols() const { return cols_; }
    int rows() const { return rows_; }
    int pointCount() const { return cols_ * rows_; }

    void setBounds(const Rect& bounds);
    const Rect& bounds() const { return bounds_; }

    DeformationPoint& pointAt(int col, int row);
    const DeformationPoint& pointAt(int col, int row) const;
    int pointIndex(int col, int row) const;

    void setPointOffset(int col, int row, const Vec2& offset);
    void lockPoint(int col, int row, bool locked);

    Vec2 originalPosition(int col, int row) const;
    Vec2 deformedPosition(int col, int row) const;

    Vec2 mapPoint(const Vec2& p) const;
    Vec2 mapPointBilinear(const Vec2& p) const;

    void reset();

private:
    int cols_;
    int rows_;
    Rect bounds_;
    std::vector<DeformationPoint> points_;
    void recalcOriginalPositions();
};

} // namespace fap
