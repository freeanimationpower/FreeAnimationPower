#pragma once

#include "core/types.hpp"
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>

namespace fap {

struct BrushTip {
    std::string name;
    std::string imagePath;  // path to tip PNG
    float spacing = 0.15f;  // spacing multiplier
    float hardness = 0.8f;  // edge hardness
    float angle = 0.0f;     // default angle
    float roundness = 1.0f; // tip roundness (1.0 = circle)
};

struct BrushDynamics {
    bool pressure_opacity = true;
    bool pressure_size = true;
    bool pressure_flow = true;
    bool tilt_angle = false;
    float min_size = 0.1f;
    float max_size = 2.0f;
    float min_opacity = 0.1f;
    float max_opacity = 1.0f;
    float min_flow = 0.05f;
    float max_flow = 0.3f;
    float smoothing = 0.5f; // 0.0 = off, 1.0 = max
};

struct BrushPreset {
    std::string name;
    BrushMode mode = BrushMode::Raster;
    BrushTip tip;
    BrushDynamics dynamics;
    Color color = { 0.0f, 0.0f, 0.0f, 1.0f };
    float size = 10.0f;
    float opacity = 1.0f;
    float flow = 0.3f;
    BlendMode blend_mode = BlendMode::Normal;
    bool use_paper_texture = false;
    std::string paper_texture_path;
};

class BrushEngine {
public:
    BrushEngine();
    ~BrushEngine();

    void setPreset(const BrushPreset& preset);
    const BrushPreset& preset() const { return current_preset_; }

    void beginStroke(const Color& color, float size);
    void addPoint(const StrokePoint& point);
    void endStroke();

    const std::vector<StrokePoint>& currentStroke() const { return stroke_points_; }

    void loadPresets(const std::string& path);
    void savePresets(const std::string& path);
    const std::vector<BrushPreset>& presets() const { return presets_; }

private:
    BrushPreset current_preset_;
    std::vector<BrushPreset> presets_;
    std::vector<StrokePoint> stroke_points_;
    bool stroking_ = false;
};

} // namespace fap
