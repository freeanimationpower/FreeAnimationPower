#include "core/tool_state.hpp"
#include <algorithm>

namespace fap {

ToolState::ToolState(QObject* parent)
    : QObject(parent) {
}

ToolType ToolState::activeTool() const { return active_tool_; }
QColor ToolState::primaryColor() const { return primary_color_; }
int ToolState::brushSize() const { return brush_size_; }
int ToolState::brushOpacity() const { return brush_opacity_; }
int ToolState::brushHardness() const { return brush_hardness_; }
QString ToolState::brushShape() const { return brush_shape_; }
bool ToolState::pressureSize() const { return pressure_size_; }
bool ToolState::pressureOpacity() const { return pressure_opacity_; }
int ToolState::stabilizerLevel() const { return stabilizer_level_; }
int ToolState::canvasWidth() const { return canvas_width_; }
int ToolState::canvasHeight() const { return canvas_height_; }
bool ToolState::onionEnabled() const { return onion_enabled_; }
int ToolState::onionPrevFrames() const { return onion_prev_frames_; }
int ToolState::onionNextFrames() const { return onion_next_frames_; }
int ToolState::onionOpacity() const { return onion_opacity_; }
int ToolState::fillType() const { return fill_type_; }
QColor ToolState::sampledColor() const { return sampled_color_; }
int ToolState::colorVariationType() const { return color_variation_type_; }
int ToolState::colorVariationCount() const { return color_variation_count_; }
int ToolState::lineStyle() const { return line_style_; }
QString ToolState::textString() const { return text_string_; }
QFont ToolState::textFont() const { return text_font_; }
int ToolState::textLeading() const { return text_leading_; }
int ToolState::textTracking() const { return text_tracking_; }
int ToolState::textAlignment() const { return text_alignment_; }
bool ToolState::textAntiAliasing() const { return text_antialiasing_; }
bool ToolState::textUnderline() const { return text_underline_; }
bool ToolState::textStrikethrough() const { return text_strikethrough_; }

void ToolState::setActiveTool(ToolType tool) {
    if (active_tool_ != tool) {
        active_tool_ = tool;
        emit activeToolChanged(tool);
        emit toolSettingsChanged();
    }
}

void ToolState::setActiveToolByIndex(int index) {
    switch (index) {
    case 0:  setActiveTool(ToolType::Brush); break;
    case 1:  setActiveTool(ToolType::Eraser); break;
    case 2:  setActiveTool(ToolType::ColorPicker); break;
    case 3:  setActiveTool(ToolType::Fill); break;
    case 4:  setActiveTool(ToolType::Text); break;
    case 5:  setActiveTool(ToolType::Line); break;
    case 6:  setActiveTool(ToolType::Rectangle); break;
    case 7:  setActiveTool(ToolType::Ellipse); break;
    case 8:  setActiveTool(ToolType::Move); break;
    case 9:  setActiveTool(ToolType::Select); break;
    case 10: setActiveTool(ToolType::Hand); break;
    default: break;
    }
}

void ToolState::setPrimaryColor(const QColor& color) {
    if (primary_color_ != color) {
        primary_color_ = color;
        emit primaryColorChanged(color);
        emit toolSettingsChanged();
    }
}

void ToolState::setBrushSize(int size) {
    int clamped = std::clamp(size, 1, 200);
    if (brush_size_ != clamped) {
        brush_size_ = clamped;
        emit brushSizeChanged(clamped);
        emit toolSettingsChanged();
    }
}

void ToolState::setBrushOpacity(int opacity) {
    int clamped = std::clamp(opacity, 1, 100);
    if (brush_opacity_ != clamped) {
        brush_opacity_ = clamped;
        emit brushOpacityChanged(clamped);
        emit toolSettingsChanged();
    }
}

void ToolState::setBrushHardness(int hardness) {
    int clamped = std::clamp(hardness, 1, 100);
    if (brush_hardness_ != clamped) {
        brush_hardness_ = clamped;
        emit brushHardnessChanged(clamped);
        emit toolSettingsChanged();
    }
}

void ToolState::setBrushShape(const QString& shape) {
    if (brush_shape_ != shape) {
        brush_shape_ = shape;
        emit brushShapeChanged(shape);
        emit toolSettingsChanged();
    }
}

void ToolState::setPressureSize(bool enabled) {
    if (pressure_size_ != enabled) {
        pressure_size_ = enabled;
        emit pressureSizeChanged(enabled);
        emit toolSettingsChanged();
    }
}

void ToolState::setPressureOpacity(bool enabled) {
    if (pressure_opacity_ != enabled) {
        pressure_opacity_ = enabled;
        emit pressureOpacityChanged(enabled);
        emit toolSettingsChanged();
    }
}

void ToolState::setStabilizerLevel(int level) {
    int clamped = std::clamp(level, 0, 50);
    if (stabilizer_level_ != clamped) {
        stabilizer_level_ = clamped;
        emit stabilizerLevelChanged(clamped);
        emit toolSettingsChanged();
    }
}

void ToolState::setCanvasWidth(int w) {
    int clamped = std::max(1, w);
    if (canvas_width_ != clamped) {
        canvas_width_ = clamped;
        emit canvasWidthChanged(clamped);
        emit canvasSizeChanged(canvas_width_, canvas_height_);
        emit toolSettingsChanged();
    }
}

void ToolState::setCanvasHeight(int h) {
    int clamped = std::max(1, h);
    if (canvas_height_ != clamped) {
        canvas_height_ = clamped;
        emit canvasHeightChanged(clamped);
        emit canvasSizeChanged(canvas_width_, canvas_height_);
        emit toolSettingsChanged();
    }
}

void ToolState::setCanvasSize(int w, int h) {
    bool wChanged = false, hChanged = false;
    int cw = std::max(1, w);
    int ch = std::max(1, h);
    if (canvas_width_ != cw) { canvas_width_ = cw; wChanged = true; }
    if (canvas_height_ != ch) { canvas_height_ = ch; hChanged = true; }
    if (wChanged) emit canvasWidthChanged(cw);
    if (hChanged) emit canvasHeightChanged(ch);
    if (wChanged || hChanged) {
        emit canvasSizeChanged(canvas_width_, canvas_height_);
        emit toolSettingsChanged();
    }
}

void ToolState::resetToDefaults() {
    setActiveTool(ToolType::Eraser);
    setPrimaryColor(QColor(0, 0, 0));
    setBrushSize(20);
    setBrushOpacity(100);
    setBrushHardness(80);
    setBrushShape("Round");
    setPressureSize(false);
    setPressureOpacity(false);
    setStabilizerLevel(0);
    setOnionEnabled(true);
    setOnionPrevFrames(3);
    setOnionNextFrames(1);
    setOnionOpacity(35);
    setFillType(0);
    setSampledColor(QColor(0, 0, 0));
    setColorVariationType(0);
    setColorVariationCount(9);
    setLineStyle(0);
    setTextString(QString());
    QFont defaultFont("Arial", 24);
    setTextFont(defaultFont);
    setTextLeading(0);
    setTextTracking(0);
    setTextAlignment(0);
    setTextAntiAliasing(true);
    setTextUnderline(false);
    setTextStrikethrough(false);
}

void ToolState::setOnionEnabled(bool enabled) {
    if (onion_enabled_ != enabled) {
        onion_enabled_ = enabled;
        emit onionEnabledChanged(enabled);
    }
}

void ToolState::setOnionPrevFrames(int count) {
    int clamped = std::clamp(count, 0, 10);
    if (onion_prev_frames_ != clamped) {
        onion_prev_frames_ = clamped;
        emit onionPrevFramesChanged(clamped);
    }
}

void ToolState::setOnionNextFrames(int count) {
    int clamped = std::clamp(count, 0, 10);
    if (onion_next_frames_ != clamped) {
        onion_next_frames_ = clamped;
        emit onionNextFramesChanged(clamped);
    }
}

void ToolState::setOnionOpacity(int opacity) {
    int clamped = std::clamp(opacity, 5, 100);
    if (onion_opacity_ != clamped) {
        onion_opacity_ = clamped;
        emit onionOpacityChanged(clamped);
    }
}

void ToolState::setFillType(int type) {
    int clamped = std::clamp(type, 0, 2);
    if (fill_type_ != clamped) {
        fill_type_ = clamped;
        emit fillTypeChanged(clamped);
        emit toolSettingsChanged();
    }
}

void ToolState::setSampledColor(const QColor& color) {
    if (sampled_color_ != color) {
        sampled_color_ = color;
        emit sampledColorChanged(color);
        emit toolSettingsChanged();
    }
}

void ToolState::setColorVariationType(int type) {
    int clamped = std::clamp(type, 0, 2);
    if (color_variation_type_ != clamped) {
        color_variation_type_ = clamped;
        emit colorVariationTypeChanged(clamped);
        emit toolSettingsChanged();
    }
}

void ToolState::setColorVariationCount(int count) {
    int clamped = std::clamp(count, 3, 25);
    if (color_variation_count_ != clamped) {
        color_variation_count_ = clamped;
        emit colorVariationCountChanged(clamped);
        emit toolSettingsChanged();
    }
}

void ToolState::setLineStyle(int style) {
    int clamped = std::clamp(style, 0, 3);
    if (line_style_ != clamped) {
        line_style_ = clamped;
        emit lineStyleChanged(clamped);
        emit toolSettingsChanged();
    }
}

void ToolState::setTextString(const QString& text) {
    if (text_string_ != text) {
        text_string_ = text;
        emit textStringChanged(text);
        emit toolSettingsChanged();
    }
}

void ToolState::setTextFont(const QFont& font) {
    if (text_font_ != font) {
        text_font_ = font;
        emit textFontChanged(font);
        emit toolSettingsChanged();
    }
}

void ToolState::setTextLeading(int leading) {
    if (text_leading_ != leading) {
        text_leading_ = leading;
        emit textLeadingChanged(leading);
        emit toolSettingsChanged();
    }
}

void ToolState::setTextTracking(int tracking) {
    if (text_tracking_ != tracking) {
        text_tracking_ = tracking;
        emit textTrackingChanged(tracking);
        emit toolSettingsChanged();
    }
}

void ToolState::setTextAlignment(int alignment) {
    int clamped = std::clamp(alignment, 0, 2);
    if (text_alignment_ != clamped) {
        text_alignment_ = clamped;
        emit textAlignmentChanged(clamped);
        emit toolSettingsChanged();
    }
}

void ToolState::setTextAntiAliasing(bool enabled) {
    if (text_antialiasing_ != enabled) {
        text_antialiasing_ = enabled;
        emit textAntiAliasingChanged(enabled);
        emit toolSettingsChanged();
    }
}

void ToolState::setTextUnderline(bool enabled) {
    if (text_underline_ != enabled) {
        text_underline_ = enabled;
        emit textUnderlineChanged(enabled);
        emit toolSettingsChanged();
    }
}

void ToolState::setTextStrikethrough(bool enabled) {
    if (text_strikethrough_ != enabled) {
        text_strikethrough_ = enabled;
        emit textStrikethroughChanged(enabled);
        emit toolSettingsChanged();
    }
}

} // namespace fap
