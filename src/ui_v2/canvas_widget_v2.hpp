#pragma once

#include <QtWidgets/QWidget>
#include <QtGui/QColor>
#include <QtGui/QImage>
#include <QtCore/QPointF>
#include <QtCore/QRect>
#include <memory>
#include <vector>

#include "core/app_state.hpp"

namespace fap {

class RasterLayer;
class Layer;
class UndoCommand;

class CanvasWidgetV2 : public QWidget {
    Q_OBJECT
public:
    explicit CanvasWidgetV2(std::shared_ptr<AppState> state, QWidget* parent = nullptr);

    void setCurrentFrame(int frame);
    void setTotalFrames(int count);
    void setCurrentLayer(int index);
    void setTool(int toolIndex);
    void setColor(const QColor& color);

    void setBrushSize(int size);
    void setBrushShape(const QString& shape);
    void setOnionEnabled(bool enabled);
    void setOnionSettings(int prevFrames, int nextFrames, int opacity);

    void fit();
    void flipH();
    void rotateCanvas();
    void toggleGrid();

    void resizeCanvas(int w, int h);
    void shiftFrameData(int fromFrame, int delta);
    void removeFrameData(int frameIdx);

    void render(QPainter* painter);

    void undo();
    void redo();

signals:
    void canvasUpdated();
    void frameChanged(int frame);
    void colorPicked(const QColor& color);
    void statusMessage(const QString& msg);
    void toolChangedByKey(int tool);
    void togglePlayPause();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    std::shared_ptr<AppState> appState_;

    int currentFrame_ = 0;
    int totalFrames_ = 24;
    int currentLayerIndex_ = 0;

    bool onionEnabled_ = true;
    int onionPrevFrames_ = 3;
    int onionNextFrames_ = 1;
    int onionOpacity_ = 35;

    bool showGrid_ = false;

    float zoom_ = 1.0f;
    float offsetX_ = 0.0f;
    float offsetY_ = 0.0f;
    float rotationAngle_ = 0.0f;
    bool flippedH_ = false;

    bool drawing_ = false;
    QPointF lastMousePos_;
    QPointF virtualCursorPos_;
    QPointF prevVirtualCursorPos_;
    QRect activeDirtyRect_;
    QImage beforeSnapshot_;
    QColor brushColor_ = Qt::black;
    int brushSize_ = 20;
    QString brushShape_ = "Round";
    float strokeDistance_ = 0.0f;

    void drawBrushStamp(int cx, int cy);
    void drawLineStamps(const QPointF& from, const QPointF& to);
    QRect stampRect(int cx, int cy) const;
    void commitStroke();
    QImage captureRect(const QRect& r);
    void growBeforeSnapshot(const QRect& oldRect, const QRect& newRect);
    QPointF widgetToCanvas(const QPointF& pos) const;
    QPointF canvasToWidget(const QPointF& pos) const;

    RasterLayer* activeRasterLayer();
};

} // namespace fap
