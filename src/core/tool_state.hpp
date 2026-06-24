#pragma once

#include <QObject>
#include <QColor>
#include <QString>
#include "types.hpp"

namespace fap {

class ToolState : public QObject {
    Q_OBJECT
    Q_PROPERTY(ToolType activeTool READ activeTool WRITE setActiveTool NOTIFY activeToolChanged)
    Q_PROPERTY(QColor primaryColor READ primaryColor WRITE setPrimaryColor NOTIFY primaryColorChanged)
    Q_PROPERTY(int brushSize READ brushSize WRITE setBrushSize NOTIFY brushSizeChanged)
    Q_PROPERTY(int brushOpacity READ brushOpacity WRITE setBrushOpacity NOTIFY brushOpacityChanged)
    Q_PROPERTY(int brushHardness READ brushHardness WRITE setBrushHardness NOTIFY brushHardnessChanged)
    Q_PROPERTY(QString brushShape READ brushShape WRITE setBrushShape NOTIFY brushShapeChanged)
    Q_PROPERTY(bool pressureSize READ pressureSize WRITE setPressureSize NOTIFY pressureSizeChanged)
    Q_PROPERTY(bool pressureOpacity READ pressureOpacity WRITE setPressureOpacity NOTIFY pressureOpacityChanged)
    Q_PROPERTY(int stabilizerLevel READ stabilizerLevel WRITE setStabilizerLevel NOTIFY stabilizerLevelChanged)
    Q_PROPERTY(int canvasWidth READ canvasWidth WRITE setCanvasWidth NOTIFY canvasWidthChanged)
    Q_PROPERTY(int canvasHeight READ canvasHeight WRITE setCanvasHeight NOTIFY canvasHeightChanged)
    Q_PROPERTY(bool onionEnabled READ onionEnabled WRITE setOnionEnabled NOTIFY onionEnabledChanged)
    Q_PROPERTY(int onionPrevFrames READ onionPrevFrames WRITE setOnionPrevFrames NOTIFY onionPrevFramesChanged)
    Q_PROPERTY(int onionNextFrames READ onionNextFrames WRITE setOnionNextFrames NOTIFY onionNextFramesChanged)
    Q_PROPERTY(int onionOpacity READ onionOpacity WRITE setOnionOpacity NOTIFY onionOpacityChanged)
    Q_PROPERTY(int fillType READ fillType WRITE setFillType NOTIFY fillTypeChanged)
    Q_PROPERTY(QColor sampledColor READ sampledColor WRITE setSampledColor NOTIFY sampledColorChanged)
    Q_PROPERTY(int colorVariationType READ colorVariationType WRITE setColorVariationType NOTIFY colorVariationTypeChanged)
    Q_PROPERTY(int colorVariationCount READ colorVariationCount WRITE setColorVariationCount NOTIFY colorVariationCountChanged)
    Q_PROPERTY(int lineStyle READ lineStyle WRITE setLineStyle NOTIFY lineStyleChanged)

public:
    explicit ToolState(QObject* parent = nullptr);

    ToolType activeTool() const;
    QColor primaryColor() const;
    int brushSize() const;
    int brushOpacity() const;
    int brushHardness() const;
    QString brushShape() const;
    bool pressureSize() const;
    bool pressureOpacity() const;
    int stabilizerLevel() const;
    int canvasWidth() const;
    int canvasHeight() const;
    bool onionEnabled() const;
    int onionPrevFrames() const;
    int onionNextFrames() const;
    int onionOpacity() const;
    int fillType() const;
    QColor sampledColor() const;
    int colorVariationType() const;
    int colorVariationCount() const;
    int lineStyle() const;

public slots:
    void setActiveTool(ToolType tool);
    void setActiveToolByIndex(int index);
    void setPrimaryColor(const QColor& color);
    void setBrushSize(int size);
    void setBrushOpacity(int opacity);
    void setBrushHardness(int hardness);
    void setBrushShape(const QString& shape);
    void setPressureSize(bool enabled);
    void setPressureOpacity(bool enabled);
    void setStabilizerLevel(int level);
    void setCanvasWidth(int w);
    void setCanvasHeight(int h);
    void setCanvasSize(int w, int h);
    void setOnionEnabled(bool enabled);
    void setOnionPrevFrames(int count);
    void setOnionNextFrames(int count);
    void setOnionOpacity(int opacity);
    void setFillType(int type);
    void setSampledColor(const QColor& color);
    void setColorVariationType(int type);
    void setColorVariationCount(int count);
    void setLineStyle(int style);

    void resetToDefaults();

signals:
    void activeToolChanged(ToolType tool);
    void primaryColorChanged(const QColor& color);
    void brushSizeChanged(int size);
    void brushOpacityChanged(int opacity);
    void brushHardnessChanged(int hardness);
    void brushShapeChanged(const QString& shape);
    void pressureSizeChanged(bool enabled);
    void pressureOpacityChanged(bool enabled);
    void stabilizerLevelChanged(int level);
    void canvasWidthChanged(int w);
    void canvasHeightChanged(int h);
    void canvasSizeChanged(int w, int h);
    void onionEnabledChanged(bool enabled);
    void onionPrevFramesChanged(int count);
    void onionNextFramesChanged(int count);
    void onionOpacityChanged(int opacity);
    void fillTypeChanged(int type);
    void sampledColorChanged(const QColor& color);
    void colorVariationTypeChanged(int type);
    void colorVariationCountChanged(int count);
    void lineStyleChanged(int style);
    void toolSettingsChanged();

private:
    ToolType active_tool_ = ToolType::Eraser;
    QColor primary_color_ = QColor(0, 0, 0);
    int brush_size_ = 20;
    int brush_opacity_ = 100;
    int brush_hardness_ = 80;
    QString brush_shape_ = "Round";
    bool pressure_size_ = false;
    bool pressure_opacity_ = false;
    int stabilizer_level_ = 0;
    int canvas_width_ = 1920;
    int canvas_height_ = 1080;
    bool onion_enabled_ = true;
    int onion_prev_frames_ = 3;
    int onion_next_frames_ = 1;
    int onion_opacity_ = 35;
    int fill_type_ = 0;
    QColor sampled_color_ = QColor(0, 0, 0);
    int color_variation_type_ = 0;
    int color_variation_count_ = 9;
    int line_style_ = 0;
};

} // namespace fap
