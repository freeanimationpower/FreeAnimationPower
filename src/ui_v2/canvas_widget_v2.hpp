#pragma once

#include <QtWidgets/QWidget>
#include <QtGui/QColor>
#include <QtGui/QImage>
#include <QtGui/QFont>
#include <QtCore/QPointF>
#include <QtCore/QRect>
#include <QtCore/QElapsedTimer>
#include <memory>
#include <vector>
#include <map>

#include "core/app_state.hpp"

namespace fap {

struct TextEntry {
    QPointF pos;
    QString text;
    QFont font;
    QColor color;
    QImage undoImage;
    QRect undoRect;

    QRectF boundingRect() const {
        QFontMetrics fm(font);
        return QRectF(
            pos.x(),
            pos.y() - static_cast<double>(fm.ascent()),
            static_cast<double>(fm.horizontalAdvance(text)),
            static_cast<double>(fm.ascent() + fm.descent())
        );
    }
};

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
    void rotateCanvasMinus();
    void toggleGrid();

    float viewZoom() const { return zoom_; }
    float viewOffsetX() const { return offsetX_; }
    float viewOffsetY() const { return offsetY_; }
    void setViewState(float zoom, float offsetX, float offsetY) {
        zoom_ = zoom; offsetX_ = offsetX; offsetY_ = offsetY; update();
    }

    void resizeCanvas(int w, int h);
    void shiftFrameData(int fromFrame, int delta);
    void removeFrameData(int frameIdx);

    void render(QPainter* painter);

    void undo();
    void redo();
    void copySelection();
    void cutSelection();
    void pasteClipboard();
    void deleteSelection();
    void resetView();

    void invalidateBackgroundCache();
    void resetState();
    bool isSequenceLocked() const;

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
    void focusOutEvent(QFocusEvent* event) override;

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

    // Middle-button panning (works regardless of active tool)
    bool panning_ = false;

    // Stroke state (DATA domain)
    bool drawing_ = false;
    QPointF lastMousePos_;
    QPointF virtualCursorPos_;
    QPointF prevVirtualCursorPos_;

    // Isolated stroke buffer: transparent ARGB32_Premultiplied.
    // Only contains dabs from the current stroke. Written during
    // mouseMove, composited onto pixelBuffer_ on commit, discarded on cancel.
    QImage strokeBuffer_;

    // Accumulated dirty region of strokeBuffer_ during active stroke
    // (in canvas coordinates).
    QRect strokeDirtyRect_;

    // Pre-composited display cache (DISPLAY domain):
    // white background + onion skin + all visible layers blended in order.
    QImage backgroundCache_;

    // Validity gate for backgroundCache_. Set false on frame change,
    // layer property change, zoom/pan. Set true after buildBackgroundCache().
    bool backgroundCacheValid_ = false;

    // Snapshot of pixelBuffer_ BEFORE stroke starts (for undo).
    QImage beforeSnapshot_;

    QPointF lastSamplePos_{-1, -1};
    QColor sampledColor_ = Qt::black;
    QColor brushColor_ = Qt::black;
    int brushSize_ = 20;
    QString brushShape_ = "Round";
    float strokeDistance_ = 0.0f;

    struct StampCache {
        QImage stamp;
        int size = -1;
        int calliRadius = -1;
        QString shape;
        QColor color;
        bool erasing = false;
        float opacity = -1.0f;
    };
    StampCache stampCache_;

    void drawBrushStamp(int cx, int cy);
    void drawLineStamps(const QPointF& from, const QPointF& to);
    QRect stampRect(int cx, int cy) const;
    void ensureStampCache(int effectiveRadius);
    void renderStampToImage(QImage& img, int radius, int cx, int cy,
                            const QString& shape, const QColor& color,
                            float opacity, bool erasing);
    void commitStroke();
    QImage captureRect(const QRect& r);
    QImage captureLayerRect(RasterLayer* layer, const QRect& r) const;
    QPointF widgetToCanvas(const QPointF& pos) const;
    QPointF canvasToWidget(const QPointF& pos) const;

    RasterLayer* activeRasterLayer();

    // Background cache management
    void buildBackgroundCache(const QRect& rect = QRect());

    // Shape drawing (Line, Rectangle, Ellipse)
    QPointF shapeStart_{-1, -1};
    QPointF shapeEnd_{-1, -1};
    void drawShape();

    // Fill tool
    void doFill(QPointF cpos);
    void floodFill(QImage& img, int x, int y, const QColor& fillColor,
                   const QColor& target, int tolerance);
    void floodFillByAlpha(QImage& img, int x, int y, const QColor& fillColor,
                          int boundaryAlpha);
    void floodFillFabric(QImage& img, int sx, int sy,
                              const QColor& baseColor, const QColor& target);
    void floodFillRamp(QImage& img, int sx, int sy, const QColor& baseColor);

    // Text tool
    void doText(QPointF cpos);
    bool editingText_ = false;
    QPointF textEditPos_;
    int textCursorPos_ = 0;
    int textSelectionAnchor_ = -1;
    bool caretVisible_ = true;
    QTimer* caretTimer_ = nullptr;
    void commitTextEdit();
    void loadTextEntry(int idx);
    std::map<int, std::vector<TextEntry>> textEntries_;
    int editingEntryIndex_ = -1;
    QPointF lastTextClickPos_;
    void deleteLastClickedTextEntry();
    void clearTextRaster(const TextEntry& entry);

    // Move tool
    bool moving_ = false;
    QImage moveImage_;
    QPointF moveStart_;
    QPointF moveOffset_;
    QRectF moveSrcRect_;
    void startMove(QPointF cpos);
    void updateMove(QPointF cpos);
    void commitMove();

    // Select tool
    enum class SelectionState : uint8_t { None, Creating, MovingContent, DraggingHandle };
    SelectionState selState_ = SelectionState::None;
    int selHandleIdx_ = -1;
    QPointF selDragStart_;
    QPointF selStart_;
    QRectF selRect_;
    QImage selImage_;
    bool floatingActive_ = false;
    QImage floatingSelection_;
    QImage originalFloatingSelection_;
    QElapsedTimer marchingTimer_;
    void commitSelection();
    void commitFloatingSelection();

    // Undo helper
    void pushFullLayerUndo(RasterLayer* layer, const QImage& beforeSnap, bool hadContentBefore = false);
    QImage snapFullLayer(RasterLayer* layer);
};

} // namespace fap
