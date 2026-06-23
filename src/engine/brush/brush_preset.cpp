#include "engine/brush/brush_engine.hpp"

namespace fap {

BrushPreset createRoundSoftPreset(float size) {
    BrushPreset p;
    p.name       = "Round Soft";
    p.mode       = BrushMode::Raster;
    p.size       = size;
    p.opacity    = 1.0f;
    p.flow       = 0.3f;
    p.blend_mode = BlendMode::Normal;

    p.tip.name      = "round_soft";
    p.tip.spacing   = 0.15f;
    p.tip.hardness  = 0.3f;
    p.tip.angle     = 0.0f;
    p.tip.roundness = 1.0f;

    p.dynamics.pressure_opacity = true;
    p.dynamics.pressure_size    = true;
    p.dynamics.pressure_flow    = true;
    p.dynamics.tilt_angle       = false;
    p.dynamics.min_size         = 0.1f;
    p.dynamics.max_size         = 2.0f;
    p.dynamics.min_opacity      = 0.1f;
    p.dynamics.max_opacity      = 1.0f;
    p.dynamics.min_flow         = 0.05f;
    p.dynamics.max_flow         = 0.3f;
    p.dynamics.smoothing        = 0.5f;

    return p;
}

BrushPreset createRoundHardPreset(float size) {
    BrushPreset p;
    p.name       = "Round Hard";
    p.mode       = BrushMode::Raster;
    p.size       = size;
    p.opacity    = 1.0f;
    p.flow       = 0.5f;
    p.blend_mode = BlendMode::Normal;

    p.tip.name      = "round_hard";
    p.tip.spacing   = 0.1f;
    p.tip.hardness  = 0.95f;
    p.tip.angle     = 0.0f;
    p.tip.roundness = 1.0f;

    p.dynamics.pressure_opacity = true;
    p.dynamics.pressure_size    = true;
    p.dynamics.pressure_flow    = false;
    p.dynamics.tilt_angle       = false;
    p.dynamics.min_size         = 0.2f;
    p.dynamics.max_size         = 2.0f;
    p.dynamics.min_opacity      = 0.3f;
    p.dynamics.max_opacity      = 1.0f;
    p.dynamics.min_flow         = 0.1f;
    p.dynamics.max_flow         = 0.5f;
    p.dynamics.smoothing        = 0.3f;

    return p;
}

BrushPreset createFlatPreset(float size, float angle) {
    BrushPreset p;
    p.name       = "Flat";
    p.mode       = BrushMode::Raster;
    p.size       = size;
    p.opacity    = 1.0f;
    p.flow       = 0.4f;
    p.blend_mode = BlendMode::Normal;

    p.tip.name      = "flat";
    p.tip.spacing   = 0.08f;
    p.tip.hardness  = 0.7f;
    p.tip.angle     = angle;
    p.tip.roundness = 0.3f;

    p.dynamics.pressure_opacity = true;
    p.dynamics.pressure_size    = true;
    p.dynamics.pressure_flow    = true;
    p.dynamics.tilt_angle       = true;
    p.dynamics.min_size         = 0.1f;
    p.dynamics.max_size         = 2.0f;
    p.dynamics.min_opacity      = 0.1f;
    p.dynamics.max_opacity      = 1.0f;
    p.dynamics.min_flow         = 0.05f;
    p.dynamics.max_flow         = 0.4f;
    p.dynamics.smoothing        = 0.4f;

    return p;
}

BrushPreset createCalligraphyPreset(float size, float angle) {
    BrushPreset p;
    p.name       = "Calligraphy";
    p.mode       = BrushMode::Raster;
    p.size       = size;
    p.opacity    = 1.0f;
    p.flow       = 0.25f;
    p.blend_mode = BlendMode::Normal;

    p.tip.name      = "calligraphy";
    p.tip.spacing   = 0.05f;
    p.tip.hardness  = 0.6f;
    p.tip.angle     = angle;
    p.tip.roundness = 0.15f;

    p.dynamics.pressure_opacity = true;
    p.dynamics.pressure_size    = true;
    p.dynamics.pressure_flow    = true;
    p.dynamics.tilt_angle       = true;
    p.dynamics.min_size         = 0.05f;
    p.dynamics.max_size         = 3.0f;
    p.dynamics.min_opacity      = 0.2f;
    p.dynamics.max_opacity      = 1.0f;
    p.dynamics.min_flow         = 0.03f;
    p.dynamics.max_flow         = 0.25f;
    p.dynamics.smoothing        = 0.6f;

    return p;
}

BrushPreset createEraserPreset(float size) {
    BrushPreset p;
    p.name       = "Eraser";
    p.mode       = BrushMode::Raster;
    p.size       = size;
    p.opacity    = 1.0f;
    p.flow       = 0.5f;
    p.blend_mode = BlendMode::Normal;
    p.color      = { 1.0f, 1.0f, 1.0f, 1.0f };

    p.tip.name      = "eraser";
    p.tip.spacing   = 0.1f;
    p.tip.hardness  = 0.9f;
    p.tip.angle     = 0.0f;
    p.tip.roundness = 1.0f;

    p.dynamics.pressure_opacity = true;
    p.dynamics.pressure_size    = true;
    p.dynamics.pressure_flow    = false;
    p.dynamics.tilt_angle       = false;
    p.dynamics.min_size         = 0.3f;
    p.dynamics.max_size         = 2.0f;
    p.dynamics.min_opacity      = 0.3f;
    p.dynamics.max_opacity      = 1.0f;
    p.dynamics.min_flow         = 0.1f;
    p.dynamics.max_flow         = 0.5f;
    p.dynamics.smoothing        = 0.2f;

    return p;
}

} // namespace fap
