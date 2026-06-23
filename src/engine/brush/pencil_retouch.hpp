#pragma once

#include "core/types.hpp"
#include "core/layer.hpp"
#include <vector>
#include <cstdint>
#include <functional>

namespace fap {

struct PencilLine {
    std::vector<Vec2> points;
    float thickness = 3.0f;
    float opacity = 1.0f;
    Color color{0, 0, 0, 1};
};

struct RetouchBrush {
    float size = 10.0f;
    float opacity = 1.0f;
    float hardness = 0.5f;
    Color color{0, 0, 0, 1};
    bool affect_opacity = true;
    bool affect_thickness = true;
};

enum class RetouchMode {
    AdjustThickness,
    AdjustOpacity,
    SmoothLines,
    RepaintColor
};

class PencilRetouchEngine {
public:
    PencilRetouchEngine();

    void setMode(RetouchMode mode) { mode_ = mode; }
    RetouchMode mode() const { return mode_; }

    void setBrush(const RetouchBrush& brush) { brush_ = brush; }
    const RetouchBrush& brush() const { return brush_; }

    void setStrength(float s) { strength_ = std::clamp(s, 0.0f, 1.0f); }
    float strength() const { return strength_; }

    void retouchAt(RasterLayer& layer,
                   const Vec2& pos,
                   float pressure = 1.0f) const;

private:
    RetouchMode mode_ = RetouchMode::AdjustThickness;
    RetouchBrush brush_;
    float strength_ = 0.5f;

    float calcBrushAlpha(float dx, float dy, float brushSize) const;
    void adjustPixelThickness(uint32_t& pixel, float intensity) const;
    void adjustPixelOpacity(uint32_t& pixel, float intensity) const;
    void smoothPixel(RasterLayer& layer, int x, int y) const;
    void repaintPixel(uint32_t& pixel, float intensity) const;
};

} // namespace fap
