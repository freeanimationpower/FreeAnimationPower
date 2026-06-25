#pragma once

#include <QWidget>
#include <QPainter>
#include <QPainterPath>
#include <QImage>
#include <QColor>
#include <QPointF>
#include <QFont>
#include <QUndoStack>
#include <QElapsedTimer>
#include <vector>
#include <map>
#include <utility>

#include "engine/vector/bezier_path.hpp"
#include "core/layer.hpp"

namespace fap {

class Document;
class Layer;
class RasterLayer;
class BrushEngine;
struct BrushPreset;

struct RawStroke {
    LayerUid layerUid = 0;
    std::vector<StrokePoint> points;
    QColor color;
    float baseWidth = 10.0f;
    float opacity = 1.0f;
    bool pressureSize = true;
    bool pressureOpacity = false;
    bool eraser = false;
};

class CanvasWidget : public QWidget {
    Q_OBJECT
public:
    CanvasWidget(Document* doc, BrushEngine* brush, QWidget* parent = nullptr);
    void setDocumentAndBrush(Document* doc, BrushEngine* brush) { doc_ = doc; brush_ = brush; currentLayerIdx_ = 0; currentLayerUid_ = 0; vectorStrokes_.clear(); undoStack_.clear(); }
    void setLayerPanel(class LayerPanel* panel) { layerPanel_ = panel; }
    void clearCanvasCache() { vectorStrokes_.clear(); undoStack_.clear(); }
    void resizeCanvas(int newW, int newH);
    void focusCanvas() { setFocus(); }

    void fit();
    void setTool(int tool);
    void setColor(const QColor& c);
    double zoom() const { return zoom_; }
    void setZoom(double z) { zoom_ = std::clamp(z, 0.02, 64.0); update(); }
    int currentFrame() const { return currentFrame_; }
    void setCurrentFrame(int f) { currentFrame_ = f; update(); }
    int totalFrames() const { return totalFrames_; }
    void setTotalFrames(int f) { totalFrames_ = f; }
    void setCurrentLayer(int idx) { currentLayerIdx_ = idx; currentLayerUid_ = 0; }
    void setCurrentLayerByUid(LayerUid uid) { currentLayerUid_ = uid; currentLayerIdx_ = -1; }
    void setOnionSettings(int prev, int next, float opacity) { onionPrev_ = prev; onionNext_ = next; onionOpacity_ = opacity; }
    int onionPrev() const { return onionPrev_; }
    int onionNext() const { return onionNext_; }
    float onionOpacity() const { return onionOpacity_; }
    void setShowGrid(bool s, int size = 64) { showGrid_ = s; gridSize_ = size; update(); }
    void toggleGrid() { showGrid_ = !showGrid_; if (showGrid_ && gridSize_ == 0) gridSize_ = 64; update(); }
    void toggleOnionSkin() { onionEnabled_ = !onionEnabled_; update(); }
    void setOnionEnabled(bool e) { onionEnabled_ = e; update(); }
    void setOnionRange(int range) { onionPrev_ = range; onionNext_ = range; update(); }
    void flipH() { flipH_ = !flipH_; update(); }
    void rotateCanvas() { rotation_ = (rotation_ + 15) % 360; update(); }
    void setPlaying(bool p) { playing_ = p; }
    bool playing() const { return playing_; }
    QImage compositeFrame(int frameNum) const;
    QPointF screenToCanvas(QPointF sp) const;
    QPointF canvasToScreen(QPointF cp) const;
    void writeLayerPixels(Layer* layer, const QImage& img);
    QImage readLayerPixels(Layer* layer);
    void shiftFrameData(int fromFrame, int delta);
    void removeFrameData(int frameIdx);
    void undo();
    void redo();
    void purgeMemory();
    void pushUndo(QUndoCommand* cmd);
    void pushFullLayerUndo(Layer* layer, const QImage& beforeSnap, const QString& desc);
    QUndoStack* undoStack() { return &undoStack_; }
    const QUndoStack* undoStack() const { return &undoStack_; }
    void setSelectionRect(const QRectF& r) { selRect_ = r; }
    GroupLayer& currentRootLayer() const;
    GroupLayer& currentRootLayer(int frame) const;
    std::map<int, std::vector<RawStroke>>& vectorStrokes() { return vectorStrokes_; }
    const std::map<int, std::vector<RawStroke>>& vectorStrokes() const { return vectorStrokes_; }
    void copySelection();
    void cutSelection();
    void pasteClipboard();
    void deleteSelection();

signals:
    void canvasUpdated();
    void colorPicked(QColor color);
    void colorCommittedToRecent(QColor color);
    void togglePlayPause();
    void toolChangedByKey(int tool);
    void statusMessage(const QString& msg);

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void showEvent(QShowEvent*) override;
    void tabletEvent(QTabletEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void keyPressEvent(QKeyEvent*) override;

private:
    void paintCheckerboard(QPainter& p, const QRectF& rect);
    void paintGrid(QPainter& p, const QRectF& rect);
    void paintOnionSkin(QPainter& p);
    void renderTintedFrame(QPainter& p, int frameIdx, QColor tint);
    void paintLiveStroke(QPainter& p);
    void paintShapePreview(QPainter& p);
    void paintInfoOverlay(QPainter& p);
    void inputPress(QPointF screenPos);
    void inputMove(QPointF screenPos);
    void inputRelease();
    void commitStroke();
    void drawShape();
    void doFill(QPointF cpos);
    void doPickColor(QPointF cpos);
    void doText(QPointF cpos);
    void doErase();
    void startMove(QPointF cpos);
    void updateMove(QPointF cpos);
    void commitMove();
    void commitSelection();
    void commitFloatingSelection();
    void commitTransform();
    void ensureTransformBounds();
    void dumpState(const QString& contextAction) const;
    void pushUndo(Layer* layer, const QImage& after);
    void floodFill(QImage& img, int x, int y, const QColor& fillColor, const QColor& target, int tolerance);
    void floodFillByAlpha(QImage& img, int x, int y, const QColor& fillColor, int boundaryAlpha);
    QImage snapCurrentLayer();
    void clearCurrentFrame();

    Document* doc_ = nullptr;
    BrushEngine* brush_ = nullptr;
    class LayerPanel* layerPanel_ = nullptr;

    QFont font_;

    double zoom_ = 1.0;
    double offX_ = 0, offY_ = 0;
    int rotation_ = 0;
    bool flipH_ = false;
    bool showGrid_ = false;
    int gridSize_ = 64;
    QColor bgColor_{30, 30, 30};
    int tool_ = 0;
    QColor color_{0, 0, 0, 255};
    int currentFrame_ = 0;
    int totalFrames_ = 1;
    int currentLayerIdx_ = 0;
    LayerUid currentLayerUid_ = 0;
    int onionPrev_ = 3, onionNext_ = 1;
    float onionOpacity_ = 0.35f;
    bool onionEnabled_ = true;
    bool playing_ = false;
    QPointF cursorPos_;
    bool drawing_ = false;
    bool panning_ = false;

    enum class SelectionState : uint8_t { None, Creating, MovingContent, DraggingHandle };
    SelectionState selState_ = SelectionState::None;
    int selHandleIdx_ = -1;
    QPointF selDragStart_;

    bool floatingActive_ = false;
    QImage floatingSelection_;
    QImage originalFloatingSelection_;
    QElapsedTimer marchingTimer_;
    bool moving_ = false;

    bool transformActive_ = false;
    int activeHandle_ = -1;
    qreal scaleX_ = 1.0, scaleY_ = 1.0;
    QPointF translate_;
    QRectF transformBounds_;
    QPointF transformDragOrigin_;
    qreal dragScaleRefX_ = 1.0, dragScaleRefY_ = 1.0;
    QPointF dragTranslateRef_;
    QPointF handleFixedPoint_;
    bool handleScaleX_ = false, handleScaleY_ = false;

    QPointF panStart_, panOffStart_;
    QPointF selStart_, moveStart_;
    QRectF selRect_;
    QImage selImage_;
    QPointF shapeStart_{-1, -1}, shapeEnd_{-1, -1};
    bool tabletActive_ = false;
    bool tabletEraser_ = false;
    float tabletPressure_ = 1.0f;
    std::vector<StrokePoint> strokePoints_;
    std::vector<QPointF> stabilizerBuffer_;
    QImage beforeImage_;
    Layer* moveLayer_ = nullptr;
    QImage moveImage_;
    QPointF moveOffset_;
    QRectF moveSrcRect_;
    bool moveModeSelection_ = false;

    QUndoStack undoStack_;

    QImage wrapRasterLayer(RasterLayer* rasterLayer);
    QImage readRasterRect(RasterLayer* raster, const QRect& rect) const;
    void writeRasterRect(RasterLayer* raster, const QRect& rect, const QImage& pixels);
    Layer* resolveCurrentLayer() const;
    QImage buildPaddedImage(const QImage& src) const;

    std::map<int, std::vector<RawStroke>> vectorStrokes_;
    std::map<LayerUid, QImage> paddedLayerCaches_;
    std::map<LayerUid, uint64_t> paddedCacheEpochs_;
    void flattenVectorStrokes(int frame);
    void renderVectorStrokes(QPainter& p, int frame, LayerUid filterUid = 0);
    void renderVectorStroke(QPainter& p, const RawStroke& vs);

    std::map<LayerUid, uint64_t> rasterEpochs_;  // TODO: make private with accessor later
};

} // namespace fap
